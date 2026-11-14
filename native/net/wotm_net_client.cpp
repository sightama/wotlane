// ABOUTME: Network match client implementation: a thread that joins with the token over the match transport (ENet or WebSocket),
// ABOUTME: relays inputs with acks and redundancy, applies drops and stops on desync. The game only calls pushLocal/waitFrame/input.
#include "wotm_net_client.hpp"

#include <algorithm>

namespace wotm_net
{
    MatchClient::~MatchClient() { stop(); }

    bool MatchClient::start(const ClientConfig &c, std::string &err)
    {
        stop();
        cfg_ = c;
        net_ = makeTransport(c.host);
        if (!net_->open(c.host, c.port, err))
        {
            net_.reset();
            return false;
        }
        connected_ = false;
        const std::string where = isWebSocketAddress(c.host) ? c.host : c.host + ":" + std::to_string(c.port);
        {
            std::lock_guard<std::mutex> lk(mu_);
            status_ = Status::Connecting;
            message_ = "connecting to " + where;
        }
        stop_ = false;
        thread_ = std::thread([this] { run(); });
        return true;
    }

    void MatchClient::stop()
    {
        if (thread_.joinable())
        {
            stop_ = true;
            thread_.join();
        }
        if (net_)
        {
            net_->close();
            net_.reset();
        }
        connected_ = false;
        std::lock_guard<std::mutex> lk(mu_);
        if (status_ == Status::Connecting || status_ == Status::Waiting || status_ == Status::Running)
            status_ = Status::Idle;
        cv_.notify_all();
    }

    void MatchClient::setStatus(Status s, const std::string &msg)
    {
        status_ = s;
        message_ = msg;
        cv_.notify_all();
    }

    void MatchClient::run()
    {
        const std::string where = isWebSocketAddress(cfg_.host) ? cfg_.host : cfg_.host + ":" + std::to_string(cfg_.port);
        while (!stop_)
        {
            TransportEvent ev;
            int wait = 1;
            while (net_->poll(ev, wait))
            {
                wait = 0;
                if (ev.kind == TransportEvent::Kind::Connected)
                {
                    connected_ = true;
                    proto::Join j;
                    j.token = cfg_.token, j.buildId = cfg_.buildId, j.configHash = cfg_.configHash, j.name = cfg_.name;
                    net_->send(proto::kControl, proto::encode(j), true);
                }
                else if (ev.kind == TransportEvent::Kind::Message)
                    handle(ev.data);
                else
                {
                    std::lock_guard<std::mutex> lk(mu_);
                    if (status_ == Status::Connecting)
                        setStatus(Status::Lost, "no answer from the server " + where);
                    else if (status_ != Status::Rejected && status_ != Status::Desynced)
                        setStatus(Status::Lost, "connection to the server dropped");
                    connected_ = false;
                }
            }
            if (connected_)
            {
                rtt_ = net_->rttMs();
                sendInput(Clock::now());
            }
        }
    }

    void MatchClient::handle(const std::vector<uint8_t> &data)
    {
        std::lock_guard<std::mutex> lk(mu_);
        switch (proto::typeOf(data))
        {
        case proto::Type::Welcome:
        {
            proto::Welcome w;
            if (proto::decode(data, w))
            {
                local_ = w.player, players_ = w.players;
                setStatus(Status::Waiting, "waiting for the other players");
            }
            break;
        }
        case proto::Type::Reject:
        {
            proto::Reject r;
            proto::decode(data, r);
            setStatus(Status::Rejected, "rejected by the server: " + r.reason);
            break;
        }
        case proto::Type::Start:
        {
            proto::Start s;
            if (proto::decode(data, s) && local_ >= 0)
            {
                delay_ = s.delay;
                names_ = s.names;
                inputs_ = InputTimeline(players_, delay_);
                for (const auto &d : drops_)
                    inputs_.drop(d.first, d.second);
                serverAck_ = delay_;
                sentAcks_.assign(static_cast<size_t>(players_), 0u);
                setStatus(Status::Running, "match in progress");
            }
            break;
        }
        case proto::Type::Inputs:
        {
            proto::Inputs in;
            if (status_ != Status::Running || !proto::decode(data, in))
                break;
            serverAck_ = std::max(serverAck_, in.ackOwn);
            bool changed = false;
            for (const proto::InputRun &r : in.runs)
                for (size_t k = 0; k < r.states.size(); ++k)
                {
                    const uint32_t f = r.first + static_cast<uint32_t>(k);
                    if (!inputs_.has(r.player, f))
                        inputs_.set(r.player, f, r.states[k]), changed = true;
                }
            if (changed)
            {
                dirty_ = true;   // acknowledge what arrived
                cv_.notify_all();
            }
            else if (!in.runs.empty())
                dirty_ = true;   // a resend: the previous ack was lost
            break;
        }
        case proto::Type::Drop:
        {
            proto::Drop d;
            if (proto::decode(data, d))
            {
                drops_.push_back({d.player, d.frame});
                if (status_ == Status::Running)
                    inputs_.drop(d.player, d.frame);
                cv_.notify_all();
            }
            break;
        }
        case proto::Type::Desync:
        {
            proto::Desync d;
            if (proto::decode(data, d))
            {
                desyncFrame_ = d.frame;
                setStatus(Status::Desynced, "desync: " + d.detail);
            }
            break;
        }
        default:
            break;
        }
    }

    // sends the local inputs the server hasn't acknowledged and the acks for what has arrived: as soon as there is something new, and
    // every 16 ms while the server is behind
    void MatchClient::sendInput(Clock::time_point now)
    {
        std::vector<uint8_t> packet;
        std::vector<proto::Hash> hashes;
        {
            std::lock_guard<std::mutex> lk(mu_);
            hashes.swap(hashQueue_);
            if (status_ == Status::Running && local_ >= 0)
            {
                const uint32_t have = inputs_.nextMissing(local_);
                std::vector<uint32_t> acks(static_cast<size_t>(players_));
                for (int q = 0; q < players_; ++q)
                    acks[static_cast<size_t>(q)] = inputs_.nextMissing(q);
                const bool pending = serverAck_ < have;
                if (dirty_ || acks != sentAcks_ || (pending && now - lastSend_ > std::chrono::milliseconds(16)))
                {
                    proto::Input m;
                    m.acks = acks;
                    m.own.player = static_cast<uint8_t>(local_);
                    m.own.first = serverAck_;
                    const uint32_t to = std::min(have, serverAck_ + static_cast<uint32_t>(proto::kMaxRun));
                    for (uint32_t f = serverAck_; f < to; ++f)
                        m.own.states.push_back(inputs_.get(local_, f));
                    packet = proto::encode(m);
                    sentAcks_ = acks;
                    dirty_ = false;
                    lastSend_ = now;
                }
            }
        }
        for (const proto::Hash &h : hashes)
        {
            net_->send(proto::kControl, proto::encode(h), true);
        }
        if (!packet.empty() && !(cfg_.dropInputPercent > 0 && static_cast<int>(rng_() % 100u) < cfg_.dropInputPercent))
            net_->send(proto::kInputs, packet, false);
    }

    MatchClient::Status MatchClient::status() const
    {
        std::lock_guard<std::mutex> lk(mu_);
        return status_;
    }

    std::string MatchClient::message() const
    {
        std::lock_guard<std::mutex> lk(mu_);
        return message_;
    }

    bool MatchClient::waitStarted(int ms)
    {
        std::unique_lock<std::mutex> lk(mu_);
        cv_.wait_for(lk, std::chrono::milliseconds(ms),
                     [&] { return status_ != Status::Connecting && status_ != Status::Waiting; });
        return status_ == Status::Running;
    }

    int MatchClient::localPlayer() const
    {
        std::lock_guard<std::mutex> lk(mu_);
        return local_;
    }

    int MatchClient::players() const
    {
        std::lock_guard<std::mutex> lk(mu_);
        return players_;
    }

    uint32_t MatchClient::delay() const
    {
        std::lock_guard<std::mutex> lk(mu_);
        return delay_;
    }

    std::vector<std::string> MatchClient::names() const
    {
        std::lock_guard<std::mutex> lk(mu_);
        return names_;
    }

    void MatchClient::pushLocal(uint32_t frame, const PadState &s)
    {
        std::lock_guard<std::mutex> lk(mu_);
        if (status_ != Status::Running || local_ < 0)
            return;
        inputs_.set(local_, frame, s);
        dirty_ = true;
    }

    bool MatchClient::waitFrame(uint32_t frame, int ms)
    {
        std::unique_lock<std::mutex> lk(mu_);
        cv_.wait_for(lk, std::chrono::milliseconds(ms), [&] { return status_ != Status::Running || inputs_.ready(frame); });
        const bool ok = status_ == Status::Running && inputs_.ready(frame);
        if (ok && frame > 256u)
            inputs_.discardBefore(frame - 256u);
        return ok;
    }

    PadState MatchClient::input(int player, uint32_t frame) const
    {
        std::lock_guard<std::mutex> lk(mu_);
        return inputs_.get(player, frame);
    }

    std::vector<int> MatchClient::missing(uint32_t frame) const
    {
        std::lock_guard<std::mutex> lk(mu_);
        std::vector<int> out;
        for (int q = 0; q < players_; ++q)
            if (!inputs_.has(q, frame))
                out.push_back(q);
        return out;
    }

    void MatchClient::sendHash(uint32_t frame, uint64_t hash, std::vector<uint64_t> blocks)
    {
        std::lock_guard<std::mutex> lk(mu_);
        hashQueue_.push_back({frame, hash, std::move(blocks)});
    }

    bool MatchClient::waitAcked(int ms)
    {
        const auto until = Clock::now() + std::chrono::milliseconds(ms);
        for (;;)
        {
            {
                std::lock_guard<std::mutex> lk(mu_);
                if (status_ == Status::Running && serverAck_ >= inputs_.nextMissing(local_))
                    return true;
            }
            if (Clock::now() >= until)
                return false;
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
    }

    uint32_t MatchClient::dropFrame(int player) const
    {
        std::lock_guard<std::mutex> lk(mu_);
        uint32_t f = 0xFFFFFFFFu;
        for (const auto &d : drops_)
            if (d.first == player)
                f = std::min(f, d.second);
        return f;
    }

    uint32_t MatchClient::desyncFrame() const
    {
        std::lock_guard<std::mutex> lk(mu_);
        return desyncFrame_;
    }
}
