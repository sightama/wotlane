#include "common.h"
#include "engine.h"


class ShellRtLoop {
public:
    void *m_db;     /* 0x0: GS double buffer of view 0 (viewGetDb), set every endField */
    char pad4[0x20 - 0x4];
    int started;

    void startField(bool b);
    void endField(bool skipDraw);
    void onStart(void);
    void onEnd(void);
};

void ShellRtLoop::startField(bool b)
{
    if (!started)
        onStart();
    if (b)
        animationRunGlobal();
}
/* Callees are bound to their retail symbols by asm label (as in game/rt). */
#define SYM(n) __asm__(n)
extern "C" int FlushCache(int mode);
extern "C" int sceGsSyncPath(int mode, int timeout);
extern "C" void hierSetCamera(int a, int b, int c);
void gsPktDma(void *gsPkt) SYM("DmaGsPktQueue__5GsPkt");
void gsPktFrameInit(void *gsPkt, int view, int parity) SYM("frameInit__5GsPktii");
void gameUpdateFadeOut(void *g) SYM("updateFadeOut__7TheGame");
void hudPrint(void *hud) SYM("print__3Hud");
int ieGsHalfTexelOffsetOnOff(void) SYM("ieGsHalfTexelOffsetOnOff__24IncognitoEntertainmentGSv");
void ieGsPutDrawEnv(void *db, int parity, int one) SYM("ieGsPutDrawEnv__24IncognitoEntertainmentGSPQ224IncognitoEntertainmentGS11ieGsDBuffDcii");
void ieGsSetHalfOffsetDc(int view, void *db, unsigned frame, int field, int numViews) SYM("ieGsSetHalfOffsetDc__24IncognitoEntertainmentGSiPQ224IncognitoEntertainmentGS11ieGsDBuffDcUiii");
void ieGsSwapDBuffDc(void *db, int parity, int circuit, int one) SYM("ieGsSwapDBuffDc__24IncognitoEntertainmentGSPQ224IncognitoEntertainmentGS11ieGsDBuffDciii");
void *viewGetDb(int view);
void viewUpdate(int view);
void hier(int view, int pass);
void rtWaitForVu1(void);
void letterboxDmaData(float f);
void particleDraw(int view);
void fontDmaFontData(void);
void timerEndOfRealTimeProcessing(void);
void timer1Handler(unsigned *p);
void timerWaitForMinUpdateRate(unsigned *p);
extern char gsPkt[];
extern unsigned g_frame;
extern unsigned g_onStartupFrame;
extern unsigned g_shellOddEven;
extern int gUserintReturn;
extern int clearBeforeFontDMA;
extern void *game;

#ifdef NON_MATCHING
/* 129/154 palavras; o retail lê g_frame e clearBeforeFontDMA ora por gp, ora por endereço absoluto */
/* End of a shell (menu) frame: swap the GS buffers (the first 4 frames only set the draw environment), draw the
   view, letterbox, fade, particles and HUD text unless `skipDraw`, then wait for the minimum update rate. */
void ShellRtLoop::endField(bool skipDraw)
{
    gsPktFrameInit(gsPkt, 0, (g_frame ^ 1) & 1);
    m_db = viewGetDb(0);
    FlushCache(0);
    sceGsSyncPath(0, 0);
    if (g_onStartupFrame < 4)
        ieGsPutDrawEnv(m_db, (g_frame ^ 1) & 1, 1);
    else
        ieGsSwapDBuffDc(m_db, g_frame & 1, 0, 1);
    sceGsSyncPath(0, 0);
    if (ieGsHalfTexelOffsetOnOff())
        ieGsSetHalfOffsetDc(0, m_db, g_frame, g_shellOddEven, 1);
    else
        ieGsSetHalfOffsetDc(0, m_db, g_frame, 0, 1);
    viewUpdate(0);
    hier(0, 0);
    rtWaitForVu1();
    hierSetCamera(0, 0, 0);
    sceGsSyncPath(0, 0);
    if (gUserintReturn == 0)
        letterboxDmaData(1.0f);
    if (clearBeforeFontDMA)
        ieGsPutDrawEnv(m_db, (g_frame ^ 1) & 1, 1);
    if (!skipDraw) {
        gameUpdateFadeOut(game);
        particleDraw(0);
        sceGsSyncPath(0, 0);
        hudPrint(game); /* TheGame::m_huds[0] */
    }
    sceGsSyncPath(0, 0);
    if (!skipDraw) {
        gsPktDma(gsPkt);
        sceGsSyncPath(0, 0);
    }
    hier(0, 1);
    rtWaitForVu1();
    fontDmaFontData();
    sceGsSyncPath(0, 0);
    timerEndOfRealTimeProcessing();
    timer1Handler(&g_shellOddEven);
    timerWaitForMinUpdateRate(&g_shellOddEven);
    g_frame++;
    g_onStartupFrame++;
}
#else
INCLUDE_ASM("asm/nonmatchings/game/ShellRtLoop", endField__11ShellRtLoopb);
#endif
void ShellRtLoop::onStart(void)
{
    started = 1;
}
void ShellRtLoop::onEnd(void)
{
    started = 0;
}
