// Save em JSON (POS_FRONTEIRA.md, item 3). Incluido pelo wotm_overrides.cpp.
//
// O jogo continua gravando e lendo os blocos binarios do memory card pelo stub do runtime (mc0/BASCUS-97197: as opcoes, 0x380
// bytes, e o savegame.dat, a aventura, 0x280; o cartao 2 em mc1/). Um JSON so ao lado da pasta do cartao (save.json) passa a ser a
// copia que vale. As opcoes e os destraves sao os mesmos nos dois cartoes; "saves" tem 20 slots, 0..9 no cartao 1 e 10..19 no 2.
//   - na entrada do MonsterMc::stdInit (comeco de toda sequencia de paginas do cartao: boot, opcoes, destrave, aventura) os
//     binarios dos dois cartoes sao refeitos a partir do JSON;
//   - na entrada do MonsterMc::stdExit (fim da sequencia), se o jogo gravou algum binario, o JSON e atualizado a partir deles.
// Sem JSON e com binarios (save antigo), o primeiro stdInit cria o JSON; se o JSON nao tem os slots de um cartao que tem aventuras
// gravadas, elas sao importadas em vez de apagadas. Chaves que o codigo nao conhece ficam no arquivo.
// WOTM_SAVEJSON=0 desliga (so o mc0/, como antes). Layout dos blocos no POS_FRONTEIRA.md; tools/mcsave.py mostra o mesmo JSON.

#include "wotm_json.hpp"

#include <atomic>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <system_error>
#include <utility>
#include <vector>

// Cada gravacao do save.json soma 1 (thread do EE); o icone de salvamento (native/wotm_saveicon.hpp, thread da janela) le.
std::atomic<uint32_t> g_wotmSaveCount{0};

namespace wotm_save
{
    namespace fs = std::filesystem;
    using wotm::json::Value;

    constexpr uint32_t kMagic = 0x12345678u;
    constexpr size_t kSettingsSize = 0x380, kSettingsMagic = 0x34C;
    constexpr size_t kAdvSize = 0x280, kAdvMagic = 0x268, kAdvSlot0 = 0x10, kAdvStride = 0x3C;
    constexpr int kAdvSlots = 10;

    // Grupos do monsterSelectMode (12 x 4 SelectSlot). 0..9: monstros na ordem do aiMonsters de Shell::RandomlySelectAI, a entrada
    // j e o traje j. 10 e 11: fases do levelUnlocked (a entrada 47 sobra).
    const char *const kGroupMonsters[10] = {"raptros", "togera", "preytor", "congar", "robo-47", "agamo", "ultra-v", "magmo", "kineticlops", "zorgulon"};
    constexpr int kSweetToothSlot = 23;   // "Agamo Costume 4", ligado pelo McCheckForSweetTooth
    struct LevelSlot { int slot; const char *name; };
    const LevelSlot kLevelSlots[] = {{40, "volcano"}, {41, "minisf"}, {42, "capitol"}, {43, "ufo"}, {44, "dball"}, {45, "bigshot"}, {46, "crush"}};
    // MonsterNewNames (indice em m_monsterSel >> 5) e GameLevelNames (m_levelNum).
    const char *const kMonsters[] = {"null", "congar", "robo-47", "togera", "ultra-v", "preytor", "spider", "kineticlops", "magmo", "raptros",
                                     "agamo", "zorgulon", "ogre", "assboss", "jelly", "final", "temp16"};
    const char *const kLevels[] = {"default", "central", "vegas", "canyon2", "metro", "airport", "3-Mile", "sanfran", "island", "tokyo",
                                   "ufo", "capitol", "training", "canyon1", "minisf", "volcano", "tokyodm", "dylan", "david", "lars",
                                   "jenn", "yeon", "jacob", "steve", "pierre", "dball", "bigshot", "crush"};

    template <size_t N> Value nameOf(const char *const (&t)[N], uint32_t i)
    {
        return i < N ? Value::string(t[i]) : Value::integer(i);
    }
    template <size_t N> uint32_t indexOf(const char *const (&t)[N], const Value *v, uint32_t def)
    {
        if (!v)
            return def;
        if (v->type == Value::Str)
        {
            for (size_t k = 0; k < N; ++k)
                if (v->s == t[k])
                    return static_cast<uint32_t>(k);
            return def;
        }
        return static_cast<uint32_t>(v->asInt(def));
    }

    uint32_t rd(const std::vector<uint8_t> &b, size_t o) { return b[o] | (b[o + 1] << 8) | (b[o + 2] << 16) | (uint32_t(b[o + 3]) << 24); }
    void wr(std::vector<uint8_t> &b, size_t o, uint32_t v)
    {
        b[o] = v & 0xFF, b[o + 1] = (v >> 8) & 0xFF, b[o + 2] = (v >> 16) & 0xFF, b[o + 3] = v >> 24;
    }
    // Campo inteiro do JSON, ou o que ja estava no bloco.
    void wrField(std::vector<uint8_t> &b, size_t o, const Value *v)
    {
        if (v && v->isInt())
            wr(b, o, static_cast<uint32_t>(v->asInt()));
    }

    bool readFile(const fs::path &p, std::vector<uint8_t> &out)
    {
        std::ifstream f(p, std::ios::binary);
        if (!f)
            return false;
        out.assign(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
        return true;
    }
    bool writeFile(const fs::path &p, const void *data, size_t n)
    {
        // Grava num temporario e troca: um JSON pela metade nunca fica no lugar do bom.
        const fs::path tmp = p.string() + ".tmp";
        {
            std::ofstream f(tmp, std::ios::binary | std::ios::trunc);
            if (!f)
                return false;
            f.write(static_cast<const char *>(data), static_cast<std::streamsize>(n));
            if (!f)
                return false;
        }
        std::error_code ec;
        fs::rename(tmp, p, ec);
        if (ec)
        {
            fs::remove(tmp, ec);
            return false;
        }
        return true;
    }

    // ---- bloco -> JSON (sobre o JSON que ja existe, para manter o que nao e nosso) ----

    void settingsToJson(const std::vector<uint8_t> &b, Value &j)
    {
        Value &opt = j.at("options");
        opt.at("difficulty") = Value::integer(rd(b, 0x00));    // TheGame+0x1203CC (m_matchMode)
        opt.at("sfxVolume") = Value::integer(rd(b, 0x04));     // Shell+0x2A30, 0..0x400
        opt.at("musicVolume") = Value::integer(rd(b, 0x08));   // Shell+0x2A34
        opt.at("mono") = Value::boolean(rd(b, 0x10) != 0);     // Shell+0x2C20
        Value ctl = Value::array(), vib = Value::array();       // Shell+0x2B8C, Shell+0x2B94
        ctl.arr = {Value::integer(rd(b, 0x14)), Value::integer(rd(b, 0x18))};
        vib.arr = {Value::integer(rd(b, 0x1C)), Value::integer(rd(b, 0x20))};
        opt.at("controllerConfig") = ctl;
        opt.at("vibration") = vib;
        j.at("battleTokens") = Value::integer(rd(b, 0x24));    // Shell+0x2B24

        Value &un = j.at("unlocks");
        Value &mons = un.at("monsters");
        for (int g = 0; g < 10; ++g)
        {
            Value skins = Value::array();
            for (int k = 0; k < 4; ++k)
                skins.arr.push_back(Value::boolean(rd(b, 0x28 + (g * 4 + k) * 4) != 0));
            mons.at(kGroupMonsters[g]).at("skins") = skins;
        }
        Value &lv = un.at("levels");
        for (const auto &l : kLevelSlots)
            lv.at(l.name) = Value::boolean(rd(b, 0x28 + l.slot * 4) != 0);
        // Mecha Sweet Tooth (Agamo, traje 4): no PS2 vinha do save do Twisted Metal: Black no cartao (McCheckForSweetTooth). Aqui e
        // uma chave do usuario; so e criada, com o estado atual, quando falta.
        if (!un.get("mechaSweetTooth"))
            un.at("mechaSweetTooth") = Value::boolean(rd(b, 0x28 + kSweetToothSlot * 4) != 0);

        // Campos que o jogo grava e ainda nao se sabe para que servem: guardados como estao.
        Value &unk = j.at("unknown");
        unk.at("shell2A40") = Value::integer(rd(b, 0x0C));
        unk.at("slot47") = Value::integer(rd(b, 0x28 + 47 * 4));
        Value g4 = Value::array();
        for (int k = 0; k < 3; ++k)
            g4.arr.push_back(Value::integer(rd(b, 0xE8 + k * 4)));   // TheGame+0x1204B0..B8
        unk.at("game1204B0") = g4;
    }

    bool isNull(const Value &v) { return v.type == Value::Null; }

    // Slots do cartao `card` (10 a partir de card * 10) em j.saves; o array termina no ultimo slot nao nulo.
    void adventureToJson(const std::vector<uint8_t> &b, Value &j, int card)
    {
        Value &saves = j.at("saves");
        if (saves.type != Value::Arr)
            saves = Value::array();
        const size_t base = static_cast<size_t>(card) * kAdvSlots;
        if (saves.arr.size() < base + kAdvSlots)
            saves.arr.resize(base + kAdvSlots);
        const uint32_t n = std::min<uint32_t>(rd(b, 0), kAdvSlots);
        for (uint32_t i = 0; i < kAdvSlots; ++i)
        {
            Value &dst = saves.arr[base + i];
            dst = Value();   // slot alem do contador ou nunca gravado: null
            if (i >= n)
                continue;
            const size_t o = kAdvSlot0 + i * kAdvStride;
            const uint32_t lvl = rd(b, o), sel = rd(b, o + 4), lives = rd(b, o + 8), score = rd(b, o + 12), cos = rd(b, o + 16);
            if (!lvl && !sel && !lives && !score && !cos)
                continue;
            dst = Value::object();
            dst.at("level") = nameOf(kLevels, lvl);            // Shell+0x2BB0
            dst.at("monster") = nameOf(kMonsters, sel >> 5);   // Shell+0x2938 (m_monsterSel[0])
            dst.at("variant") = Value::integer(sel & 31);
            dst.at("lives") = Value::integer(lives);           // Shell+0x2BBC
            dst.at("score") = Value::integer(score);           // Shell+0x2B28 + 0x2B2C
            dst.at("costume") = Value::integer(cos);           // Shell+0x2BE0
        }
        while (!saves.arr.empty() && isNull(saves.arr.back()))
            saves.arr.pop_back();
    }

    // ---- JSON -> bloco (sobre o bloco que ja existe; o que o JSON nao diz fica como estava) ----

    void jsonToSettings(const Value &j, std::vector<uint8_t> &b)
    {
        if (b.size() != kSettingsSize)
            b.assign(kSettingsSize, 0);
        // 0xF4..0x34B e depois da marca: o jogo grava ali lixo da pilha e nunca le. Vai zero.
        std::fill(b.begin() + 0xF4, b.end(), 0);
        if (const Value *opt = j.get("options"))
        {
            wrField(b, 0x00, opt->get("difficulty"));
            wrField(b, 0x04, opt->get("sfxVolume"));
            wrField(b, 0x08, opt->get("musicVolume"));
            wrField(b, 0x10, opt->get("mono"));
            if (const Value *c = opt->get("controllerConfig"))
                for (size_t k = 0; k < 2 && k < c->arr.size(); ++k)
                    wrField(b, 0x14 + k * 4, &c->arr[k]);
            if (const Value *v = opt->get("vibration"))
                for (size_t k = 0; k < 2 && k < v->arr.size(); ++k)
                    wrField(b, 0x1C + k * 4, &v->arr[k]);
        }
        wrField(b, 0x24, j.get("battleTokens"));
        if (const Value *un = j.get("unlocks"))
        {
            if (const Value *mons = un->get("monsters"))
                for (int g = 0; g < 10; ++g)
                    if (const Value *m = mons->get(kGroupMonsters[g]))
                        if (const Value *sk = m->get("skins"))
                            for (size_t k = 0; k < 4 && k < sk->arr.size(); ++k)
                                wr(b, 0x28 + (g * 4 + k) * 4, sk->arr[k].asBool() ? 1u : 0u);
            if (const Value *lv = un->get("levels"))
                for (const auto &l : kLevelSlots)
                    if (const Value *v = lv->get(l.name))
                        wr(b, 0x28 + l.slot * 4, v->asBool() ? 1u : 0u);
            // Vale mais que agamo.skins[3]: e a chave que o usuario liga e desliga.
            if (const Value *st = un->get("mechaSweetTooth"))
                wr(b, 0x28 + kSweetToothSlot * 4, st->asBool() ? 1u : 0u);
        }
        if (const Value *unk = j.get("unknown"))
        {
            wrField(b, 0x0C, unk->get("shell2A40"));
            wrField(b, 0x28 + 47 * 4, unk->get("slot47"));
            if (const Value *g4 = unk->get("game1204B0"))
                for (size_t k = 0; k < 3 && k < g4->arr.size(); ++k)
                    wrField(b, 0xE8 + k * 4, &g4->arr[k]);
        }
        wr(b, kSettingsMagic, kMagic);
    }

    // Slots do cartao `card` no bloco; o contador e o ultimo slot nao nulo + 1, como o jogo deixa.
    void jsonToAdventure(const Value &j, std::vector<uint8_t> &b, int card)
    {
        b.assign(kAdvSize, 0);
        const Value *saves = j.get("saves");
        const size_t base = static_cast<size_t>(card) * kAdvSlots;
        size_t n = 0;
        if (saves && saves->type == Value::Arr)
            for (size_t i = 0; i < kAdvSlots && base + i < saves->arr.size(); ++i)
                if (saves->arr[base + i].type == Value::Obj)
                    n = i + 1;
        wr(b, 0, static_cast<uint32_t>(n));
        for (size_t i = 0; i < n; ++i)
        {
            const Value &s = saves->arr[base + i];
            if (s.type != Value::Obj)
                continue;
            const size_t o = kAdvSlot0 + i * kAdvStride;
            wr(b, o, indexOf(kLevels, s.get("level"), 0));
            const Value *var = s.get("variant");
            wr(b, o + 4, (indexOf(kMonsters, s.get("monster"), 0) << 5) | (var ? static_cast<uint32_t>(var->asInt()) & 31 : 0));
            wrField(b, o + 8, s.get("lives"));
            wrField(b, o + 12, s.get("score"));
            wrField(b, o + 16, s.get("costume"));
        }
        wr(b, kAdvMagic, kMagic);
    }

    // ---- arquivos ----

    struct Stamp { bool exists = false; fs::file_time_type t{}; uintmax_t size = 0; };
    bool operator!=(const Stamp &a, const Stamp &b) { return a.exists != b.exists || a.t != b.t || a.size != b.size; }
    Stamp stampOf(const fs::path &p)
    {
        Stamp s;
        std::error_code ec;
        s.t = fs::last_write_time(p, ec);
        if (ec)
            return Stamp{};
        s.size = fs::file_size(p, ec);
        s.exists = !ec;
        return s;
    }

    struct Card
    {
        fs::path dir;
        Stamp settings, adventure;   // como ficaram os binarios depois do ultimo stdInit
    };
    Card g_cards[2];
    fs::path g_json;
    bool g_badJson = false;   // o JSON nao abriu: nao mexer nos binarios; antes de reescreve-lo, guardar o do usuario em .bad
    bool g_on = false;

    // Mesma regra do stub (Kernel/Stubs/MemoryCard.cpp, getMcRootPath): mcRoot, ou <pasta do ELF>/mc0; a porta 1 e o mc1 ao lado.
    void initPaths()
    {
        const PS2Runtime::IoPaths &io = PS2Runtime::getIoPaths();
        fs::path root = io.mcRoot;
        if (root.empty())
            root = !io.elfDirectory.empty() ? io.elfDirectory / "mc0" : fs::current_path() / "mc0";
        root = root.lexically_normal();
        const fs::path parent = root.parent_path();
        g_cards[0].dir = root / "BASCUS-97197";
        g_cards[1].dir = parent / "mc1" / "BASCUS-97197";
        g_json = parent / "save.json";
    }

    bool loadJson(Value &j)
    {
        std::vector<uint8_t> raw;
        if (!readFile(g_json, raw))
            return false;
        std::string err;
        if (!wotm::json::parse(std::string(raw.begin(), raw.end()), j, &err) || j.type != Value::Obj)
        {
            if (!g_badJson)
                std::fprintf(stderr, "[wotm] save: %s nao abriu (%s); o jogo usa o cartao como esta\n", g_json.string().c_str(),
                             err.empty() ? "nao e um objeto" : err.c_str());
            g_badJson = true;
            return false;
        }
        g_badJson = false;
        return true;
    }

    void saveJson(const Value &j, const char *why)
    {
        const std::string text = wotm::json::dump(j);
        if (writeFile(g_json, text.data(), text.size()))
            std::fprintf(stderr, "[wotm] save: %s atualizado (%s)\n", g_json.string().c_str(), why);
        else
            std::fprintf(stderr, "[wotm] save: nao consegui gravar %s\n", g_json.string().c_str());
    }

    bool readSettings(const Card &c, std::vector<uint8_t> &s)
    {
        return readFile(c.dir / "BASCUS-97197", s) && s.size() == kSettingsSize && rd(s, kSettingsMagic) == kMagic;
    }
    bool readAdventure(const Card &c, std::vector<uint8_t> &a)
    {
        return readFile(c.dir / "savegame.dat", a) && a.size() == kAdvSize && rd(a, kAdvMagic) == kMagic;
    }
    bool hasSlots(const std::vector<uint8_t> &a) { return rd(a, 0) != 0; }

    void restamp()
    {
        for (Card &c : g_cards)
        {
            c.settings = stampOf(c.dir / "BASCUS-97197");
            c.adventure = stampOf(c.dir / "savegame.dat");
        }
    }

    // Binarios dos cartoes em `cards` (bits) -> JSON. Com o JSON quebrado, guarda-o como .bad antes de escrever o novo.
    void binToJson(unsigned cards, const char *why)
    {
        Value j;
        std::error_code ec;
        if (fs::exists(g_json, ec) && !loadJson(j))
        {
            fs::copy_file(g_json, g_json.string() + ".bad", fs::copy_options::overwrite_existing, ec);
            std::fprintf(stderr, "[wotm] save: o JSON quebrado ficou em %s.bad\n", g_json.string().c_str());
            j = Value();
            g_badJson = false;
        }
        if (j.type != Value::Obj)
            j = Value::object();
        if (!j.get("format"))
            j.at("format") = Value::integer(1);
        bool any = false;
        // Opcoes: as do cartao que mudou (as dos dois sao as mesmas; com os dois, vale o 1)
        for (int c = 1; c >= 0; --c)
        {
            std::vector<uint8_t> s;
            if ((cards >> c & 1) && readSettings(g_cards[c], s))
                settingsToJson(s, j), any = true;
        }
        for (int c = 0; c < 2; ++c)
        {
            std::vector<uint8_t> a;
            if ((cards >> c & 1) && readAdventure(g_cards[c], a))
                adventureToJson(a, j, c), any = true;
        }
        if (any)
            saveJson(j, why);
    }

    // JSON -> binarios dos dois cartoes, antes de o jogo abrir o cartao.
    void beforeMc()
    {
        std::error_code ec;
        if (!fs::exists(g_json, ec))
        {
            binToJson(3, "criado a partir dos cartoes");
            restamp();
            return;
        }
        Value j;
        if (!loadJson(j))
        {
            restamp();
            return;
        }
        const bool hasSettings = j.get("options") || j.get("unlocks") || j.get("battleTokens");
        bool imported = false;
        // JSON de antes da chave mechaSweetTooth: cria com o estado do cartao 1
        if (const Value *un = j.get("unlocks"); un && !un->get("mechaSweetTooth"))
        {
            std::vector<uint8_t> s;
            j.at("unlocks").at("mechaSweetTooth") = Value::boolean(readSettings(g_cards[0], s) && rd(s, 0x28 + kSweetToothSlot * 4) != 0);
            imported = true;
        }
        const Value *saves = j.get("saves");
        for (int c = 0; c < 2; ++c)
        {
            Card &card = g_cards[c];
            const size_t base = static_cast<size_t>(c) * kAdvSlots;
            const bool sliceInJson = saves && saves->type == Value::Arr && saves->arr.size() > base;
            std::vector<uint8_t> a;
            const bool cardHasSlots = readAdventure(card, a) && hasSlots(a);
            // Cartao 2 sem pasta e sem slots no JSON: nao cria (o jogo veria um cartao com dados que nao existia).
            if (c > 0 && !sliceInJson && !fs::exists(card.dir, ec))
                continue;
            fs::create_directories(card.dir, ec);
            if (hasSettings)
            {
                std::vector<uint8_t> b;
                readFile(card.dir / "BASCUS-97197", b);
                jsonToSettings(j, b);
                if (!writeFile(card.dir / "BASCUS-97197", b.data(), b.size()))
                    std::fprintf(stderr, "[wotm] save: nao consegui gravar as opcoes em %s\n", card.dir.string().c_str());
            }
            if (!sliceInJson && cardHasSlots)
            {
                // O JSON nao tem os slots deste cartao e ele tem aventuras: importa em vez de apagar.
                adventureToJson(a, j, c);
                saves = j.get("saves");
                imported = true;
                std::fprintf(stderr, "[wotm] save: aventuras do cartao %d importadas para os slots %zu..%zu\n", c + 1, base, base + kAdvSlots - 1);
                continue;
            }
            if (saves || c == 0)
            {
                std::vector<uint8_t> b;
                jsonToAdventure(j, b, c);
                if (!writeFile(card.dir / "savegame.dat", b.data(), b.size()))
                    std::fprintf(stderr, "[wotm] save: nao consegui gravar a aventura em %s\n", card.dir.string().c_str());
            }
        }
        if (imported)
            saveJson(j, "chaves novas ou aventuras importadas do cartao");
        restamp();
    }

    uint32_t ramU32(const uint8_t *rdram, uint32_t a)
    {
        uint32_t v;
        std::memcpy(&v, rdram + (a & 0x1FFFFFFu), 4);
        return v;
    }

    void afterMc(const uint8_t *rdram)
    {
        // O que o jogo tem agora (confere que o JSON chegou: depois do boot, os tokens e a dificuldade do save.json)
        const uint32_t sh = ramU32(rdram, wotm::sym::shell), gm = ramU32(rdram, wotm::sym::game);
        if (sh && gm)
            std::fprintf(stderr, "[wotm] save: no jogo, %u tokens, dificuldade %u, Mecha Sweet Tooth %s\n", ramU32(rdram, sh + 0x2B24),
                         ramU32(rdram, gm + 0x1203CC), ramU32(rdram, wotm::sym::monsterSelectMode + kSweetToothSlot * 0x2C + 4) ? "liberado" : "travado");
        unsigned changed = 0;
        for (int c = 0; c < 2; ++c)
            if (stampOf(g_cards[c].dir / "BASCUS-97197") != g_cards[c].settings || stampOf(g_cards[c].dir / "savegame.dat") != g_cards[c].adventure)
                changed |= 1u << c;
        if (changed)
            binToJson(changed, changed == 3 ? "o jogo gravou nos dois cartoes" : changed == 1 ? "o jogo gravou no cartao 1" : "o jogo gravou no cartao 2");
        restamp();
    }

    // ---- gravacao direta, sem as paginas do cartao (POS_FRONTEIRA.md, item 3, passo c) ----
    //
    // Boot, opcoes, destrave, endurance e fim da historia nao abrem mais o cartao: o MonsterMc::update* de cada um (chamado num laco
    // ate MonsterMc+4) vira uma copia direta entre a RAM e o JSON e termina na mesma chamada. A aventura (updateStoryModeSave/Load)
    // continua com as paginas e os binarios (item 5). Os tokens que o jogo soma fora dessas gravacoes (fim de fase da historia, em
    // Shell::EvaluateOnePlayerStoryStatus) sao pegos pela vigia por quadro (poll). WOTM_SAVEAUTO=0 volta as paginas do cartao.

    void w32(uint8_t *rdram, uint32_t a, uint32_t v) { std::memcpy(rdram + (a & 0x1FFFFFFu), &v, 4); }
    constexpr uint32_t kSelStride = 0x2C, kSelState = 4, kSelCost = 8;   // SelectSlot (12 grupos de 4, 0xB0 por grupo)
    constexpr uint32_t kGameMatchMode = 0x1203CC, kGameUnk = 0x1204B0;
    constexpr uint32_t kShellSfx = 0x2A30, kShellMusic = 0x2A34, kShell2A40 = 0x2A40, kShellMono = 0x2C20, kShellCtl = 0x2B8C,
                       kShellVib = 0x2B94, kShellTokens = 0x2B24;

    // MonsterMc::getSettingsData: RAM -> bloco de 0x380 (o resto zero, com a marca)
    bool ramToBlock(const uint8_t *rdram, std::vector<uint8_t> &b)
    {
        const uint32_t sh = ramU32(rdram, wotm::sym::shell), gm = ramU32(rdram, wotm::sym::game);
        if (!sh || !gm)
            return false;
        b.assign(kSettingsSize, 0);
        wr(b, 0x00, ramU32(rdram, gm + kGameMatchMode));
        wr(b, 0x04, ramU32(rdram, sh + kShellSfx));
        wr(b, 0x08, ramU32(rdram, sh + kShellMusic));
        wr(b, 0x0C, ramU32(rdram, sh + kShell2A40));
        wr(b, 0x10, ramU32(rdram, sh + kShellMono));
        for (uint32_t k = 0; k < 2; ++k)
        {
            wr(b, 0x14 + k * 4, ramU32(rdram, sh + kShellCtl + k * 4));
            wr(b, 0x1C + k * 4, ramU32(rdram, sh + kShellVib + k * 4));
        }
        wr(b, 0x24, ramU32(rdram, sh + kShellTokens));
        for (uint32_t i = 0; i < 48; ++i)
            wr(b, 0x28 + i * 4, ramU32(rdram, wotm::sym::monsterSelectMode + i * kSelStride + kSelState));
        for (uint32_t k = 0; k < 3; ++k)
            wr(b, 0xE8 + k * 4, ramU32(rdram, gm + kGameUnk + k * 4));
        wr(b, kSettingsMagic, kMagic);
        return true;
    }

    // MonsterMc::setSettingsData: bloco -> RAM (mono vira 0/1, como no jogo)
    void blockToRam(uint8_t *rdram, const std::vector<uint8_t> &b)
    {
        const uint32_t sh = ramU32(rdram, wotm::sym::shell), gm = ramU32(rdram, wotm::sym::game);
        if (!sh || !gm || b.size() != kSettingsSize)
            return;
        w32(rdram, gm + kGameMatchMode, rd(b, 0x00));
        w32(rdram, sh + kShellSfx, rd(b, 0x04));
        w32(rdram, sh + kShellMusic, rd(b, 0x08));
        w32(rdram, sh + kShell2A40, rd(b, 0x0C));
        w32(rdram, sh + kShellMono, rd(b, 0x10) != 0);
        for (uint32_t k = 0; k < 2; ++k)
        {
            w32(rdram, sh + kShellCtl + k * 4, rd(b, 0x14 + k * 4));
            w32(rdram, sh + kShellVib + k * 4, rd(b, 0x1C + k * 4));
        }
        w32(rdram, sh + kShellTokens, rd(b, 0x24));
        for (uint32_t i = 0; i < 48; ++i)
            w32(rdram, wotm::sym::monsterSelectMode + i * kSelStride + kSelState, rd(b, 0x28 + i * 4));
        for (uint32_t k = 0; k < 3; ++k)
            w32(rdram, gm + kGameUnk + k * 4, rd(b, 0xE8 + k * 4));
    }

    // Tokens e destraves da ultima gravacao (a vigia compara com a RAM)
    struct Snap
    {
        bool valid = false;
        uint32_t v[49] = {};
        bool operator==(const Snap &o) const { return valid == o.valid && std::memcmp(v, o.v, sizeof v) == 0; }
    };
    Snap g_snap;
    bool g_auto = false;
    // cada gravacao do JSON soma g_wotmSaveCount (definido antes do namespace; o icone de salvamento, wotm_saveicon.hpp, le)

    Snap snapOf(const uint8_t *rdram)
    {
        Snap s;
        const uint32_t sh = ramU32(rdram, wotm::sym::shell);
        if (!sh)
            return s;
        s.valid = true;
        s.v[0] = ramU32(rdram, sh + kShellTokens);
        for (uint32_t i = 0; i < 48; ++i)
            s.v[1 + i] = ramU32(rdram, wotm::sym::monsterSelectMode + i * kSelStride + kSelState);
        return s;
    }

    // RAM -> JSON. Sem `options`, so tokens e destraves (as opcoes do arquivo ficam: o menu de opcoes pode estar com valores ainda
    // nao aceitos).
    void ramToJson(const uint8_t *rdram, bool options, const char *why)
    {
        std::vector<uint8_t> b;
        if (!ramToBlock(rdram, b))
            return;
        Value j;
        std::error_code ec;
        if (fs::exists(g_json, ec) && !loadJson(j))
        {
            fs::copy_file(g_json, g_json.string() + ".bad", fs::copy_options::overwrite_existing, ec);
            std::fprintf(stderr, "[wotm] save: o JSON quebrado ficou em %s.bad\n", g_json.string().c_str());
            j = Value();
            g_badJson = false;
        }
        if (j.type != Value::Obj)
            j = Value::object();
        if (!j.get("format"))
            j.at("format") = Value::integer(1);
        const Value *oldOpt = j.get("options");
        const bool keepOpt = !options && oldOpt;
        const Value kept = keepOpt ? *oldOpt : Value();
        settingsToJson(b, j);
        if (keepOpt)
            j.at("options") = kept;
        saveJson(j, why);
        g_snap = snapOf(rdram);
        g_wotmSaveCount.fetch_add(1, std::memory_order_relaxed);
    }

    // JSON -> RAM (boot e "load settings"). Sem JSON, cria-o dos cartoes antigos se houver (beforeMc); sem nada, o jogo fica com
    // os valores do construtor do Shell, como num cartao vazio.
    void jsonToRam(uint8_t *rdram)
    {
        beforeMc();
        Value j;
        std::vector<uint8_t> b;
        if (loadJson(j) && (j.get("options") || j.get("unlocks") || j.get("battleTokens")) && ramToBlock(rdram, b))
        {
            jsonToSettings(j, b);
            blockToRam(rdram, b);
        }
        g_snap = snapOf(rdram);
        afterMc(rdram);   // so a linha do log; os binarios nao mudaram
    }

    // A compra da tela de destrave so e efetivada dentro do McSaveSettings::update (g_monsterToUnlock/g_skinToUnlock: liga o
    // state e desconta o preco); a tela poe -1 nos dois logo depois do laco.
    void applyPendingUnlock(uint8_t *rdram)
    {
        const int32_t m = static_cast<int32_t>(ramU32(rdram, wotm::sym::g_monsterToUnlock));
        const int32_t s = static_cast<int32_t>(ramU32(rdram, wotm::sym::g_skinToUnlock));
        const uint32_t sh = ramU32(rdram, wotm::sym::shell);
        if (m < 0 || s < 0 || !sh)
            return;
        const uint32_t e = wotm::sym::monsterSelectMode + (static_cast<uint32_t>(m) * 4 + static_cast<uint32_t>(s)) * kSelStride;
        if (ramU32(rdram, e + kSelState))
            return;
        w32(rdram, sh + kShellTokens, ramU32(rdram, sh + kShellTokens) - ramU32(rdram, e + kSelCost));
        w32(rdram, e + kSelState, 1);
    }

    enum class Seq { BootLoad, SettingsLoad, SettingsSave, UnlockSave, EndOfStorySave, EndureSave };
    struct SeqHook { uint32_t fn; Seq seq; const char *why; };
    const SeqHook kSeqHooks[] = {
        {wotm::fn::updateBootLoad__9MonsterMc, Seq::BootLoad, "boot"},
        {wotm::fn::updateSettingsLoad__9MonsterMc, Seq::SettingsLoad, "load settings"},
        {wotm::fn::updateSettingsSave__9MonsterMc, Seq::SettingsSave, "opcoes"},
        {wotm::fn::updateUnlockSave__9MonsterMc, Seq::UnlockSave, "destrave"},
        {wotm::fn::updateEndOfStorySave__9MonsterMc, Seq::EndOfStorySave, "fim da historia"},
        {wotm::fn::updateEndureSave__9MonsterMc, Seq::EndureSave, "endurance"},
    };

    template <size_t I> void seqReplaced(uint8_t *rdram, R5900Context *ctx, PS2Runtime *)
    {
        const SeqHook &h = kSeqHooks[I];
        const uint32_t self = GPR_U32(ctx, 4);
        if (h.seq == Seq::BootLoad || h.seq == Seq::SettingsLoad)
            jsonToRam(rdram);
        else
        {
            applyPendingUnlock(rdram);
            ramToJson(rdram, true, h.why);
        }
        // Como o stdExit: MonsterMc+0 (montar na proxima) e +4 (pronto, o laco do chamador sai). McPageMgr+0x130: a sequencia chegou
        // a ultima pagina (a tela de opcoes so volta com ele ligado).
        w32(rdram, self + 0, 1);
        w32(rdram, self + 4, 1);
        w32(rdram, wotm::sym::x_9McPageMgr_global + 0x130, 1);
        ctx->pc = GPR_U32(ctx, 31);
    }
    template <size_t... I> void registerSeqs(PS2Runtime &runtime, std::index_sequence<I...>)
    {
        (runtime.registerFunction(kSeqHooks[I].fn, &seqReplaced<I>), ...);
    }

    // Uma vez por quadro (sceGsSyncV): tokens ou destraves mudaram fora de uma gravacao -> JSON (so eles).
    void poll(const uint8_t *rdram)
    {
        if (!g_auto || !g_snap.valid)
            return;
        const Snap now = snapOf(rdram);
        if (now.valid && !(now == g_snap))
        {
            const bool tokens = now.v[0] != g_snap.v[0];
            ramToJson(rdram, false, tokens ? "tokens" : "destraves");
        }
    }

    constexpr uint32_t kStdInit = wotm::fn::stdInit__9MonsterMc, kStdExit = wotm::fn::stdExit__9MonsterMc;
    PS2Runtime::RecompiledFunction g_origStdInit = nullptr, g_origStdExit = nullptr;

    void stdInitTapped(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
    {
        if (ctx->pc == kStdInit)
            beforeMc();
        g_origStdInit(rdram, ctx, runtime);
    }
    void stdExitTapped(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
    {
        if (ctx->pc == kStdExit)
            afterMc(rdram);
        g_origStdExit(rdram, ctx, runtime);
    }

    void install(PS2Runtime &runtime)
    {
        const char *e = std::getenv("WOTM_SAVEJSON");
        if (e && e[0] == '0')
        {
            std::fprintf(stderr, "[wotm] save so no cartao mc0/, sem JSON (WOTM_SAVEJSON=0)\n");
            return;
        }
        if (!(g_origStdInit = runtime.lookupFunction(kStdInit)) || !(g_origStdExit = runtime.lookupFunction(kStdExit)))
        {
            std::fprintf(stderr, "[wotm] save em JSON indisponivel: MonsterMc::stdInit/stdExit sem funcao no runtime\n");
            return;
        }
        initPaths();
        runtime.registerFunction(kStdInit, &stdInitTapped);
        runtime.registerFunction(kStdExit, &stdExitTapped);
        g_on = true;
        const char *a = std::getenv("WOTM_SAVEAUTO");
        g_auto = !(a && a[0] == '0');
        if (g_auto)
            registerSeqs(runtime, std::make_index_sequence<std::size(kSeqHooks)>{});
        std::fprintf(stderr, "[wotm] save em JSON: %s (WOTM_SAVEJSON=0 desliga); %s\n", g_json.string().c_str(),
                     g_auto ? "opcoes, destraves e tokens gravados sozinhos, sem as telas do cartao (WOTM_SAVEAUTO=0 volta as telas)"
                            : "telas do cartao como no PS2 (WOTM_SAVEAUTO=0)");
    }
}
