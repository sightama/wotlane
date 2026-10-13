#include "common.h"
#include "engine.h"
#include "game/shell.h"
#include "game/monster_select.h"
extern int numModsLeft;
extern int unlocked_3092 __asm__("unlocked.3092");
extern int numSelectablesOne __asm__("numSelectablesOne.3096");
extern int numSelectablesTwo __asm__("numSelectablesTwo.3097");
extern int selBeginning __asm__("beginning.3098");
extern int selEnding __asm__("ending.3099");
extern int currentSelection[];
int modelsLeft(int group);
struct _hierswitch;
struct _hierobject;
struct _animHandle;
struct _SaveGameData;
void hierSetSwitch(_hierswitch *sw, int which);
void moviesReadyScreenObject(_hierobject *o, int a, int b);
int monsterIdToMonsterEnum(int id);
class AnimQueue {
public:
    void Push(_animHandle *h, int a, int b, int c);
};
class CMovie {
public:
    void Play(bool loop, char *name);
};
extern AnimQueue *animq;
extern CMovie myMovie;
extern char MainMovie[];
extern int continueDecoding;
extern int currScreen;
extern float targetAlpha;
extern int betweenScreens;
extern int on_bit[] __asm__("on_bit_006EEE78");
extern "C" int printf(const char *, ...);
class ShellRtLoop {
public:
    char pad0[0x24];

    void startField(bool b);
    void endField(bool skipDraw);
    void onEnd(void);
};
class MonsterMc {
public:
    int pad0;
    int m_done; /* 0x4: the memory card operation started by the last update* call finished */

    void updateBootLoad(void);
    void updateUnlockSave(void);
    void updateSettingsSave(void);
    void updateSettingsLoad(void);
    void updateStoryModeLoad(void);
    static MonsterMc global;
};
int inputGetInput(int mask, int pad);
float screenGetFontAlpha(void);
void hierSetCsDrawMe(_cs *cs, unsigned char on);
void uiAlignItemWithCamera(_cs *cs, float z, float x, float y);
extern float val1_x __asm__("val1_x.3061");
extern float val_y __asm__("val_y.3062");
extern float val_z __asm__("val_z.3063");
extern float val2_x __asm__("val2_x.3064");
__asm__("#SNFIX_SMALL val1_x.3061");
__asm__("#SNFIX_SMALL val_y.3062");
__asm__("#SNFIX_SMALL val_z.3063");
__asm__("#SNFIX_SMALL val2_x.3064");
void screenSetButtonAlpha(void);
void changeScreen(int to, int from, int dir, int sfx, bool b);
void updateSelectSwitches(int a, int b, int c, int d);
void updateSelectSwitches3P(int a, int b, int c, int d, int e, int f);
void updateSelectSwitches4P(int a, int b, int c, int d, int e, int f, int g, int h);
void initSelectSwitchesMG(void);
void initSelectSwitches(void);
int screenPrepareDefault(int a, int b);
class ShellSound {
public:
    void playShellSFXSound(int id);
    void terminateShellSound(void);
    void updateSFXSliderSound(void);
    void stopSFXSliderSound(void);
};
extern int prevScreen;
#ifdef NON_MATCHING
extern int lastScreen;
extern int oldScreens[16];
extern int currAnimationIndex;
__asm__("#SNFIX_SMALL currAnimationIndex");
#endif
int levelUnlocked(int level);
int screenGetInput(int padMask);
extern int screenFirstPass;
extern char *charSelectStringOne;
extern char promptString2960[] __asm__("promptString.2960");
extern int numSelectables2857 __asm__("numSelectables.2857");
extern void *g_levelList;
extern char PO_MPFreeForAll[];
extern char PO_MPElimination[];
int screenPrepareDefault(int a, int b) __asm__("screenPrepareDefault__Fii");
extern int numSelectables2948 __asm__("numSelectables.2948");
extern int numSelectables2952 __asm__("numSelectables.2952");
extern int numSelectables2959 __asm__("numSelectables.2959");

int monsterIdToMonsterEnum(int id)
{
    return id / 32;
}
#ifdef NON_MATCHING
/* Starts the main-menu background movie on the shell's screen object. */
void screenPrepare(void)
{
    moviesReadyScreenObject(*(_hierobject **)((char *)shell + 0x160), 8, 7);
    myMovie.Play(false, MainMovie);
    continueDecoding = 1;
    currScreen = 0x10;
}
#else
INCLUDE_ASM("asm/nonmatchings/game/screen", screenPrepare__Fv);
#endif
#ifdef NON_MATCHING
/* Points the `count` reel switches of the shell (from `first`) at the model and monster enum of each saved entry (0x3C bytes apiece, from +0x10). */
void setReelSwitches(int first, int count, _SaveGameData *save)
{
    int i;

    for (i = 0; i < count; i++) {
        char *e = (char *)save + 0x10 + i * 0x3C;

        hierSetSwitch(*(_hierswitch **)((char *)shell + 0x27D4 + first * 0x28 + i * 4), *(int *)e);
        hierSetSwitch(*(_hierswitch **)((char *)shell + 0x2784 + first * 0x28 + i * 4), monsterIdToMonsterEnum(*(int *)(e + 4)));
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/game/screen", setReelSwitches__FiiP13_SaveGameData);
#endif
#ifdef NON_MATCHING
/* Queues the animation of reel `reel`: `from` is stopped, `to` started. Each reel owns 0xA0 bytes of animation handles in the shell. */
void changeReel(int reel, int from, int to)
{
    animq->Push(&shell->m_reelAnim[reel][from], 0, 0, 0);
    animq->Push(&shell->m_reelAnim[reel][to], 1, 1, 1);
}
#else
INCLUDE_ASM("asm/nonmatchings/game/screen", changeReel__Fiii);
#endif
#ifdef NON_MATCHING
extern int numSelectables2784 __asm__("numSelectables.2784");
void fontSetDefaultSize(int font);
void resetSelectables(void);
int inputIsCtlAvailable(int pad);
/* Main menu: 1 = one player (screen 2), 2 = two players (screen 5, needs pad 2), 3 = options (screen 10; the volumes are saved first so
   the screen can restore them), 4 = screen 9, 5 = screen 21. Up/down (codes 1/2) move the highlight with wraparound.
   NON_MATCHING: 25/262 words differ, 0x3EC bytes vs 0x418 (untuned; same call sequence). */
void screenMain(void)
{
    int input, sel;

    if (screenFirstPass != 0) {
        numSelectables2784 = screenPrepareDefault(1, currentSelection[currScreen]);
        screenFirstPass = 0;
        resetSelectables();
        shell->m_numAIs = 0;
        shell->m_numPlayers = 1;
        fontSetDefaultSize(0);
        fontSetDefaultSize(1);
        hierSetSwitch(shell->m_sw5D0, 0);
        moviesReadyScreenObject(shell->m_movieScreen, 8, 7);
        myMovie.Play(false, MainMovie);
        continueDecoding = 1;
    }
    myMovie.Play(false, MainMovie);
    input = screenGetInput(1) & 0xFFFF;
    switch (input) {
    case 1:
        hierSetSwitch(shell->m_menuItemSw[1][currentSelection[currScreen]], 0);
        sel = currentSelection[currScreen];
        currentSelection[currScreen] = sel >= 2 ? sel - 1 : numSelectables2784;
        hierSetSwitch(shell->m_menuItemSw[1][currentSelection[currScreen]], 1);
        ((ShellSound *)((char *)shell + 0x2918))->playShellSFXSound(2);
        break;
    case 2:
        hierSetSwitch(shell->m_menuItemSw[1][currentSelection[currScreen]], 0);
        sel = currentSelection[currScreen];
        currentSelection[currScreen] = sel < numSelectables2784 ? sel + 1 : 1;
        hierSetSwitch(shell->m_menuItemSw[1][currentSelection[currScreen]], 1);
        ((ShellSound *)((char *)shell + 0x2918))->playShellSFXSound(4);
        break;
    case 6:
        switch (currentSelection[currScreen]) {
        case 1:
            shell->m_numPlayers = currentSelection[currScreen];
            changeScreen(1, 2, 1, 0, true);
            break;
        case 2:
            if (inputIsCtlAvailable(1)) {
                shell->m_numPlayers = currentSelection[currScreen];
                changeScreen(1, 5, 1, 0, true);
            }
            break;
        case 3:
            shell->m_sfxVolumeSaved = shell->m_sfxVolume;
            shell->m_musicVolumeSaved = shell->m_musicVolume;
            changeScreen(1, 0xA, 1, 5, true);
            break;
        case 4:
            hierSetSwitch(shell->m_sw25CC, 1);
            hierSetSwitch(shell->m_sw25D0, 1);
            changeScreen(1, 9, 1, 9, true);
            break;
        case 5:
            changeScreen(1, 0x15, 1, 0xA, true);
            break;
        default:
            printf("Main Screen has screwed up item indexing! - We think we selected #%i\n", currentSelection[currScreen]);
            break;
        }
        ((ShellSound *)((char *)shell + 0x2918))->playShellSFXSound(8);
        break;
    case 5:
        break;
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/game/screen", screenMain__Fv);
#endif
#ifdef NON_MATCHING
/* Two-player monster select: same unlocked range as initSelectSwitches, then each column shows who is on it. cursor1/cursor2 are the
   1-based columns of the players; frame 1 = player 1 only, 2 = player 2 only, 3 = both; flag1/flag2 (player done choosing?) turn the
   player's highlight off (frame 0). */
extern int first2788 __asm__("first.2788");
extern int last2789 __asm__("last.2789");
__asm__("#SNFIX_SMALL first.2788");
__asm__("#SNFIX_SMALL last.2789");
#define first first2788
#define last last2789
/* 8/144 palavras; o retail repete o carregamento de shell (gp e lui) em cada ramo do laço */
void updateSelectSwitches(int cursor1, int cursor2, int flag1, int flag2)
{
    int i;

    if (monsterSelectMode[0].state == 0 && monsterSelectMode[36].state == 0) {
        first = 1;
        last = 9;
        hierSetSwitch(shell->m_selectSw[0], 4);
        hierSetSwitch(shell->m_selectSw[9], 4);
    } else if (monsterSelectMode[0].state == 1 && monsterSelectMode[36].state == 0) {
        last = 9;
        first = 0;
        hierSetSwitch(shell->m_selectSw[0], 4);
    } else if (monsterSelectMode[0].state == 0 && monsterSelectMode[36].state == 1) {
        first = 1;
        last = 10;
        hierSetSwitch(shell->m_selectSw[9], 4);
    } else if (monsterSelectMode[0].state == 1 && monsterSelectMode[36].state == 1) {
        first = 0;
        last = 10;
    }
    for (i = first; i < last; i++) {
        int frame;

        if (i == cursor1 - 1) {
            if (i == cursor2 - 1) {
                if (flag1)
                    frame = flag2 ? 0 : 2;
                else
                    frame = flag2 ? 1 : 3;
            } else {
                frame = flag1 == 0;
            }
        } else if (i == cursor2 - 1) {
            frame = flag2 ? 0 : 2;
        } else {
            frame = 0;
        }
        hierSetSwitch(shell->m_selectSw[i], frame);
    }
}
#undef first
#undef last
#else
INCLUDE_ASM("asm/nonmatchings/game/screen", updateSelectSwitches__Fiiii);
#endif
#ifdef NON_MATCHING
/* Three players: the two-player frames while player 1 or 2 is still choosing; once both are done, player 3's column shows 5 until
   flag3 is set. */
extern int first2793 __asm__("first.2793");
extern int last2794 __asm__("last.2794");
__asm__("#SNFIX_SMALL first.2793");
__asm__("#SNFIX_SMALL last.2794");
#define first first2793
#define last last2794
/* 11/161 palavras; mesma causa da versão 2P */
void updateSelectSwitches3P(int cursor1, int cursor2, int cursor3, int flag1, int flag2, int flag3)
{
    int i;

    if (monsterSelectMode[0].state == 0 && monsterSelectMode[36].state == 0) {
        first = 1;
        last = 9;
        hierSetSwitch(shell->m_selectSw[0], 4);
        hierSetSwitch(shell->m_selectSw[9], 4);
    } else if (monsterSelectMode[0].state == 1 && monsterSelectMode[36].state == 0) {
        last = 9;
        first = 0;
        hierSetSwitch(shell->m_selectSw[0], 4);
    } else if (monsterSelectMode[0].state == 0 && monsterSelectMode[36].state == 1) {
        first = 1;
        last = 10;
        hierSetSwitch(shell->m_selectSw[9], 4);
    } else if (monsterSelectMode[0].state == 1 && monsterSelectMode[36].state == 1) {
        first = 0;
        last = 10;
    }
    for (i = first; i < last; i++) {
        int frame;

        if (!flag1 || !flag2) {
            if (i == cursor1 - 1) {
                if (i == cursor2 - 1) {
                    if (flag1)
                        frame = flag2 ? 0 : 2;
                    else
                        frame = flag2 ? 1 : 3;
                } else {
                    frame = flag1 == 0;
                }
            } else if (i == cursor2 - 1) {
                frame = flag2 ? 0 : 2;
            } else {
                frame = 0;
            }
        } else if (flag3) {
            frame = 0;
        } else if (i == cursor3 - 1) {
            frame = 5;
        } else {
            frame = 0;
        }
        hierSetSwitch(shell->m_selectSw[i], frame);
    }
}
#undef first
#undef last
#else
INCLUDE_ASM("asm/nonmatchings/game/screen", updateSelectSwitches3P__Fiiiiii);
#endif
#ifdef NON_MATCHING
/* Four players: the two-player frames while player 1 or 2 is still choosing; then the same for players 3 and 4 with frames 5 (player 3),
   6 (player 4) and 7 (both). In shell mode 2 player 4 only shows once player 3 is done. */
extern int first2798 __asm__("first.2798");
extern int last2799 __asm__("last.2799");
__asm__("#SNFIX_SMALL first.2798");
__asm__("#SNFIX_SMALL last.2799");
#define first first2798
#define last last2799
/* 63/233 palavras; o retail guarda (flag4 ? 0 : 6) na pilha antes do laço e repete o carregamento de shell */
void updateSelectSwitches4P(int cursor1, int cursor2, int cursor3, int cursor4, int flag1, int flag2, int flag3, int flag4)
{
    int i;

    if (monsterSelectMode[0].state == 0 && monsterSelectMode[36].state == 0) {
        first = 1;
        last = 9;
        hierSetSwitch(shell->m_selectSw[0], 4);
        hierSetSwitch(shell->m_selectSw[9], 4);
    } else if (monsterSelectMode[0].state == 1 && monsterSelectMode[36].state == 0) {
        last = 9;
        first = 0;
        hierSetSwitch(shell->m_selectSw[0], 4);
    } else if (monsterSelectMode[0].state == 0 && monsterSelectMode[36].state == 1) {
        first = 1;
        last = 10;
        hierSetSwitch(shell->m_selectSw[9], 4);
    } else if (monsterSelectMode[0].state == 1 && monsterSelectMode[36].state == 1) {
        first = 0;
        last = 10;
    }
    for (i = first; i < last; i++) {
        int frame;

        if (!flag1 || !flag2) {
            if (i == cursor1 - 1) {
                if (i == cursor2 - 1) {
                    if (flag1)
                        frame = flag2 ? 0 : 2;
                    else
                        frame = flag2 ? 1 : 3;
                } else {
                    frame = flag1 == 0;
                }
            } else if (i == cursor2 - 1) {
                frame = flag2 ? 0 : 2;
            } else {
                frame = 0;
            }
        } else if (flag3 && flag4) {
            frame = 0;
        } else if (i == cursor3 - 1) {
            if (i == cursor4 - 1) {
                if (flag3)
                    frame = flag4 ? 0 : 6;
                else
                    frame = flag4 ? 5 : 7;
            } else {
                frame = flag3 ? 0 : 5;
            }
        } else if (shell->m_mode == 2) {
            if (i == cursor4 - 1 && !flag3)
                frame = flag4 ? 0 : 6;
            else
                frame = 0;
        } else if (i == cursor4 - 1) {
            frame = flag4 ? 0 : 6;
        } else {
            frame = 0;
        }
        hierSetSwitch(shell->m_selectSw[i], frame);
    }
}
#undef first
#undef last
#else
INCLUDE_ASM("asm/nonmatchings/game/screen", updateSelectSwitches4P__Fiiiiiiii);
#endif
#ifdef NON_MATCHING
/* updateSelectSwitches for the minigame monster select (m_selectSwMG). */
extern int first2803 __asm__("first.2803");
extern int last2804 __asm__("last.2804");
__asm__("#SNFIX_SMALL first.2803");
__asm__("#SNFIX_SMALL last.2804");
#define first first2803
#define last last2804
/* 8/144 palavras; mesma causa da versão 2P */
void updateSelectSwitchesMG(int cursor1, int cursor2, int flag1, int flag2)
{
    int i;

    if (monsterSelectMode[0].state == 0 && monsterSelectMode[36].state == 0) {
        first = 1;
        last = 9;
        hierSetSwitch(shell->m_selectSwMG[0], 4);
        hierSetSwitch(shell->m_selectSwMG[9], 4);
    } else if (monsterSelectMode[0].state == 1 && monsterSelectMode[36].state == 0) {
        last = 9;
        first = 0;
        hierSetSwitch(shell->m_selectSwMG[0], 4);
    } else if (monsterSelectMode[0].state == 0 && monsterSelectMode[36].state == 1) {
        first = 1;
        last = 10;
        hierSetSwitch(shell->m_selectSwMG[9], 4);
    } else if (monsterSelectMode[0].state == 1 && monsterSelectMode[36].state == 1) {
        first = 0;
        last = 10;
    }
    for (i = first; i < last; i++) {
        int frame;

        if (i == cursor1 - 1) {
            if (i == cursor2 - 1) {
                if (flag1)
                    frame = flag2 ? 0 : 2;
                else
                    frame = flag2 ? 1 : 3;
            } else {
                frame = flag1 == 0;
            }
        } else if (i == cursor2 - 1) {
            frame = flag2 ? 0 : 2;
        } else {
            frame = 0;
        }
        hierSetSwitch(shell->m_selectSwMG[i], frame);
    }
}
#undef first
#undef last
#else
INCLUDE_ASM("asm/nonmatchings/game/screen", updateSelectSwitchesMG__Fiiii);
#endif
#ifdef NON_MATCHING
/* Picking the monster of AI `ai` (1..3): its column shows 5, 6 or 8; any other value leaves that column as it is. */
extern int first2808 __asm__("first.2808");
extern int last2809 __asm__("last.2809");
__asm__("#SNFIX_SMALL first.2808");
__asm__("#SNFIX_SMALL last.2809");
#define first first2808
#define last last2809
/* 126/129 palavras; ordem dos carregamentos de shell */
void updateSelectSwitchesAI(int cursor, int ai)
{
    int i;

    if (monsterSelectMode[0].state == 0 && monsterSelectMode[36].state == 0) {
        first = 1;
        last = 9;
        hierSetSwitch(shell->m_selectSw[0], 4);
        hierSetSwitch(shell->m_selectSw[9], 4);
    } else if (monsterSelectMode[0].state == 1 && monsterSelectMode[36].state == 0) {
        last = 9;
        first = 0;
        hierSetSwitch(shell->m_selectSw[0], 4);
    } else if (monsterSelectMode[0].state == 0 && monsterSelectMode[36].state == 1) {
        first = 1;
        last = 10;
        hierSetSwitch(shell->m_selectSw[9], 4);
    } else if (monsterSelectMode[0].state == 1 && monsterSelectMode[36].state == 1) {
        first = 0;
        last = 10;
    }
    for (i = first; i < last; i++) {
        if (i == cursor - 1) {
            if (ai == 1)
                hierSetSwitch(shell->m_selectSw[i], 5);
            else if (ai == 2)
                hierSetSwitch(shell->m_selectSw[i], 6);
            else if (ai == 3)
                hierSetSwitch(shell->m_selectSw[i], 8);
        } else {
            hierSetSwitch(shell->m_selectSw[i], 0);
        }
    }
}
#undef first
#undef last
#else
INCLUDE_ASM("asm/nonmatchings/game/screen", updateSelectSwitchesAI__Fii);
#endif
#ifdef NON_MATCHING
/* Narrows the monster-select columns to the unlocked monsters: the first (monsterSelectMode[0]) and the last (monsterSelectMode[36],
   the first slot of monster 9) can still be locked, in which case their column shows 4 and the range [m_selFirst, m_selLast) skips it.
   The other columns are reset to 0. */
extern int first2813 __asm__("first.2813");
extern int last2814 __asm__("last.2814");
__asm__("#SNFIX_SMALL first.2813");
__asm__("#SNFIX_SMALL last.2814");
#define first first2813
#define last last2814
/* 84/98 palavras; o retail relê first/last por gp entre os ramos */
void initSelectSwitches(void)
{
    int i;

    if (monsterSelectMode[0].state == 0 && monsterSelectMode[36].state == 0) {
        first = 1;
        last = 9;
        hierSetSwitch(shell->m_selectSw[0], 4);
        hierSetSwitch(shell->m_selectSw[9], 4);
    } else if (monsterSelectMode[0].state == 1 && monsterSelectMode[36].state == 0) {
        last = 9;
        first = 0;
        hierSetSwitch(shell->m_selectSw[0], 4);
    } else if (monsterSelectMode[0].state == 0 && monsterSelectMode[36].state == 1) {
        first = 1;
        last = 10;
        hierSetSwitch(shell->m_selectSw[9], 4);
    } else if (monsterSelectMode[0].state == 1 && monsterSelectMode[36].state == 1) {
        first = 0;
        last = 10;
    }
    shell->m_selLast = last;
    shell->m_selFirst = first;
    for (i = first; i < last; i++)
        hierSetSwitch(shell->m_selectSw[i], 0);
}
#undef first
#undef last
#else
INCLUDE_ASM("asm/nonmatchings/game/screen", initSelectSwitches__Fv);
#endif
#ifdef NON_MATCHING
/* initSelectSwitches for the minigame select: same narrowing of the columns, with its own range and without storing it in the shell. */
extern int first2818 __asm__("first.2818");
extern int last2819 __asm__("last.2819");
__asm__("#SNFIX_SMALL first.2818");
__asm__("#SNFIX_SMALL last.2819");
#define first first2818
#define last last2819
/* 83/88 palavras; o retail relê first por gp no fim de cada ramo */
void initSelectSwitchesMG(void)
{
    int i;

    if (monsterSelectMode[0].state == 0 && monsterSelectMode[36].state == 0) {
        first = 1;
        last = 9;
        hierSetSwitch(shell->m_selectSw[0], 4);
        hierSetSwitch(shell->m_selectSw[9], 4);
    } else if (monsterSelectMode[0].state == 1 && monsterSelectMode[36].state == 0) {
        last = 9;
        first = 0;
        hierSetSwitch(shell->m_selectSw[0], 4);
    } else if (monsterSelectMode[0].state == 0 && monsterSelectMode[36].state == 1) {
        first = 1;
        last = 10;
        hierSetSwitch(shell->m_selectSw[9], 4);
    } else if (monsterSelectMode[0].state == 1 && monsterSelectMode[36].state == 1) {
        first = 0;
        last = 10;
    }
    for (i = first; i < last; i++)
        hierSetSwitch(shell->m_selectSw[i], 0);
}
#undef first
#undef last
#else
INCLUDE_ASM("asm/nonmatchings/game/screen", initSelectSwitchesMG__Fv);
#endif
#ifdef NON_MATCHING
extern int numSelectables2823 __asm__("numSelectables.2823");
extern int UseCommandLineLevel;
extern char PO_1PFreeForAll[];
extern char PO_1PEndurance[];
void resetModels(void);
/* One-player game modes: 1 = mode 1 (level 1 unless one came from the command line, its AI count from m_levelNumAIs; screen 3),
   2 = free for all (mode 2, level list PO_1PFreeForAll; screen 12), 3 = endurance (mode 4, 5 AIs, no kill target, PO_1PEndurance;
   screen 3). First pass puts the select cursor (currentSelection[3]) back in the unlocked range.
   NON_MATCHING: 18/378 words differ, 0x580 bytes vs 0x5E8 (untuned). */
void screenGameModes1P(void)
{
    int input, sel, i;

    if (screenFirstPass != 0) {
        screenPrepareDefault(2, currentSelection[currScreen]);
        numSelectables2823 = 3;
        shell->m_numAIs = 0;
        if ((shell->m_selFirst == 0 && shell->m_selLast == 0) || (shell->m_selFirst == 1 && shell->m_selLast == 1)) {
            initSelectSwitches();
            if (!(shell->m_selFirst < currentSelection[3] && currentSelection[3] < shell->m_selLast))
                currentSelection[3] = shell->m_selFirst + 1;
        } else if ((unsigned int)currentSelection[3] < 2) {
            initSelectSwitches();
            if (!(shell->m_selFirst < currentSelection[3] && currentSelection[3] < shell->m_selLast))
                currentSelection[3] = shell->m_selFirst + 1;
        }
        hierSetSwitch(shell->m_sw5CC, currentSelection[3]);
        for (i = 9; i >= 0; i--)
            *(float *)(shell->m_menuObj[3][i] + 0x4C) = 0.0f;
        hierSetSwitch(shell->m_sw70[0], 1);
        hierSetSwitch(shell->m_sw70[1], 1);
        hierSetSwitch(shell->m_sw70[2], 1);
        moviesReadyScreenObject(shell->m_movieScreen, 8, 7);
        myMovie.Play(false, MainMovie);
        continueDecoding = 1;
        hierSetSwitch(shell->m_sw5D0, 0);
        resetModels();
        updateSelectSwitches(currentSelection[3], 0, 0, 1);
        screenFirstPass = 0;
        currentSelection[6] = shell->m_selFirst + 1;
        currentSelection[18] = shell->m_selFirst + 1;
        currentSelection[19] = shell->m_selFirst + 1;
    }
    myMovie.Play(false, MainMovie);
    input = screenGetInput(1) & 0xFFFF;
    switch (input) {
    case 1:
        hierSetSwitch(shell->m_menuItemSw[2][currentSelection[currScreen]], 0);
        sel = currentSelection[currScreen];
        currentSelection[currScreen] = sel >= 2 ? sel - 1 : numSelectables2823;
        hierSetSwitch(shell->m_menuItemSw[2][currentSelection[currScreen]], 1);
        ((ShellSound *)((char *)shell + 0x2918))->playShellSFXSound(2);
        break;
    case 2:
        hierSetSwitch(shell->m_menuItemSw[2][currentSelection[currScreen]], 0);
        sel = currentSelection[currScreen];
        currentSelection[currScreen] = sel < numSelectables2823 ? sel + 1 : 1;
        hierSetSwitch(shell->m_menuItemSw[2][currentSelection[currScreen]], 1);
        ((ShellSound *)((char *)shell + 0x2918))->playShellSFXSound(4);
        break;
    case 6:
        sel = currentSelection[currScreen];
        if (sel == 1) {
            shell->m_mode = sel;
            if (UseCommandLineLevel == 0)
                shell->m_levelNum = sel;
            shell->m_numAIs = shell->m_levelNumAIs[shell->m_levelNum];
            changeScreen(2, 3, 1, 2, true);
            updateSelectSwitches(currentSelection[currScreen], 0, 0, 1);
        } else if (sel == 2) {
            shell->m_mode = sel;
            g_levelList = PO_1PFreeForAll;
            hierSetSwitch(shell->m_menuItemSw[12][0], currentSelection[12]);
            changeScreen(2, 0xC, 1, 2, true);
        } else if (sel == 3) {
            shell->m_numAIs = 5;
            shell->m_mode = 4;
            shell->m_killTarget = 0;
            g_levelList = PO_1PEndurance;
            changeScreen(2, 3, 1, 2, true);
        } else {
            printf("1 Player Game Modes Screen has screwed up item indexing! - We think we selected #%i\n", currentSelection[currScreen]);
        }
        updateSelectSwitches4P(0, 0, 0, 0, 1, 1, 1, 1);
        ((ShellSound *)((char *)shell + 0x2918))->playShellSFXSound(8);
        break;
    case 5:
        changeScreen(2, 1, -1, 0, true);
        ((ShellSound *)((char *)shell + 0x2918))->playShellSFXSound(6);
        shell->m_numPlayers = 0;
        break;
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/game/screen", screenGameModes1P__Fv);
#endif
#ifndef NON_MATCHING /* the C++ screenCharSelect1P has its own string */
INCLUDE_ASM("asm/nonmatchings/game/screen", D_006EE2F0);
#endif
#ifdef NON_MATCHING
extern int AINum;
extern int AINumLeft;
extern int monsterNametomonsterId[];
extern int numSelectables2827 __asm__("numSelectables.2827");
extern int textBoxAlpha2828 __asm__("m_textBoxAlpha.2828");
extern int monsterIsSelected2829[4] __asm__("monsterIsSelected.2829");
extern int numAvail2830 __asm__("numAvail1.2830");
extern int first2831 __asm__("first.2831");
extern int last2832 __asm__("last.2832");
extern int curCostume2833 __asm__("curCostume1.2833");
extern int stoppedMovie2834 __asm__("stoppedMovie.2834");
extern int showTexCounter2835 __asm__("showTexCounter.2835");
extern int levelMonsters[][12];
extern int levelMonsterModels[][4];
void moviesReleaseScreenObject(void);
void killButtons(void);
int isSlotOpen(int group, int slot);
void resetSelectables(void);
/* Costume cursor wraps around the four costumes. */
#define CS_WRAP(c) ((c) = (c) > -1 ? (c) : 3, (c) = (c) > 3 ? 0 : (c))
#define CS_SLOT(sel, c) monsterSelectMode[((sel) - 1) * 4 + (c)]
/* Up/down of the monster selects: unlights the costume under the cursor `cur`, moves it by `step` to the next open costume (uses the
   caller's `found`). The retail prints the 0-based monster on up and the 1-based one on down (`shown`). */
#define CS_MOVE_COSTUME(cur, avail, step, shown, sfx)                                                    \
    if ((avail) != 1) {                                                                                  \
        SelectSlot *s = &CS_SLOT(currentSelection[currScreen], (cur));                                   \
                                                                                                         \
        found = 0;                                                                                       \
        if (s->state == 1 && s->taken == 1)                                                              \
            hierSetSwitch(shell->m_costumeSw[(cur)], 1);                                                 \
        else                                                                                             \
            hierSetSwitch(shell->m_costumeSw[(cur)], 0);                                                 \
        (cur) += (step);                                                                                 \
        CS_WRAP((cur));                                                                                  \
        while (!found) {                                                                                 \
            if (isSlotOpen(currentSelection[currScreen] - 1, (cur)))                                     \
                found = 1;                                                                               \
            else                                                                                         \
                (cur) += (step);                                                                         \
            CS_WRAP((cur));                                                                              \
        }                                                                                                \
        hierSetSwitch(shell->m_costumeSw[(cur)], 2);                                                     \
        printf("Player 1 is on Monster %i, model %i\n", (shown), (cur));                                 \
        ((ShellSound *)((char *)shell + 0x2918))->playShellSFXSound(sfx);                                \
    }
/* One-player monster select (screen 3): left/right (codes 3/4) pick the monster column (first..last, depending on whether the first
   and last monsters are unlocked), up/down (1/2) the costume among the open ones (isSlotOpen), shown by Shell::m_costumeSw. Confirm
   (6) stores the monster in m_monsterSel[0] and the costume in m_costume[0]: in story mode (m_mode 1) it goes straight to loading
   (screen 99) with the AIs of the level (levelMonsters), otherwise to the level select (8) once no AI is left to pick (m_mode 4 draws
   the AIs at random). Back (5) restarts the menu movie and returns to the modes menu. monsterIsSelected[1..3] marks the AIs that
   need no pick. NON_MATCHING: 34/1089 words differ, 0x1104 bytes vs 0x1090 (untuned; the compiler merges one pair of hierSetSwitch
   calls of the down case into one). */
void screenCharSelect1P(void)
{
    int i, input, sel;
    int found = 0;

    AINumLeft = shell->m_numAIs;
    if (monsterSelectMode[0].state == 0 && monsterSelectMode[36].state == 0) {
        first2831 = 2;
        last2832 = 9;
        numSelectables2827 = 9;
    } else if (monsterSelectMode[0].state == 1 && monsterSelectMode[36].state == 0) {
        first2831 = 1;
        last2832 = 9;
        numSelectables2827 = 9;
    } else if (monsterSelectMode[0].state == 0 && monsterSelectMode[36].state == 1) {
        numSelectables2827 = 9;
        first2831 = 2;
        last2832 = 10;
    } else if (monsterSelectMode[0].state == 1 && monsterSelectMode[36].state == 1) {
        first2831 = 1;
        last2832 = 10;
        numSelectables2827 = 10;
    }
    if (screenFirstPass != 0) {
        AINum = 0;
        monsterIsSelected2829[0] = 0;
        if (currentSelection[currScreen] > last2832 || currentSelection[currScreen] < first2831)
            currentSelection[currScreen] = first2831;
        *(float *)((char *)shell->m_costumeBoxCs + 0x7C) = 0;
        hierSetSwitch(shell->m_sw5D0, 0);
        for (i = 3; i != 0; i--)
            monsterIsSelected2829[i] = shell->m_numAIs < i;
        switch (shell->m_mode) {
        case 1:
            for (i = 0; i < shell->m_levelNumAIs[shell->m_levelNum]; i++) {
                shell->m_monsterSel[4 + i] = levelMonsters[shell->m_levelNum][i];
                shell->m_costume[2 + i] = levelMonsterModels[shell->m_levelNum][i];
                *(int *)((char *)shell + 0x2C18) = 0;
                *(int *)((char *)shell + 0x2B2C) = 0;
            }
            for (i = 3; i != 0; i--)
                monsterIsSelected2829[i] = 1;
            break;
        case 0x3F:
        case 0x40:
            if (prevScreen == 0xC)
                shell->m_mode = 2;
            else if (prevScreen == 2)
                shell->m_mode = 1;
            break;
        }
        hierSetCsDrawMe(shell->m_costumeBoxCs, 1);
        for (i = 0; i < 4; i++) {
            if (CS_SLOT(currentSelection[currScreen], i).state == 1)
                hierSetSwitch(shell->m_costumeSw[i], 1);
            else
                hierSetSwitch(shell->m_costumeSw[i], 0);
        }
        hierSetSwitch(shell->m_costumeSw[curCostume2833], 2);
        screenFirstPass = 0;
        stoppedMovie2834 = 1;
        textBoxAlpha2828 = 0;
    }
    hierSetCsDrawMe(shell->m_costumeBoxCs, 1);
    textBoxAlpha2828 += 2;
    if (textBoxAlpha2828 > 0x80)
        textBoxAlpha2828 = 0x80;
    *(float *)((char *)shell->m_costumeBoxCs + 0x7C) = (float)textBoxAlpha2828 * 2.0f * 0.0078125f;
    resetSelectables();
    if (showTexCounter2835 >= 0x2E && (screenGetInput(1) & 0xFFFF) == 0) {
        continueDecoding = 0;
        moviesReleaseScreenObject();
        showTexCounter2835 = 0;
        stoppedMovie2834 = 1;
    }
    updateSelectSwitches(currentSelection[currScreen], 0, 0, 1);
    for (i = 0; i < 4; i++) {
        SelectSlot *s = &CS_SLOT(currentSelection[currScreen], i);

        if (s->state == 1 && s->taken == 1)
            hierSetSwitch(shell->m_costumeSw[i], 1);
        else
            hierSetSwitch(shell->m_costumeSw[i], 0);
    }
    hierSetSwitch(shell->m_costumeSw[curCostume2833], 2);
    if (monsterIsSelected2829[0] != 0) {
        hierSetSwitch(shell->m_sw5CC, 0);
        continueDecoding = 0;
        if (shell->m_mode == 1)
            changeScreen(3, 0x63, 1, 0, true);
        else if (shell->m_numAIs == 0)
            changeScreen(3, 8, 1, 0, true);
    }
    /* move the costume cursor forward to an open costume */
    while (!found) {
        if (isSlotOpen(currentSelection[currScreen] - 1, curCostume2833))
            found = 1;
        else
            curCostume2833++;
        CS_WRAP(curCostume2833);
    }
    hierSetSwitch(shell->m_costumeSw[curCostume2833], 2);
    {
        int n = 0;

        for (i = 0; i < 4; i++) {
            SelectSlot *s = &CS_SLOT(currentSelection[currScreen], i);

            if (s->state == 1 && s->taken == 1)
                n++;
        }
        numAvail2830 = n;
    }
    /* none open: go to the next monster column */
    while (!found) {
        if (isSlotOpen(currentSelection[currScreen] - 1, curCostume2833)) {
            found = 1;
        } else if (numAvail2830 == 0) {
            if (currentSelection[currScreen] != 10) {
                curCostume2833 = 0;
                currentSelection[currScreen]++;
            } else {
                currentSelection[currScreen] = 0;
            }
        } else {
            curCostume2833++;
        }
        CS_WRAP(curCostume2833);
    }
    hierSetSwitch(shell->m_sw5CC, currentSelection[currScreen]);
    hierSetSwitch(shell->m_costumeSw[curCostume2833], 2);
    moviesReleaseScreenObject();
    continueDecoding = 0;
    input = screenGetInput(1) & 0xFFFF;
    switch (input) {
    case 1:
        CS_MOVE_COSTUME(curCostume2833, numAvail2830, -1, currentSelection[currScreen] - 1, 2);
        break;
    case 2:
        CS_MOVE_COSTUME(curCostume2833, numAvail2830, 1, currentSelection[currScreen], 4);
        break;
    case 3:
    case 4:
        sel = currentSelection[currScreen];
        if (input == 3)
            currentSelection[currScreen] = first2831 < sel ? sel - 1 : last2832;
        else
            currentSelection[currScreen] = sel < last2832 ? sel + 1 : first2831;
        sel = currentSelection[currScreen];
        if (CS_SLOT(sel, 0).taken == 1)
            curCostume2833 = 0;
        hierSetSwitch(shell->m_sw5CC, sel);
        stoppedMovie2834 = 1;
        continueDecoding = 0;
        ((ShellSound *)((char *)shell + 0x2918))->playShellSFXSound(input == 3 ? 5 : 3);
        showTexCounter2835 = 0;
        break;
    case 6:
        continueDecoding = 0;
        shell->m_costume[0] = curCostume2833;
        sel = currentSelection[currScreen];
        if (CS_SLOT(sel, curCostume2833).state != 1)
            break;
        if (sel > last2832) {
            printf("Unselectable character selected\n");
        } else if (monsterIsSelected2829[0] == 0) {
            shell->m_monsterSel[0] = monsterNametomonsterId[sel - 1];
            CS_SLOT(currentSelection[currScreen], curCostume2833).taken = 0;
            monsterIsSelected2829[0] = 1;
        }
        if (shell->m_mode == 1) {
            killButtons();
            hierSetCsDrawMe(shell->m_costumeBoxCs, 0);
            for (i = first2831 - 1; i < last2832; i++)
                hierSetSwitch(shell->m_selectSw[i], 0);
            hierSetCsDrawMe(shell->m_costumeBoxCs, 0);
            changeScreen(3, 0x63, 1, 0, true);
            ((ShellSound *)((char *)shell + 0x2918))->terminateShellSound();
            ((ShellSound *)((char *)shell + 0x2918))->playShellSFXSound(1);
        } else if (AINumLeft == 0 || shell->m_mode == 4) {
            for (i = 3; i >= 0; i--)
                CS_SLOT(currentSelection[currScreen], i).taken = 0;
            if (shell->m_mode == 4)
                shell->RandomlySelectAI();
            hierSetCsDrawMe(shell->m_costumeBoxCs, 1);
            moviesReadyScreenObject(shell->m_movieScreen, 8, 7);
            myMovie.Play(false, MainMovie);
            continueDecoding = 1;
            for (i = first2831 - 1; i < last2832; i++)
                hierSetSwitch(shell->m_selectSw[i], 0);
            hierSetCsDrawMe(shell->m_costumeBoxCs, 0);
            changeScreen(3, 8, 1, 3, true);
            ((ShellSound *)((char *)shell + 0x2918))->playShellSFXSound(8);
        }
        break;
    case 5:
        for (i = first2831 - 1; i < last2832; i++)
            hierSetSwitch(shell->m_selectSw[i], 0);
        moviesReadyScreenObject(shell->m_movieScreen, 8, 7);
        myMovie.Play(false, MainMovie);
        continueDecoding = 1;
        if (shell->m_mode == 2)
            changeScreen(3, 0xC, -1, 0, true);
        else
            changeScreen(3, prevScreen, -1, 2, true);
        CS_SLOT(currentSelection[currScreen], curCostume2833).taken = 1;
        hierSetCsDrawMe(shell->m_costumeBoxCs, 0);
        ((ShellSound *)((char *)shell + 0x2918))->playShellSFXSound(6);
        break;
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/game/screen", screenCharSelect1P__Fv);
#endif
#ifdef NON_MATCHING
extern char D_006F7EB8[];
extern char promptString2840[4][16] __asm__("promptString.2840"); /* "PICK OPPONENT 1".."3", then the done prompt */
extern int firstAINum2846Guard __asm__("_$tmp_0.2846");
extern int firstAINum __asm__("D_006F8C84");
extern int numSelectables2839 __asm__("numSelectables.2839");
extern int numAvail2841 __asm__("numAvail1.2841");
extern int stoppedMovie2842 __asm__("stoppedMovie.2842");
extern int showTexCounter2843 __asm__("showTexCounter.2843");
extern int selectingIsComplete2844 __asm__("selectingIsComplete.2844");
extern int curCostume2847 __asm__("curCostume1.2847");
extern int first2848 __asm__("first.2848");
extern int last2849 __asm__("last.2849");
#define SEL currentSelection[currScreen]
/* One-player opponent select (screen 17), run once per AI (AINum counts the AIs picked, AINumLeft those still to pick): same columns
   and costume cursor as screenCharSelect1P, but left/right skip the monsters with no costume left (modelsLeft; the first two
   opponents may share a monster but not a costume). Confirm (6) gives the AI the monster and costume (m_monsterSel[4 + AINum],
   m_costume[2 + AINum]) and replays screen 17 for the next one, or goes to the level select (8) after the last. Back (5) returns to
   screenCharSelect1P. NON_MATCHING: 20/1333 words differ, 0x14D4 bytes vs 0x13A8 (untuned; same calls, the compiler lays out the
   left/right switches in another order and merges the identical tails of cases 9 and 10 of the first switch). */
void screenSelect1AI(void)
{
    int i, input;
    int found = 0;

    /* function-local static initialised with AINum */
    if (firstAINum2846Guard == 0) {
        firstAINum2846Guard = 1;
        firstAINum = AINum;
    }
    if (monsterSelectMode[0].state == 0 && monsterSelectMode[36].state == 0) {
        first2848 = 2;
        last2849 = 9;
        numSelectables2839 = 9;
    } else if (monsterSelectMode[0].state == 1 && monsterSelectMode[36].state == 0) {
        first2848 = 1;
        last2849 = 9;
        numSelectables2839 = 9;
    } else if (monsterSelectMode[0].state == 0 && monsterSelectMode[36].state == 1) {
        numSelectables2839 = 9;
        first2848 = 2;
        last2849 = 10;
    } else if (monsterSelectMode[0].state == 1 && monsterSelectMode[36].state == 1) {
        first2848 = 1;
        last2849 = 10;
        numSelectables2839 = 10;
    }
    if (shell->m_monsterSel[4] != 0)
        screenFirstPass = 0;
    if (screenFirstPass != 0) {
        screenPrepareDefault(0x11, SEL);
        numSelectables2839 = 0xB;
        firstAINum = AINum;
        charSelectStringOne = promptString2840[AINum];
        if (prevScreen == 3)
            SEL = currentSelection[3];
        else if (prevScreen == 6)
            SEL = currentSelection[6];
        hierSetCsDrawMe(shell->m_costumeBoxCs, 1);
        hierSetSwitch(shell->m_sw5CC, SEL);
        initSelectSwitches();
        screenFirstPass = 0;
        hierSetCsDrawMe(shell->m_costumeBoxCs, 1);
        for (i = 0; i < 4; i++) {
            SelectSlot *s = &CS_SLOT(SEL, i);

            if (s->state == 1 && s->taken == 1)
                hierSetSwitch(shell->m_costumeSw[i], 1);
            else
                hierSetSwitch(shell->m_costumeSw[i], 0);
        }
        hierSetSwitch(shell->m_costumeSw[curCostume2847], 2);
    }
    if (showTexCounter2843 >= 0x2E && (screenGetInput(1) & 0xFFFF) == 0) {
        continueDecoding = 0;
        moviesReleaseScreenObject();
        showTexCounter2843 = 0;
        stoppedMovie2842 = 1;
    }
    charSelectStringOne = promptString2840[AINum];
    updateSelectSwitches(SEL, 0, 0, 1);
    for (i = 0; i < 4; i++) {
        SelectSlot *s = &CS_SLOT(SEL, i);

        if (s->state == 1 && s->taken == 1)
            hierSetSwitch(shell->m_costumeSw[i], 1);
        else
            hierSetSwitch(shell->m_costumeSw[i], 0);
    }
    hierSetSwitch(shell->m_costumeSw[curCostume2847], 2);
    {
        int n = 0;

        for (i = 0; i < 4; i++) {
            SelectSlot *s = &CS_SLOT(SEL, i);

            if (s->state == 1 && s->taken == 1)
                n++;
        }
        numAvail2841 = n;
    }
    if (SEL < first2848)
        SEL = last2849;
    if (SEL > last2849)
        SEL = first2848;
    /* find an open costume; if the monster has none, move on to the next monster with costumes left */
    while (!found) {
        if (isSlotOpen(SEL - 1, curCostume2847)) {
            found = 1;
        } else if (numAvail2841 == 0) {
            switch (SEL) {
            case 1: SEL = modelsLeft(2) ? 2 : 3; break;
            case 2: SEL = modelsLeft(3) ? 3 : 4; break;
            case 3: SEL = modelsLeft(4) ? 4 : 5; break;
            case 4: SEL = modelsLeft(5) ? 5 : 6; break;
            case 5: SEL = modelsLeft(6) ? 6 : 7; break;
            case 6: SEL = modelsLeft(7) ? 7 : 8; break;
            case 7: SEL = modelsLeft(8) ? 8 : 9; break;
            case 8:
                if (last2849 == 10)
                    SEL = modelsLeft(9) ? 9 : last2849;
                else
                    SEL = modelsLeft(last2849) ? last2849 : first2848;
                break;
            case 9:
                if (last2849 == 10)
                    SEL = modelsLeft(10) ? last2849 : first2848;
                else
                    SEL = modelsLeft(first2848) ? first2848 : first2848 + 1;
                break;
            case 10:
                SEL = modelsLeft(first2848) ? first2848 : first2848 + 1;
                break;
            }
        } else {
            curCostume2847++;
        }
        CS_WRAP(curCostume2847);
    }
    hierSetSwitch(shell->m_sw5CC, SEL);
    hierSetSwitch(shell->m_costumeSw[curCostume2847], 2);
    moviesReleaseScreenObject();
    continueDecoding = 0;
    input = screenGetInput(1) & 0xFFFF;
    switch (input) {
    case 1:
        CS_MOVE_COSTUME(curCostume2847, numAvail2841, -1, SEL - 1, 2);
        break;
    case 2:
        CS_MOVE_COSTUME(curCostume2847, numAvail2841, 1, SEL, 4);
        break;
    case 3: /* previous monster with costumes left */
        switch (SEL) {
        case 1:
            if (modelsLeft(last2849))
                SEL = last2849;
            else
                SEL = modelsLeft(last2849 - 1) ? last2849 - 1 : last2849 - 2;
            break;
        case 2:
            if (first2848 == 1) {
                if (modelsLeft(1))
                    SEL = first2848;
                else
                    SEL = modelsLeft(last2849) ? last2849 : last2849 - 1;
            } else if (modelsLeft(last2849)) {
                SEL = last2849;
            } else {
                SEL = modelsLeft(last2849 - 1) ? last2849 - 1 : last2849 - 2;
            }
            break;
        case 3:
            if (first2848 == 1) {
                if (modelsLeft(2))
                    SEL = 2;
                else if (modelsLeft(1))
                    SEL = first2848;
                else
                    SEL = last2849;
            } else if (modelsLeft(2)) {
                SEL = 2;
            } else {
                SEL = modelsLeft(last2849) ? last2849 : last2849 - 1;
            }
            break;
        case 4: SEL = modelsLeft(3) ? 3 : 2; break;
        case 5: SEL = modelsLeft(4) ? 4 : 3; break;
        case 6: SEL = modelsLeft(5) ? 5 : 4; break;
        case 7: SEL = modelsLeft(6) ? 6 : 5; break;
        case 8: SEL = modelsLeft(7) ? 7 : 6; break;
        case 9: SEL = modelsLeft(8) ? 8 : 7; break;
        case 10: SEL = modelsLeft(9) ? 9 : 8; break;
        }
        /* the retail tests the group after the selected one here (no - 1); the costume is reset below anyway */
        if (monsterSelectMode[SEL * 4].taken == 1)
            curCostume2847 = 0;
        hierSetSwitch(shell->m_sw5CC, SEL);
        stoppedMovie2842 = 1;
        continueDecoding = 0;
        ((ShellSound *)((char *)shell + 0x2918))->playShellSFXSound(5);
        showTexCounter2843 = 0;
        curCostume2847 = 0;
        break;
    case 4: /* next monster with costumes left */
        switch (SEL) {
        case 1: SEL = modelsLeft(2) ? 2 : 3; break;
        case 2: SEL = modelsLeft(3) ? 3 : 4; break;
        case 3: SEL = modelsLeft(4) ? 4 : 5; break;
        case 4: SEL = modelsLeft(5) ? 5 : 6; break;
        case 5: SEL = modelsLeft(6) ? 6 : 7; break;
        case 6: SEL = modelsLeft(7) ? 7 : 8; break;
        case 7: SEL = modelsLeft(8) ? 8 : 9; break;
        case 8:
            if (last2849 == 10) {
                if (modelsLeft(9))
                    SEL = 9;
                else
                    SEL = modelsLeft(10) ? last2849 : first2848;
            } else if (modelsLeft(last2849)) {
                SEL = last2849;
            } else {
                SEL = modelsLeft(first2848) ? first2848 : first2848 + 1;
            }
            break;
        case 9:
            if (last2849 == 10) {
                if (modelsLeft(10))
                    SEL = last2849;
                else
                    SEL = modelsLeft(first2848) ? first2848 : first2848 + 1;
            } else if (modelsLeft(first2848)) {
                SEL = first2848;
            } else {
                SEL = modelsLeft(first2848 + 1) ? first2848 + 1 : first2848 + 2;
            }
            break;
        case 10:
            if (modelsLeft(first2848))
                SEL = first2848;
            else
                SEL = modelsLeft(first2848 + 1) ? first2848 + 1 : first2848 + 2;
            break;
        }
        hierSetSwitch(shell->m_sw5CC, SEL);
        stoppedMovie2842 = 1;
        continueDecoding = 0;
        ((ShellSound *)((char *)shell + 0x2918))->playShellSFXSound(3);
        showTexCounter2843 = 0;
        curCostume2847 = 0;
        break;
    case 6:
        shell->m_costume[2 + AINum] = curCostume2847;
        shell->m_monsterSel[4 + AINum] = monsterNametomonsterId[SEL - 1];
        AINum++;
        stoppedMovie2842 = 1;
        continueDecoding = 0;
        CS_SLOT(SEL, curCostume2847).taken = 0;
        hierSetSwitch(shell->m_sw5D0, (*(int **)((char *)g_levelList + 8))[0]);
        charSelectStringOne = D_006F7EB8;
        AINumLeft--;
        changeScreen(0x11, 0x11, 1, 0, false);
        if (AINumLeft != 0) {
            charSelectStringOne = D_006F7EB8;
            changeScreen(0x11, 0x11, 1, 0, false);
        } else {
            charSelectStringOne = promptString2840[3];
            hierSetCsDrawMe(shell->m_costumeBoxCs, 0);
            charSelectStringOne = D_006F7EB8;
            changeScreen(0x11, 8, 1, 3, false);
        }
        ((ShellSound *)((char *)shell + 0x2918))->playShellSFXSound(8);
        break;
    case 5:
        hierSetSwitch(shell->m_sw5CC, 0);
        hierSetCsDrawMe(shell->m_costumeBoxCs, 0);
        charSelectStringOne = D_006F7EB8;
        changeScreen(0x11, 3, -1, 0, true);
        stoppedMovie2842 = 1;
        continueDecoding = 0;
        ((ShellSound *)((char *)shell + 0x2918))->playShellSFXSound(6);
        break;
    }
    if (selectingIsComplete2844 != 0) {
        hierSetSwitch(shell->m_sw5CC, 0);
        continueDecoding = 0;
        stoppedMovie2842 = 1;
        changeScreen(0x11, 8, 1, 3, true);
    }
}
#undef SEL
#undef CS_MOVE_COSTUME
#undef CS_WRAP
#undef CS_SLOT
#else
INCLUDE_ASM("asm/nonmatchings/game/screen", screenSelect1AI__Fv);
#endif
#ifdef NON_MATCHING
extern int numSelectables2853 __asm__("numSelectables.2853");
extern int PlayerConfig;
__asm__("#SNFIX_SMALL PlayerConfig");
/* Controller configuration: left/right (codes 3/4) pick one of the configurations (m_menuItemSw[4][0] shows it), confirm stores it
   in m_ctlrCfg[PlayerConfig] (the player whose pad is being set up) and goes back. While configuring player 2 (PlayerConfig == 1)
   pad 2 drives the screen as well. NON_MATCHING: 3/297 words differ, 0x448 bytes vs 0x4A4 (untuned). */
void screenCtlrCfg(void)
{
    int sel;

    if (screenFirstPass != 0)
        screenFirstPass = 0;
    switch (screenGetInput(1) & 0xFFFF) {
    case 3:
        sel = currentSelection[currScreen];
        currentSelection[currScreen] = sel >= 2 ? sel - 1 : numSelectables2853;
        hierSetSwitch(shell->m_menuItemSw[4][0], currentSelection[currScreen]);
        ((ShellSound *)((char *)shell + 0x2918))->playShellSFXSound(2);
        break;
    case 4:
        sel = currentSelection[currScreen];
        currentSelection[currScreen] = sel < numSelectables2853 ? sel + 1 : 1;
        hierSetSwitch(shell->m_menuItemSw[4][0], currentSelection[currScreen]);
        ((ShellSound *)((char *)shell + 0x2918))->playShellSFXSound(4);
        break;
    case 6:
        switch (currentSelection[currScreen]) {
        case 1:
            if (PlayerConfig == 0)
                shell->m_ctlrCfg[0] = 1;
            else
                shell->m_ctlrCfg[1] = 1;
            changeScreen(4, prevScreen, -1, 0xB, true);
            break;
        case 2:
            if (PlayerConfig == 0)
                shell->m_ctlrCfg[0] = 2;
            else
                shell->m_ctlrCfg[1] = 2;
            changeScreen(4, prevScreen, -1, 0xB, true);
            break;
        case 3:
            if (PlayerConfig == 0)
                shell->m_ctlrCfg[0] = 3;
            else
                shell->m_ctlrCfg[1] = 3;
            changeScreen(4, prevScreen, -1, 0xB, true);
            break;
        }
        ((ShellSound *)((char *)shell + 0x2918))->playShellSFXSound(8);
        break;
    case 5:
        changeScreen(4, prevScreen, -1, 0xB, true);
        ((ShellSound *)((char *)shell + 0x2918))->playShellSFXSound(6);
        break;
    }
    if (PlayerConfig == 1) {
        switch (screenGetInput(2) & 0xFFFF) {
        case 3:
            sel = currentSelection[currScreen];
            currentSelection[currScreen] = sel >= 2 ? sel - 1 : numSelectables2853;
            hierSetSwitch(shell->m_menuItemSw[4][0], currentSelection[currScreen]);
            ((ShellSound *)((char *)shell + 0x2918))->playShellSFXSound(2);
            break;
        case 4:
            sel = currentSelection[currScreen];
            currentSelection[currScreen] = sel < numSelectables2853 ? sel + 1 : 1;
            hierSetSwitch(shell->m_menuItemSw[4][0], currentSelection[currScreen]);
            ((ShellSound *)((char *)shell + 0x2918))->playShellSFXSound(4);
            break;
        case 6:
            switch (currentSelection[currScreen]) {
            case 1:
                shell->m_ctlrCfg[1] = 1;
                changeScreen(4, prevScreen, -1, 0xB, true);
                break;
            case 2:
                shell->m_ctlrCfg[1] = 2;
                changeScreen(4, prevScreen, -1, 0xB, true);
                break;
            case 3:
                shell->m_ctlrCfg[1] = 3;
                changeScreen(4, prevScreen, -1, 0xB, true);
                break;
            }
            ((ShellSound *)((char *)shell + 0x2918))->playShellSFXSound(8);
            break;
        case 5:
            changeScreen(4, prevScreen, -1, 0xB, true);
            ((ShellSound *)((char *)shell + 0x2918))->playShellSFXSound(6);
            break;
        }
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/game/screen", screenCtlrCfg__Fv);
#endif
#ifdef NON_MATCHING
/* Two-player game modes: 1 = free for all (mode 3, level list PO_MPFreeForAll), 2 = elimination (mode 6, PO_MPElimination), 3 = minigames.
   Here the cursor moves with codes 1/2 (up/down) instead of 3/4. */
void screenGameModes2P(void)
{
    int input, sel;
    int *selSw;

    if (screenFirstPass != 0) {
        numSelectables2857 = screenPrepareDefault(5, currentSelection[currScreen]);
        *(int *)((char *)shell + 0x2948) = 0;
        shell->m_numAIs = 0;
        hierSetSwitch(*(_hierswitch **)((char *)shell + 0x5D0), 0);
        moviesReadyScreenObject(*(_hierobject **)((char *)shell + 0x160), 8, 7);
        myMovie.Play(false, MainMovie);
        hierSetSwitch(*(_hierswitch **)((char *)shell + 0x70), 0);
        hierSetSwitch(*(_hierswitch **)((char *)shell + 0x74), 0);
        hierSetSwitch(*(_hierswitch **)((char *)shell + 0x78), 0);
        hierSetSwitch(*(_hierswitch **)((char *)shell + 0x414), currentSelection[11]);
        hierSetSwitch(*(_hierswitch **)((char *)shell + 0x464), currentSelection[13]);
        continueDecoding = 1;
        initSelectSwitches();
        currentSelection[6] = *(int *)((char *)shell + 0x2B9C) + 1;
        currentSelection[28] = *(int *)((char *)shell + 0x2BA0);
        currentSelection[18] = *(int *)((char *)shell + 0x2B9C) + 1;
        currentSelection[40] = *(int *)((char *)shell + 0x2BA0);
        currentSelection[19] = *(int *)((char *)shell + 0x2B9C) + 1;
        screenFirstPass = 0;
        currentSelection[41] = *(int *)((char *)shell + 0x2BA0);
    }
    myMovie.Play(false, MainMovie);
    input = screenGetInput(1) & 0xFFFF;
    switch (input) {
    case 1:
        hierSetSwitch(*(_hierswitch **)((char *)shell + 0x324 + currentSelection[currScreen] * 4), 0);
        sel = currentSelection[currScreen];
        currentSelection[currScreen] = sel >= 2 ? sel - 1 : numSelectables2857;
        hierSetSwitch(*(_hierswitch **)((char *)shell + 0x324 + currentSelection[currScreen] * 4), 1);
        ((ShellSound *)((char *)shell + 0x2918))->playShellSFXSound(2);
        return;
    case 2:
        hierSetSwitch(*(_hierswitch **)((char *)shell + 0x324 + currentSelection[currScreen] * 4), 0);
        sel = currentSelection[currScreen];
        currentSelection[currScreen] = sel < numSelectables2857 ? sel + 1 : 1;
        hierSetSwitch(*(_hierswitch **)((char *)shell + 0x324 + currentSelection[currScreen] * 4), 1);
        ((ShellSound *)((char *)shell + 0x2918))->playShellSFXSound(4);
        return;
    case 6:
        sel = currentSelection[currScreen];
        if (sel == 1) {
            shell->m_mode = 3;
            shell->m_battleMode = 3;
            g_levelList = PO_MPFreeForAll;
            hierSetSwitch(*(_hierswitch **)((char *)shell + 0x5CC), 0);
            changeScreen(5, 0xD, 1, 2, true);
            initSelectSwitches();
        } else if (sel == 2) {
            shell->m_mode = 6;
            shell->m_battleMode = 6;
            shell->m_killTarget = 0;
            g_levelList = PO_MPElimination;
            shell->m_wins[0] = 0;
            shell->m_wins[1] = 0;
            hierSetSwitch(*(_hierswitch **)((char *)shell + 0x5CC), 0);
            changeScreen(5, 0xB, 1, 2, true);
            initSelectSwitches();
        } else if (sel == 3) {
            changeScreen(5, 0xE, 1, 8, true);
            hierSetSwitch(*(_hierswitch **)((char *)shell + 0x48C), currentSelection[14]);
            shell->m_killTarget = 0;
        } else {
            printf("Two Player Game Modes Screen has screwed up item indexing! - We think we selected #%i\n", sel);
        }
        ((ShellSound *)((char *)shell + 0x2918))->playShellSFXSound(8);
        return;
    case 5:
        hierSetSwitch(*(_hierswitch **)((char *)shell + 0x5D0), 0);
        changeScreen(5, prevScreen, -1, 0, true);
        ((ShellSound *)((char *)shell + 0x2918))->playShellSFXSound(6);
        shell->m_numPlayers = 0;
        break;
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/game/screen", screenGameModes2P__Fv);
#endif
INCLUDE_ASM("asm/nonmatchings/game/screen", D_006EE460);
INCLUDE_ASM("asm/nonmatchings/game/screen", D_006EE488);
INCLUDE_ASM("asm/nonmatchings/game/screen", screenCharSelect3P__Fv);
INCLUDE_ASM("asm/nonmatchings/game/screen", D_006EE4D8);
INCLUDE_ASM("asm/nonmatchings/game/screen", D_006EE508);
INCLUDE_ASM("asm/nonmatchings/game/screen", D_006EE530);
INCLUDE_ASM("asm/nonmatchings/game/screen", D_006EE558);
INCLUDE_ASM("asm/nonmatchings/game/screen", jtbl_006EE590);
INCLUDE_ASM("asm/nonmatchings/game/screen", jtbl_006EE5B0);
INCLUDE_ASM("asm/nonmatchings/game/screen", screenCharSelect2P__Fv);
INCLUDE_ASM("asm/nonmatchings/game/screen", screenMinigames2P__Fv);
INCLUDE_ASM("asm/nonmatchings/game/screen", D_006EE698);
INCLUDE_ASM("asm/nonmatchings/game/screen", D_006EE6D0);
INCLUDE_ASM("asm/nonmatchings/game/screen", jtbl_006EE720);
INCLUDE_ASM("asm/nonmatchings/game/screen", jtbl_006EE740);
INCLUDE_ASM("asm/nonmatchings/game/screen", jtbl_006EE770);
INCLUDE_ASM("asm/nonmatchings/game/screen", jtbl_006EE7A0);
INCLUDE_ASM("asm/nonmatchings/game/screen", jtbl_006EE7D0);
INCLUDE_ASM("asm/nonmatchings/game/screen", jtbl_006EE7F0);
INCLUDE_ASM("asm/nonmatchings/game/screen", jtbl_006EE820);
INCLUDE_ASM("asm/nonmatchings/game/screen", jtbl_006EE850);
INCLUDE_ASM("asm/nonmatchings/game/screen", D_006EE878);
INCLUDE_ASM("asm/nonmatchings/game/screen", D_006EE890);
INCLUDE_ASM("asm/nonmatchings/game/screen", D_006EE8F8);
#ifndef NON_MATCHING /* the C++ screenElimOptions2P needs no table */
INCLUDE_ASM("asm/nonmatchings/game/screen", jtbl_006EE960);
#endif
INCLUDE_ASM("asm/nonmatchings/game/screen", D_006EE978);
INCLUDE_ASM("asm/nonmatchings/game/screen", jtbl_006EE9D0);
#ifndef NON_MATCHING /* the C++ screenTransition builds its own table */
INCLUDE_ASM("asm/nonmatchings/game/screen", jtbl_006EE9F0);
#endif
INCLUDE_ASM("asm/nonmatchings/game/screen", D_006EEA48);
INCLUDE_ASM("asm/nonmatchings/game/screen", D_006EEA58);
INCLUDE_ASM("asm/nonmatchings/game/screen", D_006EEA88);
INCLUDE_ASM("asm/nonmatchings/game/screen", D_006EEA98);
INCLUDE_ASM("asm/nonmatchings/game/screen", D_006EEAA8);
INCLUDE_ASM("asm/nonmatchings/game/screen", D_006EEAB8);
INCLUDE_ASM("asm/nonmatchings/game/screen", D_006EEAD8);
INCLUDE_ASM("asm/nonmatchings/game/screen", D_006EEAF0);
INCLUDE_ASM("asm/nonmatchings/game/screen", D_006EEB00);
INCLUDE_ASM("asm/nonmatchings/game/screen", D_006EEB30);
INCLUDE_ASM("asm/nonmatchings/game/screen", D_006EEB48);
INCLUDE_ASM("asm/nonmatchings/game/screen", D_006EEB60);
INCLUDE_ASM("asm/nonmatchings/game/screen", jtbl_006EEB80);
INCLUDE_ASM("asm/nonmatchings/game/screen", D_006EEB98);
INCLUDE_ASM("asm/nonmatchings/game/screen", D_006EEBC0);
INCLUDE_ASM("asm/nonmatchings/game/screen", D_006EEBE8);
INCLUDE_ASM("asm/nonmatchings/game/screen", D_006EEC10);
INCLUDE_ASM("asm/nonmatchings/game/screen", D_006EEC40);
INCLUDE_ASM("asm/nonmatchings/game/screen", D_006EEC70);
INCLUDE_ASM("asm/nonmatchings/game/screen", D_006EECA0);
INCLUDE_ASM("asm/nonmatchings/game/screen", jtbl_006EECD0);
INCLUDE_ASM("asm/nonmatchings/game/screen", jtbl_006EECF0);
INCLUDE_ASM("asm/nonmatchings/game/screen", D_006EED08);
INCLUDE_ASM("asm/nonmatchings/game/screen", D_006EED30);
INCLUDE_ASM("asm/nonmatchings/game/screen", D_006EED40);
INCLUDE_ASM("asm/nonmatchings/game/screen", D_006EED50);
INCLUDE_ASM("asm/nonmatchings/game/screen", D_006EED68);
#ifndef NON_MATCHING /* the C++ screenWriteInstructions builds its own table */
INCLUDE_ASM("asm/nonmatchings/game/screen", jtbl_006EED80);
#endif
#ifndef NON_MATCHING /* the C++ showButtons builds its own table */
INCLUDE_ASM("asm/nonmatchings/game/screen", jtbl_006EEDD0);
#endif
#ifndef NON_MATCHING /* the C++ levelUnlocked builds its own table */
INCLUDE_ASM("asm/nonmatchings/game/screen", jtbl_006EEE30);
#endif
INCLUDE_ASM("asm/nonmatchings/game/screen", on_bit_006EEE78);
#ifdef NON_MATCHING
/* A level list of the select screen (PO_1PFreeForAll, PO_MPFreeForAll, ...): the cursor runs from `first` and wraps at `count`;
   levels[cursor] is the level number (GameLevelLongNames, Shell::m_levelNum). */
struct LevelList {
    int first;
    int count;
    int *levels;
};
/* Display name of each level number, shown by screenWriteInstructions through charSelectStringOne. */
static const char GameLevelLongNames[28][18] = {
    "Default",       "Midtown Park", "Gamblers Gulch", "Rosdale Canyon", "Metro City",       "Century Airfield", "Atomic Island",
    "Baytown",       "Club Caldera", "Tsunopolis",     "UFO",            "Capitol",          "Training",         "Lake Powell",
    "Mini Baytown",  "Volcano",      "Tokyo Deathmatch", "Dylan",        "David",            "Lars",             "Jenn",
    "Yeon",          "Jacob",        "Steve",          "Pierre",         "Dodge Ball",       "Big Shot",         "Crush",
};
extern int numSelectables2924 __asm__("numSelectables.2924");
extern int beginning2925 __asm__("beginning.2925");
extern int ending2926 __asm__("ending.2926");
extern int stoppedMovie2927 __asm__("stoppedMovie.2927");
extern int showTexCounter2928 __asm__("showTexCounter.2928");
extern char D_006F7EB8[];
void moviesReleaseScreenObject(void);
/* Monster cursors of the select screens, kept while the level select is open: 3P (screen 18), 4P (screen 19), 2P (screen 6). */
extern int savedSel3P[2] __asm__("D_007314E8");
extern int savedSel4P[4] __asm__("D_007314F0");
extern int savedSel2P[24] __asm__("D_00731488");
#define LL ((LevelList *)g_levelList)
/* Left/right (codes 3/4) walk the level list with wraparound; with two or more AIs in mode 2, or any AI in mode 3, the fourth entry
   (index 3) is skipped. A locked level shows m_lockedSw; after 0x2E frames idle on an unlocked one the preview movie is let go.
   Confirm (6) on an unlocked level stores it in m_levelNum and starts loading (screen 99); back (5) restores the monster select it
   came from. NON_MATCHING: 12/587 words differ, 0x92C bytes vs 0x8EC (untuned). */
void screenLevelSelect(void)
{
    int input, cur, count;

    if (screenFirstPass != 0) {
        currentSelection[currScreen] = LL->first;
        ending2926 = LL->count - 1;
        numSelectables2924 = LL->count;
        beginning2925 = LL->first;
        continueDecoding = 0;
        moviesReleaseScreenObject();
        hierSetSwitch(shell->m_sw5D0, LL->levels[currentSelection[currScreen]]);
        stoppedMovie2927 = 1;
        screenFirstPass = 0;
        charSelectStringOne = (char *)GameLevelLongNames[LL->levels[currentSelection[currScreen]]];
    }
    hierSetSwitch(shell->m_lockedSw, 0);
    cur = levelUnlocked(LL->levels[currentSelection[currScreen]]);
    if (cur == 1) {
        if (showTexCounter2928 >= 0x2E && (screenGetInput(1) & 0xFFFF) == 0) {
            continueDecoding = 0;
            moviesReleaseScreenObject();
            stoppedMovie2927 = cur;
            showTexCounter2928 = 0;
        }
    } else {
        continueDecoding = 0;
        moviesReleaseScreenObject();
        hierSetSwitch(shell->m_lockedSw, 1);
    }
    input = screenGetInput(1) & 0xFFFF;
    switch (input) {
    case 3:
        if (beginning2925 < currentSelection[currScreen]) {
            if ((shell->m_mode == 2 && shell->m_numAIs >= 2) || (shell->m_mode == 3 && shell->m_numAIs > 0)) {
                cur = currentSelection[currScreen];
                count = LL->count;
                if (cur == 4)
                    currentSelection[currScreen] = (count + 2) % count;
                else
                    currentSelection[currScreen] = (cur + (count - 1)) % count;
            } else {
                count = LL->count;
                currentSelection[currScreen] = (currentSelection[currScreen] + (count - 1)) % count;
            }
        } else {
            currentSelection[currScreen] = ending2926;
        }
        hierSetSwitch(shell->m_sw5D0, LL->levels[currentSelection[currScreen]]);
        stoppedMovie2927 = 1;
        continueDecoding = 0;
        charSelectStringOne = (char *)GameLevelLongNames[LL->levels[currentSelection[currScreen]]];
        ((ShellSound *)((char *)shell + 0x2918))->playShellSFXSound(2);
        showTexCounter2928 = 0;
        break;
    case 4:
        if (currentSelection[currScreen] < ending2926) {
            if ((shell->m_mode == 2 && shell->m_numAIs >= 2) || (shell->m_mode == 3 && shell->m_numAIs > 0)) {
                cur = currentSelection[currScreen];
                count = LL->count;
                if (cur == 2)
                    currentSelection[currScreen] = (count + 4) % count;
                else
                    currentSelection[currScreen] = (cur + (count + 1)) % count;
            } else {
                count = LL->count;
                currentSelection[currScreen] = (currentSelection[currScreen] + 1) % count;
            }
        } else {
            currentSelection[currScreen] = beginning2925;
        }
        hierSetSwitch(shell->m_sw5D0, LL->levels[currentSelection[currScreen]]);
        stoppedMovie2927 = 1;
        continueDecoding = 0;
        charSelectStringOne = (char *)GameLevelLongNames[LL->levels[currentSelection[currScreen]]];
        ((ShellSound *)((char *)shell + 0x2918))->playShellSFXSound(4);
        showTexCounter2928 = 0;
        break;
    case 6:
        cur = levelUnlocked(LL->levels[currentSelection[currScreen]]);
        if (cur == 1) {
            shell->m_levelNum = LL->levels[currentSelection[currScreen]];
            stoppedMovie2927 = cur;
            continueDecoding = 0;
            moviesReleaseScreenObject();
            charSelectStringOne = D_006F7EB8;
            hierSetSwitch(shell->m_lockedSw, 0);
            changeScreen(8, 99, 1, 4, true);
            ((ShellSound *)((char *)shell + 0x2918))->terminateShellSound();
            ((ShellSound *)((char *)shell + 0x2918))->playShellSFXSound(1);
        }
        break;
    case 5:
        charSelectStringOne = D_006F7EB8;
        *(int *)((char *)shell + 0x2948) = 0;
        if (prevScreen == 3) {
            hierSetSwitch(shell->m_sw5CC, currentSelection[3]);
            *(float *)(shell->m_menuObj[3][1] + 0x4C) = 0.0f;
            updateSelectSwitches(0, 0, 1, 1);
        } else if (prevScreen == 0x12 && shell->m_mode == 3) {
            currentSelection[18] = savedSel3P[0];
            currentSelection[40] = savedSel3P[1];
            hierSetSwitch(*(_hierswitch **)((char *)shell + 0x1C0), savedSel3P[0]);
            hierSetSwitch(*(_hierswitch **)((char *)shell + 0x1C4), savedSel3P[1]);
            updateSelectSwitches3P(0, 0, 0, 1, 1, 1);
        } else if (prevScreen == 0x13 && shell->m_mode == 3) {
            currentSelection[19] = savedSel4P[0];
            currentSelection[41] = savedSel4P[1];
            hierSetSwitch(*(_hierswitch **)((char *)shell + 0x1CC), savedSel4P[0]);
            hierSetSwitch(*(_hierswitch **)((char *)shell + 0x1D0), savedSel4P[1]);
            updateSelectSwitches4P(0, 0, 0, 0, 1, 1, 1, 1);
        } else if (prevScreen == 0x13 && shell->m_mode == 2) {
            currentSelection[19] = savedSel4P[0];
            hierSetSwitch(*(_hierswitch **)((char *)shell + 0x1CC), savedSel4P[0]);
            hierSetSwitch(*(_hierswitch **)((char *)shell + 0x1D0), 0);
            hierSetSwitch(*(_hierswitch **)((char *)shell + 0x1D4), 0);
            hierSetSwitch(*(_hierswitch **)((char *)shell + 0x1D8), 0);
            updateSelectSwitches4P(0, 0, 0, 0, 1, 1, 1, 1);
        } else if (prevScreen == 0x12 && shell->m_mode == 2) {
            currentSelection[18] = savedSel3P[0];
            hierSetSwitch(*(_hierswitch **)((char *)shell + 0x1C4), 0);
            hierSetSwitch(*(_hierswitch **)((char *)shell + 0x1C8), 0);
            updateSelectSwitches3P(0, 0, 0, 1, 1, 1);
        } else if (prevScreen == 6 && shell->m_mode == 2) {
            currentSelection[6] = savedSel2P[0];
            hierSetSwitch(*(_hierswitch **)((char *)shell + 0x1E0), 0);
            updateSelectSwitches(0, 0, 1, 1);
        }
        hierSetSwitch(shell->m_lockedSw, 0);
        moviesReadyScreenObject(shell->m_movieScreen, 8, 7);
        myMovie.Play(false, MainMovie);
        continueDecoding = 1;
        changeScreen(8, prevScreen, -1, 3, true);
        ((ShellSound *)((char *)shell + 0x2918))->playShellSFXSound(6);
        break;
    }
}
#undef LL
#else
INCLUDE_ASM("asm/nonmatchings/game/screen", screenLevelSelect__Fv);
#endif
void moviesReleaseScreenObject(void);
extern int levelMonsters[][12];
extern int levelMonsterModels[][4];
/* Story mode "load game": stops the menu movie, loads the save from the memory card and takes the AI monsters and costumes of the
   saved level from the level tables (0x29E0: number of AIs per level). */
#ifdef NON_MATCHING
/* 10/50 palavras; o retail relê m_levelNum a cada volta e anda com um ponteiro em m_costume */
void screenLoadSave(void)
{
    int i;

    moviesReleaseScreenObject();
    continueDecoding = 0;
    MonsterMc::global.updateStoryModeLoad();
    Shell *sh = shell;
    int *numAIs = (int *)((char *)sh + 0x29E0);
    for (i = 0; i < numAIs[sh->m_levelNum]; i++) {
        sh->m_monsterSel[4 + i] = levelMonsters[sh->m_levelNum][i];
        sh->m_costume[2 + i] = levelMonsterModels[sh->m_levelNum][i];
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/game/screen", screenLoadSave__Fv);
#endif
#ifdef NON_MATCHING
/* Moves the knob of slider `which` (0 or 1) along its bar; `v` is clamped to 0..1. */
void setSlider(int which, float v)
{
    if (v < 0.0f)
        v = 0.0f;
    if (v > 1.0f)
        v = 1.0f;
    switch (which) {
    case 0: {
        float *k = *(float **)((char *)shell + 0x604);

        k[4] = v * -1.565f;
        k[5] = v * 1.564f;
        k[6] = v * -0.003f;
        break;
    }
    case 1: {
        float *k = *(float **)((char *)shell + 0x608);

        k[4] = v * -1.553f;
        k[5] = v * 1.555f;
        k[6] = v * -0.012f;
        break;
    }
    default:
        printf("Trying to set a slider on a bar that doesn't exist!!\n");
        break;
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/game/screen", setSlider__Fif);
#endif
#ifdef NON_MATCHING
extern "C" void snd_SetMasterVolume(int which, int vol);
class TheGame {
public:
    void SetVibration(int pad, bool on);
};
extern TheGame *game;
void inputSetInputMode(int mode);
void saveSettings(void);
void loadSettings(void);
extern char mcPageMgrGlobal[] __asm__("_9McPageMgr$global");
extern int numSelectables2938 __asm__("numSelectables.2938");
/* options as they were when the screen was opened, put back by back (5) */
extern int savedDifficulty __asm__("Difficulty.2939");
extern int savedMono __asm__("audio.2940");
extern int savedSfxVolume __asm__("fxVol.2941");
extern int savedMusicVolume __asm__("musVol.2942");
extern int savedVibration0 __asm__("vib.2943");
extern int savedVibration1 __asm__("vib2.2944");
#define GAME_DIFFICULTY (*(int *)((char *)game + 0x1203CC))
#define GAME_VIBRATION(pad) (*(int *)((char *)game + 0x120498 + (pad) * 4))
/* Lights the difficulty switch of the current difficulty (0..2). */
#define OPT_SHOW_DIFFICULTY()                                    \
    for (i = 0; i < 3; i++) {                                    \
        if (GAME_DIFFICULTY == i) {                              \
            hierSetSwitch(shell->m_difficultySw[i], 1);          \
            GAME_DIFFICULTY = i;                                 \
        } else {                                                 \
            hierSetSwitch(shell->m_difficultySw[i], 0);          \
        }                                                        \
    }
/* Toggles the vibration of `pad` and shows it. */
#define OPT_TOGGLE_VIBRATION(pad, sfx)                                              \
    game->SetVibration((pad), GAME_VIBRATION(pad) ^ 1);                             \
    hierSetSwitch(shell->m_vibrationSw[pad][0], GAME_VIBRATION(pad));               \
    hierSetSwitch(shell->m_vibrationSw[pad][1], GAME_VIBRATION(pad) ^ 1);           \
    shell->m_vibration[pad] = GAME_VIBRATION(pad);                                  \
    ((ShellSound *)((char *)shell + 0x2918))->playShellSFXSound(sfx)
/* Opens the controller configuration (screen 4) for `player`. */
#define OPT_CTLR_CFG(player)                                                        \
    currentSelection[4] = shell->m_ctlrCfg[player];                                 \
    hierSetSwitch(shell->m_menuItemSw[4][0], shell->m_ctlrCfg[player]);             \
    PlayerConfig = (player);                                                        \
    changeScreen(0xA, 4, 1, 0xB, true);                                             \
    ((ShellSound *)((char *)shell + 0x2918))->playShellSFXSound(8)
/* Options screen (10). Items: 0 difficulty, 1/2 controller configuration of player 1/2 (opened with input code 7), 3 stereo/mono,
   4 sfx volume (0..0x400, step 0x33; the slider sound plays while it is highlighted), 5 music volume (0..0x33E, step 0x29), 6/7
   vibration of pad 1/2, 8 load settings, 9 save settings. Left/right (3/4) change the highlighted item, confirm (6) on 0..7 leaves,
   back (5) restores what the options were when the screen was opened. Pad 2 can also toggle its own vibration (left/right on 7)
   and open its controller configuration (code 7 on item 2). NON_MATCHING: 56/997 words differ, 0xF78 bytes vs 0xF94 (untuned; same
   calls in the same order). */
void screenOptions(void)
{
    int i;

    if (screenFirstPass != 0) {
        numSelectables2938 = screenPrepareDefault(0xA, currentSelection[currScreen]);
        moviesReleaseScreenObject();
        continueDecoding = 0;
        savedDifficulty = GAME_DIFFICULTY;
        savedMono = shell->m_mono;
        savedSfxVolume = shell->m_sfxVolume;
        savedMusicVolume = shell->m_musicVolume;
        savedVibration0 = GAME_VIBRATION(0);
        savedVibration1 = GAME_VIBRATION(1);
        screenFirstPass = 0;
    }
    if (currentSelection[currScreen] == 4)
        ((ShellSound *)((char *)shell + 0x2918))->updateSFXSliderSound();
    switch (screenGetInput(1) & 0xFFFF) {
    case 1:
        hierSetSwitch(shell->m_menuItemSw[10][currentSelection[currScreen]], 0);
        currentSelection[currScreen] = currentSelection[currScreen] > 0 ? currentSelection[currScreen] - 1 : numSelectables2938;
        hierSetSwitch(shell->m_menuItemSw[10][currentSelection[currScreen]], 1);
        ((ShellSound *)((char *)shell + 0x2918))->playShellSFXSound(2);
        ((ShellSound *)((char *)shell + 0x2918))->stopSFXSliderSound();
        break;
    case 2:
        hierSetSwitch(shell->m_menuItemSw[10][currentSelection[currScreen]], 0);
        if (currentSelection[currScreen] < numSelectables2938)
            currentSelection[currScreen]++;
        else
            currentSelection[currScreen] = 0;
        hierSetSwitch(shell->m_menuItemSw[10][currentSelection[currScreen]], 1);
        ((ShellSound *)((char *)shell + 0x2918))->playShellSFXSound(4);
        ((ShellSound *)((char *)shell + 0x2918))->stopSFXSliderSound();
        break;
    case 3:
        switch (currentSelection[currScreen]) {
        case 0:
            if (GAME_DIFFICULTY > 0) {
                GAME_DIFFICULTY--;
                OPT_SHOW_DIFFICULTY();
                ((ShellSound *)((char *)shell + 0x2918))->playShellSFXSound(5);
            } else {
                hierSetSwitch(shell->m_difficultySw[2], 1);
                hierSetSwitch(shell->m_difficultySw[0], 0);
                ((ShellSound *)((char *)shell + 0x2918))->playShellSFXSound(5);
                GAME_DIFFICULTY = 2;
            }
            break;
        case 1:
        case 2:
        case 8:
        case 9:
            break;
        case 3:
            shell->m_mono = shell->m_mono != 1;
            hierSetSwitch(shell->m_stereoSw, shell->m_mono ^ 1);
            hierSetSwitch(shell->m_monoSw, shell->m_mono);
            ((ShellSound *)((char *)shell + 0x2918))->playShellSFXSound(5);
            break;
        case 4:
            inputSetInputMode(1);
            shell->m_sfxVolume -= 0x33;
            if (shell->m_sfxVolume < 0)
                shell->m_sfxVolume = 0;
            snd_SetMasterVolume(0, shell->m_sfxVolume);
            setSlider(0, (float)shell->m_sfxVolume * (1.0f / 1024.0f));
            inputSetInputMode(0);
            break;
        case 5:
            inputSetInputMode(1);
            shell->m_musicVolume -= 0x29;
            if (shell->m_musicVolume < 0)
                shell->m_musicVolume = 0;
            snd_SetMasterVolume(1, shell->m_musicVolume);
            setSlider(1, (float)shell->m_musicVolume / 830.0f);
            inputSetInputMode(0);
            break;
        case 6:
            OPT_TOGGLE_VIBRATION(0, 5);
            break;
        case 7:
            OPT_TOGGLE_VIBRATION(1, 5);
            break;
        default:
            printf("Options Screen has screwed up item indexing! - We think we selected #%i\n", currentSelection[currScreen]);
            break;
        }
        break;
    case 4:
        switch (currentSelection[currScreen]) {
        case 0:
            if (GAME_DIFFICULTY < 2) {
                GAME_DIFFICULTY++;
                OPT_SHOW_DIFFICULTY();
                ((ShellSound *)((char *)shell + 0x2918))->playShellSFXSound(3);
            } else {
                hierSetSwitch(shell->m_difficultySw[0], 1);
                hierSetSwitch(shell->m_difficultySw[2], 0);
                ((ShellSound *)((char *)shell + 0x2918))->playShellSFXSound(3);
                GAME_DIFFICULTY = 0;
            }
            break;
        case 1:
        case 2:
        case 8:
        case 9:
            break;
        case 3:
            shell->m_mono = shell->m_mono != 1;
            hierSetSwitch(shell->m_stereoSw, shell->m_mono ^ 1);
            hierSetSwitch(shell->m_monoSw, shell->m_mono);
            ((ShellSound *)((char *)shell + 0x2918))->playShellSFXSound(3);
            break;
        case 4:
            inputSetInputMode(1);
            shell->m_sfxVolume += 0x33;
            if (shell->m_sfxVolume > 0x400)
                shell->m_sfxVolume = 0x400;
            snd_SetMasterVolume(0, shell->m_sfxVolume);
            setSlider(0, (float)shell->m_sfxVolume * (1.0f / 1024.0f));
            inputSetInputMode(0);
            break;
        case 5:
            inputSetInputMode(1);
            shell->m_musicVolume += 0x29;
            if (shell->m_musicVolume > 0x33E)
                shell->m_musicVolume = 0x33E;
            snd_SetMasterVolume(1, shell->m_musicVolume);
            setSlider(1, (float)shell->m_musicVolume / 830.0f);
            inputSetInputMode(0);
            break;
        case 6:
            OPT_TOGGLE_VIBRATION(0, 3);
            break;
        case 7:
            OPT_TOGGLE_VIBRATION(1, 3);
            break;
        default:
            printf("Options Screen has screwed up item indexing! - We think we selected #%i\n", currentSelection[currScreen]);
            break;
        }
        break;
    case 7:
        switch (currentSelection[currScreen]) {
        case 1:
            OPT_CTLR_CFG(0);
            break;
        case 2:
            OPT_CTLR_CFG(1);
            break;
        }
        break;
    case 6:
        switch (currentSelection[currScreen]) {
        case 0:
        case 1:
        case 2:
        case 3:
        case 4:
        case 5:
        case 6:
        case 7:
            changeScreen(0xA, prevScreen, -1, 5, true);
            ((ShellSound *)((char *)shell + 0x2918))->playShellSFXSound(6);
            ((ShellSound *)((char *)shell + 0x2918))->stopSFXSliderSound();
            break;
        case 8:
            ((ShellSound *)((char *)shell + 0x2918))->playShellSFXSound(8);
            loadSettings();
            OPT_SHOW_DIFFICULTY();
            if (shell->m_mono == 1) {
                hierSetSwitch(shell->m_stereoSw, 0);
                hierSetSwitch(shell->m_monoSw, 1);
            } else {
                hierSetSwitch(shell->m_stereoSw, 1);
                hierSetSwitch(shell->m_monoSw, 0);
            }
            setSlider(0, (float)shell->m_sfxVolume * (1.0f / 1024.0f));
            snd_SetMasterVolume(0, shell->m_sfxVolume);
            setSlider(1, (float)shell->m_musicVolume / 830.0f);
            snd_SetMasterVolume(1, shell->m_musicVolume);
            game->SetVibration(0, shell->m_vibration[0]);
            game->SetVibration(1, shell->m_vibration[1]);
            hierSetSwitch(shell->m_vibrationSw[0][0], GAME_VIBRATION(0));
            hierSetSwitch(shell->m_vibrationSw[0][1], GAME_VIBRATION(0) ^ 1);
            hierSetSwitch(shell->m_vibrationSw[1][0], GAME_VIBRATION(1));
            hierSetSwitch(shell->m_vibrationSw[1][1], GAME_VIBRATION(1) ^ 1);
            break;
        case 9:
            ((ShellSound *)((char *)shell + 0x2918))->playShellSFXSound(8);
            saveSettings();
            if (*(int *)(mcPageMgrGlobal + 0x130) != 0) {
                changeScreen(0xA, prevScreen, -1, 5, true);
                ((ShellSound *)((char *)shell + 0x2918))->stopSFXSliderSound();
            }
            break;
        default:
            printf("Options Screen has screwed up item indexing! - We think we selected #%i\n", currentSelection[currScreen]);
            break;
        }
        ((ShellSound *)((char *)shell + 0x2918))->stopSFXSliderSound();
        break;
    case 5:
        GAME_DIFFICULTY = savedDifficulty;
        shell->m_sfxVolume = savedSfxVolume;
        shell->m_musicVolume = savedMusicVolume;
        shell->m_mono = savedMono != 0;
        game->SetVibration(0, savedVibration0 != 0);
        game->SetVibration(1, savedVibration1 != 0);
        shell->m_vibration[1] = savedVibration1 != 0;
        shell->m_vibration[0] = savedVibration0 != 0;
        changeScreen(0xA, prevScreen, -1, 5, true);
        ((ShellSound *)((char *)shell + 0x2918))->playShellSFXSound(6);
        ((ShellSound *)((char *)shell + 0x2918))->stopSFXSliderSound();
        shell->m_sfxVolume = shell->m_sfxVolumeSaved;
        shell->m_musicVolume = shell->m_musicVolumeSaved;
        snd_SetMasterVolume(0, shell->m_sfxVolumeSaved);
        snd_SetMasterVolume(1, shell->m_musicVolumeSaved);
        break;
    }
    /* the second pad */
    switch (screenGetInput(2) & 0xFFFF) {
    case 3:
        switch (currentSelection[currScreen]) {
        case 7:
            OPT_TOGGLE_VIBRATION(1, 3);
            break;
        }
        break;
    case 4:
        switch (currentSelection[currScreen]) {
        case 7:
            OPT_TOGGLE_VIBRATION(1, 3);
            break;
        }
        break;
    case 7:
        switch (currentSelection[currScreen]) {
        case 2:
            OPT_CTLR_CFG(1);
            break;
        }
        break;
    }
}
#undef GAME_DIFFICULTY
#undef GAME_VIBRATION
#undef OPT_SHOW_DIFFICULTY
#undef OPT_TOGGLE_VIBRATION
#undef OPT_CTLR_CFG
#else
INCLUDE_ASM("asm/nonmatchings/game/screen", screenOptions__Fv);
#endif
#ifdef NON_MATCHING
/* One-player free-for-all options: how many AIs (1..3) fight the player. */
void screenFreeForAllOptions1P(void)
{
    int input, sel, i;

    if (screenFirstPass != 0) {
        numSelectables2948 = 3;
        hierSetSwitch(*(_hierswitch **)((char *)shell + 0x43C), currentSelection[currScreen]);
        screenFirstPass = 0;
        for (i = 2; i >= 0; i--)
            *(int *)((char *)shell + 0x2948 + i * 4) = 0;
        hierSetSwitch(*(_hierswitch **)((char *)shell + 0x1D0), 0);
        hierSetSwitch(*(_hierswitch **)((char *)shell + 0x1D4), 0);
        hierSetSwitch(*(_hierswitch **)((char *)shell + 0x1D8), 0);
        hierSetSwitch(*(_hierswitch **)((char *)shell + 0x1C4), 0);
        hierSetSwitch(*(_hierswitch **)((char *)shell + 0x1C8), 0);
        hierSetSwitch(*(_hierswitch **)((char *)shell + 0x1E0), 0);
        updateSelectSwitches(0, 0, 1, 1);
        updateSelectSwitches3P(0, 0, 0, 1, 1, 1);
        updateSelectSwitches4P(0, 0, 0, 0, 1, 1, 1, 1);
    }
    hierSetSwitch(*(_hierswitch **)((char *)shell + 0x43C), currentSelection[currScreen]);
    myMovie.Play(false, MainMovie);
    input = screenGetInput(1) & 0xFFFF;
    switch (input) {
    case 3:
        sel = currentSelection[currScreen];
        currentSelection[currScreen] = sel >= 2 ? sel - 1 : numSelectables2948;
        hierSetSwitch(*(_hierswitch **)((char *)shell + 0x43C), currentSelection[currScreen]);
        ((ShellSound *)((char *)shell + 0x2918))->playShellSFXSound(2);
        return;
    case 4:
        sel = currentSelection[currScreen];
        currentSelection[currScreen] = sel < numSelectables2948 ? sel + 1 : 1;
        hierSetSwitch(*(_hierswitch **)((char *)shell + 0x43C), currentSelection[currScreen]);
        ((ShellSound *)((char *)shell + 0x2918))->playShellSFXSound(4);
        return;
    case 6:
        sel = currentSelection[currScreen];
        if (sel == 0) {
            shell->m_numAIs = 0;
            printf("Number of AI set to %i\n", 0);
            changeScreen(0xC, 3, 1, 0, true);
        } else if (sel >= 1 && sel <= 3) {
            shell->m_numAIs = sel;
            printf("Number of AI set to %i\n", sel);
            changeScreen(0xC, 0xF, 1, 0, true);
        } else {
            printf("One Player Free for All Options Screen has screwed up item indexing! - We think we selected #%i\n", sel);
        }
        ((ShellSound *)((char *)shell + 0x2918))->playShellSFXSound(8);
        return;
    case 5:
        shell->m_numAIs = 0;
        hierSetSwitch(*(_hierswitch **)((char *)shell + 0x5D0), 0);
        changeScreen(0xC, prevScreen, -1, 2, true);
        ((ShellSound *)((char *)shell + 0x2918))->playShellSFXSound(6);
        break;
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/game/screen", screenFreeForAllOptions1P__Fv);
#endif
#ifdef NON_MATCHING
/* Two-player free-for-all options: how many AIs join (0, 1 or 2). Input codes from screenGetInput: 3/4 move the cursor, 5 goes back, 6 confirms. */
void screenFreeForAllOptions2P(void)
{
    int *cur = &currentSelection[currScreen];
    int input, sel;

    if (screenFirstPass != 0) {
        numSelectables2952 = 2;
        updateSelectSwitches(0, 0, 1, 1);
        updateSelectSwitches3P(0, 0, 0, 1, 1, 1);
        updateSelectSwitches4P(0, 0, 0, 0, 1, 1, 1, 1);
        screenFirstPass = 0;
    }
    hierSetSwitch(*(_hierswitch **)((char *)shell + 0x5CC), 0);
    hierSetSwitch(*(_hierswitch **)((char *)shell + 0x464), currentSelection[currScreen]);
    myMovie.Play(false, MainMovie);
    input = screenGetInput(1) & 0xFFFF;
    switch (input) {
    case 3:
        cur = &currentSelection[currScreen];
        *cur = *cur > 0 ? *cur - 1 : numSelectables2952;
        hierSetSwitch(*(_hierswitch **)((char *)shell + 0x464), currentSelection[currScreen]);
        ((ShellSound *)((char *)shell + 0x2918))->playShellSFXSound(2);
        return;
    case 4:
        cur = &currentSelection[currScreen];
        if (*cur < numSelectables2952)
            *cur = *cur + 1;
        else
            *cur = 0;
        hierSetSwitch(*(_hierswitch **)((char *)shell + 0x464), currentSelection[currScreen]);
        ((ShellSound *)((char *)shell + 0x2918))->playShellSFXSound(4);
        return;
    case 6:
        sel = currentSelection[currScreen];
        if (sel == 0) {
            shell->m_numAIs = 0;
            changeScreen(0xD, 0xF, 1, 0, true);
        } else if (sel == 1 || sel == 2) {
            shell->m_numAIs = sel;
            changeScreen(0xD, 0xF, 1, 0, true);
        } else {
            printf("Two Player Free for All Options Screen has screwed up item indexing! - We think we selected #%i\n", sel);
        }
        ((ShellSound *)((char *)shell + 0x2918))->playShellSFXSound(8);
        return;
    case 5:
        shell->m_numAIs = 0;
        changeScreen(0xD, 5, -1, 2, true);
        ((ShellSound *)((char *)shell + 0x2918))->playShellSFXSound(6);
        break;
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/game/screen", screenFreeForAllOptions2P__Fv);
#endif
#ifdef NON_MATCHING
/* Two-player elimination options: the cursor (0..4) picks the elimination setting, stored (+1) in shell->m_elimination and copied to both m_continues. */
void screenElimOptions2P(void)
{
    int input, sel;

    if (screenFirstPass != 0) {
        int a = *(int *)((char *)shell + 0x2B9C);
        int b = *(int *)((char *)shell + 0x2BA0);

        if (a != 0 || b == 0) {
            initSelectSwitches();
            a = *(int *)((char *)shell + 0x2B9C);
            if (!(a < currentSelection[6] && currentSelection[28] < *(int *)((char *)shell + 0x2BA0))) {
                currentSelection[6] = a + 1;
                currentSelection[28] = *(int *)((char *)shell + 0x2BA0) + 1;
            }
        }
        hierSetSwitch(*(_hierswitch **)((char *)shell + 0x1DC), currentSelection[6]);
        hierSetSwitch(*(_hierswitch **)((char *)shell + 0x1E0), currentSelection[28]);
        screenPrepareDefault(0xB, currentSelection[currScreen]);
        screenFirstPass = 0;
    }
    myMovie.Play(false, MainMovie);
    input = screenGetInput(1) & 0xFFFF;
    switch (input) {
    case 3:
        sel = currentSelection[currScreen];
        currentSelection[currScreen] = sel > 0 ? sel - 1 : 4;
        hierSetSwitch(*(_hierswitch **)((char *)shell + 0x414), currentSelection[currScreen]);
        ((ShellSound *)((char *)shell + 0x2918))->playShellSFXSound(2);
        return;
    case 4:
        sel = currentSelection[currScreen];
        currentSelection[currScreen] = sel < 4 ? sel + 1 : 0;
        hierSetSwitch(*(_hierswitch **)((char *)shell + 0x414), currentSelection[currScreen]);
        ((ShellSound *)((char *)shell + 0x2918))->playShellSFXSound(4);
        return;
    case 6:
        sel = currentSelection[currScreen];
        if (sel >= 0 && sel <= 4) {
            shell->m_elimination = sel + 1;
            changeScreen(0xB, 6, 1, 0, true);
            initSelectSwitches();
        }
        shell->m_continues[1] = shell->m_elimination;
        shell->m_continues[0] = shell->m_elimination;
        ((ShellSound *)((char *)shell + 0x2918))->playShellSFXSound(8);
        return;
    case 5:
        changeScreen(0xB, prevScreen, -1, 2, true);
        ((ShellSound *)((char *)shell + 0x2918))->playShellSFXSound(6);
        break;
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/game/screen", screenElimOptions2P__Fv);
#endif
#ifdef NON_MATCHING
/* Minigame choice: slot 1..3 picks level 25/26/27 and game mode 7/8/9, each only once its unlock flag is set. Input codes as in screenFreeForAllOptions2P. */
void screenMGSelect(void)
{
    int input, sel;

    if (screenFirstPass != 0) {
        initSelectSwitchesMG();
        currentSelection[7] = *(int *)((char *)shell + 0x2B9C) + 1;
        currentSelection[29] = *(int *)((char *)shell + 0x2BA0);
        hierSetSwitch(*(_hierswitch **)((char *)shell + 0x1EC), currentSelection[7]);
        hierSetSwitch(*(_hierswitch **)((char *)shell + 0x1F0), currentSelection[29]);
        charSelectStringOne = promptString2960 + 0x30;
        screenFirstPass = 0;
        hierSetSwitch(*(_hierswitch **)((char *)shell + 0x48C), currentSelection[currScreen]);
    }
    sel = currentSelection[currScreen];
    if ((sel == 1 && levelUnlocked(25) == 1) || (sel == 2 && levelUnlocked(26) == 1) || (sel == 3 && levelUnlocked(27) == 1))
        hierSetSwitch(*(_hierswitch **)((char *)shell + 0x600), 0);
    else
        hierSetSwitch(*(_hierswitch **)((char *)shell + 0x600), 1);
    input = screenGetInput(1) & 0xFFFF;
    switch (input) {
    case 3:
        sel = currentSelection[currScreen];
        currentSelection[currScreen] = sel >= 2 ? sel - 1 : numSelectables2959;
        hierSetSwitch(*(_hierswitch **)((char *)shell + 0x48C), currentSelection[currScreen]);
        ((ShellSound *)((char *)shell + 0x2918))->playShellSFXSound(2);
        return;
    case 4:
        sel = currentSelection[currScreen];
        currentSelection[currScreen] = sel < numSelectables2959 ? sel + 1 : 1;
        hierSetSwitch(*(_hierswitch **)((char *)shell + 0x48C), currentSelection[currScreen]);
        ((ShellSound *)((char *)shell + 0x2918))->playShellSFXSound(4);
        return;
    case 6: {
        int level = 0, mode = 0;

        sel = currentSelection[currScreen];
        if (sel == 1 && *(int *)((char *)monsterSelectMode + 0x794) == 1) {
            level = 25;
            mode = 7;
        } else if (sel == 2 && *(int *)((char *)monsterSelectMode + 0x7C0) == 1) {
            level = 26;
            mode = 8;
        } else if (sel == 3 && *(int *)((char *)monsterSelectMode + 0x7EC) == 1) {
            level = 27;
            mode = 9;
        }
        if (level != 0) {
            shell->m_levelNum = level;
            shell->m_battleMode = mode;
            shell->m_mode = mode;
            hierSetSwitch(*(_hierswitch **)((char *)shell + 0x48C), 4);
            changeScreen(0xE, 7, 1, 0, true);
        }
        ((ShellSound *)((char *)shell + 0x2918))->playShellSFXSound(3);
        return;
    }
    case 5:
        hierSetSwitch(*(_hierswitch **)((char *)shell + 0x5D0), 0);
        changeScreen(0xE, prevScreen, -1, 8, true);
        ((ShellSound *)((char *)shell + 0x2918))->playShellSFXSound(6);
        break;
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/game/screen", screenMGSelect__Fv);
#endif
INCLUDE_ASM("asm/nonmatchings/game/screen", screenBestOf__Fv);
#ifdef NON_MATCHING
extern int clearBeforeFontDMA;
void fontSetDefaultSize(int font);
/* Runs frames until the boot-time memory card load is over, then applies the loaded options: mono flag, music and sfx volumes,
   vibration of both pads, default font sizes. The retail leaves the loop object uninitialised (startField reads its `started`). */
/* 20/67 palavras; alocação de registradores do laço */
void screenMemoryCardInit(void)
{
    ShellRtLoop loop;
    int done = 0;

    clearBeforeFontDMA = 1;
    do {
        loop.startField(true);
        MonsterMc::global.updateBootLoad();
        if (MonsterMc::global.m_done) {
            done = 1;
            clearBeforeFontDMA = 0;
            *(int *)((char *)shell + 0x292C) = 1;
            shell->m_mono = shell->m_mono != 0;
            snd_SetMasterVolume(1, shell->m_musicVolume);
            snd_SetMasterVolume(0, shell->m_sfxVolume);
            game->SetVibration(0, shell->m_vibration[0]);
            game->SetVibration(1, shell->m_vibration[1]);
            fontSetDefaultSize(0);
            fontSetDefaultSize(1);
        }
        loop.endField(true);
    } while (!done);
}
#else
INCLUDE_ASM("asm/nonmatchings/game/screen", screenMemoryCardInit__Fv);
#endif
#ifdef NON_MATCHING
extern int stNeedUpdate __asm__("needUpdate.2972");
extern int stWaitingForAnimation __asm__("waitingForAnimation.2977");
extern int stWaitingForAlphaFade __asm__("waitingForAlphaFade.2978");
extern int stFadingIn __asm__("fadingIn.2979");
extern int stFadingOut __asm__("fadingOut.2980");
extern int stHalfWayMark __asm__("halfWayMark.2981");
extern int stAnimDir __asm__("D_006F8C88");        /* direction of the running transition animation */
extern int stAnimFrames __asm__("D_006F8C8C");     /* its total run frames */
extern int stAnimLastFrame __asm__("D_006F8C90");  /* its last frame */
extern int stFadeInFrame __asm__("D_006F8C94");    /* frame past which the new screen fades in (20 frames from the end) */
__asm__("#SNFIX_SMALL needUpdate.2972");
__asm__("#SNFIX_SMALL waitingForAnimation.2977");
__asm__("#SNFIX_SMALL waitingForAlphaFade.2978");
__asm__("#SNFIX_SMALL fadingIn.2979");
__asm__("#SNFIX_SMALL fadingOut.2980");
__asm__("#SNFIX_SMALL halfWayMark.2981");
__asm__("#SNFIX_SMALL D_006F8C88");
__asm__("#SNFIX_SMALL D_006F8C8C");
__asm__("#SNFIX_SMALL D_006F8C90");
__asm__("#SNFIX_SMALL D_006F8C94");
extern int g_fadeFrameOffset;
extern int AINumLeft;
extern int mSelectWins;
__asm__("#SNFIX_SMALL g_fadeFrameOffset");
__asm__("#SNFIX_SMALL AINumLeft");
__asm__("#SNFIX_SMALL mSelectWins");
void moviesReleaseScreenObject(void);
int animationGetDirection(_animHandle h);
float animationGetCurrentFrame(_animHandle h);
float animationGetLastFrame(_animHandle h);
float animationGetTotalRunFrames(_animHandle h);
void killButtons(void);
void showButtons(int screen);
void setSlider(int which, float v);
void screenWriteInstructions(int screen, float alpha);
/* Sets every object of menu `screen` to `alpha`; on screen 17 while AIs are still being picked the objects of screen 3 are forced
   opaque instead. */
#define ST_SET_SCREEN_ALPHA(screen, alpha)                                       \
    for (i = 0; i < 90; i++) {                                                   \
        if (currScreen == 17 && AINumLeft) {                                     \
            if (shell->m_menuObj[3][i])                                          \
                *(float *)(shell->m_menuObj[3][i] + 0x4C) = 1.0f;                \
        } else if (shell->m_menuObj[screen][i])                                  \
            *(float *)(shell->m_menuObj[screen][i] + 0x4C) = (alpha);            \
    }
/* 15/648 palavras; não afinada (o retail desenrola os laços de alpha e testa AINumLeft fora deles) */
/* Runs every frame while the menus move from lastScreen to currScreen (changeScreen started transition animation
   currAnimationIndex). On the first pass it measures the animation and sets up the new screen (options switches and sliders,
   background movie); halfway through the animation the old buttons go away; then the old screen fades out (alpha -0.05 per frame),
   the new one is prepared (panel, highlighted item) and fades in once the animation is 20 frames from its end. When both the
   animation and the fade are over, the transition ends (betweenScreens = 0). Screen 0x63 is a pseudo screen with no fade. */
void screenTransition(bool b)
{
    float pct;
    int i, ready;

    if (screenFirstPass) {
        if (currScreen == 20)
            g_fadeFrameOffset = 0;
        targetAlpha = 1.0f;
        stNeedUpdate = 1;
        stFadingOut = 1;
        stFadingIn = 0;
        stHalfWayMark = 0;
        screenFirstPass = 0;
        stAnimDir = animationGetDirection(shell->m_screenAnim[currAnimationIndex]);
        stAnimFrames = (int)animationGetTotalRunFrames(shell->m_screenAnim[currAnimationIndex]);
        stAnimLastFrame = (int)animationGetLastFrame(shell->m_screenAnim[currAnimationIndex]);
        if (stAnimDir)
            stFadeInFrame = stAnimLastFrame - 20;
        else
            stFadeInFrame = stAnimLastFrame + stAnimFrames + 20;
        if (currScreen == 10) {
            hierSetCsDrawMe(shell->m_sliderCs[0], 1);
            hierSetCsDrawMe(shell->m_sliderCs[1], 1);
            for (i = 0; i < 3; i++)
                hierSetSwitch(shell->m_difficultySw[i], *(int *)((char *)game + 0x1203CC) == i);
            hierSetSwitch(shell->m_stereoSw, shell->m_mono ^ 1);
            hierSetSwitch(shell->m_monoSw, shell->m_mono);
            hierSetSwitch(shell->m_vibrationSw[0][0], *(int *)((char *)game + 0x120498));
            hierSetSwitch(shell->m_vibrationSw[0][1], *(int *)((char *)game + 0x120498) ^ 1);
            hierSetSwitch(shell->m_vibrationSw[1][0], *(int *)((char *)game + 0x12049C));
            hierSetSwitch(shell->m_vibrationSw[1][1], *(int *)((char *)game + 0x12049C) ^ 1);
            setSlider(0, (float)shell->m_sfxVolume * (1.0f / 1024.0f));
            setSlider(1, (float)shell->m_musicVolume / 830.0f);
        } else if (currScreen == 0x63) {
            stFadingOut = 0;
            screenFirstPass = 1;
            stFadingIn = 0;
            betweenScreens = 0;
            stHalfWayMark = 0;
        }
        if (currScreen > 0 && (currScreen < 3 || currScreen == 5)) {
            moviesReadyScreenObject(shell->m_movieScreen, 8, 7);
            myMovie.Play(false, MainMovie);
            continueDecoding = 1;
        }
    }
    if (currScreen == 8)
        hierSetSwitch(*(_hierswitch **)((char *)shell + 0x5D0), **(int **)((char *)g_levelList + 8));
    if (currScreen == 15) {
        if (currentSelection[15] == 2) {
            hierSetSwitch(shell->m_menuItemSw[15][5], mSelectWins);
            hierSetSwitch(shell->m_menuItemSw[15][4], mSelectWins);
        } else
            hierSetSwitch(shell->m_menuItemSw[15][4], mSelectWins);
    }
    pct = animationGetCurrentPercent(shell->m_screenAnim[currAnimationIndex]);
    if (stAnimDir ? pct > 0.6f : pct < 0.4f) {
        stHalfWayMark = 1;
        killButtons();
        stNeedUpdate = 1;
    }
    if (stHalfWayMark) {
        screenWriteInstructions(currScreen, 0.0f);
        switch (currScreen) {
        case 1:
        case 3:
        case 6:
            moviesReadyScreenObject(shell->m_movieScreen, 8, 7);
            myMovie.Play(false, MainMovie);
            continueDecoding = 1;
            break;
        case 18:
            hierSetSwitch(*(_hierswitch **)((char *)shell + 0x1C8), 0);
            break;
        case 19:
            hierSetSwitch(*(_hierswitch **)((char *)shell + 0x1D4), 0);
            hierSetSwitch(*(_hierswitch **)((char *)shell + 0x1D8), 0);
            break;
        case 21:
            moviesReleaseScreenObject();
            break;
        }
    }
    if (pct > 0.001f && pct < 0.999f)
        stWaitingForAnimation = 1;
    else
        stWaitingForAnimation = 0;
    if (stFadingOut) {
        targetAlpha -= 0.05f;
        stWaitingForAlphaFade = 1;
        screenWriteInstructions(lastScreen, targetAlpha);
        ST_SET_SCREEN_ALPHA(lastScreen, targetAlpha);
        if (targetAlpha <= 0.0f) {
            targetAlpha = 0.0f;
            stFadingIn = 1;
            stFadingOut = 0;
            for (i = 0; i < 90; i++) {
                if (currScreen == 17 && AINumLeft) {
                    if (shell->m_menuObj[3][i])
                        *(float *)(shell->m_menuObj[3][i] + 0x4C) = targetAlpha;
                } else if (shell->m_menuObj[currScreen][i]) {
                    *(float *)(shell->m_menuObj[currScreen][i] + 0x4C) = targetAlpha;
                    screenWriteInstructions(currScreen, targetAlpha);
                    killButtons();
                }
            }
            hierSetSwitch(shell->m_screenSw, currScreen - 1);
            if (lastScreen == 20 && currScreen == 1)
                hierSetCsDrawMe(shell->m_menuCs, 1);
            screenPrepareDefault(currScreen, currentSelection[currScreen]);
        }
        if (currScreen != 0x63) {
            showButtons(lastScreen);
            stNeedUpdate = 0;
        }
    } else if (stFadingIn) {
        ready = 0;
        if ((stAnimDir && stFadeInFrame < (int)animationGetCurrentFrame(shell->m_screenAnim[currAnimationIndex]))
            || (!stAnimDir && (int)animationGetCurrentFrame(shell->m_screenAnim[currAnimationIndex]) < stFadeInFrame))
            ready = 1;
        if (ready) {
            targetAlpha += 0.05f;
            stWaitingForAlphaFade = 1;
            if (currScreen != 20)
                screenWriteInstructions(currScreen, targetAlpha);
            ST_SET_SCREEN_ALPHA(currScreen, targetAlpha);
            if (targetAlpha >= 1.0f) {
                stFadingIn = 0;
                stWaitingForAlphaFade = 0;
            }
        }
        if (currScreen != 0x63) {
            showButtons(currScreen);
            stNeedUpdate = 0;
        }
    } else
        stWaitingForAlphaFade = 0;
    if (!stWaitingForAnimation && !stWaitingForAlphaFade) {
        currAnimationIndex = 0;
        screenFirstPass = 1;
        betweenScreens = 0;
    }
}
#undef ST_SET_SCREEN_ALPHA
#else
INCLUDE_ASM("asm/nonmatchings/game/screen", screenTransition__Fb);
#endif
INCLUDE_ASM("asm/nonmatchings/game/screen", screenUnlocker__Fv);
INCLUDE_ASM("asm/nonmatchings/game/screen", screenCharSelect4P__Fv);
#ifdef NON_MATCHING
/* Title screen ("press start"): on the first pass hides the menus, selects item 1 of every screen for the four pads, gives both
   players a controller configuration, sets up the select columns and the default selections; then keeps the main movie playing
   until pad 1 presses start (action 0x10), which moves on to screen 1. */
/* 43/108 palavras; o retail desenrola diferente o laço das seleções */
void screenWaitForStart(void)
{
    int p, s, i;

    if (screenFirstPass) {
        hierSetCsDrawMe(shell->m_menuCs, 0);
        for (p = 0; p < 4; p++)
            for (s = 21; s >= 0; s--)
                currentSelection[p * 22 + s] = 1;
        for (i = 0; i < 2; i++)
            if (shell->m_ctlrCfg[i] == 0)
                shell->m_ctlrCfg[i] = 1;
        currentSelection[12] = 0;
        currentSelection[13] = 0;
        initSelectSwitches();
        currentSelection[6] = shell->m_selFirst + 1;
        currentSelection[22 + 6] = shell->m_selLast;
        currentSelection[7] = shell->m_selFirst + 1;
        currentSelection[22 + 7] = shell->m_selLast;
        currentSelection[18] = shell->m_selFirst + 1;
        currentSelection[22 + 18] = shell->m_selLast;
        currentSelection[19] = shell->m_selFirst + 1;
        currentSelection[22 + 19] = shell->m_selLast;
        currentSelection[12] = 1;
        currentSelection[10] = 0;
        currentSelection[4] = 1;
        currentSelection[11] = 0;
        currentSelection[14] = 1;
        screenFirstPass = 0;
    }
    myMovie.Play(false, MainMovie);
    if ((screenGetInput(1) & 0xFFFF) == 0x10) {
        changeScreen(0x14, 1, 1, 0, true);
        ((ShellSound *)((char *)shell + 0x2918))->playShellSFXSound(8);
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/game/screen", screenWaitForStart__Fv);
#endif
#ifdef NON_MATCHING
extern char D_006EED08[];
/* Leaves screen `from` for screen `to`. dir 1 goes forward (when `track`, `from` becomes prevScreen and the old prevScreen is pushed
   on the 16-deep oldScreens stack), dir -1 goes back (pops prevScreen from the stack); anything else just prints a warning.
   A nonzero `anim` plays transition animation `anim` of the shell, forward for dir 1, reversed otherwise. */
/* 79/100 palavras; ordem dos testes de dir/track */
void changeScreen(int from, int to, int dir, int anim, bool track)
{
    int i;

    lastScreen = from;
    if (dir == 1) {
        if (track) {
            for (i = 15; i > 0; i--)
                oldScreens[i] = oldScreens[i - 1];
            oldScreens[0] = prevScreen;
            prevScreen = from;
        }
    } else if (dir == -1) {
        if (track) {
            prevScreen = oldScreens[0];
            for (i = 0; i < 15; i++)
                oldScreens[i] = oldScreens[i + 1];
            oldScreens[15] = 0;
        }
    } else {
        printf(D_006EED08);
    }
    if (anim) {
        if (dir == 1)
            animationStart(shell->m_screenAnim[anim], true);
        else
            animationStartReverse(shell->m_screenAnim[anim], true);
        currAnimationIndex = anim;
    }
    currScreen = to;
    screenFirstPass = 1;
    betweenScreens = 1;
}
#else
INCLUDE_ASM("asm/nonmatchings/game/screen", changeScreen__Fiiiib);
#endif
#ifdef NON_MATCHING
/* Polls the pads allowed by `padMask` (on_bit[pad]) and returns (pad << 16) | action, 0 when nothing is pressed or a screen change is running.
   The 16 pad masks are tried in the order of `map`, each giving its action code 1..0x10. */
int screenGetInput(int padMask)
{
    static const struct {
        int mask;
        int action;
    } map[16] = {
        { 0x10, 1 }, { 0x40, 2 }, { 0x80, 3 }, { 0x20, 4 },
        { 0x2000, 8 }, { 0x8000, 7 }, { 0x4000, 6 }, { 0x1000, 5 },
        { 0x400, 9 }, { 0x800, 0xC }, { 0x100, 0xA }, { 0x200, 0xD },
        { 2, 0xB }, { 4, 0xE }, { 8, 0x10 }, { 1, 0xF },
    };
    int pad, i;

    if (betweenScreens == 0) {
        for (pad = 0; pad < 4; pad++) {
            if (!(on_bit[pad] & padMask))
                continue;
            for (i = 0; i < 16; i++)
                if (inputGetInput(map[i].mask, pad))
                    return (pad << 16) | map[i].action;
        }
    }
    return 0;
}
#else
INCLUDE_ASM("asm/nonmatchings/game/screen", screenGetInput__Fi);
#endif
#ifdef NON_MATCHING
/* Shows the panel of menu `screen` and lights item `sel` among its items (the others off). Items are counted from 1, except on
   screen 10, whose list starts at 0; the list ends at the first null switch or after 10. Returns the index of the last item. */
/* 6/75 palavras; o retail relê shell a cada volta e mantém dois índices */
int screenPrepareDefault(int screen, int sel)
{
    int i;

    hierSetSwitch(shell->m_screenSw, screen - 1);
    if (screen == 10) {
        for (i = 0; i < 10 && shell->m_menuItemSw[10][i]; i++)
            hierSetSwitch(shell->m_menuItemSw[10][i], i == sel);
    } else {
        for (i = 1; i < 10 && shell->m_menuItemSw[screen][i]; i++)
            hierSetSwitch(shell->m_menuItemSw[screen][i], i == sel);
    }
    return i - 1;
}
#else
INCLUDE_ASM("asm/nonmatchings/game/screen", screenPrepareDefault__Fii);
#endif
#ifdef NON_MATCHING
extern int wiLoop __asm__("loop.3044");
extern int wiHigh __asm__("high.3045");
extern int wiLow __asm__("low.3046");
extern int wiAlpha __asm__("alpha.3047");
extern int wiFadingIn __asm__("fadingIn.3048");
__asm__("#SNFIX_SMALL loop.3044");
__asm__("#SNFIX_SMALL high.3045");
__asm__("#SNFIX_SMALL low.3046");
__asm__("#SNFIX_SMALL alpha.3047");
__asm__("#SNFIX_SMALL fadingIn.3048");
extern int PlayerConfig;
extern int UnlockTitle;
__asm__("#SNFIX_SMALL PlayerConfig");
__asm__("#SNFIX_SMALL UnlockTitle");
extern char *screenTitles[];
extern char *screenInstructions[];
extern char D_006EED30[]; /* "PLAYER 1 SELECT" */
extern char D_006EED40[]; /* "PLAYER 2 SELECT" */
extern char D_006EED50[]; /* "   LOAD       CANCEL" */
extern char D_006EED68[]; /* "   SAVE       CANCEL" */
extern char D_006EEA98[]; /* "MONSTER UNLOCKS" */
extern char D_006EEAA8[]; /* "LEVEL UNLOCKS" */
extern char D_006EEAD8[]; /* "MINIGAME UNLOCKS" */
void fontSetColor(int font, int r, int g, int b, int a);
void fontSetSize(int font, int size);
void fontSetSpacing(int font, int spacing);
void fontSpritePrintCenteredXY(int font, int x, int y, char *str);
/* 14/167 palavras; o retail repete a impressão do charSelectStringOne em dois casos */
/* Draws the title (y 10) and the instruction line (y 205) of menu `screen` with the fade `alpha` (0..1, font alpha 0..128).
   The title screen (20) instead pulses the text between low and high; screen 16 draws nothing but the character-select prompt.
   Screen 4 names the player configuring the pad, 21 the kind of unlock, and 10 (load/save) picks its own instruction line. */
void screenWriteInstructions(int screen, float alpha)
{
    if (screen >= 22)
        screen = 0;
    if (screen != 16) {
        if (screen == 20) {
            if (wiFadingIn == 1) {
                if (wiLoop < wiHigh)
                    wiLoop++;
                else
                    wiFadingIn = 0;
            } else {
                if (wiLow < wiLoop)
                    wiLoop--;
                else
                    wiFadingIn = 1;
            }
            fontSetColor(0, 0x1E, 0x1E, 0x1E, wiLoop);
        } else {
            wiAlpha = (int)(alpha * 128.0f);
            fontSetColor(0, 0x1E, 0x1E, 0x1E, wiAlpha);
        }
        fontSetSize(0, 0xF);
        fontSetSpacing(0, 2);
        if (screen == 4)
            fontSpritePrintCenteredXY(0, 0x140, 0xA, PlayerConfig ? D_006EED40 : D_006EED30);
        else if (screen == 21) {
            char *title;

            if (UnlockTitle == 10)
                title = D_006EEAA8;
            else if (UnlockTitle == 11)
                title = D_006EEAD8;
            else
                title = D_006EEA98;
            fontSpritePrintCenteredXY(0, 0x140, 0xA, title);
        } else
            fontSpritePrintCenteredXY(0, 0x140, 0xA, screenTitles[screen]);
        fontSetSize(0, 0xF);
        fontSetSpacing(0, 1);
        if (screen != 10)
            fontSpritePrintCenteredXY(0, 0x140, 0xCD, screenInstructions[screen]);
        else if (currentSelection[10] == 8)
            fontSpritePrintCenteredXY(0, 0x140, 0xCD, D_006EED50);
        else if (currentSelection[10] == 9)
            fontSpritePrintCenteredXY(0, 0x140, 0xCD, D_006EED68);
        else
            fontSpritePrintCenteredXY(0, 0x140, 0xCD, screenInstructions[10]);
    }
    switch (screen) {
    case 3:
    case 6:
    case 7:
    case 8:
    case 14:
    case 17:
    case 18:
    case 19:
        if (charSelectStringOne)
            fontSpritePrintCenteredXY(0, 0x140, 0xA, charSelectStringOne);
        break;
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/game/screen", screenWriteInstructions__Fif);
#endif
#ifdef NON_MATCHING
float screenGetFontAlpha(void)
{
    return targetAlpha;
}
#else
INCLUDE_ASM("asm/nonmatchings/game/screen", screenGetFontAlpha__Fv);
#endif
#ifdef NON_MATCHING
void setButtonAlpha(float a)
{
    targetAlpha = a;
    screenSetButtonAlpha();
}
#else
INCLUDE_ASM("asm/nonmatchings/game/screen", setButtonAlpha__Ff);
#endif
/* the fade alpha of the menu text goes to the material (+0x4C) of each button prompt's object */
void screenSetButtonAlpha(void)
{
    char *o;

    o = *(char **)(*(char **)shell->m_buttonCs[0] + 0x20);
    if (o)
        *(float *)(o + 0x4C) = screenGetFontAlpha();
    o = *(char **)(*(char **)shell->m_buttonCs[2] + 0x20);
    if (o)
        *(float *)(o + 0x4C) = screenGetFontAlpha();
    o = *(char **)(*(char **)shell->m_buttonCs[1] + 0x20);
    if (o)
        *(float *)(o + 0x4C) = screenGetFontAlpha();
    o = *(char **)(*(char **)shell->m_buttonCs[3] + 0x20);
    if (o)
        *(float *)(o + 0x4C) = screenGetFontAlpha();
}
#ifdef NON_MATCHING
/* 105/108 palavras; a tabela de saltos do switch vai para o .rodata deste TU (a do retail, jtbl_006EEDD0, fica em asm) */
/* lays out the button prompts for screen `screen`: one centred prompt (screen 1), two (screen 21, wider apart), two
   on most screens (7 only when monster select slot 44 is set), none on screens 9, 16 and 20 */
void showButtons(int screen)
{
    _cs *b3 = shell->m_buttonCs[3];
    _cs *b2 = shell->m_buttonCs[2];
    _cs *b1 = shell->m_buttonCs[1];
    _cs *b0 = shell->m_buttonCs[0];

    switch (screen) {
    case 1:
        val1_x = -0.14f;
        uiAlignItemWithCamera(b3, val_z, -0.14f, val_y);
        hierSetCsDrawMe(b3, 1);
        hierSetCsDrawMe(b1, 0);
        hierSetCsDrawMe(b0, 0);
        hierSetCsDrawMe(b2, 0);
        break;
    case 2: case 3: case 4: case 5: case 6: case 7: case 8:
    case 10: case 11: case 12: case 13: case 14: case 15:
    case 17: case 18: case 19:
        if (screen == 7 && monsterSelectMode[44].state == 0)
            break;
        val2_x = 0.12f;
        val1_x = -0.31f;
        uiAlignItemWithCamera(b3, val_z, -0.31f, val_y);
        hierSetCsDrawMe(b3, 1);
        uiAlignItemWithCamera(b1, val_z, val2_x, val_y);
        hierSetCsDrawMe(b1, 1);
        hierSetCsDrawMe(b0, 0);
        hierSetCsDrawMe(b2, 0);
        break;
    case 21:
        val2_x = 0.28f;
        val1_x = -0.47f;
        uiAlignItemWithCamera(b3, val_z, -0.47f, val_y);
        hierSetCsDrawMe(b3, 1);
        uiAlignItemWithCamera(b1, val_z, val2_x, val_y);
        hierSetCsDrawMe(b1, 1);
        break;
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/game/screen", showButtons__Fi);
#endif
void killButtons(void)
{
    _cs *b0 = shell->m_buttonCs[0];
    _cs *b2 = shell->m_buttonCs[2];
    _cs *b3 = shell->m_buttonCs[3];
    _cs *b1 = shell->m_buttonCs[1];

    hierSetCsDrawMe(b2, 0);
    hierSetCsDrawMe(b0, 0);
    hierSetCsDrawMe(b3, 0);
    hierSetCsDrawMe(b1, 0);
}
void setButtonAlpha(float a);
void showButtons(int screen);
/* Runs frames until the unlocks are written to the memory card, then shows the menu buttons again. */
void saveUnlocks(void)
{
    ShellRtLoop loop;

    do {
        loop.startField(true);
        MonsterMc::global.updateUnlockSave();
        loop.endField(true);
    } while (!MonsterMc::global.m_done);
    setButtonAlpha(1.0f);
}
/* Runs frames until the options are written to the memory card. */
void saveSettings(void)
{
    ShellRtLoop loop;

    do {
        loop.startField(true);
        MonsterMc::global.updateSettingsSave();
        loop.endField(true);
    } while (!MonsterMc::global.m_done);
    showButtons(currScreen);
}
/* Runs frames until the options are read back from the memory card. */
void loadSettings(void)
{
    ShellRtLoop loop;

    do {
        loop.startField(true);
        MonsterMc::global.updateSettingsLoad();
        loop.endField(true);
    } while (!MonsterMc::global.m_done);
    loop.onEnd();
    showButtons(currScreen);
}
#ifdef NON_MATCHING
/* 7/19 words: address math scheduling */
int isSlotOpen(int group, int slot)
{
    SelectSlot *s = (SelectSlot *)((char *)monsterSelectMode + slot * 0x2C + group * 0xB0);

    if (s->state == 1 && s->taken == s->state)
        return 1;
    return 0;
}
#else
INCLUDE_ASM("asm/nonmatchings/game/screen", isSlotOpen__Fii);
#endif
#ifdef NON_MATCHING
/* Every one of the 12 groups of 4 select slots becomes available again. */
void resetSelectables(void)
{
    int g, i;

    for (g = 0; g < 12; g++)
        for (i = 3; i >= 0; i--)
            monsterSelectMode[g * 4 + i].taken = 1;
}
#else
INCLUDE_ASM("asm/nonmatchings/game/screen", resetSelectables__Fv);
#endif
void resetGameMode(void)
{
    int mode = shell->m_battleMode;

    if (mode != 0)
        shell->m_mode = mode;
}

#ifdef NON_MATCHING
/* 6/26 words: numModsLeft not gp-relative on first store */
int modelsLeft(int group)
{
    int i = 3;
    SelectSlot *s = (SelectSlot *)((char *)monsterSelectMode + (group - 1) * 0xB0);

    numModsLeft = 4;
    do {
        if (s->taken == 0 || s->state == 0)
            numModsLeft--;
        s = (SelectSlot *)((char *)s + 0x2C);
    } while (--i >= 0);
    return numModsLeft;
}
#else
INCLUDE_ASM("asm/nonmatchings/game/screen", modelsLeft__Fi);
#endif

/* Levels 10, 11, 14, 15, 25, 26 and 27 are locked until their flag in the select table is set; every other level is open. */
#ifdef NON_MATCHING
/* 9/56 words: retail keeps a jump table */
int levelUnlocked(int level)
{
    switch (level) {
    case 10:
        unlocked_3092 = *(int *)((char *)monsterSelectMode + 0x768) != 0;
        break;
    case 11:
        unlocked_3092 = *(int *)((char *)monsterSelectMode + 0x73C) != 0;
        break;
    case 14:
        unlocked_3092 = *(int *)((char *)monsterSelectMode + 0x710) != 0;
        break;
    case 15:
        unlocked_3092 = *(int *)((char *)monsterSelectMode + 0x6E4) != 0;
        break;
    case 25:
        unlocked_3092 = *(int *)((char *)monsterSelectMode + 0x794) != 0;
        break;
    case 26:
        unlocked_3092 = *(int *)((char *)monsterSelectMode + 0x7C0) != 0;
        break;
    case 27:
        unlocked_3092 = *(int *)((char *)monsterSelectMode + 0x7EC) != 0;
        break;
    default:
        unlocked_3092 = 1;
        break;
    }
    return unlocked_3092;
}
#else
INCLUDE_ASM("asm/nonmatchings/game/screen", levelUnlocked__Fi);
#endif
#ifdef NON_MATCHING
/* Steps player `player`'s choice in list `list` by `dir` (wrapping inside the selectable range) until a model with a free copy is found.
   The range depends on the two unlock flags: 1..9 or 2..9 by default, up to 10 once the bonus monster is open. */
void findAvailableMonster(int dir, int player, int list)
{
    int *cur;
    int a = *(int *)((char *)monsterSelectMode + 4);
    int b = *(int *)((char *)monsterSelectMode + 0x634);

    if (a == 0 && b == 0) {
        selBeginning = 2;
        selEnding = 9;
        numSelectablesOne = 9;
        numSelectablesTwo = 9;
    } else if (a == 1 && b == 0) {
        selBeginning = a;
        selEnding = 9;
        numSelectablesOne = 9;
        numSelectablesTwo = 9;
    } else if (a == 0 && b == 1) {
        numSelectablesTwo = 9;
        selBeginning = 2;
        selEnding = 10;
        numSelectablesOne = 9;
    } else if (a == 1 && b == a) {
        selBeginning = b;
        selEnding = 10;
        numSelectablesOne = 10;
        numSelectablesTwo = 10;
    }
    cur = (int *)((char *)currentSelection + player * 0x58 + list * 4);
    do {
        *cur += dir;
        if (*cur < selBeginning)
            *cur = selEnding;
        if (selEnding < *cur)
            *cur = selBeginning;
    } while (modelsLeft(*cur) == 0);
}
#else
INCLUDE_ASM("asm/nonmatchings/game/screen", findAvailableMonster__Fiii);
#endif
#ifdef NON_MATCHING
/* Clears the ten model choices kept in the shell (0x2BE8..0x2C0C). */
void resetModels(void)
{
    int i;

    for (i = 9; i >= 0; i--)
        *(int *)((char *)shell + 0x2BE8 + i * 4) = 0;
}
#else
INCLUDE_ASM("asm/nonmatchings/game/screen", resetModels__Fv);
#endif
INCLUDE_ASM("asm/nonmatchings/game/screen", __static_initialization_and_destruction_0_001A6668);
INCLUDE_ASM("asm/nonmatchings/game/screen", _GLOBAL_$I$NamecardDistance);
