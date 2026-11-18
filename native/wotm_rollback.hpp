// ABOUTME: Rollback building blocks with no game in them: a snapshot made of named pieces of state (saved, restored and compared byte
// ABOUTME: by byte, naming the first piece and offset that differ) and the synctest schedule (every frame, go n frames back and redo them).
#pragma once

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <functional>
#include <memory>
#include <string>
#include <type_traits>
#include <vector>

namespace wotm::rollback {

using Blob = std::shared_ptr<const std::vector<uint8_t>>;

// Serialized state: bytes, plus data that never changes once made (a music stream's samples), shared by every snapshot instead of
// copied each frame. A blob is stored as its index in the snapshot's table.
struct Writer {
    std::vector<uint8_t> &out;
    std::vector<Blob> &blobs;

    void bytes(const void *p, size_t n) {
        const auto *b = static_cast<const uint8_t *>(p);
        out.insert(out.end(), b, b + n);
    }
    template <class T> void pod(const T &v) {
        static_assert(std::is_trivially_copyable_v<T>);
        bytes(&v, sizeof v);
    }
    void blob(const Blob &b) {
        uint32_t i = 0;
        while (i < blobs.size() && blobs[i] != b) ++i;
        if (i == blobs.size()) blobs.push_back(b);
        pod(i);
    }
};

struct Reader {
    const uint8_t *p;
    const uint8_t *end;
    const std::vector<Blob> &blobs;
    bool ok = true;   // false once a read ran past the end (a snapshot that doesn't match the code reading it)

    void bytes(void *dst, size_t n) {
        if (ok && static_cast<size_t>(end - p) >= n) {
            std::memcpy(dst, p, n);
            p += n;
            return;
        }
        ok = false;
        std::memset(dst, 0, n);
    }
    template <class T> T pod() {
        static_assert(std::is_trivially_copyable_v<T>);
        T v;
        bytes(&v, sizeof v);
        return v;
    }
    template <class T> void pod(T &v) { v = pod<T>(); }
    // a count of items of at least `each` bytes that follow: 0 and not ok when the bytes left can't hold that many
    uint32_t count(size_t each) {
        const uint32_t n = pod<uint32_t>();
        if (ok && n <= static_cast<size_t>(end - p) / std::max<size_t>(each, 1)) return n;
        ok = false;
        return 0;
    }
    Blob blob() {
        const auto i = pod<uint32_t>();
        if (i < blobs.size()) return blobs[i];
        ok = false;
        return nullptr;
    }
};

// One part of the state: `save` appends it, `load` reads back exactly what `save` wrote. `address` is where byte 0 lives in the
// guest's memory map, so a difference in a RAM piece is reported as a guest address (kNoAddress: host state).
struct Piece {
    static constexpr uint32_t kNoAddress = 0xFFFFFFFFu;
    std::string name;
    std::function<void(Writer &)> save;
    std::function<void(Reader &)> load;
    uint32_t address = kNoAddress;
};

// A piece over a fixed block of memory (guest RAM, the CPU context, a host static)
inline Piece memoryPiece(std::string name, void *data, size_t size, uint32_t address = Piece::kNoAddress) {
    return {std::move(name), [data, size](Writer &w) { w.bytes(data, size); }, [data, size](Reader &r) { r.bytes(data, size); }, address};
}

struct Snapshot {
    uint32_t frame = 0;
    std::vector<uint8_t> bytes;
    std::vector<Blob> blobs;
    std::vector<size_t> ends;   // where each piece's bytes end
};

class State {
public:
    void add(Piece p) { pieces_.push_back(std::move(p)); }
    size_t size() const { return pieces_.size(); }
    const Piece &piece(size_t i) const { return pieces_[i]; }

    void save(Snapshot &s, uint32_t frame) const {
        s.frame = frame;
        s.bytes.clear();
        s.blobs.clear();
        s.ends.clear();
        Writer w{s.bytes, s.blobs};
        for (const Piece &p : pieces_) {
            p.save(w);
            s.ends.push_back(s.bytes.size());
        }
    }

    // false when a piece read a different size than its save wrote (the snapshot is then partly applied)
    bool load(const Snapshot &s) const {
        if (s.ends.size() != pieces_.size()) return false;
        size_t at = 0;
        for (size_t i = 0; i < pieces_.size(); ++i) {
            Reader r{s.bytes.data() + at, s.bytes.data() + s.ends[i], s.blobs};
            pieces_[i].load(r);
            if (!r.ok || r.p != r.end) return false;
            at = s.ends[i];
        }
        return true;
    }

    struct Diff {
        bool equal = true;
        size_t piece = 0;    // the first piece that differs
        size_t offset = 0;   // its first differing byte
        size_t bytes = 0;    // how many of its bytes differ (its size when the sizes differ)
    };

    // a and b were saved from this State; a difference past the last piece (piece == size()) is in the shared data
    Diff compare(const Snapshot &a, const Snapshot &b) const {
        Diff d;
        if (a.ends.size() != b.ends.size())
            return {false, std::min(a.ends.size(), b.ends.size()), 0, 0};
        for (size_t i = 0; i < pieces_.size() && i < a.ends.size() && i < b.ends.size(); ++i) {
            const size_t atA = i ? a.ends[i - 1] : 0, atB = i ? b.ends[i - 1] : 0;
            const size_t na = a.ends[i] - atA, nb = b.ends[i] - atB;
            const uint8_t *pa = a.bytes.data() + atA, *pb = b.bytes.data() + atB;
            if (na != nb) {
                const size_t n = std::min(na, nb);
                return {false, i, static_cast<size_t>(std::mismatch(pa, pa + n, pb).first - pa), std::max(na, nb)};
            }
            if (std::memcmp(pa, pb, na) != 0) {
                d.equal = false;
                d.piece = i;
                d.offset = static_cast<size_t>(std::mismatch(pa, pa + na, pb).first - pa);
                for (size_t k = d.offset; k < na; ++k) d.bytes += pa[k] != pb[k];
                return d;
            }
        }
        // the same data read twice (a stream started again in a redone frame) is two blobs with the same bytes
        const auto same = [](const Blob &x, const Blob &y) { return x == y || (x && y && *x == *y); };
        if (a.blobs.size() != b.blobs.size() || !std::equal(a.blobs.begin(), a.blobs.end(), b.blobs.begin(), same))
            d = {false, pieces_.size(), 0, 0};
        return d;
    }

private:
    std::vector<Piece> pieces_;
};

// GGPO's synctest, single player: at every frame boundary after n frames of history, the state is saved, the one from n frames
// back is restored and those n frames run again with the same inputs; back at the frame where it started, the state must equal
// the one saved there. A barrier (a stage load, a dialog loop: frames that are not boundaries of the fight loop) starts the
// history over, so a rollback never crosses one.
class SyncTest {
public:
    enum class Step {
        Save,          // save the state of this frame
        SaveAndBack,   // save it, then restore `back()` and run the frames up to this one again
        Resimulated,   // a frame on the way back to the target: nothing to do
        Compare,       // back at the target: compare the state with the one saved here
        Overshot,      // a redo reached a later frame than the target without passing it: it diverged (the history starts over)
    };

    explicit SyncTest(uint32_t frames) : n_(std::max<uint32_t>(frames, 1u)) {}
    uint32_t frames() const { return n_; }
    uint32_t slots() const { return n_ + 1u; }
    uint32_t slot(uint32_t frame) const { return frame % slots(); }
    bool resimulating() const { return resim_; }
    uint32_t target() const { return target_; }
    uint32_t back() const { return target_ - n_; }

    Step boundary(uint32_t frame) {
        if (resim_) {
            if (frame < target_) return Step::Resimulated;
            resim_ = false;
            if (frame != target_) started_ = false;
            return frame == target_ ? Step::Compare : Step::Overshot;
        }
        if (!started_) {
            started_ = true;
            first_ = frame;
        }
        if (frame - first_ < n_) return Step::Save;
        resim_ = true;
        target_ = frame;
        return Step::SaveAndBack;
    }

    // true when the barrier came in the middle of a resimulation: the original frames had none there, so the run diverged
    bool barrier() {
        const bool broke = resim_;
        started_ = resim_ = false;
        return broke;
    }

private:
    uint32_t n_;
    bool started_ = false, resim_ = false;
    uint32_t first_ = 0, target_ = 0;
};

}  // namespace wotm::rollback
