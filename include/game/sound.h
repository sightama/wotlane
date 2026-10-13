#ifndef SOUND_H
#define SOUND_H

/* game/Sound: the game side of the 989snd library. SoundManager (embedded in Shell at 0x2C30) owns the bank manager and the
   master volumes, the per-level ambience classes (LevelObjectSoundManager at TheGame+0x121570 and one class per level),
   and the small sound objects embedded in monsters, pickups, vehicles and the shell. */

struct _fvector;
class Monster;

extern "C" {
int printf(const char *, ...);
void snd_StartSoundSystem(void);
int snd_FlushSoundCommands(void);
void snd_ResolveBankXREFS(void);
int snd_SRAMGetFreeMem(void);
void snd_SetMasterVolume(int which, int vol);
int snd_GetMasterVolume(int which);
void snd_SetPlaybackMode(int mode);
void snd_SetGroupVoiceRange(int group, int min, int max);
unsigned snd_PlaySoundVolPanPMPB(void *bank, int sound, int vol, int pan, int pm, int pb);
unsigned snd_PlaySoundVolPanPMPB_A(void *bank, int sound, int vol, int pan, int pm, int pb);
void snd_StopSound(unsigned handle);
void snd_StopAllSounds(void);
void snd_PauseAllSoundsInGroup(unsigned groups);
void snd_ContinueAllSoundsInGroup(unsigned groups);
unsigned snd_SoundIsStillPlaying(unsigned handle);
void snd_SetSoundVolPan(unsigned handle, int vol, int pan);
void snd_SetSoundPitch(unsigned handle, int pitch);
void snd_SetSoundPitchBend(unsigned handle, int bend);
void snd_SetSoundPitchModifier(unsigned handle, int mod);
void snd_SetReverbType(int core, int type);
void snd_SetReverbDepth(int core, int left, int right);
void snd_InitVAGStreamingEx(int channels, int bufSize, int a, int b);
void snd_CloseVAGStreaming(void);
}

/* Banks loaded by name from \SND\ (SoundBankManager in common/SoundBank). */
class SoundBankManager {
public:
    struct Bank {
        void **handle;
        void *unloadingBank;
        int autoUnload;
        int unloading;
        int rtPending;
        int pad14;
        int inUse;
    };
    Bank banks[30];         // 0x0
    int numLoaded;          // 0x348
    int numUnloading;       // 0x34C
    int unloadPending;      // 0x350
    int rtLoading;          // 0x354

    SoundBankManager();
    void loadSoundBank(void **handle, char *name, int autoUnload);
    void loadRTSoundBank(void **handle, char *name, int autoUnload);
    void unloadSoundBankBlocking(void *bank);
    void updateSoundBankManager(void);
    void unloadAllSoundBanks(void);
};

class LoopManager {
public:
    unsigned *loops[50];
    int count;
    void addErrantLoop(unsigned *handle);
    void updateErrantLoops(void);
    void terminateLoop(unsigned *handle);
};
extern LoopManager gLoopManager;

/* Bank handles: [0..10] one per monster type (index = typeBits >> 5, minus one), [12] level, [13] level extra (Count_s),
   [14] shell sfx, [15] shell music, [21] ambience, [22] mech boss, [23] plant boss, [24] final level. */
enum {
    BANK_LEVEL = 12,
    BANK_LEVEL2 = 13,
    BANK_SHELL = 14,
    BANK_SHELLMUS = 15,
    BANK_AMBIENT = 21,
    BANK_MECH = 22,
    BANK_PLANT = 23,
    BANK_FINAL = 24,
};
extern void *gSoundBanks[25];

class SoundManager {
public:
    struct VolumeInfo {
        int vol;            // 0x0
        char pad4[0xC];
        _fvector pos;       // 0x10
    };
    VolumeInfo m_volInfo[4][4];   // 0x000: per player, the sounds that set a volume this frame (stride 0x80)
    int m_volCount[4];            // 0x200
    SoundBankManager m_banks;     // 0x210
    int m_vagStreaming;           // 0x568: snd_InitVAGStreamingEx has run
    char pad56C[0x574 - 0x56C];
    int f574;                     // 0x574
    int m_sfxVolume;              // 0x578: faded towards shell->m_sfxVolume while unpausing
    int m_musicVolume;            // 0x57C
    int m_loadPlantBank;          // 0x580: load Plant_s.bnk once the level banks are unloaded
    int m_loadMechBank;           // 0x584
    int m_reverbOn;               // 0x588

    SoundManager();
    void initPause(void);
    void updatePauseAllSounds(void);
    void setSoundVolumeInfo(Monster *m, int vol, _fvector *pos);
    void unpauseAllSounds(void);
    void doOneTimeInit(void);
    void manageReverb(void);
    void initSoundManager(void);
    void disableSoundForCinema(void);
    void updateSoundManager(void);
    void initVagStreaming(void);
    void loadRTSoundBanks(void);
    void swapMonsterSoundBankBlocking(int oldType, int newType);
    void loadMechBossSoundBank(void);
    void loadMonsterBank(int type, int autoUnload);
    void loadLoResMonsterBank(int type, int autoUnload);
    void loadShellSoundBanks(void);
};

#define SOUND_MANAGER() ((SoundManager *)((char *)shell + 0x2C30))

#endif
