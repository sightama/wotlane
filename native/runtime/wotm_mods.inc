// wotm_mods.inc: pasta mods/ (POS_FRONTEIRA.md, item 4; registro em native/wotm_mods.hpp). Incluido por wotm_overrides.cpp.
// Dois ganchos atendem todo arquivo que o jogo le do disco:
// - fileCdSearchFile (src/common/file.cpp): fases, monstros, SHELL, bancos de som, musicas e filmes acham o arquivo por ele
//   (setor e tamanho). Com o arquivo em mods/ a original roda (para o log comparar o tamanho) e o sceCdlFILE sai com o LSN falso
//   e o tamanho do mod.
// - sceCdRead (stub do runtime): fileReadf/fileReada/fileReads e o snd_StreamSafeCdRead leem por ele; o LSN falso vem do mod.
// O som nativo (wotm_snd.inc, readLsnRaw) le bancos e musicas pelo LSN e consulta o registro antes do disco.
// Arquivo de mod maior que o do disco: o jogo carrega em regioes de tamanho fixo e pode sobrescrever o que vem depois (o log
// avisa; abrir espaco fica para o item 8). WOTM_MODS=<pasta> troca a pasta (padrao: mods/ ao lado do ELF); WOTM_MODS=0 desliga.
#include "wotm_mods.hpp"

namespace wotm_mods_hooks
{
using wotm::mods::Registry;

PS2Runtime::RecompiledFunction g_origSearch = nullptr, g_origCdRead = nullptr;

inline uint32_t rd32(const uint8_t *rdram, uint32_t a) { uint32_t v; std::memcpy(&v, rdram + (a & PS2_RAM_MASK), 4); return v; }
inline void wr32(uint8_t *rdram, uint32_t a, uint32_t v) { std::memcpy(rdram + (a & PS2_RAM_MASK), &v, 4); }

// int fileCdSearchFile(sceCdlFILE *f, char *path); sceCdlFILE = {u32 lsn, u32 size, char name[16], u8 date[8]}
void searchFile(uint8_t *rdram, R5900Context *ctx, PS2Runtime *rt)
{
    const uint32_t fAddr = getRegU32(ctx, 4), pAddr = getRegU32(ctx, 5);
    const char *p = reinterpret_cast<const char *>(rdram + (pAddr & PS2_RAM_MASK));
    const std::string rel = Registry::keyFromGame(std::string(p, strnlen(p, 256)));
    const wotm::mods::File *m = Registry::get().find(rel);
    g_origSearch(rdram, ctx, rt);
    if (!m)
        return;
    const bool onDisc = getRegU32(ctx, 2) != 0;
    const uint32_t discSize = onDisc ? rd32(rdram, fAddr + 4) : 0;
    wr32(rdram, fAddr + 0, m->lsn);
    wr32(rdram, fAddr + 4, uint32_t(m->size));
    if (!onDisc)   // arquivo novo: o nome do sceCdlFILE vem do mod (a original nao achou nada para copiar)
    {
        std::string base = m->rel.substr(m->rel.find_last_of('/') + 1);
        base.resize(std::min<size_t>(base.size(), 15));
        std::memset(rdram + ((fAddr + 8) & PS2_RAM_MASK), 0, 16 + 8);
        std::memcpy(rdram + ((fAddr + 8) & PS2_RAM_MASK), base.data(), base.size());
    }
    setReturnS32(ctx, 1);
    if (!m->logged)
    {
        m->logged = true;
        if (!onDisc)
            std::fprintf(stderr, "[mods] %s <- %s (%llu bytes; arquivo novo, nao esta no disco)\n", m->rel.c_str(), m->mod.c_str(),
                         (unsigned long long)m->size);
        else
            std::fprintf(stderr, "[mods] %s <- %s (%llu bytes; disco %u)%s\n", m->rel.c_str(), m->mod.c_str(), (unsigned long long)m->size,
                         discSize, m->size > discSize ? " AVISO: maior que o do disco, pode sobrescrever o que o jogo carrega depois" : "");
    }
}

// int sceCdRead(u32 lsn, u32 sectors, void *buf, sceCdRMode *mode)
void cdRead(uint8_t *rdram, R5900Context *ctx, PS2Runtime *rt)
{
    const uint32_t lsn = getRegU32(ctx, 4);
    if (!Registry::isModLsn(lsn))
    {
        g_origCdRead(rdram, ctx, rt);
        return;
    }
    const uint32_t sectors = getRegU32(ctx, 5), offset = getRegU32(ctx, 6) & PS2_RAM_MASK;
    const size_t bytes = size_t(std::min<uint64_t>(uint64_t(sectors) * 2048, uint64_t(PS2_RAM_SIZE) - offset));
    if (!Registry::get().read(lsn, sectors, rdram + offset, bytes))
    {
        static int warned = 0;
        if (warned++ < 16)
            std::fprintf(stderr, "[mods] sceCdRead: LSN %#x (%u setores) nao e de nenhum arquivo de mod; devolvendo zeros\n", lsn, sectors);
        std::memset(rdram + offset, 0, bytes);
    }
    setReturnS32(ctx, 1);   // o stub tambem le na hora: sceCdSync devolve "pronto"
}
} // namespace wotm_mods_hooks

void wotmModsInstall(PS2Runtime &runtime, const std::string &elfPath)
{
    using namespace wotm_mods_hooks;
    namespace fs = std::filesystem;
    fs::path dir;
    if (const char *e = std::getenv("WOTM_MODS"))
    {
        if (std::strcmp(e, "0") == 0)
        {
            std::fprintf(stderr, "[mods] desligado (WOTM_MODS=0)\n");
            return;
        }
        dir = e;
    }
    else
        dir = fs::path(elfPath).parent_path() / "mods";
    const size_t n = Registry::get().scan(dir);
    if (n == 0)
        return;   // sem arquivos de mod: nenhum gancho, o jogo le o disco como sempre
    g_origSearch = runtime.lookupFunction(wotm::fn::fileCdSearchFile__FP10sceCdlFILEPc);
    g_origCdRead = runtime.lookupFunction(wotm::fn::sceCdRead);
    if (!g_origSearch || !g_origCdRead)
    {
        std::fprintf(stderr, "[mods] fileCdSearchFile/sceCdRead nao achados: mods desligados\n");
        return;
    }
    runtime.registerFunction(wotm::fn::fileCdSearchFile__FP10sceCdlFILEPc, hostLeaf<searchFile>);
    runtime.registerFunction(wotm::fn::sceCdRead, hostLeaf<cdRead>);
    std::string names;
    for (const std::string &m : Registry::get().modNames())
        names += (names.empty() ? "" : ", ") + m;
    std::fprintf(stderr, "[mods] %zu arquivo(s) de %s em %s\n", n, names.c_str(), dir.string().c_str());
}
