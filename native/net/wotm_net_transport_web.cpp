// ABOUTME: Browser build only: the match transport over the page's WebSocket. The socket lives on the main thread (browsers only
// ABOUTME: allow it there for the game's module); its events reach the client's thread through a queue, and sends go back as async calls.
#ifdef __EMSCRIPTEN__
#include "wotm_net_transport.hpp"

#include <emscripten.h>
#include <emscripten/threading.h>

#include <chrono>
#include <condition_variable>
#include <cstring>
#include <deque>
#include <map>
#include <memory>
#include <mutex>

namespace wotm_net
{
    namespace
    {
        // events from the page's WebSocket, per connection id
        struct Inbox
        {
            std::mutex mu;
            std::condition_variable cv;
            std::map<int, std::deque<TransportEvent>> events;
            int nextId = 1;
        };
        Inbox &inbox()
        {
            static Inbox box;
            return box;
        }
    }
}

extern "C"
{
    // the page's WebSocket calls these (main thread): a buffer for an incoming message, then the event (kind 0 open, 1 message, 2 closed)
    EMSCRIPTEN_KEEPALIVE uint8_t *wotm_net_web_alloc(int size) { return new uint8_t[static_cast<size_t>(size > 0 ? size : 1)]; }
    EMSCRIPTEN_KEEPALIVE void wotm_net_web_free(uint8_t *data) { delete[] data; }
    EMSCRIPTEN_KEEPALIVE void wotm_net_web_event(int id, int kind, uint8_t *data, int size)
    {
        using wotm_net::TransportEvent;
        TransportEvent ev;
        ev.kind = kind == 0 ? TransportEvent::Kind::Connected : kind == 1 ? TransportEvent::Kind::Message : TransportEvent::Kind::Closed;
        // a message is [channel][encoded message]; the channel only matters to ENet
        if (kind == 1 && data && size > 1)
            ev.data.assign(data + 1, data + size);
        delete[] data;
        auto &box = wotm_net::inbox();
        {
            std::lock_guard<std::mutex> lk(box.mu);
            box.events[id].push_back(std::move(ev));
        }
        box.cv.notify_all();
    }
}

namespace wotm_net
{
    namespace
    {
        EM_JS(void, wotm_net_web_open, (int id, const char *url), {
            const ws = new WebSocket(UTF8ToString(url));
            ws.binaryType = 'arraybuffer';
            (globalThis.wotmSockets ??= {})[id] = ws;
            ws.onopen = () => _wotm_net_web_event(id, 0, 0, 0);
            ws.onmessage = (e) => {
                if (!(e.data instanceof ArrayBuffer)) return;
                const bytes = new Uint8Array(e.data);
                const p = _wotm_net_web_alloc(bytes.length);
                HEAPU8.set(bytes, p);
                _wotm_net_web_event(id, 1, p, bytes.length);
            };
            let closed = false;
            const gone = () => { if (!closed) { closed = true; delete globalThis.wotmSockets[id]; _wotm_net_web_event(id, 2, 0, 0); } };
            ws.onclose = gone;
            ws.onerror = gone;
        });

        // `data` was allocated by the sender and is freed here, once the page has copied it
        EM_JS(void, wotm_net_web_send, (int id, uint8_t *data, int size), {
            const ws = globalThis.wotmSockets && globalThis.wotmSockets[id];
            if (ws && ws.readyState === 1) ws.send(HEAPU8.slice(data, data + size));
            _wotm_net_web_free(data);
        });

        EM_JS(void, wotm_net_web_close, (int id), {
            const ws = globalThis.wotmSockets && globalThis.wotmSockets[id];
            if (ws) { delete globalThis.wotmSockets[id]; ws.onopen = ws.onmessage = ws.onclose = ws.onerror = null; ws.close(); }
        });

        class WebTransport : public Transport
        {
        public:
            ~WebTransport() override { close(); }

            bool open(const std::string &host, uint16_t, std::string &) override
            {
                url_ = host;
                {
                    std::lock_guard<std::mutex> lk(inbox().mu);
                    id_ = inbox().nextId++;
                }
                // url_ outlives the call: it is only freed by close(), which also runs on the main thread, after this
                emscripten_async_run_in_main_runtime_thread(EM_FUNC_SIG_VII, &wotm_net_web_open, id_, url_.c_str());
                return true;
            }

            void send(uint8_t channel, const std::vector<uint8_t> &data, bool) override
            {
                if (!id_)
                    return;
                const int size = static_cast<int>(data.size() + 1);
                uint8_t *copy = wotm_net_web_alloc(size);
                copy[0] = channel;
                std::memcpy(copy + 1, data.data(), data.size());
                emscripten_async_run_in_main_runtime_thread(EM_FUNC_SIG_VIII, &wotm_net_web_send, id_, copy, size);
            }

            bool poll(TransportEvent &ev, int waitMs) override
            {
                if (!id_)
                    return false;
                auto &box = inbox();
                std::unique_lock<std::mutex> lk(box.mu);
                auto &q = box.events[id_];
                if (q.empty() && waitMs > 0)
                    box.cv.wait_for(lk, std::chrono::milliseconds(waitMs), [&] { return !q.empty(); });
                if (q.empty())
                    return false;
                ev = std::move(q.front());
                q.pop_front();
                return true;
            }

            // the browser answers the server's pings itself and doesn't tell the page the time: the server measures it
            uint32_t rttMs() const override { return 0; }

            void close() override
            {
                if (!id_)
                    return;
                emscripten_async_run_in_main_runtime_thread(EM_FUNC_SIG_VI, &wotm_net_web_close, id_);
                std::lock_guard<std::mutex> lk(inbox().mu);
                inbox().events.erase(id_);
                id_ = 0;
            }

        private:
            std::string url_;
            int id_ = 0;
        };
    }

    std::unique_ptr<Transport> makeWebSocketTransport() { return std::make_unique<WebTransport>(); }
}
#endif
