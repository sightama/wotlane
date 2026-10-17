// Verificador offline do som nativo (wotm_snd.hpp), sem o jogo e sem o runtime: o mesmo motor que o runtime usa ao vivo
// grava em WAV o que tocaria.
//   sndplay disc/SND/SHELLMUS.BNK 0 musica.wav [segundos]    som N do banco (0x400 de volume, pan do banco)
//   sndplay disc/VAG/CITYAMB.VPK - cidade.wav [segundos]      stream .VPK/.VAG
//   sndplay --all disc/SND                                    toca 3 s de cada som de cada banco e imprime pico/RMS (sem WAV)
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>

#include "wotm_snd.hpp"

using namespace wotm::snd;

static std::vector<u8> readFile(const std::string &p) {
    std::ifstream f(p, std::ios::binary);
    return std::vector<u8>((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
}

static void writeWav(const std::string &path, const std::vector<s16> &s) {
    FILE *f = std::fopen(path.c_str(), "wb");
    if (!f) return;
    const uint32_t rate = kRate, data = uint32_t(s.size() * 2), riff = 36 + data, fmt = 16, br = rate * 4;
    const uint16_t pcm = 1, ch = 2, ba = 4, bits = 16;
    std::fwrite("RIFF", 1, 4, f); std::fwrite(&riff, 4, 1, f); std::fwrite("WAVEfmt ", 1, 8, f);
    std::fwrite(&fmt, 4, 1, f); std::fwrite(&pcm, 2, 1, f); std::fwrite(&ch, 2, 1, f); std::fwrite(&rate, 4, 1, f);
    std::fwrite(&br, 4, 1, f); std::fwrite(&ba, 2, 1, f); std::fwrite(&bits, 2, 1, f);
    std::fwrite("data", 1, 4, f); std::fwrite(&data, 4, 1, f); std::fwrite(s.data(), 2, s.size(), f);
    std::fclose(f);
}

// Renderiza ate `secs` segundos ou ate o som acabar; devolve as amostras estereo
static std::vector<s16> render(Engine &e, u32 h, double secs) {
    std::vector<s16> out;
    const unsigned chunk = 1024;
    std::vector<s16> buf(chunk * 2);
    for (double t = 0; t < secs; t += double(chunk) / kRate) {
        e.mix(buf.data(), chunk);
        out.insert(out.end(), buf.begin(), buf.end());
        if (!e.playing(h)) break;
    }
    return out;
}

static void stats(const std::vector<s16> &s, int &peak, double &rms) {
    peak = 0;
    double acc = 0;
    for (s16 v : s) { peak = std::max(peak, std::abs(int(v))); acc += double(v) * v; }
    rms = s.empty() ? 0 : std::sqrt(acc / double(s.size()));
}

int main(int argc, char **argv) {
    Engine &e = Engine::get();
    if (argc >= 3 && std::string(argv[1]) == "--all") {
        int banks = 0, sounds = 0, silent = 0;
        for (auto &ent : std::filesystem::directory_iterator(argv[2])) {
            if (ent.path().extension() != ".BNK") continue;
            const u32 b = e.loadBankData(readFile(ent.path().string()));
            if (!b) { std::fprintf(stderr, "FALHOU %s\n", ent.path().string().c_str()); return 1; }
            ++banks;
            for (size_t i = 0; i < e.bank(b)->sounds.size(); ++i) {
                const u32 h = e.play(b, int(i), 0x400, -1, 0, 0);
                const auto s = render(e, h, 3.0);
                e.stopAll();
                int peak; double rms;
                stats(s, peak, rms);
                ++sounds;
                if (peak == 0) ++silent;
                std::printf("%-14s som %3zu  %5.2f s  pico %5d  rms %7.1f%s\n", ent.path().filename().string().c_str(), i,
                            double(s.size()) / 2 / kRate, peak, rms, peak == 0 ? "  (silencio)" : "");
            }
            e.unloadBank(b);
        }
        std::printf("%d bancos, %d sons, %d em silencio\n", banks, sounds, silent);
        return 0;
    }
    if (argc < 4) {
        std::fprintf(stderr, "uso: sndplay banco.BNK som saida.wav [segundos] | sndplay stream.VPK - saida.wav [segundos] | sndplay --all disc/SND\n");
        return 1;
    }
    const std::string path = argv[1], which = argv[2], out = argv[3];
    const double secs = argc > 4 ? std::atof(argv[4]) : 10.0;
    const std::vector<u8> file = readFile(path);
    u32 h;
    if (which == "-") {
        // o motor le streams por LSN: aqui o "disco" e o proprio arquivo, a partir do setor 0
        e.setReader([&file](u32 lsn, u32 sectors, u8 *dst) {
            const size_t at = size_t(lsn) * 2048, n = size_t(sectors) * 2048;
            std::memset(dst, 0, n);
            if (at < file.size()) std::memcpy(dst, file.data() + at, std::min(n, file.size() - at));
            return true;
        });
        h = e.playStream(0, 0x400, 0, 15);
    } else {
        const u32 b = e.loadBankData(file);
        h = b ? e.play(b, std::atoi(which.c_str()), 0x400, -1, 0, 0) : 0;
    }
    if (!h) { std::fprintf(stderr, "nao tocou\n"); return 1; }
    const auto s = render(e, h, secs);
    int peak; double rms;
    stats(s, peak, rms);
    writeWav(out, s);
    std::fprintf(stderr, "%s: %.2f s, pico %d, rms %.1f\n", out.c_str(), double(s.size()) / 2 / kRate, peak, rms);
    return 0;
}
