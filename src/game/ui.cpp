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
extern unsigned g_shellOddEven;
extern int D_00731840;                /* bss word cleared when the movie loop starts; no other reader */

struct _fvector;
void viewSetBgColor(unsigned long r, unsigned long g, unsigned long b, unsigned long a);
void viewSetAmbient(float r, float g, float b);
void viewGetCenter(int view, int *x, int *y);
void mathfRotMatrixRPH(float (*m)[4], _fvector *rph);
int inputGetInput(int mask, int pad);
void uiIntro(void);
void uiOutro(void);
void playMovie(void);
void updateMpegMovieDecoding(bool decode);
enum PMODESetting { PMODE_SETTING_DUMMY };
void setPMODE(PMODESetting mode);
extern "C" void sceGsSetHalfOffset(void *draw, short centerX, short centerY, short halfOff);
extern "C" void sceGsSetHalfOffset2(void *draw, short centerX, short centerY, short halfOff);

/* untuned: 21/297 words (the retail clears the packet with por/sq; register allocation); tools/difftest.py 60/60 (modes 0..2, 1..80
 * frames; the stack packet and the DMA registers are not compared, checked by hand) */
/* Movie loop of the front end: mode 0 plays the intro movies (uiIntro), mode 1 the outro ones (uiOutro), until one of them sets
 * exitUi. Each frame it lets pad 0 skip the playing movie (button mask 8; the fourth movie only after 600 frames), clears the screen
 * with a one-register GIF packet that also flips SCANMSK between 2 and 3 (odd and even lines), feeds the MPEG decoder and blinks the
 * missing-controller message from frame 61 on. Always returns 1. */
int uiMain(int mode)
{
    unsigned long pkt[6] __attribute__((aligned(16)));
    int skip = 0;
    int scanMask;
    char *viewCs;
    char *db;
    int cx, cy;

    startTime = timerGetFieldCount();
    whichPlayerIsPicking = 0;
    viewCs = (char *)viewGetViewCs(0);
    exitUi = 0;
    viewSetBgColor(0, 0, 0, 0);
    viewSetAmbient(0.0f, 0.0f, 0.0f);
    movieEnable = 1;
    currScreen = 0;
    select = 0;
    gIntroMoviesHavePlayed = 0;
    mathfRotMatrixRPH((float (*)[4])(viewCs + 0x20), (_fvector *)(viewCs + 0x60));
    uiInitScreens((_cs *)viewCs);
    D_00731840 = 0;
    g_frame = 0;
    g_fadeFrameOffset = 0;
    g_shellOddEven = sceGsSyncV(0) == 0;
    scanMask = g_shellOddEven ? 2 : 3;
    db = (char *)viewGetDb(0);
    FlushCache(0);
    sceGsSyncPath(0, 0);
    ieGsPutDrawEnv(db, 0, 1);
    ieGsPutDrawEnv(db, 1, 1);
    sceGsSyncPath(0, 0);
    pkt[0] = 0x70000002;          /* DMA tag: end, two quadwords */
    pkt[1] = 0;
    pkt[2] = 0x1000000000008001;  /* GIF tag: NLOOP 1, EOP, NREG 1 */
    pkt[3] = 0xE;                 /* A+D */
    pkt[4] = scanMask;
    pkt[5] = 0x22;                /* SCANMSK */
    while (exitUi == 0) {
        if (select == 1) {
            currScreen = nextScreen;
            nextScreen = -1;
            select = 0;
        }
        inputUpdate();
        if (movieRequested != 0 && (nextMovie != 4 || (float)g_frame > 600.0f))
            skip = inputGetInput(8, 0) != 0;
        if (mode == 0)
            uiIntro();
        else if (mode == 1)
            uiOutro();
        else
            printf("We're pretty screwed up in uiMain here.\n");
        db = (char *)viewGetDb(0);
        *(unsigned char *)&pkt[4] = scanMask;
        FlushCache(0);
        sceGsSyncPath(0, 0);
        *(volatile unsigned *)0x1000A020 = 0;                               /* GIF channel QWC */
        *(volatile unsigned *)0x1000A030 = (unsigned)pkt & 0x7FFFFFFF;    /* TADR */
        *(volatile unsigned *)0x1000E010 = 4;                               /* D_STAT: clear channel 2 */
        *(volatile unsigned *)0x1000A000 = 0x145;                           /* CHCR: chain, start */
        sceGsSyncPath(0, 0);
        pMode = 1;
        playMovie();
        if (movieRequested != 0) {
            updateMpegMovieDecoding(!skip);
            skip = 0;
        }
        FlushCache(0);
        sceGsSyncPath(0, 0);
        setPMODE((PMODESetting)pMode);
        ieGsPutDrawEnv(db, 0, 1);
        sceGsSyncPath(0, 0);
        viewUpdate(0);
        if (g_frame >= 0x3D && !inputIsCtlAvailable(0) && (g_frame & 0x7F) < 0x60) {
            fontSetSize(0, 18);
            fontSetColor(0, 0xFF, 0x3C, 0x3C, 0x80);
            fontSpritePrintCenteredXY(0, 320, 40, D_006F1C90);
        }
        fontDmaFontData();
        sceGsSyncPath(0, 0);
        timerEndOfRealTimeProcessing();
        viewGetCenter(0, &cx, &cy);
        sceGsSetHalfOffset((g_frame & 1) ? db + 0x200 : db + 0x90, cx, cy, g_shellOddEven);
        sceGsSetHalfOffset2((g_frame & 1) ? db + 0x280 : db + 0x110, cx, cy, g_shellOddEven);
        scanMask = scanMask == 2 ? 3 : 2;
        g_frame++;
        timer1Handler(&g_shellOddEven);
    }
    sceGsSyncPath(0, 0);
    return 1;
}
#else
INCLUDE_ASM("asm/nonmatchings/game/ui", uiMain__Fi);
#endif
INCLUDE_ASM("asm/nonmatchings/game/ui", D_006F1C90);
#ifdef NON_MATCHING

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
#ifdef NON_MATCHING
extern int needIntro;
extern int needOutro;
void playMpegMovieWithDB(char *name, bool b);
/* untuned: 1/254 words, 0x3F8 bytes vs 0x3E8 (the retail shares the tail of cases 4 and 20); tools/difftest.py 296/300 (nextMovie
 * 0..35, random flags; the 4 skipped seeds fault only in the retail run, cases 21 and 29 with needOutro clear, while 24 and 28 on the
 * same path agree) */
/* Starts the movie nextMovie names, once per movieEnable, and moves nextMovie along its chain: 3 the intro (then 4, the main intro),
 * 5..8 the four act intros, 21..30 each monster's "_p" movie (then 9..18, its "_g" one, then 19, the credits); 20 is the main intro
 * again, under needOutro. 3..8 need needIntro, the others needOutro; when the flag is clear the chain still advances. Any other
 * value only resets nextMovie to 0. */
void playMovie(void)
{
    if (movieEnable != 1)
        return;
    switch (nextMovie) {
    case 3:
        if (needIntro)
            playMpegMovieWithDB("mov/intro_m.pss", true);
        nextMovie = 4;
        break;
    case 4:
        if (needIntro)
            playMpegMovieWithDB("mov/int_main.pss", true);
        nextMovie = 0;
        break;
    case 5:
        if (needIntro)
            playMpegMovieWithDB("mov/int_act1.pss", false);
        nextMovie = 0;
        break;
    case 6:
        if (needIntro)
            playMpegMovieWithDB("mov/int_act2.pss", false);
        nextMovie = 0;
        break;
    case 7:
        if (needIntro)
            playMpegMovieWithDB("mov/int_act3.pss", false);
        nextMovie = 0;
        break;
    case 8:
        if (needIntro)
            playMpegMovieWithDB("mov/int_act4.pss", false);
        nextMovie = 0;
        break;
    case 21:
        if (needOutro)
            playMpegMovieWithDB("mov/monkey_p.pss", true);
        nextMovie = 9;
        break;
    case 22:
        if (needOutro)
            playMpegMovieWithDB("mov/lizard_p.pss", true);
        nextMovie = 10;
        break;
    case 23:
        if (needOutro)
            playMpegMovieWithDB("mov/mantis_p.pss", true);
        nextMovie = 11;
        break;
    case 24:
        if (needOutro)
            playMpegMovieWithDB("mov/dragon_p.pss", true);
        nextMovie = 12;
        break;
    case 25:
        if (needOutro)
            playMpegMovieWithDB("mov/robot_p.pss", true);
        nextMovie = 13;
        break;
    case 26:
        if (needOutro)
            playMpegMovieWithDB("mov/rock_p.pss", true);
        nextMovie = 14;
        break;
    case 27:
        if (needOutro)
            playMpegMovieWithDB("mov/shogun_p.pss", true);
        nextMovie = 15;
        break;
    case 28:
        if (needOutro)
            playMpegMovieWithDB("mov/lava_p.pss", true);
        nextMovie = 16;
        break;
    case 29:
        if (needOutro)
            playMpegMovieWithDB("mov/alien_p.pss", true);
        nextMovie = 17;
        break;
    case 30:
        if (needOutro)
            playMpegMovieWithDB("mov/energy_p.pss", true);
        nextMovie = 18;
        break;
    case 9:
        if (needOutro)
            playMpegMovieWithDB("mov/monkey_g.pss", true);
        nextMovie = 19;
        break;
    case 10:
        if (needOutro)
            playMpegMovieWithDB("mov/lizard_g.pss", true);
        nextMovie = 19;
        break;
    case 11:
        if (needOutro)
            playMpegMovieWithDB("mov/mantis_g.pss", true);
        nextMovie = 19;
        break;
    case 12:
        if (needOutro)
            playMpegMovieWithDB("mov/dragon_g.pss", true);
        nextMovie = 19;
        break;
    case 13:
        if (needOutro)
            playMpegMovieWithDB("mov/robot_g.pss", true);
        nextMovie = 19;
        break;
    case 14:
        if (needOutro)
            playMpegMovieWithDB("mov/rock_g.pss", true);
        nextMovie = 19;
        break;
    case 15:
        if (needOutro)
            playMpegMovieWithDB("mov/shogun_g.pss", true);
        nextMovie = 19;
        break;
    case 16:
        if (needOutro)
            playMpegMovieWithDB("mov/lava_g.pss", true);
        nextMovie = 19;
        break;
    case 17:
        if (needOutro)
            playMpegMovieWithDB("mov/alien_g.pss", true);
        nextMovie = 19;
        break;
    case 18:
        if (needOutro)
            playMpegMovieWithDB("mov/energy_g.pss", true);
        nextMovie = 19;
        break;
    case 19:
        if (needOutro)
            playMpegMovieWithDB("mov/credits.pss", true);
        nextMovie = 0;
        break;
    case 20:
        if (needOutro)
            playMpegMovieWithDB("mov/int_main.pss", true);
        nextMovie = 0;
        break;
    default:
        nextMovie = 0;
        break;
    }
    movieEnable = 0;
}
#else
INCLUDE_ASM("asm/nonmatchings/game/ui", playMovie__Fv);
#endif
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
