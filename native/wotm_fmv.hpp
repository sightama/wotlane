// Filmes (FMV) na camada nativa: os .PSS do disco tocados com FFmpeg (video MPEG-2) e raylib (imagem e som).
//
// No retail, CMovie (common/movies) e playMpegMovieWithDB/updateMpegMovieDecoding (common/MpegMovie) montam o exemplo
// "ezmpeg" da SCE: leitura do disco, sceMpegDemuxPssRing, uma thread que alimenta a IPU por DMA e um handler de vblank que manda
// cada quadro para a VRAM pelo PATH3. Nada disso tem hardware no PC: o corte aqui e nessas funcoes do jogo (hooks no runtime,
// `wotm::fmv::hooks`), e o filme e lido direto do arquivo do disco.
//
// Usos no jogo (lidos do asm):
//   - Tela cheia: `playMovie` (game/ui) chama `playMpegMovieWithDB("mov/intro_m.pss", comAudio)`; depois, a cada quadro do menu,
//     `uiMain` chama `updateMpegMovieDecoding(continuar)` enquanto `movieRequested` (0x6F8840) for 1. No fim (ou com continuar = 0,
//     o jogador pulou) o `termAll` zera `movieRequested` e `movieIsPlaying` (0x6F8650) e liga `movieIsCompletelyFinished` (0x6F8844),
//     que o `uiIntro`/`uiOutro` esperam. Filmes: intro_m, int_main, int_act1..4, <monstro>_p e <monstro>_g (finais), credits.
//   - Textura no menu 3D: as telas do menu chamam `moviesReadyScreenObject(obj, 8, 7)` e `myMovie.Play(false, "mov/sheldemo.pss")`
//     a cada quadro (o Play so reinicia quando o filme anterior acabou: e assim que ele fica em laco) e `myMovie.Update(continuar)`.
//     O `Update` chama `moviesSetScreenObject` depois de 16 quadros (ou quando o quadro ja esta na VRAM), que troca a textura do
//     objeto pela do filme: texId 999 (0x3E7), numTextures 1 e TEX0 = CT32 em 0x3BA0 com 2^wid x 2^hei (256x128). A escolha de monstro e a de
//     fase tambem chamam o Play (com outros nomes); os .PSS pequenos (256x128, sem audio: alien.pss, central.pss...) devem ser
//     essas previas (nao conferido no jogo).
//   - CMovie (myMovie, 0x423D40): +0x50 com audio, +0x88 comecou a mostrar, +0x94 parado (1 = sem filme; o Play so age com 1).
//
// Formato do .PSS (conferido com tools/pss.py e o ffprobe): MPEG-2 program stream em pacotes de 2048 bytes; video no fluxo 0xE0
// (MPEG-2 MP@ML, 640x368 nos de tela cheia e 256x128 nos do menu, 29,97 quadros/s); audio no fluxo privado 0xBD (subfluxo 0xFF,
// 4 bytes de cabecalho por PES), com cabecalho "SShd" (tipo 1 = PCM 16 bits, taxa 48000, 2 canais, intercalacao 0x200) seguido de
// "SSbd" + tamanho e dos dados: blocos de 512 bytes alternando esquerdo e direito. O FFmpeg nao reconhece esse audio; o
// demultiplexador daqui separa os dois fluxos e o PCM e tocado direto.
#pragma once

#include <algorithm>
#include <atomic>
#include <cctype>
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

// ---------------------------------------------------------------- demultiplexador do .PSS

// Onde ler um filme: um arquivo solto (disco extraido) ou um trecho de uma imagem ISO
struct Source {
    std::string path;
    uint64_t offset = 0;
    uint64_t size = UINT64_MAX;
    bool ok() const { return !path.empty(); }
};

inline int seek64(FILE *f, uint64_t off) {
#ifdef _WIN32
    return _fseeki64(f, int64_t(off), SEEK_SET);
#else
    return fseeko(f, off_t(off), SEEK_SET);
#endif
}

// Arquivo dentro de uma ISO9660 ("MOV/INTRO_M.PSS", sem diferenciar maiusculas; a versao ";1" e ignorada)
inline Source isoFind(const std::string &iso, const std::string &name) {
    FILE *f = std::fopen(iso.c_str(), "rb");
    if (!f) return {};
    auto readAt = [&](uint64_t off, uint8_t *dst, size_t n) { return seek64(f, off) == 0 && std::fread(dst, 1, n, f) == n; };
    auto le32 = [](const uint8_t *p) { return uint32_t(p[0]) | (uint32_t(p[1]) << 8) | (uint32_t(p[2]) << 16) | (uint32_t(p[3]) << 24); };
    auto upper = [](std::string s) { for (char &c : s) c = char(std::toupper(static_cast<unsigned char>(c))); return s; };
    Source out;
    uint8_t pvd[2048];
    if (readAt(16 * 2048ull, pvd, sizeof pvd) && pvd[0] == 1 && std::memcmp(pvd + 1, "CD001", 5) == 0) {
        uint32_t ext = le32(pvd + 156 + 2), size = le32(pvd + 156 + 10);
        bool isDir = true, found = true;
        size_t start = 0;
        const std::string n = upper(name);
        while (found && start < n.size()) {
            const size_t slash = n.find('/', start);
            const std::string part = n.substr(start, slash == std::string::npos ? std::string::npos : slash - start);
            start = slash == std::string::npos ? n.size() : slash + 1;
            if (part.empty()) continue;
            if (!isDir || size > (64u << 20)) { found = false; break; }
            std::vector<uint8_t> dir(size);
            if (!readAt(uint64_t(ext) * 2048, dir.data(), dir.size())) { found = false; break; }
            found = false;
            for (size_t o = 0; o < dir.size();) {
                const uint8_t len = dir[o];
                if (len == 0) { o = (o / 2048 + 1) * 2048; continue; }   // registros nao cruzam setores
                if (o + len > dir.size() || len < 34) break;
                std::string id(reinterpret_cast<const char *>(&dir[o + 33]), dir[o + 32]);
                if (const auto sc = id.find(';'); sc != std::string::npos) id.resize(sc);
                if (upper(id) == part) {
                    ext = le32(&dir[o + 2]);
                    size = le32(&dir[o + 10]);
                    isDir = (dir[o + 25] & 2) != 0;
                    found = true;
                    break;
                }
                o += len;
            }
        }
        if (found && !isDir) out = Source{iso, uint64_t(ext) * 2048, size};
    }
    std::fclose(f);
    return out;
}

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

    // Proximo pedaco de fluxo elementar (video ou audio sem os 4 bytes do subfluxo); End no fim do arquivo
    Kind next(const uint8_t *&data, size_t &n) {
        for (;;) {
            if (!fill(4)) return End;
            const uint8_t *p = buf_.data() + pos_;
            if (p[0] || p[1] || p[2] != 1) { ++pos_; continue; }
            const uint8_t sid = p[3];
            if (sid == 0xBA) {   // cabecalho do pacote (MPEG-2: 14 bytes + enchimento)
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
            if (ln < 3 || (pay[0] & 0xC0) != 0x80) continue;   // so PES de MPEG-2
            const size_t hl = 3u + pay[2];
            if (hl > ln) continue;
            data = pay + hl;
            n = ln - hl;
            if (video) return Video;
            if (n <= 4) continue;
            data += 4;   // subfluxo (0xFF) + 3 bytes
            n -= 4;
            return Audio;
        }
    }

private:
    FILE *f_ = nullptr;
    std::vector<uint8_t> buf_;
    size_t pos_ = 0, len_ = 0;
    bool eof_ = false;
    uint64_t left_ = UINT64_MAX;   // bytes que ainda pertencem ao arquivo (dentro da ISO)

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

// ---------------------------------------------------------------- um filme tocando

struct Frame {
    std::vector<uint32_t> rgba;
    int w = 0, h = 0;
};

class Movie {
public:
    int w = 0, h = 0;
    float aspect = 4.f / 3.f;   // largura/altura de exibicao (o MPEG-2 traz a proporcao do pixel)
    Texture2D tex{};
    bool hasFrame = false;
    std::vector<int16_t> *audioTap = nullptr;   // diagnostico (fmvplay --wav): copia do PCM decodificado

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
        audioHdr_.clear();
        audioState_ = 0;
        demuxEnd_ = flushed_ = false;
        open_ = hasFrame = false;
        shown_ = 0;
        t0_ = -1.0;
        w = h = 0;
    }

    bool isOpen() const { return open_; }
    long shownFrames() const { return shown_; }      // quadros ja mostrados desde o inicio
    const Frame &current() const { return stage_; } // pixels do quadro na tela (RGBA)

    // Avanca ate o quadro devido em `now` (segundos, relogio qualquer) e sobe a textura. Falso quando o filme terminou.
    bool update(double now) {
        if (!open_) return false;
        if (t0_ < 0) t0_ = now;
        const double fps = 30000.0 / 1001.0;
        const double elapsed = now - t0_;
        pump();
        bool due = false;
        while (!frames_.empty() && double(shown_) / fps <= elapsed) {
            if (due) recycle(std::move(stage_));   // quadro atrasado: fica so o mais novo
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

    // audio: cabecalho SShd/SSbd e PCM estereo intercalado (s16)
    bool wantAudio_ = false;
    int audioState_ = 0;                 // 0 lendo cabecalho, 1 dados PCM, 2 ignorado (formato desconhecido)
    std::vector<uint8_t> audioHdr_;
    uint32_t rate_ = 48000, channels_ = 2, interleave_ = 0x200;
    std::vector<uint8_t> blk_;           // um par de blocos (esquerdo + direito) em montagem
    std::deque<int16_t> pcm_;            // amostras intercaladas L R L R
    AudioStream stream_{};
    bool streamOn_ = false;
    static constexpr int kChunk = 2048;  // quadros de audio por envio

    void recycle(Frame &&f) { if (pool_.size() < 4) pool_.push_back(std::move(f)); }

    // Le do arquivo ate ter alguns quadros prontos (e audio suficiente), ou ate o fim
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
        if (!d) {   // fim: esvazia o parser e o decodificador
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

    // YUV 4:2:0 (BT.601, faixa de TV) -> RGBA, como o CSC da IPU
    static void toRgba(const AVFrame *fr, Frame &f) {
        f.w = fr->width;
        f.h = fr->height;
        f.rgba.resize(size_t(f.w) * f.h);
        const bool p420 = fr->format == AV_PIX_FMT_YUV420P || fr->format == AV_PIX_FMT_YUVJ420P;
        const int csh = p420 ? 1 : 0;   // 4:2:2 (raro) usa a mesma linha de croma
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
                std::fprintf(stderr, "[fmv] audio do PSS nao suportado (tipo %u, %u Hz, %u canais, intercalacao %u)\n", type, rate_, channels_, interleave_);
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
        // blocos de `interleave_` bytes por canal: L0 R0 L1 R1 ... (mono: so L)
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
            } else if (demuxEnd_) {   // resto do audio no fim, completado com silencio
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

// ---------------------------------------------------------------- tocador do runtime (pedidos da thread do EE, desenho na thread da janela)

// Nome do jogo ("mov/intro_m.pss", "cdrom0:\MOV\INTRO_M.PSS;1") -> caminho relativo "mov/intro_m.pss"
inline std::string discRelative(std::string name) {
    if (const auto c = name.find(':'); c != std::string::npos) name = name.substr(c + 1);
    if (const auto sc = name.find(';'); sc != std::string::npos) name = name.substr(0, sc);
    std::replace(name.begin(), name.end(), '\\', '/');
    while (!name.empty() && name[0] == '/') name.erase(0, 1);
    return name;
}

// Caminho relativo contra a raiz de um disco extraido, sem diferenciar maiusculas; vazio se nao existe
inline std::string resolveDiscPath(const std::filesystem::path &root, const std::string &rel) {
    namespace fs = std::filesystem;
    std::error_code ec;
    if (root.empty() || !fs::is_directory(root, ec)) return {};
    auto upper = [](std::string s) { for (char &c : s) c = char(std::toupper(static_cast<unsigned char>(c))); return s; };
    fs::path cur = root;
    size_t start = 0;
    while (start < rel.size()) {
        const size_t slash = rel.find('/', start);
        const std::string part = rel.substr(start, slash == std::string::npos ? std::string::npos : slash - start);
        start = slash == std::string::npos ? rel.size() : slash + 1;
        if (part.empty()) continue;
        fs::path next = cur / part;
        if (!fs::exists(next, ec)) {
            bool hit = false;
            for (const auto &e : fs::directory_iterator(cur, ec))
                if (upper(e.path().filename().string()) == upper(part)) { next = e.path(); hit = true; break; }
            if (!hit) return {};
        }
        cur = next;
    }
    return fs::is_regular_file(cur, ec) ? cur.string() : std::string{};
}

// Escrita de um quadro RGBA na VRAM do GS (PSMCT32, enderecamento de pagina/bloco/coluna do GS) em `tbp` com largura `tbw` x 64
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
            const uint32_t gs = (c & 0x00FFFFFFu) | 0x80000000u;   // alfa 0x80 = 1,0 no GS
            std::memcpy(vram + a, &gs, 4);
        }
}

class Player {
public:
    enum Slot { Full = 0, Screen = 1 };

    static Player &get() { static Player p; return p; }

    // Onde estao os filmes: pasta do disco extraido e/ou imagem ISO (WOTM_DISC / WOTM_ISO tem prioridade)
    std::filesystem::path discRoot, isoPath;

    Source locate(const std::string &gameName) const {
        const std::string rel = discRelative(gameName);
        if (const char *d = std::getenv("WOTM_DISC"))
            if (std::string p = resolveDiscPath(d, rel); !p.empty()) return Source{p};
        if (std::string p = resolveDiscPath(discRoot, rel); !p.empty()) return Source{p};
        const char *iso = std::getenv("WOTM_ISO");
        const std::string isoFile = iso ? std::string(iso) : isoPath.string();
        if (!isoFile.empty()) return isoFind(isoFile, rel);
        return {};
    }

    // ---- thread do EE (hooks)
    void start(Slot s, const std::string &gameName, bool audio) {
        const Source src = locate(gameName);
        std::lock_guard<std::mutex> lk(m_);
        Req &r = req_[s];
        r.src = src;
        r.name = gameName;
        r.audio = audio;
        r.open = true;
        r.stop = false;
        done_[s].store(false);
        active_[s].store(true);
        std::fprintf(stderr, "[fmv] %s: %s -> %s @%llu\n", s == Full ? "tela cheia" : "textura", gameName.c_str(), src.ok() ? src.path.c_str() : "(nao achado)",
                     (unsigned long long)src.offset);
    }
    void stop(Slot s) {
        std::lock_guard<std::mutex> lk(m_);
        req_[s].stop = true;
        req_[s].open = false;
        active_[s].store(false);
    }
    bool finished(Slot s) const { return done_[s].load(); }
    bool active(Slot s) const { return active_[s].load(); }
    bool hasFrame(Slot s) const { return shown_[s].load(); }

    // ---- thread da janela (antes de desenhar): abre/fecha e avanca os filmes. Com `vram`, o quadro do filme do menu tambem vai
    // para a VRAM do GS onde o retail o poe (0x3BA0, largura 320: VramAddrForMovies/VramWidthForMovies), para o menu desenhado
    // pelo GS emulado.
    void pump(uint8_t *vram = nullptr, size_t vramSize = 0, double clock = -1.0) {
        Req r[2];
        {
            std::lock_guard<std::mutex> lk(m_);
            for (int i = 0; i < 2; ++i) { r[i] = req_[i]; req_[i].open = req_[i].stop = false; }
        }
        const double now = clock >= 0.0 ? clock : GetTime();   // clock: relogio de teste
        for (int i = 0; i < 2; ++i) {
            if (r[i].stop) { mov_[i].close(); shown_[i].store(false); }
            if (r[i].open) {
                shown_[i].store(false);
                lastFrame_[i] = -1;
                if (!r[i].src.ok() || !mov_[i].open(r[i].src, r[i].audio)) {
                    std::fprintf(stderr, "[fmv] nao abriu %s\n", r[i].name.c_str());
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

    // Tamanho da textura do objeto do menu (2^wid x 2^hei do moviesReadyScreenObject); thread do EE
    void setScreenTexSize(uint32_t w, uint32_t h) {
        screenW_.store(std::clamp<uint32_t>(w, 8, 1024));
        screenH_.store(std::clamp<uint32_t>(h, 8, 1024));
    }

    // Filme de tela cheia por cima de tudo, na proporcao do video, com faixas pretas. Falso se nao ha filme.
    bool drawFullscreen() {
        Movie &m = mov_[Full];
        if (!m.isOpen() && !active_[Full].load()) return false;
        const float sw = float(GetScreenWidth()), sh = float(GetScreenHeight());
        DrawRectangle(0, 0, int(sw), int(sh), BLACK);
        if (!m.hasFrame || !m.tex.id) return true;
        const float a = m.aspect > 0.1f ? m.aspect : 4.f / 3.f;
        const float k = std::min(sw / a, sh);
        const Rectangle dst{(sw - k * a) * 0.5f, (sh - k) * 0.5f, k * a, k};
        DrawTexturePro(m.tex, Rectangle{0, 0, float(m.w), float(m.h)}, dst, Vector2{0, 0}, 0.f, WHITE);
        return true;
    }

    // Textura do filme do menu (objeto com texId 999); 0 sem quadro. So na thread da janela. Como na VRAM, e uma area de
    // 2^wid x 2^hei (256x128) com o quadro no canto superior esquerdo: os filmes de 254x128 ou 256x160 nao sao esticados.
    unsigned screenTexture() const {
        const Movie &m = mov_[Screen];
        return m.isOpen() && m.hasFrame ? screenTex_.id : 0;
    }

private:
    struct Req { Source src; std::string name; bool audio = false, open = false, stop = false; };
    std::mutex m_;
    Req req_[2];
    std::atomic<bool> done_[2]{}, active_[2]{}, shown_[2]{};
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
            SetTextureWrap(screenTex_, TEXTURE_WRAP_CLAMP);
        } else {
            UpdateTexture(screenTex_, canvas_.data());
        }
    }
};

// ---------------------------------------------------------------- hooks (funcoes do jogo substituidas no runtime)
// Cada hook recebe a RDRAM e os argumentos ($a0..$a2) e devolve o $v0. Enderecos: symbol_addrs.txt.
namespace hooks {

namespace ga {   // globais do jogo
constexpr uint32_t movieIsPlaying = 0x006F8650;
constexpr uint32_t movieRequested = 0x006F8840;
constexpr uint32_t movieIsCompletelyFinished = 0x006F8844;
constexpr uint32_t IsInVram = 0x006F884C;
constexpr uint32_t isWithAudio = 0x006F8864;
constexpr uint32_t isStarted = 0x006F8874;
constexpr uint32_t originalTexId = 0x006F88A0;
constexpr uint32_t originalNumTextures = 0x006F88A4;
constexpr uint32_t gO = 0x006F88A8;
constexpr uint32_t ScreenObjectPending = 0x006F88B0;
constexpr uint32_t gMovObj = 0x006F88B4;
constexpr uint32_t gMovWid = 0x006F88B8;
constexpr uint32_t gMovHei = 0x006F88BC;
constexpr uint32_t movieFrameCounter = 0x006F88C0;
constexpr uint32_t moviePreFrameCounter = 0x006F88C4;
constexpr uint32_t CurrentMovieScreen = 0x006F88C8;
constexpr uint32_t partialMovie = 0x006F8974;
constexpr uint32_t originalTexZero = 0x0054DB60;   // 16 bytes: TEX0 original do objeto
constexpr uint32_t movieTexId = 999;               // texInfo[999]: reservado para o filme
}

inline uint32_t rd(const uint8_t *ram, uint32_t a) { uint32_t v; std::memcpy(&v, ram + (a & 0x01FFFFFFu), 4); return v; }
inline void wr(uint8_t *ram, uint32_t a, uint32_t v) { std::memcpy(ram + (a & 0x01FFFFFFu), &v, 4); }
inline void wr16(uint8_t *ram, uint32_t a, uint16_t v) { std::memcpy(ram + (a & 0x01FFFFFFu), &v, 2); }
inline std::string str(const uint8_t *ram, uint32_t a) {
    std::string s;
    for (uint32_t i = 0; i < 256; ++i) { const char c = char(ram[(a + i) & 0x01FFFFFFu]); if (!c) break; s.push_back(c); }
    return s;
}

// playMpegMovieWithDB(char *name, bool comAudio) (0x20F4B0): comeca o filme de tela cheia; o jogo segue chamando updateMpegMovieDecoding
inline uint32_t playMpegMovieWithDB(uint8_t *ram, uint32_t name, uint32_t withAudio) {
    wr(ram, ga::movieIsCompletelyFinished, 0);   // initAll
    wr(ram, ga::isWithAudio, withAudio & 0xFF);
    wr(ram, ga::partialMovie, 0);
    wr(ram, ga::isStarted, 0);
    wr(ram, ga::movieIsPlaying, 1);
    Player::get().start(Player::Full, str(ram, name), (withAudio & 0xFF) != 0);
    wr(ram, ga::movieRequested, 1);
    return 0;
}

// termAll: o que o fim natural (ou o pulo) deixa nos globais
inline void termAll(uint8_t *ram) {
    Player::get().stop(Player::Full);
    wr(ram, ga::movieIsPlaying, 0);
    wr(ram, ga::movieIsCompletelyFinished, 1);
    wr(ram, ga::movieRequested, 0);
}

// updateMpegMovieDecoding(bool continuar) (0x20F540). Como no retail, continuar = 0 so aborta depois que a imagem comecou
// (isStarted); antes disso so marca o inicio (o uiMain faz a primeira chamada com 0).
inline uint32_t updateMpegMovieDecoding(uint8_t *ram, uint32_t keepGoing) {
    Player &p = Player::get();
    const bool started = rd(ram, ga::isStarted) != 0;
    if (p.finished(Player::Full) || !p.active(Player::Full) || ((keepGoing & 0xFF) == 0 && started)) {
        wr(ram, ga::isStarted, 0);
        termAll(ram);
        return 0;
    }
    if (!started) wr(ram, ga::isStarted, 1);
    return 0;
}

// moviesSetScreenObject (0x210808): troca a textura do objeto pela do filme (texId 999, 1 textura, TEX0 CT32 em 0x3BA0 com 2^wid x 2^hei)
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

// CMovie::Play(bool, char *name) (0x210090): so age com o filme parado (+0x94 = 1); devolve 1 se ja havia filme
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

// CMovie::Update(bool continuar) (0x210408): poe o filme no objeto (depois de 16 quadros, como o retail, ou quando ja ha quadro);
// 0 se parado, 1 enquanto toca; no fim (ou continuar = 0) chama End e devolve 1
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
    // como no retail: continuar = 0 so encerra depois que o filme comecou a aparecer (+0x88)
    if (p.finished(Player::Screen) || ((keepGoing & 0xFF) == 0 && rd(ram, self + 0x88))) {
        cmovieEnd(ram, self);
        return 1;
    }
    if (p.hasFrame(Player::Screen)) { wr(ram, ga::IsInVram, 1); wr(ram, self + 0x88, 1); }
    return 1;
}

}  // namespace hooks

}  // namespace wotm::fmv
