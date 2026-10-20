"""Mostra o save do memory card (pasta BASCUS-97197 do mc0/ do runtime) como JSON, com chaves por nome.

Uso: python tools/mcsave.py <pasta BASCUS-97197>

Layout em docs/passos/POS_FRONTEIRA.md (item 3). Só lê; é o rascunho do formato do save em JSON.
"""
import json
import os
import struct
import sys

MAGIC = 0x12345678

# monsterSelectMode: 12 grupos de 4 SelectSlot. Grupos 0..9 na ordem de Shell::RandomlySelectAI (aiMonsters),
# a entrada j do grupo é o traje j. Grupos 10 e 11 são as fases (levelUnlocked); a última entrada sobra.
GROUP_MONSTERS = ["raptros", "togera", "preytor", "congar", "robo-47", "agamo", "ultra-v", "magmo", "kineticlops", "zorgulon"]
LEVEL_SLOTS = {40: "volcano", 41: "minisf", 42: "capitol", 43: "ufo", 44: "dball", 45: "bigshot", 46: "crush"}

# MonsterNewNames (índice do monstro em m_monsterSel >> 5) e GameLevelNames (m_levelNum).
MONSTERS = ["null", "congar", "robo-47", "togera", "ultra-v", "preytor", "spider", "kineticlops", "magmo", "raptros",
            "agamo", "zorgulon", "ogre", "assboss", "jelly", "final", "temp16"]
LEVELS = ["default", "central", "vegas", "canyon2", "metro", "airport", "3-Mile", "sanfran", "island", "tokyo", "ufo",
          "capitol", "training", "canyon1", "minisf", "volcano", "tokyodm", "dylan", "david", "lars", "jenn", "yeon",
          "jacob", "steve", "pierre", "dball", "bigshot", "crush"]


def u32(b, o):
    return struct.unpack_from("<I", b, o)[0]


def name(table, i):
    return table[i] if 0 <= i < len(table) else i


def settings(b):
    """BASCUS-97197 (0x380 bytes): MonsterMc::getSettingsData."""
    flags = [u32(b, 0x28 + i * 4) for i in range(48)]
    out = {
        "difficulty": u32(b, 0x00),          # TheGame+0x1203CC (m_matchMode)
        "sfxVolume": u32(b, 0x04),           # Shell+0x2A30, 0..0x400
        "musicVolume": u32(b, 0x08),         # Shell+0x2A34
        "shell2A40": u32(b, 0x0C),           # Shell+0x2A40: 50 no construtor, ninguém mais lê
        "mono": bool(u32(b, 0x10)),          # Shell+0x2C20
        "controllerConfig": [u32(b, 0x14), u32(b, 0x18)],  # Shell+0x2B8C
        "vibration": [u32(b, 0x1C), u32(b, 0x20)],         # Shell+0x2B94
        "battleTokens": u32(b, 0x24),        # Shell+0x2B24
        "monsters": {m: {"skins": [bool(flags[g * 4 + j]) for j in range(4)]} for g, m in enumerate(GROUP_MONSTERS)},
        "levels": {lv: bool(flags[i]) for i, lv in LEVEL_SLOTS.items()},
        "game1204B0": [u32(b, 0xE8 + i * 4) for i in range(3)],  # TheGame+0x1204B0..B8, não identificado
    }
    if flags[47]:
        out["slot47"] = flags[47]
    if u32(b, 0x34C) != MAGIC:
        out["badMagic"] = hex(u32(b, 0x34C))
    return out


def adventure(b):
    """savegame.dat (0x280 bytes): MonsterMc::getSaveData, 10 slots de 0x3C (só 0x14 usados)."""
    saves = []
    for i in range(u32(b, 0)):
        lvl, sel, lives, score, costume = struct.unpack_from("<5I", b, 0x10 + i * 0x3C)
        saves.append({
            "level": name(LEVELS, lvl),      # Shell+0x2BB0 (m_levelNum)
            "monster": name(MONSTERS, sel >> 5),  # Shell+0x2938 (m_monsterSel[0])
            "variant": sel & 31,
            "lives": lives,                  # Shell+0x2BBC (m_lives[0])
            "score": score,                  # Shell+0x2B28 + 0x2B2C; ao carregar vai todo para 0x2B2C
            "costume": costume,              # Shell+0x2BE0 (m_costume[0])
        })
    out = {"saves": saves}
    if u32(b, 0x268) != MAGIC:
        out["badMagic"] = hex(u32(b, 0x268))
    return out


def main():
    d = sys.argv[1]
    out = {}
    for fn, parse, size in (("BASCUS-97197", settings, 0x380), ("savegame.dat", adventure, 0x280)):
        p = os.path.join(d, fn)
        if os.path.exists(p):
            b = open(p, "rb").read()
            if len(b) != size:
                print("%s: %d bytes, esperado %d" % (fn, len(b), size), file=sys.stderr)
            out.update(parse(b))
    print(json.dumps(out, indent=2))


if __name__ == "__main__":
    main()
