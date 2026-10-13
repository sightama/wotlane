#include "common.h"
#include "game/game.h"
#include "game/shell.h"
#include "game/power_ups.h"
#include "game/crush_level.h"
#include "game/levels.h"
#include "game/level_pickups.h"
#include "game/streaming_sound.h"
#include "task_manager.h"
#include "memory_stack.h"
#include "game/power_up_tool.h"
#include "game/start_point_tool.h"
#include "game/token_manager.h"

class Debris {
public:
    static void InitAfter(void);
};
class SpecFxAnim {
public:
    static void initAfterDbLoad(void);
};
class HomingBug {
public:
    static void initAfterDbLoad(void);
};
class Ai {
public:
    static void globalInit(void);
};
class ActionDispatch {
public:
    static void initGenericEvent(void (*handler)(unsigned));
};
class LevelObjectSoundManager {
public:
    void initLevelObjectSoundManager(void);
};

class Destructibles {
public:
    void Update(void);
};
extern Destructibles *destructibles;
extern "C" int printf(const char *, ...);
class DbInteractive;
class Interactives {
public:
    static int addInteractive(DbInteractive *p);
};
_cs *dbGetCSForModel(_hierhead *h);
void hierSetCsDrawMe(_cs *cs, unsigned char v);

void checkTriggerTree(void);


void TheGame::Init(void)
{
}
INCLUDE_ASM("asm/nonmatchings/game/TheGame", InitBeforeDbLoad__7TheGame);
INCLUDE_ASM("asm/nonmatchings/game/TheGame", traversalCallback__7TheGameP3_csUiUiRA3_A3_fP8_fvector);
INCLUDE_ASM("asm/nonmatchings/game/TheGame", genericEventHandler__7TheGameUi);
#ifdef NON_MATCHING
/* 128/216 words, untuned: written from the m2c draft */
void TheGame::InitAfterDbLoad(void)
{
    int i;

    f120468 = 1;
    if (m_gameMode != 8) {
        PowerUps::instance.initPowerUpsAfter();
        PowerUpTool::instance.loadPoints(0, true);
        StartPointTool::instance.loadPoints();
    }
    particleInitAfter();
    hdInit();
    getWeapons()->InitWeaponsAfter();
    Debris::InitAfter();
    LevelPickups::initAfterDbLoad();
    SpecFxAnim::initAfterDbLoad();
    HomingBug::initAfterDbLoad();
    Ai::globalInit();
    m_minUpdateRate = timerGetMinUpdateRate();
    f120460 = 1;
    f120454 = 0;
    f120458 = 0;
    f120450 = 0;
    f12045C = 0;
    for (i = m_numSlots - 1; i >= 0; i--) {
        m_slots[i].initAfterDbLoad();
        m_slots[i].m_winsThisGame = 0;
    }
    MemoryStack::global.pushMark();
    hierSetTraversalCallback(traversalCallback);
    ActionDispatch::initGenericEvent(genericEventHandler);
    ((LevelObjectSoundManager *)((char *)this + 0x121570))->initLevelObjectSoundManager();
    gUseUnifiedView = 0;
    Cameras::SetCameraPOV(2, Camera::POV_3);
    switch (m_levelId) {
    case 1:
        if (shell->m_mode == 1)
            centralInitAfter();
        break;
    case 2:
        if (shell->m_mode == 1)
            vegasInitAfter();
        break;
    case 3:
        if (shell->m_mode == 1)
            canyon2InitAfter();
        break;
    case 5:
        airportInitAfter();
        break;
    case 8:
    case 15:
        islandInitAfter();
        for (i = 0; i < m_numMonsters; i++) {
            m_tokens[i].setMax("Destructibles", 0);
            m_tokens[i].setMilestone("Destructibles", 1, 1, TokenManager::MILESTONE_1);
        }
        break;
    case 6:
        threeMileInitAfter();
        break;
    case 9:
        tokyoInitAfter();
        break;
    case 7:
        sanFranInitAfter();
        break;
    case 10:
        if (shell->m_mode == 1)
            ufoInitAfter();
        break;
    case 11:
        if (shell->m_mode == 1)
            finalBoss.initAfter();
        break;
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/game/TheGame", InitAfterDbLoad__7TheGame);
#endif
#ifdef NON_MATCHING
/* 23/252 words, untuned: written from the m2c draft */
void TheGame::Update(void)
{
    int i;
    Monster *m;

    m = m_slots;
    for (i = m_numSlots; i != 0; i--, m++) {
        if (m->m_playerNum != 0) {
            int active = ((m_playerMask & 1) && m->m_playerNum == 1) || ((m_playerMask & 2) && m->m_playerNum == 2);

            if (active)
                m->update();
            else
                m->updateCinema();
        }
    }
    m = m_slots;
    for (i = m_numSlots; i != 0; i--, m++) {
        if (m->m_playerNum != 0) {
            int active = ((m_playerMask & 1) && m->m_playerNum == 1) || ((m_playerMask & 2) && m->m_playerNum == 2);

            if (active)
                m->updatePosition();
        }
    }
    gameResolveLifeAndDeath();
    checkTriggerTree();
    if (m_gameMode != 7 && m_gameMode != 9)
        gameCheckForCloseCombat();
    destructibles->Update();
    Cameras::Update();
    PowerUps::instance.update();
    gTaskManager0.update();
    switch (m_levelId) {
    case 1:
        if (shell->m_mode == 1)
            centralUpdate();
        break;
    case 2:
        if (shell->m_mode == 1)
            vegasUpdate();
        break;
    case 3:
        if (shell->m_mode == 1)
            canyon2Update();
        break;
    case 5:
        airportUpdate();
        break;
    case 8:
    case 15:
        islandUpdate();
        break;
    case 6:
        threeMileUpdate();
        break;
    case 9:
        tokyoUpdate();
        break;
    case 7:
        sanFranUpdate();
        break;
    case 10:
        if (shell->m_mode == 1)
            ufoUpdate();
        break;
    case 11:
        if (shell->m_mode == 1)
            finalBoss.update();
        break;
    case 26:
        if (shell->m_mode == 8)
            (BigShotLevel::instance.*BigShotLevel::UPDATE_FUNK[BigShotLevel::instance.m_state])();
        break;
    case 27:
        if (shell->m_mode == 9)
            (CrushLevel::instance.*CrushLevel::UPDATE_FUNK[CrushLevel::instance.m_suddenDeath])();
        break;
    }
    ((StreamingSoundManager *)((char *)this + 0x1204C0))->updateStreamingSoundManager(Cameras::m_cameras.m_state == 7);
}
#else
INCLUDE_ASM("asm/nonmatchings/game/TheGame", Update__7TheGame);
#endif
void TheGame::Update2(void)
{
    gameResolveCollisions();
    TaskManager::global.update();
    getWeapons()->UpdateWeapons();
    if (m_playerMask & 4)
        LevelPickups::update();
}
/* The first monster model parsed for a (type, dup) pair keeps the highest `dup` seen so far: models of the same monster type
   (the object id with its low 5 bits cleared) are numbered 1.. in the order the level lists them. */
#ifdef NON_MATCHING
/* 1/65 words: untuned, from the m2c draft */
void TheGame::MonsterParse(_hierhead *h, _fvector *pos)
{
    int found = -1;
    int dup;
    unsigned id = *(unsigned *)h >> 18;
    int rem = id & 0x1F;
    int type = id - rem;

    for (dup = 0; dup < 0x32; dup++) {
        if (GetMonsterFromName(type, dup) != 0)
            found = dup;
    }
    if (rem == 0) {
        _cs *cs = dbGetCSForModel(h);

        m_curDupId = found + 1;
        AddMonster(cs, type, found + 1);
        return;
    }
    if ((int)id < 0x400) {
        Monster *m = GetMonsterFromName(type, found);

        if (m != 0)
            m->addAttachment(h);
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/game/TheGame", MonsterParse__7TheGameP9_hierheadP8_fvector);
#endif

#ifdef NON_MATCHING
/* 38/73 words: untuned */
void TheGame::AddMonster(_cs *cs, int type, int dup)
{
    Monster *m = &m_slots[m_numSlots];
    unsigned *head;

    m->m_dupId = dup;
    m_slotInteractive[m_numSlots] = Interactives::addInteractive((DbInteractive *)&m_slots[m_numSlots]);
    m->m_typeBits = type;
    m->m_id = m_slotInteractive[m_numSlots];
    m->m_cs = cs;
    m->m_playerNum = 0;
    cs->drawMe = 0;
    head = (unsigned *)cs->epNode;
    *head = (*head & 0x3FFFF) | (type << 18);
    head = (unsigned *)cs->epNode;
    *head = (*head & 0xFFFC007F) | ((m_slotInteractive[m_numSlots] & 0x7FF) << 7);
    m_numSlots++;
    printf("Got monster %d with cs %p, interactiveIndex %d\n", type, cs, m->m_id);
}
#else
INCLUDE_ASM("asm/nonmatchings/game/TheGame", AddMonster__7TheGameP3_csii);
#endif

#ifdef NON_MATCHING
/* 0/33 words: retail peels the first slot */
Monster *TheGame::GetMonsterFromName(int type, int dup)
{
    int i;

    for (i = 0; i < m_numSlots; i++) {
        if (m_slots[i].m_typeBits == type && m_slots[i].m_dupId == dup)
            return &m_slots[i];
    }
    return 0;
}
#else
INCLUDE_ASM("asm/nonmatchings/game/TheGame", GetMonsterFromName__7TheGameii);
#endif

#ifdef NON_MATCHING
/* 4/84 words: untuned */
void TheGame::SetPlayerMonster(int pIdx, int type, int dup, int view, int skin)
{
    int i;

    if (m_numSlots == 0)
        printf(">>>>>>>>>>  No Monsters In Database!!!  <<<<<<<<<<\n");
    for (i = 0; i < m_numSlots; i++) {
        Monster *m = &m_slots[i];

        if (m->m_typeBits == type && m->m_dupId == dup) {
            m->m_index = pIdx;
            m->m_monsterNum = i;
            m->m_playerInfo = (PlayerDat *)((char *)this + 0x112480 + pIdx * 4);
            m_monsters[pIdx] = m;
            m->playerInit();
            m->m_skinNum = skin;
            if (i < 2)
                *(int *)((char *)m + 0x672C) = *(int *)((char *)shell + 0x2B8C + i * 4) - 1;
            if (view >= 0)
                gameInitCamera(view, i);
        }
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/game/TheGame", SetPlayerMonster__7TheGameiiiii);
#endif

#ifdef NON_MATCHING
/* A monster that is not driven by a player or an AI (the boss halves, the inactive crowd) is switched off: no owner, nothing drawn. */
static void disableMonster(Monster *m)
{
    m->m_playerNum = 0;
    m->m_cs->drawMe = 0;
    m->m_shadow->drawMe = 0;
    (*(_cs **)((char *)m + 0x71F8))->drawMe = 0;
    (*(_cs **)((char *)m + 0x71FC))->drawMe = 0;
}

/* 3/205 words: retail inlines the five disable blocks */
void TheGame::SetAIMonster(int aiIdx, int type, int dup, int skin)
{
    int i;

    m_numAIs = 0;
    for (i = 0; i < m_numSlots; i++) {
        Monster *m = &m_slots[i];

        if (m->m_typeBits == type && m->m_dupId == dup) {
            m->m_monsterNum = i;
            m->m_index = aiIdx;
            m->m_playerInfo = 0;
            m_monsters[4 + aiIdx] = m;
            m->m_skinNum = skin;
            m->aiInit();
            if (m->m_shadow != 0)
                hierSetCsDrawMe(m->m_shadow, 1);
            if (m_numMonsters < 2)
                gameInitCamera(1, i);
            else
                gameInitCamera(3, i);
        }
        if (m_slots[i].m_playerNum == 2 || m_slots[i].m_typeBits == 0x1C0 || m_slots[i].m_typeBits == 0x1E0)
            m_numAIs++;
        if (m_slots[i].m_typeBits == 0x1A0)
            disableMonster(&m_slots[i]);
        if (m_levelId == 2 && m_gameMode == 1 && m_slots[i].m_typeBits == 0x40 && m_slots[i].m_playerNum == 2)
            disableMonster(&m_slots[i]);
        if (m_slots[i].m_typeBits == 0x1C0) {
            disableMonster(&m_slots[i]);
            *(Monster **)((char *)&finalBoss + 0x74) = &m_slots[i];
        }
        if (m_slots[i].m_typeBits == 0x1E0) {
            disableMonster(&m_slots[i]);
            *(Monster **)((char *)&finalBoss + 0x78) = &m_slots[i];
        }
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/game/TheGame", SetAIMonster__7TheGameiiii);
#endif
void TheGame::gameInitCamera(int view, int slot)
{
    m_viewSlot[view] = slot;
    Cameras::SetCameraToFollowMonster(view, &m_slots[slot]);
}
float TheGame::GetCameraMaxHeight(_fvector *pos)
{
    return 10000.0f;
}
#ifdef NON_MATCHING
void hdIgnore(_cs *cs);
void hdClearIgnore(void);

/* Collision pass of a frame: every active monster resolves its cs-to-cs collisions, then all monsters' cs are put on the hit-detection ignore list
   (so a monster never hits itself or the others' bodies) while every active monster tests its attack against the world; the list is cleared at the end. */
void TheGame::gameResolveCollisions(void)
{
    int i;
    Monster *m;

    m = m_slots;
    for (i = 0; i < m_numSlots; i++, m++) {
        if (((m_playerMask & 1) && m->m_playerNum == 1) || ((m_playerMask & 2) && m->m_playerNum == 2))
            m->collisResolveCsToCsCollisions();
    }
    m = m_slots;
    for (i = 0; i < m_numSlots; i++, m++)
        hdIgnore(m->m_cs);
    m = m_slots;
    for (i = m_numSlots; i != 0; i--, m++) {
        if (((m_playerMask & 1) && m->m_playerNum == 1) || ((m_playerMask & 2) && m->m_playerNum == 2))
            m->collisTestForCollisions();
    }
    hdClearIgnore();
}
#else
INCLUDE_ASM("asm/nonmatchings/game/TheGame", gameResolveCollisions__7TheGame);
#endif
#ifdef NON_MATCHING
void rtReturnToShell(int code, int delay);
extern int aiLeft __asm__("D_006F8BF8");
extern char BigShotLevel_instance[] __asm__("_12BigShotLevel$instance");
class StateVictory {
public:
    int transitionOK(void);
};
class SoundManager {
public:
    void swapMonsterSoundBankBlocking(int from, int to);
};

#define SHN(o) (*(int *)((char *)shell + (o)))

/* Checks every frame whether a round ended and asks the shell to leave the loop: rtReturnToShell(code, delay): code 1 = a player died (the mode's
   Evaluate* decides what follows), 0 = a win; 0x1E frames of delay while the screen fades. m_unkF6 marks a monster that is out for good, m_unkF7 one that
   has won (victory state). Modes: 1 story, 2/3/11 free for all, 4 endurance, 6 elimination, 7 dodgeball, 8 bigshot, 9 crush. */
void TheGame::gameResolveLifeAndDeath(void)
{
    int i, j;

    switch (m_gameMode) {
    case 1:
        aiLeft = 0;
        if (m_numAIs == 0)
            aiLeft = 1;
        for (i = 0; i < m_numAIs; i++)
            if (m_monsters[4 + i]->m_unkF6 == 0)
                aiLeft = 1;
        for (i = 0; i < m_numMonsters; i++) {
            if (m_monsters[i]->m_unkF6 != 0) {
                if (SHN(0x2BBC) == 1) {
                    rtReturnToShell(1, 0x1E);
                    fadeOut(4);
                } else {
                    rtReturnToShell(1, 0x1E);
                    fadeOutAndIn(4);
                }
            }
        }
        if (m_levelId != 6 && m_levelId != 3 && m_levelId != 9 && m_levelId != 10 && m_levelId != 11) {
            if (aiLeft == 0) {
                Monster *p = m_monsters[0];

                if (p->m_unkF7 != 0) {
                    rtReturnToShell(0, 0x1E);
                    fadeOut(4);
                } else if (p->m_dead == 0 && *p->m_state != 0x40 && ((StateVictory *)p->m_victoryState)->transitionOK()) {
                    m_monsters[0]->enterNewState((MonsterState *)m_monsters[0]->m_victoryState);
                }
            }
        }
        break;
    case 4: {
        Monster **ais = &m_monsters[4];
        Monster *m = ais[SHN(0x2A70)];

        if (m->m_unkF6 != 0) {
            int oldType = m->m_typeBits;

            printf("We just noticed that an AI died in Endurance mode!!, We'll clean him up and provide a new one.
");
            ais[SHN(0x2A70)]->m_playerNum = 0;
            hierSetCsDrawMe(ais[SHN(0x2A70)]->m_cs, 0);
            SHN(0x2A70)++;
            if (SHN(0x2A70) >= m_numAIs)
                SHN(0x2A70) = 0;
            hierSetCsDrawMe(ais[SHN(0x2A70)]->m_cs, 1);
            ais[SHN(0x2A70)]->aiInit();
            ((SoundManager *)((char *)shell + 0x2C30))->swapMonsterSoundBankBlocking(oldType, ais[SHN(0x2A70)]->m_typeBits);
        }
        for (i = 0; i < m_numMonsters; i++) {
            if (m_monsters[i]->m_unkF6 != 0) {
                rtReturnToShell(1, 0x1E);
                fadeOut(4);
            }
        }
        break;
    }
    case 6:
        for (i = 0; i < m_numMonsters; i++) {
            Monster *m = m_monsters[i];

            if (m->m_unkF6 != 0) {
                int lives = i == 0 ? SHN(0x2B40) : SHN(0x2B44);

                if (lives == 0) {
                    Monster *k = m->m_killer;

                    if (k != 0) {
                        if (k->m_unkF7 != 0) {
                            rtReturnToShell(1, 0x1E);
                            fadeOut(4);
                        }
                    } else {
                        rtReturnToShell(1, 0x1E);
                    }
                } else {
                    rtReturnToShell(1, 0x1E);
                }
            }
        }
        break;
    case 2:
    case 3:
    case 11:
        for (i = 0; i < m_numSlots; i++) {
            Monster *s = &m_slots[i];

            if (s->m_unkF6 != 0) {
                Monster *k = s->m_killer;

                if (k != 0) {
                    int target = SHN(0x2A44);

                    if (target <= 0 || m_slots[k->m_monsterNum].m_winsThisGame < target) {
                        rtReturnToShell(1, 0x1E);
                        if (s->m_playerNum == 1 && m_gameMode == 2)
                            fadeOutAndIn(4);
                    } else if (k->m_unkF7 != 0) {
                        rtReturnToShell(1, 0x1E);
                        fadeOut(4);
                    }
                } else {
                    rtReturnToShell(1, 0x1E);
                    if (s->m_playerNum == 1 && m_gameMode == 2)
                        fadeOutAndIn(4);
                }
            }
        }
        break;
    case 8:
        if (*(int *)(BigShotLevel_instance + 0xE8) == 1) {
            for (i = 0; i < m_numMonsters; i++) {
                if (m_monsters[i]->m_unkF6 != 0) {
                    for (j = 0; j < m_numMonsters; j++) {
                        if (j != i && m_monsters[j]->m_unkF7 != 0) {
                            m_won[1] = i;
                            m_won[0] = j;
                            rtReturnToShell(0, 0);
                        }
                    }
                }
            }
        }
        break;
    case 9:
        for (i = 0; i < m_numMonsters; i++) {
            if (m_monsters[i]->m_unkF7 != 0) {
                m_won[1] = i == 0;
                m_won[0] = i;
                rtReturnToShell(0, 0);
            }
        }
        break;
    case 7:
        for (i = 0; i < m_numMonsters; i++) {
            Monster *m = m_monsters[i];

            if (m->m_unkF6 != 0) {
                Monster *k = m->m_killer;

                if (k == 0 || k->m_dead != 0) {
                    rtReturnToShell(1, 0x1E);
                    fadeOutAndIn(4);
                } else {
                    for (j = 0; j < m_numMonsters; j++) {
                        if (j != i && m_monsters[j]->m_unkF7 != 0) {
                            m_won[1] = i;
                            m_won[0] = j;
                            rtReturnToShell(1, 0x1E);
                            fadeOut(4);
                        }
                    }
                }
            }
        }
        break;
    case 0x3F:
    case 0x40:
        printf("Something is really screwed up if we think we're in the shell here.
");
        break;
    default:
        printf("ERROR - TheGame::gameResolveLifeAndDeath doesn't recognize %i as a valid game mode
", m_gameMode);
        break;
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/game/TheGame", gameResolveLifeAndDeath__7TheGame);
#endif
INCLUDE_ASM("asm/nonmatchings/game/TheGame", gameCheckForCloseCombat__7TheGame);
void TheGame::SetGravity(float g)
{
    m_gravity = g;
}
void TheGame::SetOkToUnify(void)
{
    int i;

    for (i = 0; i < m_numSlots; i++)
        m_slots[i].m_camUnify = 1;
}
#ifdef NON_MATCHING
#include "game/start_points.h"
int hdCsCollect2D(_fvector *pos, float radius, unsigned a, unsigned b, _cs **out, int c);
float hdHatTest(_fvector *pos, _fvector *normal, float a, bool b, float c);
extern int craterSwitched;

/* Places a freshly (re)started monster on one of the level's start points. Which list is used (StartPoints types 0..3) depends on the game mode, whether the
   monster is a player (m_playerNum 1) and whether it is its first life (m_numInits 0): type 3 is the first-spawn list, tried for a point with
   nothing within 120 units (up to the list size), falling back to type 2 when that list is empty. The point's heading is in degrees; points flagged
   at +0x14 are dropped to the ground with hdHatTest. On the three-mile level after the crater switched the monster goes to a fixed spot. */
void TheGame::gameGetStartPoint(Monster *m)
{
    _fvector pos;
    _fvector normal;
    _cs *hits[4];
    int list, tries;
    StartPoints::Point *pt;

    if (m_levelId == 6 && craterSwitched != 0) {
        pos.x = 535.0f;
        pos.y = 568.0f;
        pos.z = 75.0f;
        pos.w = 0.0f;
        m->setTrans(pos);
        m->setRot(-2.3736477f, 0.0f, 0.0f);
        hdReparentCsGrid(m->m_cs);
        return;
    }
    switch (m_gameMode) {
    case 1:
        if (m->m_playerNum != 1)
            list = m->m_numInits != 0 ? 3 : 2;
        else
            list = m->m_numInits == 0 ? 0 : 3;
        break;
    case 2:
    case 3:
    case 4:
    case 5:
    case 6:
        list = 3;
        if (m->m_numInits <= 0) {
            if (m->m_playerNum == 1) {
                list = 1;
            } else {
                list = 2;
                if (shell->m_numPlayers == 1)
                    list = m == m_monsters[4] ? 1 : 2;
            }
        }
        break;
    case 7:
    case 10:
        list = 1;
        break;
    default:
        list = 2;
        break;
    }
    pt = 0;
    if (list == 3 && StartPoints::m_instance.getNumPoints(3) == 0)
        list = 2;
    for (tries = 0; tries < StartPoints::m_instance.getNumPoints(list); tries++) {
        int idx;

        if (list == 3 && tries == 0)
            idx = mathfRand(0, StartPoints::m_instance.getNumPoints(3) - 1);
        else
            idx = StartPoints::m_instance.getNextPoint(list);
        pt = StartPoints::m_instance.getPoint(list, idx);
        if (hdCsCollect2D(&pt->pos, 120.0f, 0x20, 0x400, hits, 1) == 0)
            break;
    }
    if (pt != 0) {
        pos = pt->pos;
        if (pt->f14 != 0)
            pos.z -= hdHatTest(&pos, &normal, 5.0f, false, 4096.0f);
        m->setTrans(pos);
        m->setRot(pt->f10 * 0.017453292f, 0.0f, 0.0f);
    } else {
        pos.x = 0.0f;
        pos.y = 0.0f;
        pos.z = 500.0f;
        pos.w = 0.0f;
        m->setTrans(pos);
        m->setRot(0.0f, 0.0f, 0.0f);
    }
    hdReparentCsGrid(m->m_cs);
}
#else
INCLUDE_ASM("asm/nonmatchings/game/TheGame", gameGetStartPoint__7TheGameP7Monster);
#endif
#ifdef NON_MATCHING
/* The living slot whose cs is nearest to `pos` and closer than maxDist; distSq receives its squared distance (maxDist squared when none). */
Monster *TheGame::getClosestMonster(_fvector &pos, float maxDist, float &distSq)
{
    Monster *best = 0;
    float limit = maxDist * maxDist;
    int i;
    Monster *m = m_slots;

    for (i = m_numSlots; i != 0; i--, m++) {
        if (m->m_playerNum != 0) {
            float dx = m->m_cs->trans.x - pos.x;
            float dy = m->m_cs->trans.y - pos.y;
            float dz = m->m_cs->trans.z - pos.z;
            float d = dx * dx + dy * dy + dz * dz;

            if (d < limit) {
                limit = d;
                best = m;
            }
        }
    }
    distSq = limit;
    return best;
}
#else
INCLUDE_ASM("asm/nonmatchings/game/TheGame", getClosestMonster__7TheGameR8_fvectorfRf);
#endif
#ifdef NON_MATCHING
/* Same for the player monsters (m_monsters), skipping cloaked ones. */
Monster *TheGame::getClosestPlayer(_fvector &pos, float maxDist, float &distSq)
{
    Monster *best = 0;
    float limit = maxDist * maxDist;
    int i;

    for (i = 0; i < m_numMonsters; i++) {
        Monster *m = m_monsters[i];

        if (m->m_playerNum != 0 && m->m_cloaked == 0) {
            float dx = m->m_cs->trans.x - pos.x;
            float dy = m->m_cs->trans.y - pos.y;
            float dz = m->m_cs->trans.z - pos.z;
            float d = dx * dx + dy * dy + dz * dz;

            if (d < limit) {
                limit = d;
                best = m;
            }
        }
    }
    distSq = limit;
    return best;
}
#else
INCLUDE_ASM("asm/nonmatchings/game/TheGame", getClosestPlayer__7TheGameR8_fvectorfRf);
#endif
int TheGame::GetNumAIsAlive(void)
{
    int i;

    m_numAIsAlive = 0;
    for (i = 0; i < m_numAIs; i++) {
        if (m_monsters[4 + i]->m_dead == 0)
            m_numAIsAlive++;
    }
    return m_numAIsAlive;
}
#ifdef NON_MATCHING
/* 55/91 words, untuned: loop shape */
void TheGame::ResetLevel(void)
{
    int i;
    int unified;

    for (i = 0; i < 4; i++)
        m_huds[i].initForReplay();
    for (i = 0; i < m_numMonsters; i++) {
        PadFlags &pf = m_monsters[i]->m_padFlags;

        pf.saveAndClear(1);
        pf[0]->f3E = 0;
        pf.clearModifiers();
        if (gUseUnifiedView == 0)
            pf.f16E8 = 0;
    }
    for (i = 0; i < m_numSlots; i++) {
        Monster *m = &m_slots[i];

        if (m->m_playerNum != 0) {
            m->m_unkF6 = 0;
            m->m_unkF7 = 0;
            m->m_unk49 = 1;
        }
    }
    unified = gUseUnifiedView;
    if (m_gameMode == 8)
        BigShotLevel::instance.initForReplay();
    if (unified == 0)
        Cameras::SetCameraPOV(2, Camera::POV_3);
}
#else
INCLUDE_ASM("asm/nonmatchings/game/TheGame", ResetLevel__7TheGame);
#endif
void TheGame::UnpauseLevel(void)
{
    int i;

    for (i = 0; i < m_numMonsters; i++) {
        PadFlags &pf = m_monsters[i]->m_padFlags;

        pf.saveAndClear(1);
        pf[0]->f3E = 0;
        pf.clearModifiers();
        if (gUseUnifiedView == 0 && m_gameMode != 9)
            pf.f16E8 = 0;
    }
}
#ifdef NON_MATCHING
/* 13/62 words, untuned: loop shape */
void TheGame::UpdatePadTweaks(void)
{
    int i;

    for (i = 0; i < m_numMonsters; i++) {
        PadFlags &pf = m_monsters[i]->m_padFlags;

        pf.tweak16C8 = m_padTweaks.t16C8;
        pf.tweak16C4 = m_padTweaks.t16C4;
        pf.tweak16DC = m_padTweaks.t16DC;
        pf.tweak16D0 = m_padTweaks.t16D0;
        pf.tweak16D4 = m_padTweaks.t16D4;
        pf.tweak16F4 = m_padTweaks.t16F4;
        pf.tweak16F8 = m_padTweaks.t16F8;
        pf.tweak16FC = m_padTweaks.t16FC;
        pf.tweak17AC = m_padTweaks.t17AC;
        pf.tweak17B0 = m_padTweaks.t17B0;
        pf.tweak16E0 = m_padTweaks.t16E0;
    }
    for (i = 0; i < 8; i++)
        inputUseActuator(i, m_actuator[i]);
}
#else
INCLUDE_ASM("asm/nonmatchings/game/TheGame", UpdatePadTweaks__7TheGame);
#endif
INCLUDE_ASM("asm/nonmatchings/game/TheGame", gameReestablishViews__7TheGame);
INCLUDE_ASM("asm/nonmatchings/game/TheGame", monsterPush__7TheGamePP9_hierheadUi);
INCLUDE_ASM("asm/nonmatchings/game/TheGame", monsterPop__7TheGameP13_monsterstackPP9_hierhead);
INCLUDE_ASM("asm/nonmatchings/game/TheGame", fadeOutAndIn__7TheGamei);
INCLUDE_ASM("asm/nonmatchings/game/TheGame", updateFadeOutAndIn__7TheGame);
INCLUDE_ASM("asm/nonmatchings/game/TheGame", fadeOut__7TheGamei);
INCLUDE_ASM("asm/nonmatchings/game/TheGame", updateFadeOut__7TheGame);
/* turns pad's rumble on or off and reapplies the flags of all eight pads */
void TheGame::SetVibration(int pad, bool on)
{
    int i;

    m_actuator[pad] = on;
    for (i = 0; i < 8; i++)
        inputUseActuator(i, m_actuator[i]);
}
INCLUDE_ASM("asm/nonmatchings/game/TheGame", AddCrushMessage__7TheGamei);
INCLUDE_ASM("asm/nonmatchings/game/TheGame", __static_initialization_and_destruction_0_00139F00);
INCLUDE_ASM("asm/nonmatchings/game/TheGame", func_00139F78);
INCLUDE_ASM("asm/nonmatchings/game/TheGame", func_00139F98);
void TheGame::updateFadeOutAndIn(void *p)
{
    ((TheGame *)p)->updateFadeOutAndIn();
}
void TheGame::updateFadeOut(void *p)
{
    ((TheGame *)p)->updateFadeOut();
}
INCLUDE_ASM("asm/nonmatchings/game/TheGame", _GLOBAL_$I$resetObj);
