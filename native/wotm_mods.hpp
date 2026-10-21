// Pasta mods/ (POS_FRONTEIRA.md, item 4): arquivos que sobrepoem os do disco, sem mexer na ISO.
// Cada subpasta de mods/ e um mod e repete a arvore do disco (mods/meu_mod/MON/CONGAR1.TEX troca o \MON\CONGAR1.TEX). Pastas
// que comecam com '_' ou '.' ficam desligadas. Com dois mods trocando o mesmo arquivo vence o ultimo em ordem alfabetica (como
// camadas: "10_base", "20_extra"); o log diz quem perdeu.
// O jogo acha os arquivos pelo fileCdSearchFile (setor e tamanho) e le por setor; cada arquivo de mod ganha uma faixa de
// setores falsos acima do fim de qualquer DVD (kFirstLsn), que os ganchos (runtime/wotm_mods.inc) e o leitor do som nativo
// atendem daqui. A camada nativa (wotm_disc.hpp: filmes, icone, fotos e monstro da lista de saves) procura aqui pelo nome.
// So host: nao depende do runtime. A varredura roda uma vez, antes do boot (scan); depois tudo e so leitura.
#pragma once
#include <algorithm>
#include <atomic>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <map>
#include <string>
#include <vector>

namespace wotm::mods {

struct File {
    std::string rel;              // "MON/CONGAR1.TEX" (maiusculas, '/')
    std::filesystem::path host;   // arquivo no PC
    std::string mod;              // nome da pasta do mod
    uint64_t size = 0;
    uint32_t lsn = 0, sectors = 0;   // faixa falsa: [lsn, lsn + sectors)
    mutable bool logged = false;     // primeira busca do jogo ja saiu no log
};

class Registry {
public:
    static constexpr uint32_t kFirstLsn = 0x00800000u;   // 16 GB em setores de 2 KB: alem de qualquer DVD (~2,3 M setores)
    static constexpr uint64_t kMaxSize = 256ull << 20;

    static Registry &get() { static Registry r; return r; }

    static std::string key(std::string rel) {
        std::replace(rel.begin(), rel.end(), '\\', '/');
        while (!rel.empty() && rel[0] == '/') rel.erase(0, 1);
        for (char &c : rel) c = char(std::toupper(static_cast<unsigned char>(c)));
        return rel;
    }
    // Nome como o jogo o monta ("\LVL\CENTRAL.NGP;1", "cdrom0:\SND\SHELL_S.BNK;1") -> "LVL/CENTRAL.NGP"
    static std::string keyFromGame(std::string name) {
        if (const auto c = name.find(':'); c != std::string::npos) name = name.substr(c + 1);
        if (const auto sc = name.find(';'); sc != std::string::npos) name = name.substr(0, sc);
        return key(name);
    }

    // Le a pasta mods/ (uma vez). Devolve quantos arquivos entraram.
    size_t scan(const std::filesystem::path &dir) {
        namespace fs = std::filesystem;
        std::error_code ec;
        m_dir = dir;
        if (dir.empty() || !fs::is_directory(dir, ec)) { m_ready = true; return 0; }
        std::vector<fs::path> mods;
        for (const auto &e : fs::directory_iterator(dir, ec)) {
            const std::string n = e.path().filename().string();
            if (e.is_directory(ec) && !n.empty() && n[0] != '_' && n[0] != '.') mods.push_back(e.path());
        }
        std::sort(mods.begin(), mods.end(), [](const fs::path &a, const fs::path &b) { return key(a.filename().string()) < key(b.filename().string()); });
        for (const fs::path &m : mods) {
            m_mods.push_back(m.filename().string());
            for (auto it = fs::recursive_directory_iterator(m, ec); it != fs::recursive_directory_iterator(); it.increment(ec)) {
                if (ec) break;
                if (!it->is_regular_file(ec)) continue;
                const std::string rel = key(fs::relative(it->path(), m, ec).generic_string());
                const uint64_t size = it->file_size(ec);
                if (rel.empty() || ec) continue;
                if (size > kMaxSize) { std::fprintf(stderr, "[mods] %s/%s ignorado: maior que 256 MB\n", m_mods.back().c_str(), rel.c_str()); continue; }
                File &f = m_files[rel];
                if (!f.mod.empty()) std::fprintf(stderr, "[mods] %s: %s vence %s\n", rel.c_str(), m_mods.back().c_str(), f.mod.c_str());
                f.rel = rel;
                f.host = it->path();
                f.mod = m_mods.back();
                f.size = size;
            }
        }
        // Faixas de setores na ordem dos nomes (estavel entre execucoes), com um setor de folga entre arquivos
        uint32_t next = kFirstLsn;
        for (auto &[rel, f] : m_files) {
            f.sectors = uint32_t((f.size + 2047) / 2048);
            f.lsn = next;
            next += std::max(f.sectors, 1u) + 1;
            m_byLsn.push_back(&f);
        }
        m_ready = true;
        return m_files.size();
    }

    bool ready() const { return m_ready; }
    const std::filesystem::path &dir() const { return m_dir; }
    const std::vector<std::string> &modNames() const { return m_mods; }
    size_t count() const { return m_files.size(); }

    const File *find(const std::string &rel) const {
        if (!m_ready || m_files.empty()) return nullptr;
        const auto it = m_files.find(key(rel));
        return it == m_files.end() ? nullptr : &it->second;
    }
    std::string hostPath(const std::string &rel) const {
        const File *f = find(rel);
        return f ? f->host.string() : std::string{};
    }
    static bool isModLsn(uint32_t lsn) { return lsn >= kFirstLsn; }
    const File *findLsn(uint32_t lsn) const {
        if (!isModLsn(lsn)) return nullptr;
        auto it = std::upper_bound(m_byLsn.begin(), m_byLsn.end(), lsn, [](uint32_t l, const File *f) { return l < f->lsn; });
        if (it == m_byLsn.begin()) return nullptr;
        const File *f = *(it - 1);
        return lsn < f->lsn + std::max(f->sectors, 1u) ? f : nullptr;
    }

    // Le `sectors` setores a partir do LSN falso; o que passa do fim do arquivo vem zerado. false: LSN fora dos mods.
    bool read(uint32_t lsn, uint32_t sectors, uint8_t *dst, size_t bytes) const {
        const File *f = findLsn(lsn);
        if (!f) return false;
        std::memset(dst, 0, bytes);
        const uint64_t off = uint64_t(lsn - f->lsn) * 2048;
        if (off >= f->size) return true;
        const size_t n = size_t(std::min<uint64_t>({uint64_t(sectors) * 2048, uint64_t(bytes), f->size - off}));
        std::ifstream in(f->host, std::ios::binary);
        if (!in || !in.seekg(static_cast<std::streamoff>(off))) return false;
        in.read(reinterpret_cast<char *>(dst), static_cast<std::streamsize>(n));
        return in.gcount() == static_cast<std::streamsize>(n);
    }

private:
    std::filesystem::path m_dir;
    std::vector<std::string> m_mods;
    std::map<std::string, File> m_files;
    std::vector<const File *> m_byLsn;   // em ordem de LSN (o map ja vem em ordem de nome, e os LSN seguem essa ordem)
    std::atomic<bool> m_ready{false};
};

} // namespace wotm::mods
