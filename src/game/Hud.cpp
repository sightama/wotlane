#include "common.h"
#include "game/hud.h"
#include "game/game.h"
#include "game/level_pickups.h"
#include "cs_pool.h"
#include "hieri_types.h"

class Letterboxing {
public:
    void initBefore(void);
    void setLetterboxCS(_hierhead *h);
};
extern Letterboxing hudLetterbox __asm__("_3Hud$m_letterbox");
class ScreenPolys {
public:
    static int initSourceObject(_hierobject *o, int n);
};
class McPageMgr {
public:
    static McPageMgr global;
    void initButton(_hierhead *h);
};
extern int gHudTextureObjNum;
__asm__("#SNFIX_SMALL gHudTextureObjNum");

extern "C" int sprintf(char *, const char *, ...);
extern char **gMessageBuf;
__asm__("#SNFIX_SMALL gMessageBuf");

/* Message ids: 4 combo, 7 health, 8 energy. A message already on screen gets its alpha/timer refreshed, otherwise it is added. */
#define REFRESH(id, t) do {         if (msgId[0] == (id)) { msgAlpha[0] = 0x80; msgTimer[0] = (t); }         else if (msgId[1] == (id)) { msgAlpha[1] = 0x80; msgTimer[1] = (t); }         else if (msgId[2] == (id)) { msgAlpha[2] = 0x80; msgTimer[2] = (t); }     } while (0)

INCLUDE_ASM("asm/nonmatchings/game/Hud", initBefore__3Hudi);
INCLUDE_ASM("asm/nonmatchings/game/Hud", initAfter__3Hudi);
INCLUDE_ASM("asm/nonmatchings/game/Hud", initForReplay__3Hud);
INCLUDE_ASM("asm/nonmatchings/game/Hud", update__3Hud);
#ifdef NON_MATCHING
/* 2/128 palavras; o retail repete o caso para cada um dos 4 botões, lê `game` por gp e por endereço absoluto e a tabela de saltos fica no .rodata */
/* HUD nodes of the shell/level database, by node id (id1) */
void Hud::parseHudElements(_hierhead *h)
{
    Hud *hud;

    switch (h->id1) {
    case 0x1F42:
        LevelPickups::createPickupHighlight(h);
        break;
    case 0x1F43:
        game->m_huds[0].m_cs1F43 = CsPool::csHPActivate();
        hierSetCsEpNode(game->m_huds[0].m_cs1F43, h);
        game->m_huds[0].m_cs1F43->drawMe = 0;
        break;
    case 0x1F44:
        hudLetterbox.setLetterboxCS(h);
        break;
    case 0x1F46:
        gHudTextureObjNum = ScreenPolys::initSourceObject((_hierobject *)h, 5000);
        break;
    case 0x1F47:
        hud = &game->m_huds[h->id2 != 0];
        hud->m_node1F47 = h;
        break;
    case 0x1F4D:
        hud = &game->m_huds[h->id2 != 0];
        hud->m_node1F4D = h;
        break;
    case 0x1F55:
        game->m_huds[0].m_node1F55 = h;
        break;
    case 0x1F51:
        McPageMgr::global.initButton(h);
        game->m_huds[0].m_mcButtonCs[0] = CsPool::csHPActivate();
        hierSetCsEpNode(game->m_huds[0].m_mcButtonCs[0], h);
        break;
    case 0x1F52:
        McPageMgr::global.initButton(h);
        game->m_huds[0].m_mcButtonCs[1] = CsPool::csHPActivate();
        hierSetCsEpNode(game->m_huds[0].m_mcButtonCs[1], h);
        break;
    case 0x1F53:
        McPageMgr::global.initButton(h);
        game->m_huds[0].m_mcButtonCs[2] = CsPool::csHPActivate();
        hierSetCsEpNode(game->m_huds[0].m_mcButtonCs[2], h);
        break;
    case 0x1F54:
        McPageMgr::global.initButton(h);
        game->m_huds[0].m_mcButtonCs[3] = CsPool::csHPActivate();
        hierSetCsEpNode(game->m_huds[0].m_mcButtonCs[3], h);
        break;
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/game/Hud", parseHudElements__3HudP9_hierhead);
#endif
void Hud::print(void)
{
}
INCLUDE_ASM("asm/nonmatchings/game/Hud", buildPacketHead__3HudP6QwData);
INCLUDE_ASM("asm/nonmatchings/game/Hud", dmaPacket__3Hud);
INCLUDE_ASM("asm/nonmatchings/game/Hud", buildSeparator__3Hud);
INCLUDE_ASM("asm/nonmatchings/game/Hud", addHealthIndicators__3Hud);
INCLUDE_ASM("asm/nonmatchings/game/Hud", drawHealthIcon__3HudP7Monsteri);
INCLUDE_ASM("asm/nonmatchings/game/Hud", drawBossHealthBar__3Hudf);
#ifdef NON_MATCHING
void Hud::registerComboHit(void)
{
    comboHits++;
    sprintf(gMessageBuf[4], "%d-Hit Combo", comboHits);
    if (msgId[0] != 4 && msgId[1] != 4 && msgId[2] != 4 && comboHits >= 2)
        addMessage(4, 0);
    else
        REFRESH(4, 0x64);
}
#else
INCLUDE_ASM("asm/nonmatchings/game/Hud", registerComboHit__3Hud);
#endif
#ifdef NON_MATCHING
/* Only while the viewed monster's stamina is not full. */
void Hud::registerStaminaCredit(int amount)
{
    StaminaMeter &st = game->m_slots[game->m_viewSlot[view]].m_stamina;

    if (!(st.maxLevel <= st.cur)) {
        staminaCredit += amount;
        sprintf(gMessageBuf[8], "Energy +%d", staminaCredit);
        if (msgId[0] != 8 && msgId[1] != 8 && msgId[2] != 8)
            addMessage(8, 0);
        else
            REFRESH(8, 0x3C);
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/game/Hud", registerStaminaCredit__3Hudi);
#endif
#ifdef NON_MATCHING
void Hud::registerHealthCredit(int amount)
{
    healthCredit += amount;
    sprintf(gMessageBuf[7], "Health +%d", healthCredit);
    if (msgId[0] != 7 && msgId[1] != 7 && msgId[2] != 7)
        addMessage(7, 0);
    else
        REFRESH(7, 0x3C);
}
#else
INCLUDE_ASM("asm/nonmatchings/game/Hud", registerHealthCredit__3Hudi);
#endif
INCLUDE_ASM("asm/nonmatchings/game/Hud", addMessage__3Hudii);
INCLUDE_ASM("asm/nonmatchings/game/Hud", updateMessage__3Hud);
INCLUDE_ASM("asm/nonmatchings/game/Hud", addTextBoxMessage__3Hudi);
INCLUDE_ASM("asm/nonmatchings/game/Hud", updateTextBoxMessage__3Hud);
INCLUDE_ASM("asm/nonmatchings/game/Hud", MonsterNewNames_006EAA78);
void Letterboxing::initBefore(void)
{
    *(int *)((char *)this + 0x2C) = 0;
    *(int *)((char *)this + 0x0) = 0;
    *(int *)((char *)this + 0x24) = 0;
}
INCLUDE_ASM("asm/nonmatchings/game/Hud", initAfter__12Letterboxing);
INCLUDE_ASM("asm/nonmatchings/game/Hud", setEnabled__12Letterboxingb);
INCLUDE_ASM("asm/nonmatchings/game/Hud", setLetterboxCS__12LetterboxingP9_hierhead);
INCLUDE_ASM("asm/nonmatchings/game/Hud", update__12Letterboxing);
INCLUDE_ASM("asm/nonmatchings/game/Hud", __static_initialization_and_destruction_0_00147118);
INCLUDE_ASM("asm/nonmatchings/game/Hud", _GLOBAL_$I$gLowStaminaThreshold);
