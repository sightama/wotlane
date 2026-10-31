// Movies (FMV) in the native layer: the disc's .PSS files played with FFmpeg (MPEG-2 video) and raylib (picture and sound).
//
// In retail, CMovie (common/movies) and playMpegMovieWithDB/updateMpegMovieDecoding (common/MpegMovie) build on SCE's "ezmpeg"
// sample: disc reads, sceMpegDemuxPssRing, a thread that feeds the IPU over DMA and a vblank handler that sends
// each frame to VRAM via PATH3. None of that hardware exists on PC: the cut here is at those game functions (hooks in the runtime,
// `wotm::fmv::hooks`), and the movie is read straight from the disc file.
//
// Uses in the game (read from the asm):
//   - Fullscreen: `playMovie` (game/ui) calls `playMpegMovieWithDB("mov/intro_m.pss", withAudio)`; then, every menu frame,
//     `uiMain` calls `updateMpegMovieDecoding(keepGoing)` while `movieRequested` (0x6F8840) is 1. At the end (or with keepGoing = 0,
//     the player skipped) `termAll` clears `movieRequested` and `movieIsPlaying` (0x6F8650) and sets `movieIsCompletelyFinished` (0x6F8844),
//     which `uiIntro`/`uiOutro` wait on. Movies: intro_m, int_main, int_act1..4, <monster>_p and <monster>_g (endings), credits.
//   - Texture in the 3D menu: the menu screens call `moviesReadyScreenObject(obj, 8, 7)` and `myMovie.Play(false, "mov/sheldemo.pss")`
//     every frame (Play only restarts once the previous movie has ended: that's how it loops) and `myMovie.Update(keepGoing)`.
//     `Update` calls `moviesSetScreenObject` after 16 frames (or once the frame is already in VRAM), which swaps the object's texture
//     for the movie's: texId 999 (0x3E7), numTextures 1 and TEX0 = CT32 at 0x3BA0 with 2^wid x 2^hei (256x128). Monster select and
//     stage select also call Play (with other names); the small .PSS files (256x128, no audio: alien.pss, central.pss...) should be
//     those previews (not verified in the game).
//   - CMovie (myMovie, 0x423D40): +0x50 with audio, +0x88 started showing, +0x94 stopped (1 = no movie; Play only acts on 1).
//
// .PSS format (checked with tools/pss.py and ffprobe): MPEG-2 program stream in 2048-byte packs; video in stream 0xE0
// (MPEG-2 MP@ML, 640x368 for the fullscreen ones and 256x128 for the menu ones, 29.97 frames/s); audio in private stream 0xBD (substream 0xFF,
// 4 header bytes per PES), with an "SShd" header (type 1 = 16-bit PCM, rate 48000, 2 channels, interleave 0x200) followed by
// "SSbd" + size and the data: 512-byte blocks alternating left and right. FFmpeg doesn't recognize this audio; the
// demuxer here separates the two streams and the PCM is played directly.
#pragma once
#include "wotm_syms.h"   // wotm::sym / wotm::fn: ELF addresses by name (tools/native_syms.py --gen)
#include "wotm_disc.hpp"
#include "wotm_settings.hpp"   // SkipIntro

#include <algorithm>
#include <atomic>
#include <cctype>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <filesystem>
#include <mutex>
#include <string>
#include <vector>

#include "raylib.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/frame.h>
}

namespace wotm::fmv {

// Disc reads (folder or ISO): wotm_disc.hpp
using wotm::disc::Source;
using wotm::disc::seek64;
using wotm::disc::isoFind;
using wotm::disc::discRelative;
using wotm::disc::resolveDiscPath;

// ---------------------------------------------------------------- .PSS demuxer

class PssDemux {
public:
    enum Kind { End = 0, Video = 1, Audio = 2 };

    bool open(const Source &src) {
        close();
        f_ = std::fopen(src.path.c_str(), "rb");
        if (f_ && src.offset && seek64(f_, src.offset) != 0) close();
        left_ = src.size;
        buf_.assign(1 << 18, 0);
        pos_ = len_ = 0;
        eof_ = false;
        return f_ != nullptr;
    }
    void close() {
        if (f_) std::fclose(f_);
        f_ = nullptr;
    }
    ~PssDemux() { close(); }

    // Next chunk of elementary stream (video, or audio without the 4 substream bytes); End at end of file
    Kind next(const uint8_t *&data, size_t &n) {
        for (;;) {
            if (!fill(4)) return End;
            const uint8_t *p = buf_.data() + pos_;
            if (p[0] || p[1] || p[2] != 1) { ++pos_; continue; }
            const uint8_t sid = p[3];
            if (sid == 0xBA) {   // pack header (MPEG-2: 14 bytes + stuffing)
                if (!fill(14)) return End;
                p = buf_.data() + pos_;
                pos_ += (p[4] >> 6) == 1 ? 14u + (p[13] & 7u) : 12u;
                continue;
            }
            if (sid == 0xB9) return End;
            if (sid < 0xBB) { ++pos_; continue; }
            if (!fill(6)) return End;
            p = buf_.data() + pos_;
            const size_t ln = (size_t(p[4]) << 8) | p[5];
            if (!fill(6 + ln)) return End;
            p = buf_.data() + pos_;
            pos_ += 6 + ln;
            const bool video = sid >= 0xE0 && sid <= 0xEF;
            if (!video && sid != 0xBD) continue;
            const uint8_t *pay = p + 6;
            if (ln < 3 || (pay[0] & 0xC0) != 0x80) continue;   // MPEG-2 PES only
            const size_t hl = 3u + pay[2];
            if (hl > ln) continue;
            data = pay + hl;
            n = ln - hl;
            if (video) return Video;
            if (n <= 4) continue;
            data += 4;   // substream (0xFF) + 3 bytes
            n -= 4;
            return Audio;
        }
    }

private:
    FILE *f_ = nullptr;
    std::vector<uint8_t> buf_;
    size_t pos_ = 0, len_ = 0;
    bool eof_ = false;
    uint64_t left_ = UINT64_MAX;   // bytes that still belong to the file (inside the ISO)

    bool fill(size_t need) {
        if (len_ - pos_ >= need) return true;
        if (pos_) { std::memmove(buf_.data(), buf_.data() + pos_, len_ - pos_); len_ -= pos_; pos_ = 0; }
        if (need > buf_.size()) buf_.resize(need);
        while (!eof_ && len_ < need && f_) {
            const size_t want = size_t(std::min<uint64_t>(buf_.size() - len_, left_));
            const size_t got = want ? std::fread(buf_.data() + len_, 1, want, f_) : 0;
            if (got == 0) eof_ = true;
            len_ += got;
            left_ -= got;
        }
        return len_ - pos_ >= need;
    }
};

// ---------------------------------------------------------------- a movie playing

struct Frame {
    std::vector<uint32_t> rgba;
    int w = 0, h = 0;
};

class Movie {
public:
    int w = 0, h = 0;
    float aspect = 4.f / 3.f;   // display width/height (MPEG-2 carries the pixel aspect ratio)
    Texture2D tex{};
    bool hasFrame = false;
    std::vector<int16_t> *audioTap = nullptr;   // diagnostic (fmvplay --wav): copy of the decoded PCM

    ~Movie() { close(); }

    bool open(const Source &src, bool withAudio) {
        close();
        if (!demux_.open(src)) return false;
        const AVCodec *c = avcodec_find_decoder(AV_CODEC_ID_MPEG2VIDEO);
        ctx_ = c ? avcodec_alloc_context3(c) : nullptr;
        if (!ctx_ || avcodec_open2(ctx_, c, nullptr) < 0) { close(); return false; }
        parser_ = av_parser_init(AV_CODEC_ID_MPEG2VIDEO);
        pkt_ = av_packet_alloc();
        frame_ = av_frame_alloc();
        if (!parser_ || !pkt_ || !frame_) { close(); return false; }
        wantAudio_ = withAudio;
        open_ = true;
        return true;
    }

    void close() {
        if (streamOn_) { StopAudioStream(stream_); UnloadAudioStream(stream_); streamOn_ = false; }
        if (tex.id) UnloadTexture(tex);
        tex = Texture2D{};
        if (parser_) av_parser_close(parser_);
        if (ctx_) avcodec_free_context(&ctx_);
        if (pkt_) av_packet_free(&pkt_);
        if (frame_) av_frame_free(&frame_);
        parser_ = nullptr;
        demux_.close();
        frames_.clear();
        pcm_.clear();
        blk_.clear();   // otherwise the leftover of an L/R pair from the previous movie shifts the next one's PCM (hiss)
        audioHdr_.clear();
        audioState_ = 0;
        demuxEnd_ = flushed_ = false;
        open_ = hasFrame = false;
        shown_ = 0;
        t0_ = -1.0;
        w = h = 0;
    }

    bool isOpen() const { return open_; }
    long shownFrames() const { return shown_; }      // frames shown since the start
    const Frame &current() const { return stage_; } // pixels of the frame on screen (RGBA)

    // Advances to the frame due at `now` (seconds, any clock) and uploads the texture. False when the movie has ended.
    bool update(double now) {
        if (!open_) return false;
        if (t0_ < 0) t0_ = now;
        const double fps = 30000.0 / 1001.0;
        const double elapsed = now - t0_;
        pump();
        bool due = false;
        while (!frames_.empty() && double(shown_) / fps <= elapsed) {
            if (due) recycle(std::move(stage_));   // late frame: keep only the newest
            stage_ = std::move(frames_[0]);
            frames_.pop_front();
            due = true;
            ++shown_;
            pump();
        }
        if (due) upload(stage_);
        feedAudio();
        return !(demuxEnd_ && flushed_ && frames_.empty() && double(shown_) / fps <= elapsed);
    }

private:
    PssDemux demux_;
    AVCodecContext *ctx_ = nullptr;
    AVCodecParserContext *parser_ = nullptr;
    AVPacket *pkt_ = nullptr;
    AVFrame *frame_ = nullptr;
    std::deque<Frame> frames_;
    std::vector<Frame> pool_;
    Frame stage_;
    bool open_ = false, demuxEnd_ = false, flushed_ = false;
    long shown_ = 0;
    double t0_ = -1.0;

    // audio: SShd/SSbd header and interleaved stereo PCM (s16)
    bool wantAudio_ = false;
    int audioState_ = 0;                 // 0 reading header, 1 PCM data, 2 ignored (unknown format)
    std::vector<uint8_t> audioHdr_;
    uint32_t rate_ = 48000, channels_ = 2, interleave_ = 0x200;
    std::vector<uint8_t> blk_;           // a pair of blocks (left + right) being assembled
    std::deque<int16_t> pcm_;            // interleaved samples L R L R
    AudioStream stream_{};
    bool streamOn_ = false;
    static constexpr int kChunk = 2048;  // audio frames per submission

    void recycle(Frame &&f) { if (pool_.size() < 4) pool_.push_back(std::move(f)); }

    // Reads from the file until a few frames are ready (and enough audio), or until the end
    void pump() {
        while (!flushed_ && (frames_.size() < 3 || (streamOn_ && pcm_.size() < size_t(kChunk) * 4)) && frames_.size() < 24) {
            if (demuxEnd_) { decode(nullptr, 0); flushed_ = true; break; }
            const uint8_t *d = nullptr;
            size_t n = 0;
            switch (demux_.next(d, n)) {
            case PssDemux::Video: decode(d, n); break;
            case PssDemux::Audio: audio(d, n); break;
            default: demuxEnd_ = true; break;
            }
        }
    }

    void decode(const uint8_t *d, size_t n) {
        if (!d) {   // end: flushes the parser and the decoder
            uint8_t *out = nullptr;
            int outn = 0;
            av_parser_parse2(parser_, ctx_, &out, &outn, nullptr, 0, AV_NOPTS_VALUE, AV_NOPTS_VALUE, 0);
            if (outn) send(out, outn);
            avcodec_send_packet(ctx_, nullptr);
            receive();
            return;
        }
        while (n > 0) {
            uint8_t *out = nullptr;
            int outn = 0;
            const int used = av_parser_parse2(parser_, ctx_, &out, &outn, d, int(n), AV_NOPTS_VALUE, AV_NOPTS_VALUE, 0);
            if (used < 0) return;
            d += used;
            n -= size_t(used);
            if (outn) send(out, outn);
        }
    }

    void send(uint8_t *data, int size) {
        pkt_->data = data;
        pkt_->size = size;
        if (avcodec_send_packet(ctx_, pkt_) >= 0) receive();
    }

    void receive() {
        while (avcodec_receive_frame(ctx_, frame_) >= 0) {
            Frame f;
            if (!pool_.empty()) { f = std::move(pool_.back()); pool_.pop_back(); }
            toRgba(frame_, f);
            if (frame_->sample_aspect_ratio.num > 0 && frame_->sample_aspect_ratio.den > 0)
                aspect = float(frame_->width) * float(frame_->sample_aspect_ratio.num) / (float(frame_->height) * float(frame_->sample_aspect_ratio.den));
            else
                aspect = float(frame_->width) / float(std::max(1, frame_->height));
            frames_.push_back(std::move(f));
            av_frame_unref(frame_);
        }
    }

    // YUV 4:2:0 (BT.601, TV range) -> RGBA, like the IPU's CSC
    static void toRgba(const AVFrame *fr, Frame &f) {
        f.w = fr->width;
        f.h = fr->height;
        f.rgba.resize(size_t(f.w) * f.h);
        const bool p420 = fr->format == AV_PIX_FMT_YUV420P || fr->format == AV_PIX_FMT_YUVJ420P;
        const int csh = p420 ? 1 : 0;   // 4:2:2 (rare) uses the same chroma row
        for (int y = 0; y < f.h; ++y) {
            const uint8_t *Y = fr->data[0] + size_t(y) * fr->linesize[0];
            const uint8_t *U = fr->data[1] + size_t(y >> csh) * fr->linesize[1];
            const uint8_t *V = fr->data[2] + size_t(y >> csh) * fr->linesize[2];
            uint32_t *o = f.rgba.data() + size_t(y) * f.w;
            for (int x = 0; x < f.w; ++x) {
                const int c = 298 * (int(Y[x]) - 16), d = int(U[x >> 1]) - 128, e = int(V[x >> 1]) - 128;
                const int r = std::clamp((c + 409 * e + 128) >> 8, 0, 255);
                const int g = std::clamp((c - 100 * d - 208 * e + 128) >> 8, 0, 255);
                const int b = std::clamp((c + 516 * d + 128) >> 8, 0, 255);
                o[x] = uint32_t(r) | (uint32_t(g) << 8) | (uint32_t(b) << 16) | 0xFF000000u;
            }
        }
    }

    void upload(const Frame &f) {
        if (f.rgba.empty()) return;
        if (!tex.id || tex.width != f.w || tex.height != f.h) {
            if (tex.id) UnloadTexture(tex);
            Image img{const_cast<uint32_t *>(f.rgba.data()), f.w, f.h, 1, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8};
            tex = LoadTextureFromImage(img);
            SetTextureFilter(tex, TEXTURE_FILTER_BILINEAR);
            SetTextureWrap(tex, TEXTURE_WRAP_CLAMP);
        } else {
            UpdateTexture(tex, f.rgba.data());
        }
        w = f.w;
        h = f.h;
        hasFrame = true;
    }

    void audio(const uint8_t *d, size_t n) {
        if (!wantAudio_ || audioState_ == 2) return;
        if (audioState_ == 0) {
            audioHdr_.insert(audioHdr_.end(), d, d + n);
            if (audioHdr_.size() < 40) return;
            auto rd = [&](size_t o) { uint32_t v; std::memcpy(&v, audioHdr_.data() + o, 4); return v; };
            const uint32_t hsz = rd(4);
            if (std::memcmp(audioHdr_.data(), "SShd", 4) != 0 || hsz < 16 || audioHdr_.size() < 8 + hsz + 8 ||
                std::memcmp(audioHdr_.data() + 8 + hsz, "SSbd", 4) != 0) {
                audioState_ = 2;
                return;
            }
            const uint32_t type = rd(8);
            rate_ = rd(12);
            channels_ = rd(16);
            interleave_ = rd(20);
            if (type != 1 || channels_ < 1 || channels_ > 2 || rate_ < 8000 || rate_ > 96000 || interleave_ == 0 || interleave_ > 0x10000) {
                std::fprintf(stderr, "[fmv] unsupported PSS audio (type %u, %u Hz, %u channels, interleave %u)\n", type, rate_, channels_, interleave_);
                audioState_ = 2;
                return;
            }
            audioState_ = 1;
            const std::vector<uint8_t> rest(audioHdr_.begin() + 8 + hsz + 8, audioHdr_.end());
            audioHdr_.clear();
            if (IsAudioDeviceReady() && !std::getenv("WOTM_FMV_MUTE")) {
                SetAudioStreamBufferSizeDefault(kChunk);
                stream_ = LoadAudioStream(rate_, 16, 2);
                streamOn_ = IsAudioStreamValid(stream_);
                if (streamOn_) PlayAudioStream(stream_);
            }
            if (!rest.empty()) audio(rest.data(), rest.size());
            return;
        }
        // blocks of `interleave_` bytes per channel: L0 R0 L1 R1 ... (mono: L only)
        const size_t pair = size_t(interleave_) * channels_;
        while (n > 0) {
            const size_t take = std::min(n, pair - blk_.size());
            blk_.insert(blk_.end(), d, d + take);
            d += take;
            n -= take;
            if (blk_.size() == pair) {
                const size_t ns = interleave_ / 2;
                for (size_t i = 0; i < ns; ++i) {
                    int16_t l, r;
                    std::memcpy(&l, blk_.data() + 2 * i, 2);
                    if (channels_ == 2) std::memcpy(&r, blk_.data() + interleave_ + 2 * i, 2);
                    else r = l;
                    pcm_.push_back(l);
                    pcm_.push_back(r);
                    if (audioTap) { audioTap->push_back(l); audioTap->push_back(r); }
                }
                blk_.clear();
            }
        }
    }

    void feedAudio() {
        if (!streamOn_) return;
        static int16_t buf[kChunk * 2];
        while (IsAudioStreamProcessed(stream_)) {
            if (pcm_.size() >= size_t(kChunk) * 2) {
                for (int i = 0; i < kChunk * 2; ++i) { buf[i] = pcm_.front(); pcm_.pop_front(); }
            } else if (demuxEnd_) {   // remaining audio at the end, padded with silence
                const size_t k = pcm_.size();
                for (size_t i = 0; i < size_t(kChunk) * 2; ++i) buf[i] = i < k ? pcm_[i] : 0;
                pcm_.clear();
                if (!k) break;
            } else {
                break;
            }
            UpdateAudioStream(stream_, buf, kChunk);
        }
    }
};

// ---------------------------------------------------------------- runtime player (requests from the EE thread, drawing on the window thread)

// Writes an RGBA frame into GS VRAM (PSMCT32, GS page/block/column addressing) at `tbp` with width `tbw` x 64
inline void vramWriteCT32(uint8_t *vram, size_t vramSize, uint32_t tbp, uint32_t tbw, const Frame &f) {
    static constexpr uint8_t blk[4][8] = {{0, 1, 4, 5, 16, 17, 20, 21}, {2, 3, 6, 7, 18, 19, 22, 23}, {8, 9, 12, 13, 24, 25, 28, 29}, {10, 11, 14, 15, 26, 27, 30, 31}};
    static constexpr uint8_t col[8][8] = {{0, 1, 4, 5, 8, 9, 12, 13}, {2, 3, 6, 7, 10, 11, 14, 15}, {16, 17, 20, 21, 24, 25, 28, 29}, {18, 19, 22, 23, 26, 27, 30, 31},
                                          {32, 33, 36, 37, 40, 41, 44, 45}, {34, 35, 38, 39, 42, 43, 46, 47}, {48, 49, 52, 53, 56, 57, 60, 61}, {50, 51, 54, 55, 58, 59, 62, 63}};
    const uint32_t w = std::min<uint32_t>(uint32_t(f.w), tbw * 64);
    for (uint32_t y = 0; y < uint32_t(f.h); ++y)
        for (uint32_t x = 0; x < w; ++x) {
            const uint32_t page = (tbp >> 5) + (y >> 5) * tbw + (x >> 6);
            const uint32_t b = (tbp & 0x1F) + blk[(y >> 3) & 3][(x >> 3) & 7];
            const size_t a = (size_t(page) << 13) + (size_t(b >> 5) << 13) + size_t(b & 0x1F) * 256 + size_t(col[y & 7][x & 7]) * 4;
            if (a + 4 > vramSize) continue;
            const uint32_t c = f.rgba[size_t(y) * f.w + x];
            const uint32_t gs = (c & 0x00FFFFFFu) | 0x80000000u;   // alpha 0x80 = 1.0 on the GS
            std::memcpy(vram + a, &gs, 4);
        }
}

class Player {
public:
    enum Slot { Full = 0, Screen = 1 };

    static Player &get() { static Player p; return p; }

    // Where the movies are: extracted disc folder and/or ISO image (WOTM_DISC / WOTM_ISO take priority)
    std::filesystem::path discRoot, isoPath;

    Source locate(const std::string &gameName) const {
        const std::string rel = discRelative(gameName);
        if (std::string p = wotm::mods::Registry::get().hostPath(rel); !p.empty()) return Source{p};   // mods/ (wotm_mods.hpp)
        if (const char *d = std::getenv("WOTM_DISC"))
            if (std::string p = resolveDiscPath(d, rel); !p.empty()) return Source{p};
        if (std::string p = resolveDiscPath(discRoot, rel); !p.empty()) return Source{p};
        const char *iso = std::getenv("WOTM_ISO");
        const std::string isoFile = iso ? std::string(iso) : isoPath.string();
        if (!isoFile.empty()) return isoFind(isoFile, rel);
        return {};
    }

    // ---- EE thread (hooks)
    void start(Slot s, const std::string &gameName, bool audio) {
        const Source src = locate(gameName);
        std::lock_guard<std::mutex> lk(m_);
        Req &r = req_[s];
        // held (setHold) and already playing the same movie: keep going (returning from the native select screen to the menu calls Play again,
        // and reopening left the big screen black until the first frame)
        if (hold_[s].load() && active_[s].load() && !done_[s].load() && r.name == gameName) return;
        r.src = src;
        r.name = gameName;
        r.audio = audio;
        r.open = true;
        r.stop = false;
        done_[s].store(false);
        active_[s].store(true);
        if (s == Full) {
            skip_.store(false);
            startedAt_.store(std::chrono::steady_clock::now().time_since_epoch().count());
        }
        std::fprintf(stderr, "[fmv] %s: %s -> %s @%llu\n", s == Full ? "fullscreen" : "texture", gameName.c_str(), src.ok() ? src.path.c_str() : "(not found)",
                     (unsigned long long)src.offset);
    }
    void stop(Slot s) {
        if (hold_[s].load()) return;   // setHold: the game ends the movie, but whoever holds it keeps it playing
        std::lock_guard<std::mutex> lk(m_);
        req_[s].stop = true;
        req_[s].open = false;
        active_[s].store(false);
    }
    // Holds the slot's movie: stop() has no effect while on (native select screen: the game's screen 3 ends the big-screen movie, which it
    // keeps showing without restarting). Turning it off doesn't stop anything.
    void setHold(Slot s, bool on) { hold_[s].store(on); }
    bool finished(Slot s) const { return done_[s].load(); }
    bool active(Slot s) const { return active_[s].load(); }
    bool hasFrame(Slot s) const { return shown_[s].load(); }
    // The player pressed confirm (X/Space, Start/Enter) during the fullscreen movie; the update hook treats it as "skipped"
    bool skipRequested() const { return skip_.load(); }
    // The same skip via the pad the game reads (emulated pad, PS2X_PAD script): a new press of X or Start, without the 600-frame wait that
    // uiMain imposes on intro_m. EE thread (updateMpegMovieDecoding hook); `buttons` active-low, as in libpad.
    void padInput(uint16_t buttons) {
        const uint16_t down = uint16_t(~buttons) & 0x4008u;   // cross 0x4000, start 0x0008
        if ((down & ~padDown_) && skipAllowed()) skip_.store(true);
        padDown_ = down;
    }
    // Skipping only works 100 ms after the movie starts: a long press that skipped the previous one doesn't skip the next one too
    bool skipAllowed() const {
        const auto t0 = std::chrono::steady_clock::time_point(std::chrono::steady_clock::duration(startedAt_.load()));
        return std::chrono::steady_clock::now() - t0 >= std::chrono::milliseconds(100);
    }

    // ---- window thread (before drawing): opens/closes and advances the movies. With `vram`, the menu movie's frame also goes
    // into GS VRAM where retail puts it (0x3BA0, width 320: VramAddrForMovies/VramWidthForMovies), for the menu drawn
    // by the emulated GS.
    void pump(uint8_t *vram = nullptr, size_t vramSize = 0, double clock = -1.0) {
        Req r[2];
        {
            std::lock_guard<std::mutex> lk(m_);
            for (int i = 0; i < 2; ++i) { r[i] = req_[i]; req_[i].open = req_[i].stop = false; }
        }
        const double now = clock >= 0.0 ? clock : GetTime();   // clock: test clock
        for (int i = 0; i < 2; ++i) {
            if (r[i].stop) { mov_[i].close(); shown_[i].store(false); }
            if (r[i].open) {
                shown_[i].store(false);
                lastFrame_[i] = -1;
                if (!r[i].src.ok() || !mov_[i].open(r[i].src, r[i].audio)) {
                    std::fprintf(stderr, "[fmv] could not open %s\n", r[i].name.c_str());
                    done_[i].store(true);
                }
            }
            if (mov_[i].isOpen() && !mov_[i].update(now)) {
                mov_[i].close();
                done_[i].store(true);
            }
            if (mov_[i].isOpen() && mov_[i].hasFrame) {
                shown_[i].store(true);
                if (i == Screen && mov_[i].shownFrames() != lastFrame_[i]) {
                    lastFrame_[i] = mov_[i].shownFrames();
                    if (vram) vramWriteCT32(vram, vramSize, 0x3BA0, 5, mov_[i].current());
                    uploadScreen(mov_[i].current());
                }
            }
        }
    }

    // Size of the menu object's texture (2^wid x 2^hei from moviesReadyScreenObject); EE thread
    void setScreenTexSize(uint32_t w, uint32_t h) {
        screenW_.store(std::clamp<uint32_t>(w, 8, 1024));
        screenH_.store(std::clamp<uint32_t>(h, 8, 1024));
    }

    // Fullscreen movie over everything, at the video's aspect ratio, with black bars. False if there's no movie.
    bool drawFullscreen() {
        Movie &m = mov_[Full];
        if (!m.isOpen() && !active_[Full].load()) return false;
        // Retail only skips with Start (uiMain reads the button with mask 8); here X/Space also skips, the way the rest of the menu confirms.
        if (IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER) ||
            (IsGamepadAvailable(0) && (IsGamepadButtonPressed(0, GAMEPAD_BUTTON_RIGHT_FACE_DOWN) || IsGamepadButtonPressed(0, GAMEPAD_BUTTON_MIDDLE_RIGHT))))
            if (skipAllowed()) skip_.store(true);
        const float sw = float(GetScreenWidth()), sh = float(GetScreenHeight());
        DrawRectangle(0, 0, int(sw), int(sh), BLACK);
        if (!m.hasFrame || !m.tex.id) return true;
        const float a = m.aspect > 0.1f ? m.aspect : 4.f / 3.f;
        const float k = std::min(sw / a, sh);
        const Rectangle dst{(sw - k * a) * 0.5f, (sh - k) * 0.5f, k * a, k};
        DrawTexturePro(m.tex, Rectangle{0, 0, float(m.w), float(m.h)}, dst, Vector2{0, 0}, 0.f, WHITE);
        return true;
    }

    // Texture of the menu movie (object with texId 999); 0 without a frame. Window thread only. As in VRAM, it's an area of
    // 2^wid x 2^hei (256x128) with the frame in the top-left corner: 254x128 or 256x160 movies aren't stretched.
    unsigned screenTexture() const {
        const Movie &m = mov_[Screen];
        return m.isOpen() && m.hasFrame ? screenTex_.id : 0;
    }

private:
    struct Req { Source src; std::string name; bool audio = false, open = false, stop = false; };
    std::mutex m_;
    Req req_[2];
    std::atomic<bool> done_[2]{}, active_[2]{}, shown_[2]{}, skip_{}, hold_[2]{};
    uint16_t padDown_ = 0;   // the pad's X/Start on the previous frame (EE thread only)
    std::atomic<std::chrono::steady_clock::rep> startedAt_{};
    std::atomic<uint32_t> screenW_{256}, screenH_{128};
    long lastFrame_[2] = {-1, -1};
    Movie mov_[2];
    Texture2D screenTex_{};
    std::vector<uint32_t> canvas_;

    void uploadScreen(const Frame &f) {
        const int W = int(screenW_.load()), H = int(screenH_.load());
        canvas_.assign(size_t(W) * H, 0xFF000000u);
        for (int y = 0; y < std::min(H, f.h); ++y)
            std::memcpy(&canvas_[size_t(y) * W], &f.rgba[size_t(y) * f.w], size_t(std::min(W, f.w)) * 4);
        if (!screenTex_.id || screenTex_.width != W || screenTex_.height != H) {
            if (screenTex_.id) UnloadTexture(screenTex_);
            Image img{canvas_.data(), W, H, 1, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8};
            screenTex_ = LoadTextureFromImage(img);
            SetTextureFilter(screenTex_, TEXTURE_FILTER_BILINEAR);
            SetTextureWrap(screenTex_, TEXTURE_WRAP_REPEAT);   // the big-screen mesh has UVs outside 0..1 (the game's GS repeats)
        } else {
            UpdateTexture(screenTex_, canvas_.data());
        }
    }
};

// ---------------------------------------------------------------- hooks (game functions replaced in the runtime)
// Each hook receives RDRAM and the arguments ($a0..$a2) and returns $v0. Addresses: symbol_addrs.txt.
namespace hooks {

namespace ga {   // game globals
constexpr uint32_t movieIsPlaying = wotm::sym::movieIsPlaying;
constexpr uint32_t movieRequested = wotm::sym::movieRequested;
constexpr uint32_t movieIsCompletelyFinished = wotm::sym::movieIsCompletelyFinished;
constexpr uint32_t IsInVram = wotm::sym::IsInVram;
constexpr uint32_t isWithAudio = wotm::sym::isWithAudio;
constexpr uint32_t isStarted = wotm::sym::isStarted;
constexpr uint32_t originalTexId = wotm::sym::originalTexId;
constexpr uint32_t originalNumTextures = wotm::sym::originalNumTextures;
constexpr uint32_t gO = wotm::sym::gO;
constexpr uint32_t ScreenObjectPending = wotm::sym::ScreenObjectPending;
constexpr uint32_t gMovObj = wotm::sym::gMovObj;
constexpr uint32_t gMovWid = wotm::sym::gMovWid;
constexpr uint32_t gMovHei = wotm::sym::gMovHei;
constexpr uint32_t movieFrameCounter = wotm::sym::movieFrameCounter;
constexpr uint32_t moviePreFrameCounter = wotm::sym::moviePreFrameCounter;
constexpr uint32_t CurrentMovieScreen = wotm::sym::CurrentMovieScreen;
constexpr uint32_t partialMovie = wotm::sym::partialMovie;
constexpr uint32_t originalTexZero = wotm::sym::originalTexZero;   // 16 bytes: the object's original TEX0
constexpr uint32_t movieTexId = 999;               // texInfo[999]: reserved for the movie
constexpr uint32_t inputPadData = 0x00779D00u;     // D_00779D00 (common/input): libpad buffer for pad 0, buttons at +2 (active-low)
}

inline uint32_t rd(const uint8_t *ram, uint32_t a) { uint32_t v; std::memcpy(&v, ram + (a & 0x01FFFFFFu), 4); return v; }
inline void wr(uint8_t *ram, uint32_t a, uint32_t v) { std::memcpy(ram + (a & 0x01FFFFFFu), &v, 4); }
inline void wr16(uint8_t *ram, uint32_t a, uint16_t v) { std::memcpy(ram + (a & 0x01FFFFFFu), &v, 2); }
inline std::string str(const uint8_t *ram, uint32_t a) {
    std::string s;
    for (uint32_t i = 0; i < 256; ++i) { const char c = char(ram[(a + i) & 0x01FFFFFFu]); if (!c) break; s.push_back(c); }
    return s;
}

inline bool &skipHold() { static bool v = false; return v; }

// playMpegMovieWithDB(char *name, bool withAudio) (0x20F4B0): starts the fullscreen movie; the game keeps calling updateMpegMovieDecoding
inline uint32_t playMpegMovieWithDB(uint8_t *ram, uint32_t name, uint32_t withAudio) {
    wr(ram, ga::movieIsCompletelyFinished, 0);   // initAll
    wr(ram, ga::isWithAudio, withAudio & 0xFF);
    wr(ram, ga::partialMovie, 0);
    wr(ram, ga::isStarted, 0);
    wr(ram, ga::movieIsPlaying, 1);
    // SKIP INTRO MOVIES (panel's FEATURES tab; WOTM_SKIPINTRO in settings.ini or the environment): the two boot movies (intro_m and
    // int_main, only the first time for each; the int_main that comes back when the menu sits idle does play) don't start, and end as a skip on the second updateMpegMovieDecoding (Player without the movie active -> termAll). Not on
    // the first: uiIntro only marks the movie as started if it sees movieRequested on some frame, and without that the boot hung waiting.
    static bool skippedBoot[2] = {};
    const std::string movie = str(ram, name);
    static const bool skipIntro = wotm::settings::on(wotm::settings::SkipIntro);   // read on the first movie: changing it in the panel takes effect on the next boot
    if (skipIntro) {
        std::string rel = discRelative(movie);
        for (char &c : rel) c = char(std::tolower(static_cast<unsigned char>(c)));
        const int boot = rel == "mov/intro_m.pss" ? 0 : rel == "mov/int_main.pss" ? 1 : -1;
        if (boot >= 0 && !skippedBoot[boot]) {
            skippedBoot[boot] = true;
            std::fprintf(stderr, "[fmv] fullscreen skipped (SKIP INTRO MOVIES): %s\n", movie.c_str());
            Player::get().stop(Player::Full);
            skipHold() = true;
            wr(ram, ga::movieRequested, 1);
            return 0;
        }
    }
    Player::get().start(Player::Full, movie, (withAudio & 0xFF) != 0);
    wr(ram, ga::movieRequested, 1);
    return 0;
}

// termAll: what the natural end (or a skip) leaves in the globals
inline void termAll(uint8_t *ram) {
    Player::get().stop(Player::Full);
    wr(ram, ga::movieIsPlaying, 0);
    wr(ram, ga::movieIsCompletelyFinished, 1);
    wr(ram, ga::movieRequested, 0);
}

// updateMpegMovieDecoding(bool keepGoing) (0x20F540). As in retail, keepGoing = 0 only aborts after the picture has started
// (isStarted); before that it only marks the start (uiMain makes the first call with 0).
inline uint32_t updateMpegMovieDecoding(uint8_t *ram, uint32_t keepGoing) {
    Player &p = Player::get();
    const bool started = rd(ram, ga::isStarted) != 0;
    p.padInput(uint16_t(rd(ram, ga::inputPadData) >> 16));
    if (!p.skipAllowed()) keepGoing = 1;   // also ignores the Start the game reads (level-triggered) during the first 100 ms
    else if (p.skipRequested()) keepGoing = 0;
    if (skipHold()) {   // boot movie skipped (WOTM_SKIPINTRO): stays "requested" until the next frame, so uiIntro sees it
        skipHold() = false;
        wr(ram, ga::isStarted, 1);
        return 0;
    }
    if (p.finished(Player::Full) || !p.active(Player::Full) || ((keepGoing & 0xFF) == 0 && started)) {
        std::fprintf(stderr, "[fmv] fullscreen ended (%s)\n", p.finished(Player::Full) ? "end" : "skip");
        wr(ram, ga::isStarted, 0);
        termAll(ram);
        return 0;
    }
    if (!started) wr(ram, ga::isStarted, 1);
    return 0;
}

// moviesSetScreenObject (0x210808): swaps the object's texture for the movie's (texId 999, 1 texture, TEX0 CT32 at 0x3BA0 with 2^wid x 2^hei)
inline uint32_t moviesSetScreenObject(uint8_t *ram) {
    const uint32_t o = rd(ram, ga::gMovObj);
    if (!o) return 1;
    const uint64_t tex0 = (uint64_t(rd(ram, ga::gMovWid)) << 26) | (uint64_t(rd(ram, ga::gMovHei)) << 30) | 0x17BA0u;
    wr(ram, ga::originalTexId, ram[(o + 0x50) & 0x01FFFFFFu] | (ram[(o + 0x51) & 0x01FFFFFFu] << 8));
    std::memcpy(ram + ga::originalTexZero, ram + ((o + 0x70) & 0x01FFFFFFu), 16);
    wr(ram, ga::originalNumTextures, ram[(o + 0x5A) & 0x01FFFFFFu] | (ram[(o + 0x5B) & 0x01FFFFFFu] << 8));
    wr(ram, ga::gO, o);
    Player::get().setScreenTexSize(1u << (rd(ram, ga::gMovWid) & 15), 1u << (rd(ram, ga::gMovHei) & 15));
    wr16(ram, o + 0x50, uint16_t(ga::movieTexId));
    wr16(ram, o + 0x5A, 1);
    std::memcpy(ram + ((o + 0x70) & 0x01FFFFFFu), &tex0, 8);
    wr(ram, ga::CurrentMovieScreen, o);
    return 0;
}

// CMovie::Play(bool, char *name) (0x210090): only acts with the movie stopped (+0x94 = 1); returns 1 if there was already a movie
inline uint32_t cmoviePlay(uint8_t *ram, uint32_t self, uint32_t, uint32_t name) {
    if (rd(ram, self + 0x94) == 0) return 1;
    wr(ram, ga::IsInVram, 0);
    wr(ram, ga::movieFrameCounter, 0);
    wr(ram, ga::partialMovie, 1);
    wr(ram, self + 0x88, 0);
    Player::get().start(Player::Screen, str(ram, name), rd(ram, self + 0x50) != 0);
    wr(ram, self + 0x94, 0);
    return 0;
}

// CMovie::End (0x210360)
inline uint32_t cmovieEnd(uint8_t *ram, uint32_t self) {
    if (rd(ram, self + 0x94)) return 1;
    Player::get().stop(Player::Screen);
    wr(ram, self + 0x94, 1);
    return 0;
}

// CMovie::Update(bool keepGoing) (0x210408): puts the movie on the object (after 16 frames, like retail, or once there's a frame);
// 0 if stopped, 1 while playing; at the end (or keepGoing = 0) calls End and returns 1
inline uint32_t cmovieUpdate(uint8_t *ram, uint32_t self, uint32_t keepGoing) {
    Player &p = Player::get();
    if (rd(ram, ga::ScreenObjectPending)) {
        const uint32_t n = rd(ram, ga::moviePreFrameCounter) + 1;
        wr(ram, ga::moviePreFrameCounter, n);
        if (p.hasFrame(Player::Screen) || rd(ram, ga::IsInVram) || n >= 16) {
            moviesSetScreenObject(ram);
            wr(ram, ga::ScreenObjectPending, 0);
        }
    }
    if (rd(ram, self + 0x94)) return 0;
    wr(ram, ga::movieFrameCounter, rd(ram, ga::movieFrameCounter) + 1);
    // as in retail: keepGoing = 0 only ends it after the movie has started appearing (+0x88)
    if (p.finished(Player::Screen) || ((keepGoing & 0xFF) == 0 && rd(ram, self + 0x88))) {
        cmovieEnd(ram, self);
        return 1;
    }
    if (p.hasFrame(Player::Screen)) { wr(ram, ga::IsInVram, 1); wr(ram, self + 0x88, 1); }
    return 1;
}

}  // namespace hooks

}  // namespace wotm::fmv
