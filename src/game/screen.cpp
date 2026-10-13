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
};
extern int prevScreen;
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
INCLUDE_ASM("asm/nonmatchings/game/screen", screenMain__Fv);
INCLUDE_ASM("asm/nonmatchings/game/screen", updateSelectSwitches__Fiiii);
INCLUDE_ASM("asm/nonmatchings/game/screen", updateSelectSwitches3P__Fiiiiii);
INCLUDE_ASM("asm/nonmatchings/game/screen", updateSelectSwitches4P__Fiiiiiiii);
INCLUDE_ASM("asm/nonmatchings/game/screen", updateSelectSwitchesMG__Fiiii);
INCLUDE_ASM("asm/nonmatchings/game/screen", updateSelectSwitchesAI__Fii);
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
INCLUDE_ASM("asm/nonmatchings/game/screen", initSelectSwitchesMG__Fv);
INCLUDE_ASM("asm/nonmatchings/game/screen", screenGameModes1P__Fv);
INCLUDE_ASM("asm/nonmatchings/game/screen", D_006EE2F0);
INCLUDE_ASM("asm/nonmatchings/game/screen", screenCharSelect1P__Fv);
INCLUDE_ASM("asm/nonmatchings/game/screen", screenSelect1AI__Fv);
INCLUDE_ASM("asm/nonmatchings/game/screen", screenCtlrCfg__Fv);
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
            printf("Two Player Game Modes Screen has screwed up item indexing! - We think we selected #%i
", sel);
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
INCLUDE_ASM("asm/nonmatchings/game/screen", jtbl_006EE9F0);
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
INCLUDE_ASM("asm/nonmatchings/game/screen", screenLevelSelect__Fv);
INCLUDE_ASM("asm/nonmatchings/game/screen", screenLoadSave__Fv);
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
        printf("Trying to set a slider on a bar that doesn't exist!!
");
        break;
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/game/screen", setSlider__Fif);
#endif
INCLUDE_ASM("asm/nonmatchings/game/screen", screenOptions__Fv);
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
            printf("Number of AI set to %i
", 0);
            changeScreen(0xC, 3, 1, 0, true);
        } else if (sel >= 1 && sel <= 3) {
            shell->m_numAIs = sel;
            printf("Number of AI set to %i
", sel);
            changeScreen(0xC, 0xF, 1, 0, true);
        } else {
            printf("One Player Free for All Options Screen has screwed up item indexing! - We think we selected #%i
", sel);
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
            printf("Two Player Free for All Options Screen has screwed up item indexing! - We think we selected #%i
", sel);
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
class ShellRtLoop {
public:
    char pad0[0x24];

    void startField(bool b);
    void endField(bool skipDraw);
};
class MonsterMc {
public:
    int pad0;
    int m_bootLoaded; /* 0x4: the boot-time memory card load finished */

    void updateBootLoad(void);
    static MonsterMc global;
};
extern int clearBeforeFontDMA;
extern "C" void snd_SetMasterVolume(int which, int vol);
class TheGame {
public:
    void SetVibration(int pad, bool on);
};
extern TheGame *game;
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
        if (MonsterMc::global.m_bootLoaded) {
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
INCLUDE_ASM("asm/nonmatchings/game/screen", screenTransition__Fb);
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
extern int lastScreen;
extern int oldScreens[16];
extern int currAnimationIndex;
__asm__("#SNFIX_SMALL currAnimationIndex");
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
INCLUDE_ASM("asm/nonmatchings/game/screen", saveUnlocks__Fv);
INCLUDE_ASM("asm/nonmatchings/game/screen", saveSettings__Fv);
INCLUDE_ASM("asm/nonmatchings/game/screen", loadSettings__Fv);
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
