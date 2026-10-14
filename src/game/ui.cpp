#include "common.h"
#include "game/shell.h"

extern "C" int printf(const char *, ...);
extern "C" void snd_StopAllSounds(void);

struct UserintReturn {
    int pending;  /* set by userintReturnToShell */
    int frames;   /* frames left before the menu really returns */
    int result;   /* what the shell is asked to do next */
};
extern UserintReturn gUserintReturn;
extern int pMode;
extern int continueDecoding;
extern int oldScreens[16];
extern int clutterDelay;
extern int clutterFrame;
extern int testPattern;
extern int whichPlayerIsPicking;
extern int incogTimer;
extern int currentMonster;
extern int lastMonster;
extern int currentLevel;
extern int uiWhichPad;
extern int clutterDelayCnt;
extern int chosenAI[10];
extern int AItoChoose;
extern int movieEnable;
extern int select;
extern int goLoadLevel;
extern int exitUi;
extern int gManualMonsterChangeHappened;
extern int gRealWorldEpNode;
extern int lastTicCount;
extern int thisTicCount;
extern int startTime;
extern int frameTime;
extern int movieTime;
extern int innerFrameTime;
extern int movieRequested;
extern int movieIsCompletelyFinished;
extern int nextMovie;
extern int nextScreen;
extern int gIntroMoviesHavePlayed;
extern int D_006F8D08;
extern int movieHasStarted_2745 __asm__("movieHasStarted.2745");
extern int movieHasStarted_2749 __asm__("movieHasStarted.2749");

INCLUDE_ASM("asm/nonmatchings/game/ui", uiMain__Fi);
INCLUDE_ASM("asm/nonmatchings/game/ui", D_006F1C90);
#ifdef NON_MATCHING
#define SYM(n) __asm__(n)
struct _cs;
class TheGame;
extern TheGame *game;
extern "C" int FlushCache(int mode);
extern "C" int sceGsSyncV(int mode);
extern "C" void sceGsSyncPath(int mode, int timeout);
extern "C" void hierSetCamera(int a, int b, int c);
int viewGetNumViews(void);
_cs *viewGetViewCs(int view);
void *viewGetDb(int view);
void viewUpdate(int view);
int timerGetFieldCount(void);
void timerSetFrameStart(void);
void timerGetCurTics(void);
void timerEndOfRealTimeProcessing(void);
void timer1Handler(unsigned *p);
void timerWaitForMinUpdateRate(unsigned *p);
void animationRunGlobal(void);
void inputUpdate(void);
int inputIsCtlAvailable(int pad);
void fontSetSize(int font, int size);
void fontSetColor(int font, int r, int g, int b, int a);
void fontSpritePrintCenteredXY(int font, int x, int y, char *str);
void fontDmaFontData(void);
void hierSetCsDrawMe(_cs *cs, unsigned char on);
void hier(int view, int pass);
void rtWaitForVu1(void);
void uiInitScreens(_cs *cs);
void uiFadeScreen(unsigned amount);
void letterboxDmaData(float alpha);
int userintTimeToReturnToShell(void);
void userintReturnToShell(int result, int frames, int extra);
void changeScreen(int to, int from, int dir, int sfx, bool b);
void screenTransition(bool first);
void screenWriteInstructions(int screen, float alpha);
float screenGetFontAlpha(void);
void screenSetButtonAlpha(void);
void killButtons(void);
void moviesReleaseScreenObject(void);
void screenPrepare(void);
void screenMain(void);
void screenGameModes1P(void);
void screenCharSelect1P(void);
void screenCtlrCfg(void);
void screenGameModes2P(void);
void screenCharSelect2P(void);
void screenMinigames2P(void);
void screenLevelSelect(void);
void screenLoadSave(void);
void screenOptions(void);
void screenElimOptions2P(void);
void screenFreeForAllOptions1P(void);
void screenFreeForAllOptions2P(void);
void screenMGSelect(void);
void screenBestOf(void);
void screenSelect1AI(void);
void screenCharSelect3P(void);
void screenCharSelect4P(void);
void screenWaitForStart(void);
void screenUnlocker(void);
void motionBlurInit(int views, float a, float b) SYM("init__12MotionBlurAAiff");
void resetcomSetFileName(void *r, char *name, bool b) SYM("setFileName__8resetcomPcb");
void resetcomInit(void *r) SYM("init__8resetcom");
void resetcomForceRead(void *r) SYM("forceRead__8resetcom");
void taskManagerInit(void *m, unsigned n) SYM("init__11TaskManagerUi");
void taskManagerUpdate(void *m) SYM("update__11TaskManager");
void movieUpdate(void *m, bool decode) SYM("Update__6CMovieb");
void shellSoundUpdate(void *s) SYM("updateShellSound__10ShellSound");
int ieGs2CircuitOnOff(void) SYM("ieGs2CircuitOnOff__24IncognitoEntertainmentGSv");
void ieGsPutDrawEnv(void *db, int parity, int one) SYM("ieGsPutDrawEnv__24IncognitoEntertainmentGSPQ224IncognitoEntertainmentGS11ieGsDBuffDcii");
void ieGsSetHalfOffsetDc(int view, void *db, unsigned frame, int field, int numViews) SYM("ieGsSetHalfOffsetDc__24IncognitoEntertainmentGSiPQ224IncognitoEntertainmentGS11ieGsDBuffDcUiii");
void ieGsSwapDBuffDc(void *db, int parity, int circuit, int one) SYM("ieGsSwapDBuffDc__24IncognitoEntertainmentGSPQ224IncognitoEntertainmentGS11ieGsDBuffDciii");

extern unsigned g_frame;
extern unsigned g_fadeFrameOffset;
extern int gFieldCounter;
extern int doTweaks;
extern int tweakLastFrame;
extern int gUseUnifiedView;
extern int currScreen;
extern int prevScreen;
extern int initialAnimation;
extern int betweenScreens;
extern int clearBeforeFontDMA;
extern int currentSelection[];
extern char myMovie[];
extern char userintResetObj[];
extern char taskManagerGlobal[] SYM("_11TaskManager$global");
extern char D_006F84D8[];             /* file name of the userint reset object */
extern char D_006F1C90[];             /* message shown while a needed controller is missing */
extern char GameLevelNames_006F2208[][10];

/* untuned: 167/475 words (register allocation and branch layout); tools/difftest.py 60/60 (1..3 frames, every screen) */
/* Main loop of the front-end menus: each frame runs the screen in currScreen (or the transition between two screens), draws
 * the menu scene and blinks a message while a needed controller is missing (pad 0, or pads 0 and 1 on the multiplayer
 * screens). Screen 99 is "start the level": it logs the setup and asks to return to the shell. Returns the result
 * userintReturnToShell was given. */
int userintMain(void)
{
    int first = 1;
    unsigned oddEven;
    _cs *viewCs;
    void *db;

    gFieldCounter = 0;
    doTweaks = 0;
    viewGetNumViews();
    viewCs = viewGetViewCs(0);
    tweakLastFrame = 0;
    motionBlurInit(viewGetNumViews(), 40.0f, 0.0f);
    startTime = timerGetFieldCount();
    gUserintReturn.pending = 0;
    g_frame = 0;
    g_fadeFrameOffset = 0;
    oddEven = sceGsSyncV(0) == 0;
    gUseUnifiedView = 0;
    resetcomSetFileName(userintResetObj, D_006F84D8, false);
    resetcomInit(userintResetObj);
    resetcomForceRead(userintResetObj);
    uiInitScreens(viewCs);
    currentMonster = 0;
    taskManagerInit(taskManagerGlobal, 100);
    timerSetFrameStart();
    while (!userintTimeToReturnToShell()) {
        int noPad = 0;

        viewGetNumViews();
        animationRunGlobal();
        inputUpdate();
        if ((currScreen >= 2 && shell->m_numPlayers >= 2) || (currScreen == 1 && currentSelection[1] == 2)) {
            if (!inputIsCtlAvailable(0))
                noPad = 1;
            else if (!inputIsCtlAvailable(1))
                noPad = 1;
        } else if (gUserintReturn.pending == 0) {
            noPad = inputIsCtlAvailable(0) ^ 1;
        }
        if (noPad && (g_frame & 0x7F) < 0x60) {
            fontSetSize(0, 18);
            fontSetColor(0, 0xFF, 0x3C, 0x3C, 0x80);
            fontSpritePrintCenteredXY(0, 320, 180, D_006F1C90);
        }
        if (gUserintReturn.pending == 0) {
            if (initialAnimation != 0) {
                continueDecoding = 1;
                hierSetCsDrawMe(shell->m_menuCs, 1);
                changeScreen(prevScreen, currScreen, 1, initialAnimation, false);
                initialAnimation = 0;
            }
            if (betweenScreens != 0) {
                screenTransition(first);
                first = 0;
            } else {
                if (gUserintReturn.pending == 0)
                    screenWriteInstructions(currScreen, 1.0f);
                switch (currScreen) {
                case 0: screenPrepare(); break;
                case 1: screenMain(); break;
                case 2: screenGameModes1P(); break;
                case 3: screenCharSelect1P(); break;
                case 4: screenCtlrCfg(); break;
                case 5: screenGameModes2P(); break;
                case 6: screenCharSelect2P(); break;
                case 7: screenMinigames2P(); break;
                case 8: screenLevelSelect(); break;
                case 9: screenLoadSave(); break;
                case 10: screenOptions(); break;
                case 11: screenElimOptions2P(); break;
                case 12: screenFreeForAllOptions1P(); break;
                case 13: screenFreeForAllOptions2P(); break;
                case 14: screenMGSelect(); break;
                case 15: screenBestOf(); break;
                case 16: changeScreen(16, 20, 1, 1, true); break;
                case 17: screenSelect1AI(); break;
                case 18: screenCharSelect3P(); break;
                case 19: screenCharSelect4P(); break;
                case 20: screenWaitForStart(); break;
                case 21: screenUnlocker(); break;
                case 99:
                    killButtons();
                    shell->FadeScreen(0, false, 0, 0, 0, 0, 0x80, 2);
                    shell->DisplayLoadBackground(false);
                    printf("------------------------------------------------------------\n");
                    printf("| Starting level %s with %i Player(s), %i AI\n", GameLevelNames_006F2208[shell->m_levelNum],
                           shell->m_numPlayers, shell->m_numAIs);
                    printf("| Players %i %i %i %i  -  AI %i %i %i %i\n", shell->m_monsterSel[0], shell->m_monsterSel[1],
                           shell->m_monsterSel[2], shell->m_monsterSel[3], shell->m_monsterSel[4], shell->m_monsterSel[5],
                           shell->m_monsterSel[6], shell->m_monsterSel[7]);
                    printf("------------------------------------------------------------\n");
                    oldScreens[0] = 1;
                    oldScreens[1] = 1;
                    oldScreens[2] = 0;
                    currScreen = 1;
                    initialAnimation = 1;
                    prevScreen = 1;
                    continueDecoding = 0;
                    moviesReleaseScreenObject();
                    userintReturnToShell(0, 4, g_frame);
                    break;
                default:
                    printf("We are on screen %i - it doesn't exist!!\n", currScreen);
                    break;
                }
            }
        }
        taskManagerUpdate(taskManagerGlobal);
        db = viewGetDb(0);
        FlushCache(0);
        sceGsSyncPath(0, 0);
        if (g_frame < 4)
            ieGsPutDrawEnv(db, (g_frame ^ 1) & 1, 1);
        else
            ieGsSwapDBuffDc(db, g_frame & 1, ieGs2CircuitOnOff(), 1);
        sceGsSyncPath(0, 0);
        ieGsSetHalfOffsetDc(0, db, g_frame, oddEven, *(int *)((char *)game + 0x1203D8));
        viewUpdate(0);
        movieUpdate(myMovie, continueDecoding != 0);
        hier(0, 0);
        rtWaitForVu1();
        timerGetCurTics();
        hierSetCamera(0, 0, 0);
        timerGetCurTics();
        sceGsSyncPath(0, 0);
        if (gUserintReturn.pending == 0) {
            letterboxDmaData(1.0f);
            if (gUserintReturn.pending == 0) {
                letterboxDmaData(1.0f - screenGetFontAlpha());
                screenSetButtonAlpha();
                sceGsSyncPath(0, 0);
            }
        }
        hier(0, 1);
        rtWaitForVu1();
        shellSoundUpdate((char *)shell + 0x2918);
        if (clearBeforeFontDMA != 0)
            ieGsPutDrawEnv(db, (g_frame ^ 1) & 1, 1);
        fontDmaFontData();
        sceGsSyncPath(0, 0);
        if (g_fadeFrameOffset < 0x81)
            uiFadeScreen(g_fadeFrameOffset);
        g_fadeFrameOffset++;
        g_frame++;
        timerEndOfRealTimeProcessing();
        timer1Handler(&oddEven);
        timerWaitForMinUpdateRate(&oddEven);
    }
    gUserintReturn.pending = 0;
    return gUserintReturn.result;
}
#undef SYM
#else
INCLUDE_ASM("asm/nonmatchings/game/ui", userintMain__Fv);
#endif
#ifdef NON_MATCHING
/* 2/12 words: store order */
void userintReturnToShell(int result, int frames, int extra)
{
    if (gUserintReturn.pending == 0) {
        gUserintReturn.frames = frames + (extra & 1);
        gUserintReturn.pending = 1;
        gUserintReturn.result = result;
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/game/ui", userintReturnToShell__Fiii);
#endif
#ifdef NON_MATCHING
/* 5/12 words: branch layout */
int userintTimeToReturnToShell(void)
{
    if (gUserintReturn.pending == 0 || --gUserintReturn.frames >= 0)
        return 0;
    return 1;
}
#else
INCLUDE_ASM("asm/nonmatchings/game/ui", userintTimeToReturnToShell__Fv);
#endif
INCLUDE_ASM("asm/nonmatchings/game/ui", uiFadeScreen__FUi);
#ifdef NON_MATCHING
void hierSetCsDrawMe(_cs *cs, unsigned char on);
/* 71/146 palavras; alocação dos índices dos laços */
/* Binds the menu animations of the shell (screen transitions 1..11, the two select reels and their ten positions each) and pauses
   them, clears the monster flags and makes the six starting monsters selectable. `cs` is not used. */
void uiInitScreens(_cs *cs)
{
    int i, r;

    for (i = 1; i < 12; i++) {
        animationGetHandle(&shell->m_screenAnim[i], 0x270E, 0, i);
        animationPause(shell->m_screenAnim[i]);
    }
    for (r = 0; r < 2; r++) {
        animationGetHandle(&shell->m_reelSpinAnim[r], 0x2585 + r, 0, 0);
        animationPause(shell->m_reelSpinAnim[r]);
        for (i = 0; i < 10; i++) {
            animationGetHandle(&shell->m_reelAnim[r][i], 0x259E + r * 10 + i, 0, 0);
            animationPause(shell->m_reelAnim[r][i]);
        }
    }
    shell->m_monsterFlags[0] = 0;
    shell->m_monsterFlags[2] = 0;
    shell->m_monsterFlags[1] = 0;
    shell->EnableMonsterSelection(0xA0);
    shell->EnableMonsterSelection(0x40);
    shell->EnableMonsterSelection(0x60);
    shell->EnableMonsterSelection(0x140);
    shell->EnableMonsterSelection(0x20);
    shell->EnableMonsterSelection(0x120);
    hierSetCsDrawMe(shell->m_cs5C, 1);
}
#else
INCLUDE_ASM("asm/nonmatchings/game/ui", uiInitScreens__FP3_cs);
#endif
#ifdef NON_MATCHING
/* 7/36 words: branch layout */
void uiIntro(void)
{
    if (movieRequested != 0)
        movieHasStarted_2745 = 1;
    if (movieHasStarted_2745 != 0 && movieIsCompletelyFinished != 0) {
        if (nextMovie == 0) {
            nextScreen = 0;
            select = 1;
            snd_StopAllSounds();
            exitUi = 1;
            movieHasStarted_2745 = 0;
            gIntroMoviesHavePlayed = 1;
            return;
        }
        movieHasStarted_2745 = 0;
        movieEnable = 1;
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/game/ui", uiIntro__Fv);
#endif
#ifdef NON_MATCHING
/* 7/36 words: branch layout */
void uiOutro(void)
{
    if (movieRequested != 0)
        movieHasStarted_2749 = 1;
    if (movieHasStarted_2749 != 0 && movieIsCompletelyFinished != 0) {
        if (nextMovie == 0) {
            nextScreen = 0;
            select = 1;
            snd_StopAllSounds();
            exitUi = 1;
            movieHasStarted_2749 = 0;
            gIntroMoviesHavePlayed = 1;
            return;
        }
        movieHasStarted_2749 = 0;
        movieEnable = 1;
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/game/ui", uiOutro__Fv);
#endif
INCLUDE_ASM("asm/nonmatchings/game/ui", playMovie__Fv);
/* Moves currentMonster (wrapping 0..15) by `step` until it names a monster that exists and nobody has chosen yet. */
#ifdef NON_MATCHING
/* 14/56 words: loop shape */
void uiValidateMonster(int step)
{
    int ok = 0;

    do {
        if (currentMonster < 0)
            currentMonster = 0xF;
        if (currentMonster >= 0x10)
            currentMonster = 0;
        if (shell->MonsterExists(currentMonster) != 0 && shell->MonsterIsChosen(currentMonster) == 0) {
            ok = 1;
        } else {
            if (shell->MonsterIsChosen(currentMonster) != 0)
                gManualMonsterChangeHappened = 1;
            currentMonster += step;
            if (step == 0)
                printf("Big fuckup!! we need to go somewhere, not sit here going nowhere.\n");
        }
    } while (ok == 0);
    D_006F8D08 = currentMonster;
}
#else
INCLUDE_ASM("asm/nonmatchings/game/ui", uiValidateMonster__Fi);
#endif
INCLUDE_ASM("asm/nonmatchings/game/ui", uiAlignItemWithCamera__FP3_csfff);
INCLUDE_ASM("asm/nonmatchings/game/ui", letterboxDmaData__Ff);
INCLUDE_ASM("asm/nonmatchings/game/ui", blackenScreen__Fv);
#ifdef NON_MATCHING
/* 4/78 words: retail clears the arrays with pointer loops */
void uiInit(void)
{
    int i;

    pMode = 1;
    continueDecoding = 1;
    for (i = 15; i >= 0; i--)
        oldScreens[i] = 0;
    clutterDelay = 2;
    clutterFrame = -1;
    testPattern = 1;
    whichPlayerIsPicking = 0;
    incogTimer = 0;
    currentMonster = 0;
    lastMonster = 0;
    currentLevel = 1;
    uiWhichPad = -1;
    clutterDelayCnt = 0;
    for (i = 9; i >= 0; i--)
        chosenAI[i] = 0;
    AItoChoose = 0;
    movieEnable = 1;
    select = 0;
    goLoadLevel = 0;
    exitUi = 0;
    gManualMonsterChangeHappened = 0;
    gRealWorldEpNode = 0;
    lastTicCount = 0;
    thisTicCount = 0;
    startTime = 0;
    frameTime = 0;
    movieTime = 0;
    innerFrameTime = 0;
}
#else
INCLUDE_ASM("asm/nonmatchings/game/ui", uiInit__Fv);
#endif
INCLUDE_ASM("asm/nonmatchings/game/ui", __static_initialization_and_destruction_0_001DAE40);
INCLUDE_ASM("asm/nonmatchings/game/ui", _GLOBAL_$I$myMovie);
INCLUDE_ASM("asm/nonmatchings/game/ui", GameLevelNames_006F2208);
