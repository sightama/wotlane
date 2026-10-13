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
/* _fvector members make these classes 16-byte aligned (sizes round up to 16). */

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

/* Sound action of an interactive (the timed building collapse). */
class ActPlaySound {
public:
    char pad0[0xC];
    unsigned m_time;                // 0xC: fields
    _fvector m_offset;              // 0x10: where the second (late) sound plays, relative to the first
};

/* Level ambience, destruction sounds and fire loops; embedded in TheGame at 0x121570. */
class LevelObjectSoundManager {
public:
    /* A collapse: two loops that fade out over `fade` seconds after `delay`, plus an optional one-shot at pos2. */
    struct DestructLoop {
        unsigned handle;            // 0x0
        unsigned handle2;           // 0x4
        int start;                  // 0x8 timerGetFieldCount at the trigger
        int minDist;                // 0xC
        int maxDist;                // 0x10
        float delay;                // 0x14 seconds
        float fade;                 // 0x18 seconds
        char pad1C[4];
        _fvector pos;               // 0x20
        int hasSecond;              // 0x30
        char pad34[0xC];
        _fvector pos2;              // 0x40
    };

    CanyonLevelSound m_canyon;      // 0x0
    Canyon2LevelSound m_canyon2;    // 0x30
    IslandLevelSound m_island;      // 0xA0
    VegasLevelSound m_vegas;        // 0x1A0
    SanFranLevelSound m_sanFran;    // 0x250
    ThreeMileLevelSound m_threeMile; // 0x500
    TokyoLevelSound m_tokyo;        // 0x760
    AirportLevelSound m_airport;    // 0x810
    char pad838[8];
    VolcanoLevelSound m_volcano;    // 0x840
    char pad890[0x40];
    UFOLevelSound m_ufo;            // 0x8D0
    char pad950[0x20];
    MilitaryLevelSound m_military;  // 0x970
    char pad971[3];
    MetroLevelSound m_metro;        // 0x974
    BigTossLevelSound m_bigToss;    // 0x978
    char pad98C[4];
    FinalLevelSound m_final;        // 0x990
    RangedTriggerVolumeSound m_trigger; // 0x9C0: trigger 0x157D
    DestructLoop m_destruct[20];    // 0xA50
    int m_startTime;                // 0x1090: no debris sounds in the first 24 fields
    int m_lastCreak;                // 0x1094
    float m_creakDelay;             // 0x1098: seconds, random 0.2..0.6
    unsigned m_fireHandles[100];    // 0x109C: one loop per active fire

    void initLevelObjectSoundManager(void);
    void updateLevelObjectSoundManager(void);
    void updateDestructLoops(void);
    void triggerLevelDestructSound(_fvector *pos, int minDist, int maxDist, float delay, float fade, int sound, ActPlaySound *act);
    void playDebrisImpactSound(_fvector *pos, int type);
    void playTimedBuildingDestructibleSound(ActPlaySound *act, _fvector *pos);
    void playDestructibleSound(unsigned id, _fvector *pos);
    int findAvailableDestructHandle(void);
};
typedef char _size_LevelObjectSoundManager[sizeof(LevelObjectSoundManager) == 0x1230 ? 1 : -1];
int particleActiveFires(_fvector **fires);
float mathfRandf(float lo, float hi);

/* ---- vehicles, pickups, shell ---- */
extern "C" int snd_GetDopplerPitchMod(int speed);
extern int assBossActive;
class TankPickup;
class HeliPickup;
class CarPickup;

class TankSound {
public:
    int f0;                     // 0x0
    unsigned m_handle;          // 0x4: engine
    void initTankSound(void);
    void updateTankSound(TankPickup *tank, int moving);
    void terminateTankSound(void);
    void playTankFireSound(_fvector *pos);
};

class HeliSound {
public:
    unsigned m_crashHandle;     // 0x0: while the helicopter is in state 4
    unsigned m_rotorHandle;     // 0x4
    unsigned m_gunHandle;       // 0x8: type 12 in state 2
    float m_prevDist;           // 0xC: for the doppler pitch
    void initHeliSound(void);
    void updateHeliSound(HeliPickup *heli);
    void terminateHeliSound(void);
};

class CarSound {
public:
    float m_prevDist;           // 0x0: for the doppler pitch
    CarPickup *m_car;           // 0x4
    unsigned m_handle;          // 0x8: engine
    void initCarSound(CarPickup *car);
    void updateCarSound(void);
    void playHornSound(void);
    void terminateCarSound(void);
};

#include "game/pickup_sound.h"

/* Front-end music, token counter and options slider loops. */
class ShellSound {
public:
    unsigned m_musicHandle;     // 0x0
    unsigned m_tokenHandle;     // 0x4
    char pad8[8];
    int m_musicStopped;         // 0x10
    int m_musicOn;              // 0x14
    int f18;                    // 0x18: nonzero keeps the music off
    unsigned m_sliderHandle;    // 0x1C

    ShellSound();
    void resetShellSoundFlags(void);
    void updateShellSound(void);
    void updateTokenSound(void);
    void playTokenCountFinishedSound(void);
    void stopTokenSound(void);
    void updateSFXSliderSound(void);
    void stopSFXSliderSound(void);
    void terminateShellSound(void);
    void playShellSFXSound(int sound);
    void playSweetUnlockedSound(void);
    void playPauseMenuSound(int sound);
};

class HudSound {
public:
    void initHudSound(void);
    void updateHudSound(void);
};

/* A positioned loop from any bank. */
class GenericSound {
public:
    unsigned m_handle;          // 0x0
    int m_bank;                 // 0x4 gSoundBanks index
    int m_sound;                // 0x8
    int m_minDist;              // 0xC
    int m_maxDist;              // 0x10
    void initGenericSound(int bank, int sound, int minDist, int maxDist);
    void terminateGenericSound(void);
    void updateGenericSound(_fvector *pos);
};

/* Crowd loop of a pedestrian group (only groups of 16 or more). */
class PedSound {
public:
    unsigned m_handle;          // 0x0
    int f4;                     // 0x4: set to 0x14 when the loop starts
    void initPedSound(void);
    void terminatePedSound(void);
    void updatePedSound(_fvector *pos, int count);
};
extern int gInnerPed;           /* crowd sound distances */
extern int gOutterPed;

/* A burning monster: ignition one-shot, then the fire loop. */
class FireSound {
public:
    unsigned m_handle;          // 0x0
    void initFireSound(void);
    void terminateFireSound(void);
    void updateFireSound(_fvector *pos);
};

/* Airport plane: engine loop, plus a loop in state 1 (before t 485) and one in state 6 (before t 1170). */
class PlaneSound {
public:
    unsigned m_engineHandle;    // 0x0
    unsigned m_handle4;         // 0x4
    unsigned m_handle8;         // 0x8
    void initPlaneSound(void);
    void terminatePlaneSound(void);
    void updatePlaneSound(_fvector *pos, int state, float t);
};

#endif
