#ifndef SOUND_H
#define SOUND_H

/* game/Sound: the game side of the 989snd library. SoundManager (embedded in Shell at 0x2C30) owns the bank manager and the
   master volumes, the per-level ambience classes (LevelObjectSoundManager at TheGame+0x121570 and one class per level),
   and the small sound objects embedded in monsters, pickups, vehicles and the shell. */

#include "engine.h"
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
    struct SoundEvent {
        int id;             // 0x0 sound event id (a trigger, e.g. 0x157C..0x157F)
        char pad4[0xC];
        _fvector pos;       // 0x10
    };
    SoundEvent m_events[4][4];    // 0x000: per player, the sound events at its monster this frame (stride 0x80)
    int m_eventCount[4];            // 0x200
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
    void setSoundVolumeInfo(Monster *m, int id, _fvector *pos);
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

/* ---- looping sounds ----
   A looping sound's handle is 0 while silent and SOUND_PENDING between snd_PlaySoundVolPanPMPB_CB and soundPlayCB storing the
   real handle; LoopManager::terminateLoop stops it (later, if it is still pending). */
#define SOUND_PENDING 0xFFFFFFFF
typedef void (*SoundCB)(unsigned, unsigned long);
void soundPlayCB(unsigned id, unsigned long user);
extern "C" {
unsigned snd_PlaySoundVolPanPMPB_CB(void *bank, int sound, int vol, int pan, int pm, int pb, SoundCB cb, void *user);
void snd_SetSoundParams_A(unsigned handle, unsigned mask, int vol, int pan, int pm, int pb);
void snd_SoundIsStillPlaying_CB(unsigned handle, SoundCB cb, void *user);
}

/* Distance volume and dual-mono pan of a sound at pos for the players' cameras (silent beyond maxDist); mute[i] == 1 skips
   player i. With one player the pan comes from the camera heading; with two, players 0 and 1 are the left and right channels. */
void calcDualMonoMixVolPan(_fvector *pos, int minDist, int maxDist, int &vol, int &pan, int *mute);
int calcCheapDualMonoMixPan(int player);
float findApproachingSpeed(_fvector *pos, int *speed, float *prevDist);
/* Volume by the cameras' height between bottom and top. */
void calcDualMonoMixHeightBasedSoundVol(float top, float bottom, int &vol, int &pan, int *mute);
int timerGetFieldCount(void);
int animationIsRunning(_animHandle h);
float animationGetCurrentFrame(_animHandle h);

/* A looping sound at a fixed point of the level. */
class LevelObjectSound {
public:
    unsigned m_handle;      // 0x0
    char pad4[0xC];
    _fvector m_pos;         // 0x10
    int m_bank;             // 0x20 gSoundBanks index
    int m_sound;            // 0x24
    int m_minDist;          // 0x28
    int m_maxDist;          // 0x2C

    void initLevelObjectSound(int bank, int sound, int minDist, int maxDist, int x, int y, int z);
    void stopLevelObjectSound(void);
    void updateLevelObjectSound(void);
};
typedef char _size_LevelObjectSound[sizeof(LevelObjectSound) == 0x30 ? 1 : -1];

/* Level bank, centred pan. */
class NoPanLevelObjectSound : public LevelObjectSound {
public:
    void updateLevelObjectSound(void);
};

/* Level bank, louder the farther away. */
class InvertedLevelObjectSound : public LevelObjectSound {
public:
    void updateLevelObjectSound(void);
};

/* Level bank wind whose volume follows the cameras' height. Muted for a player who is inside a trigger volume this frame. */
class HeightBasedSound {
public:
    unsigned m_handle;      // 0x0
    int m_sound;            // 0x4
    int m_top;              // 0x8
    int m_bottom;           // 0xC
    char pad10[4];

    void initHeightBasedSound(int sound, int top, int bottom);
    void updateHeightBasedSound(void);
};

/* Level bank loop at full volume while any player triggered sound event m_trigger this frame (setSoundVolumeInfo). */
class TriggerVolumeSound {
public:
    unsigned m_handle;      // 0x0
    int m_trigger;          // 0x4
    int m_sound;            // 0x8

    void initTriggerVolumeSound(int trigger, unsigned sound);
    void terminateTriggerVolumeSound(void);
    void updateTriggerVolumeSound(void);
};

/* Up to four positioned loops, one per place where a player triggered event m_trigger. */
class RangedTriggerVolumeSound {
public:
    struct Slot {
        unsigned handle;    // 0x0
        int active;         // 0x4
        char pad8[8];
        _fvector pos;       // 0x10
    };
    Slot m_slots[4];        // 0x0
    int m_trigger;          // 0x80
    int m_sound;            // 0x84
    int m_minDist;          // 0x88
    int m_maxDist;          // 0x8C

    void initTriggerVolumeSound(int trigger, unsigned sound, int minDist, int maxDist);
    void terminateTriggerVolumeSound(void);
    void updateTriggerVolumeSound(void);
};

/* ---- per-level ambience ---- */
class CanyonLevelSound {
public:
    LevelObjectSound m_ambient;     // 0x0
    void initCanyonLevelSound(void);
    void updateCanyonLevelSound(void);
};

class VegasLevelSound {
public:
    int f0;                         // 0x0: nonzero silences the trigger loops
    char pad4[0xC];
    RangedTriggerVolumeSound m_trigger; // 0x10
    _animHandle m_anim;             // 0xA0
    void initVegasLevelSound(void);
    void updateVegasLevelSound(void);
};

class FinalLevelSound {
public:
    LevelObjectSound m_ambient;     // 0x0
    void initFinalLevelSound(void);
    void updateFinalLevelSound(void);
};

class AirportLevelSound {
public:
    TriggerVolumeSound m_trigger;   // 0x0
    char padC[8];
    HeightBasedSound m_wind;        // 0x14
    void initAirportLevelSound(void);
    void updateAirportLevelSound(void);
    void playPlaneLandingSound(_fvector *pos);
};

class SanFranLevelSound {
public:
    NoPanLevelObjectSound m_quake;  // 0x0
    LevelObjectSound m_ambient[3];  // 0x30
    HeightBasedSound m_wind;        // 0xC0
    TriggerVolumeSound m_trigger;   // 0xD4
    char padE0[8];
    int m_quaking;                  // 0xE8
    int m_quakeStart;               // 0xEC: timerGetFieldCount when the quake started
    LevelObjectSound m_fires[9];    // 0xF0
    int m_firesOn;                  // 0x2A0
    void initSanFranLevelSound(void);
    void updateSanFranLevelSound(void);
    void startQuakeSound(void);
};

class TokyoLevelSound {
public:
    LevelObjectSound m_moving;      // 0x0: follows an animation
    int m_movingOn;                 // 0x30
    TriggerVolumeSound m_trigger;   // 0x34
    char pad40[8];
    HeightBasedSound m_wind;        // 0x48
    _animHandle m_moveAnim;         // 0x5C
    _animHandle m_anim;             // 0x6C
    char pad7C[4];
    LevelObjectSound m_animSound;   // 0x80: while m_anim runs
    void initTokyoLevelSound(void);
    void updateTokyoLevelSound(void);
};

class BigTossLevelSound {
public:
    unsigned m_handle;              // 0x0
    _animHandle m_anim;             // 0x4
    void initBigTossLevelSound(void);
    void updateBigTossLevelSound(void);
};

class ThreeMileLevelSound {
public:
    LevelObjectSound m_ambient;     // 0x0: on while f21C
    LevelObjectSound m_30;          // 0x30: on while f218
    LevelObjectSound m_stacks[4];   // 0x60: smoke stack i intact (gStackDestroyed[i] == 0)
    LevelObjectSound m_stacksDown[4]; // 0x120: smoke stack i destroyed (== 1)
    LevelObjectSound m_1E0;         // 0x1E0: on while f220
    char pad210[8];
    int f218;                       // 0x218
    int f21C;                       // 0x21C
    int f220;                       // 0x220
    int f224;                       // 0x224
    unsigned m_countdownHandle;     // 0x228
    HeightBasedSound m_wind;        // 0x22C
    TriggerVolumeSound m_trigger;   // 0x240
    char pad24C[8];
    unsigned m_craterHandle;        // 0x254
    void initThreeMileLevelSound(void);
    void setAllStacksDestroyed(void);
    void resetReactorSoundInfo(int i);
    void updateThreeMileLevelSound(void);
    void playWickedBigExplosion(void);
    void playShockSound(_fvector *pos);
    void playThreeMileCountdown(int n);
};
extern int gStackDestroyed[4];
extern int craterSwitched;

class VolcanoLevelSound {
public:
    NoPanLevelObjectSound m_lava;   // 0x0
    HeightBasedSound m_wind;        // 0x30
    TriggerVolumeSound m_trigger;   // 0x44
    void initVolcanoLevelSound(void);
    void updateVolcanoLevelSound(void);
};

class UFOLevelSound {
public:
    unsigned m_handle;              // 0x0
    _animHandle m_anim;             // 0x4
    char pad14[0xC];
    LevelObjectSound m_left;        // 0x20
    LevelObjectSound m_right;       // 0x50
    void initUFOLevelSound(void);
    void updateUFOLevelSound(void);
};

class MetroLevelSound {
public:
    unsigned m_handle;              // 0x0
    void initMetroLevelSound(void);
    void updateMetroLevelSound(void);
};

class MilitaryLevelSound {
public:
    void initMilitaryLevelSound(void);
    void updateMilitaryLevelSound(void);
};

class IslandLevelSound {
public:
    NoPanLevelObjectSound m_surf;   // 0x0
    HeightBasedSound m_wind;        // 0x30
    TriggerVolumeSound m_trigger;   // 0x44
    char pad50[0x40];
    LevelObjectSound m_ambient;     // 0x90
    char padC0[0x30];
    int fF0;                        // 0xF0
    void initIslandLevelSound(void);
    void updateIslandLevelSound(void);
};

class Canyon2LevelSound {
public:
    LevelObjectSound m_0;           // 0x0
    LevelObjectSound m_30;          // 0x30
    int m_damDestroyed;             // 0x60
    char pad64[4];
    int f68;                        // 0x68
    int f6C;                        // 0x6C
    void initCanyon2LevelSound(void);
    void updateCanyon2LevelSound(void);
    void playDamDestroyedSound(void);
};

#endif
