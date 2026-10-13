#include "common.h"
#include "hieri_types.h"
#include "vecmath.h"

/* Sound bank bookkeeping over the 989snd library: banks are loaded from \SND\<NAME>;1 (blocking, or in the background for
   the real-time banks), the level's banks are unloaded in the background when unloadPending is set, and looping sounds whose
   start is still pending (handle 0xFFFFFFFF) are stopped later by LoopManager. Plus distance volume and stereo pan helpers. */

struct sceCdlFILE {
    unsigned int lsn;
    unsigned int size;
    char name[16];
    unsigned char date[8];
    unsigned int flag;
};

class SoundBankManager {
public:
    struct Bank {
        void **handle;      // 0x0 the caller's bank handle variable
        void *unloadingBank;  // 0x4 *handle when its unload started (the unload callback clears *handle)
        int autoUnload;     // 0x8 unloaded with the level (unloadPending)
        int unloading;      // 0xC
        int rtPending;      // 0x10 background load not confirmed yet
        int pad14;          // 0x14
        int inUse;          // 0x18
    };

    Bank banks[30];         // 0x0
    int numLoaded;          // 0x348
    int numUnloading;       // 0x34C
    int unloadPending;      // 0x350 set to unload the autoUnload banks
    int rtLoading;          // 0x354 background loads in flight

    SoundBankManager();
    void loadSoundBank(void **handle, char *name, int autoUnload);
    int findFirstAvailableBankSlot(void);
    void loadRTSoundBank(void **handle, char *name, int autoUnload);
    void unloadSoundBankBlocking(void *bank);
    void updateSoundBankManager(void);
    void unloadAllSoundBanks(void);
};
typedef char _size_Bank[sizeof(SoundBankManager::Bank) == 0x1C ? 1 : -1];

class LoopManager {
public:
    unsigned *loops[50];    // 0x0 handles of loops to stop once they have started
    int count;              // 0xC8

    LoopManager();
    void addErrantLoop(unsigned *handle);
    void updateErrantLoops(void);
    void terminateLoop(unsigned *handle);
};

extern "C" {
int strlen(const char *);
char *strcat(char *, const char *);
char *strcpy(char *, const char *);
int sprintf(char *, const char *, ...);
int printf(const char *, ...);
float atan2f(float, float);
void snd_StreamSafeCdSync(int);
void *snd_BankLoadByLoc(unsigned lsn, int offset);
void snd_BankLoadByLoc_CB(unsigned lsn, int offset, void (*cb)(unsigned, unsigned long), void *user);
void snd_UnloadBank(void *bank);
void snd_UnloadBank_CB(void *bank, void (*cb)(unsigned, unsigned long), void *user);
int snd_FlushSoundCommands(void);
void snd_ResolveBankXREFS(void);
int snd_SRAMGetFreeMem(void);
void snd_StopSound(unsigned handle);
}
int fileCdSearchFile(sceCdlFILE *f, char *path);
float mathfManhatDist(_fvector *a, _fvector *b);
void soundPlayCB(unsigned id, unsigned long user);
void soundUnloadBankCB(unsigned id, unsigned long user);

#define SOUND_PENDING 0xFFFFFFFF

#ifdef NON_MATCHING
/* equivalent (untuned): 1/21 words, size 0x54 vs 0x4c */
SoundBankManager::SoundBankManager()
{
    Bank *b;

    for (b = banks; b < &banks[30]; b++) {
        b->handle = 0;
        b->unloadingBank = 0;
        b->autoUnload = 0;
        b->unloading = 0;
        b->rtPending = 0;
        b->pad14 = 0;
        b->inUse = 0;
    }
    numLoaded = 0;
    unloadPending = 0;
    numUnloading = 0;
}
#else
INCLUDE_ASM("asm/nonmatchings/common/SoundBank", __16SoundBankManager);
#endif

#ifdef NON_MATCHING
/* the retail strings in SoundBank's sdata (a literal would land in an sdata the NM link drops); the retail inlines the strcpy of
   the literal, here it is a call */
extern char sndDir[] __asm__("D_006F85D8");      /* the "\SND\" directory */
extern char isoVersion[] __asm__("D_006F85E0");  /* ";1" */
/* \SND\<NAME>;1, the name uppercased in place (retail tests islower() through newlib's _ctype_; same for ASCII names) */
static inline void bankPath(char *path, char *name)
{
    int n = strlen(name);
    int i;

    strcpy(path, sndDir);
    for (i = 0; i < n; i++) {
        if (name[i] >= 'a' && name[i] <= 'z')
            name[i] -= 0x20;
    }
    strcat(path, name);
    strcat(path, isoVersion);
}
#endif

#ifdef NON_MATCHING
/* equivalent (untuned): 8/102 words, size 0x198 vs 0x188 */
/* Loads bank name into *handle (blocking) unless *handle is already set. */
void SoundBankManager::loadSoundBank(void **handle, char *name, int autoUnload)
{
    int slot = findFirstAvailableBankSlot();
    char path[0x70];
    sceCdlFILE f;
    char msg[0x40];

    if (*handle)
        return;
    bankPath(path, name);
    snd_StreamSafeCdSync(0);
    fileCdSearchFile(&f, path);
    sprintf(msg, "Cant find Sound Bank! %s", path);
    *handle = snd_BankLoadByLoc(f.lsn, 0);
    banks[slot].inUse = 1;
    sprintf(msg, "Failure Loading Sound Bank! (%s)\n", path);
    banks[slot].handle = handle;
    banks[slot].autoUnload = autoUnload;
    numLoaded++;
}
#else
INCLUDE_ASM("asm/nonmatchings/common/SoundBank", loadSoundBank__16SoundBankManagerPPvPci);
#endif

#ifdef NON_MATCHING
/* equivalent (untuned): 1/17 words, size 0x40 vs 0x44 */
/* The first free slot (0 when all are taken). */
int SoundBankManager::findFirstAvailableBankSlot(void)
{
    int slot = 0;
    int i;

    for (i = 0; i < 30; i++) {
        if (!banks[i].inUse) {
            slot = i;
            break;
        }
    }
    return slot;
}
#else
INCLUDE_ASM("asm/nonmatchings/common/SoundBank", findFirstAvailableBankSlot__16SoundBankManager);
#endif

#ifdef NON_MATCHING
/* equivalent (untuned): 9/91 words, size 0x16c vs 0x15c */
/* Starts loading bank name in the background; soundPlayCB stores it in *handle (updateSoundBankManager confirms it). */
void SoundBankManager::loadRTSoundBank(void **handle, char *name, int autoUnload)
{
    int slot = findFirstAvailableBankSlot();
    char path[0x70];
    sceCdlFILE f;

    if (*handle)
        return;
    banks[slot].handle = handle;
    banks[slot].autoUnload = autoUnload;
    bankPath(path, name);
    snd_StreamSafeCdSync(0);
    fileCdSearchFile(&f, path);
    snd_BankLoadByLoc_CB(f.lsn, 0, soundPlayCB, banks[slot].handle);
    banks[slot].rtPending = 1;
    rtLoading = 1;
    numLoaded++;
}
#else
INCLUDE_ASM("asm/nonmatchings/common/SoundBank", loadRTSoundBank__16SoundBankManagerPPvPci);
#endif

#ifdef NON_MATCHING
/* equivalent (untuned): 17/42 words, size 0xa8 vs 0xa8 */
void SoundBankManager::unloadSoundBankBlocking(void *bank)
{
    Bank *b;

    for (b = banks; b < &banks[30]; b++) {
        if (*b->handle == bank) {
            snd_UnloadBank(bank);
            numLoaded--;
            *b->handle = 0;
            b->inUse = 0;
            b->handle = 0;
            b->unloadingBank = 0;
            b->autoUnload = 0;
            b->unloading = 0;
            break;
        }
    }
    while (snd_FlushSoundCommands())
        ;
    snd_ResolveBankXREFS();
}
#else
INCLUDE_ASM("asm/nonmatchings/common/SoundBank", unloadSoundBankBlocking__16SoundBankManagerPv);
#endif

#ifdef NON_MATCHING
/* equivalent (untuned): 4/161 words, size 0x254 vs 0x284 */
/* Per frame: runs the background unload of the autoUnload banks (when unloadPending) and confirms background loads. */
void SoundBankManager::updateSoundBankManager(void)
{
    int allUnloaded = 1;
    int allLoaded = 1;
    Bank *b;

    if (unloadPending) {
        for (b = banks; b < &banks[30]; b++) {
            if (b->autoUnload == 1 && !b->unloading && *b->handle) {
                printf("set bank to unload %p. Memory before unload %d\n", *b->handle, snd_SRAMGetFreeMem());
                b->unloadingBank = *b->handle;
                snd_UnloadBank_CB(*b->handle, soundUnloadBankCB, b->handle);
                b->unloading = 1;
                numUnloading++;
            }
            if (b->unloading == 1 && *b->handle != b->unloadingBank) {
                *b->handle = 0;
                b->handle = 0;
                b->unloadingBank = 0;
                b->autoUnload = 0;
                b->unloading = 0;
                b->inUse = 0;
            } else if (b->unloading == 1) {
                allUnloaded = 0;
            }
        }
        if (allUnloaded) {
            numLoaded -= numUnloading;
            numUnloading = 0;
            unloadPending = 0;
            snd_ResolveBankXREFS();
            printf("unloading banks done. Avail memory %d\n", snd_SRAMGetFreeMem());
            while (snd_FlushSoundCommands())
                ;
        }
    }
    if (rtLoading) {
        for (b = banks; b < &banks[30]; b++) {
            if (b->rtPending != 1)
                continue;
            if (b->handle) {
                b->rtPending = 0;
                b->inUse = 1;
            } else {
                allLoaded = 0;
                printf("loading RT sound bank...\n");
            }
        }
        if (allLoaded) {
            rtLoading = 0;
            snd_ResolveBankXREFS();
            printf("loading banks done!!!!!!!!!!!!!!!!!!!!!!!!! %d\n", snd_SRAMGetFreeMem());
            while (snd_FlushSoundCommands())
                ;
        }
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/common/SoundBank", updateSoundBankManager__16SoundBankManager);
#endif

#ifdef NON_MATCHING
/* equivalent (untuned): 8/48 words, size 0xc0 vs 0xb8 */
void SoundBankManager::unloadAllSoundBanks(void)
{
    Bank *b;

    for (b = banks; b < &banks[30]; b++) {
        if (b->inUse == 1) {
            snd_UnloadBank(*b->handle);
            *b->handle = 0;
            b->handle = 0;
            b->autoUnload = 0;
            b->unloading = 0;
            b->pad14 = 0;
            b->inUse = 0;
        }
    }
    numLoaded = 0;
    while (snd_FlushSoundCommands())
        ;
    snd_ResolveBankXREFS();
}
#else
INCLUDE_ASM("asm/nonmatchings/common/SoundBank", unloadAllSoundBanks__16SoundBankManager);
#endif

LoopManager::LoopManager()
{
    int i;

    for (i = 49; i >= 0; i--)
        loops[i] = 0;
    count = 0;
}

/* Remembers a loop whose start is still pending, to stop it once it has a handle (the first free slot; no bound check). */
void LoopManager::addErrantLoop(unsigned *handle)
{
    int i;

    printf("Adding errant sound loop: %p\n", handle);
    for (i = 0; i < 50 && loops[i]; i++)
        ;
    loops[i] = handle;
    count++;
}

void LoopManager::updateErrantLoops(void)
{
    int i;

    if (count <= 0)
        return;
    for (i = 0; i < 50; i++) {
        if (loops[i] && *loops[i] != SOUND_PENDING && *loops[i]) {
            snd_StopSound(*loops[i]);
            *loops[i] = 0;
            printf("Stopped errant sound loop: %p\n", loops[i]);
            count--;
            loops[i] = 0;
        }
    }
}

#ifdef NON_MATCHING
/* equivalent (untuned): 9/22 words, size 0x58 vs 0x54 */
/* Stops the loop in *handle now, or later if it has not started yet. */
void LoopManager::terminateLoop(unsigned *handle)
{
    if (*handle == SOUND_PENDING) {
        addErrantLoop(handle);
    } else if (*handle) {
        snd_StopSound(*handle);
        *handle = 0;
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/common/SoundBank", terminateLoop__11LoopManagerPUi);
#endif

/* 989snd callbacks: user is the address of the handle variable */
void soundPlayCB(unsigned id, unsigned long user)
{
    *(unsigned *)(int)user = id;
}

void soundUnloadBankCB(unsigned id, unsigned long user)
{
    *(unsigned *)(int)user = 0;
}

#ifdef NON_MATCHING
/* equivalent (untuned): 81/98 words, size 0x188 vs 0x188 */
/* Pan of a sound at src heard from listener facing heading (radians): degrees 0-359 clockwise from straight ahead. */
int calculatePanAngle(float heading, _fvector *listener, _fvector *src)
{
    FVECTOR d;
    float h = heading * 57.29578f;
    float a;

    if (h < 0.0f)
        h += 360.0f;
    vecSub(&d, src, listener);
    a = atan2f(__builtin_fabsf(d.x), __builtin_fabsf(d.y)) * 57.29578f;
    if (d.x == 0.0f && d.y == 0.0f)
        return 0;
    if (d.x <= 0.0f && 0.0f <= d.y)
        a = 360.0f - a;
    else if (0.0f <= d.x && d.y <= 0.0f)
        a = 180.0f - a;
    else if (d.x <= 0.0f && d.y <= 0.0f)
        a += 180.0f;
    a -= h;
    if (a < 0.0f)
        a += 360.0f;
    return (int)a;
}
#else
INCLUDE_ASM("asm/nonmatchings/common/SoundBank", calculatePanAngle__FfP8_fvectorT1);
#endif

/* 0x400 within nearDist, 0 past farDist, linear in between. */
int calculateRangedVolume(_fvector *a, _fvector *b, int nearDist, int farDist)
{
    float d = mathfManhatDist(a, b);

    if (d < (float)nearDist)
        return 0x400;
    if ((float)farDist < d)
        return 0;
    return (int)(((float)farDist - d) / (float)(farDist - nearDist) * 1024.0f);
}
