#include "common.h"
#include "game/game.h"
#include "game/shell.h"
#include "game/streaming_sound.h"
#include "game/sound.h"
#include "vecmath.h"
#include "game/pickup.h"

extern char mechBankName[] __asm__("D_006EFFB0");   /* "Mech_s.bnk" */
#define MI(m, o) (*(int *)((char *)(m) + (o)))
__asm__("#SNFIX_SMALL gInnerPed");
__asm__("#SNFIX_SMALL gOutterPed");

/* Only the fields the sounds read. */
class TankPickup : public Pickup {
};
class HeliPickup : public Pickup {
public:
    char padE0[0x174 - 0xE0];
    int m_state;                /* 0x174: 2 firing (type 12), 4 crashing */
};
class CarPickup : public Pickup {
public:
    char padE0[0x1A8 - 0xE0];
    float m_maxSpeed;           /* 0x1A8 */
};
class Weapon {
public:
    char pad0[0x40];
    int m_active;               /* 0x40 */
    char pad44[0x4C - 0x44];
    int m_type;                 /* 0x4C: Weapon::WeaponType */
    char pad50[0x150 - 0x50];
    _cs *m_cs;                  /* 0x150 */
};

SoundManager::SoundManager()
{
    m_vagStreaming = 0;
    m_reverbOn = 0;
    f574 = 0;
    for (int i = 0; i < 25; i++)
        gSoundBanks[i] = 0;
}

void SoundManager::initPause(void)
{
    m_sfxVolume = shell->m_sfxVolume;
    m_musicVolume = shell->m_musicVolume;
}

/* Fades the master volumes out while the game is paused; once both reach zero every sound is paused. */
#ifdef NON_MATCHING
/* equivalent (untuned): 3/64 words, size 0xf0 vs 0x100 */
void SoundManager::updatePauseAllSounds(void)
{
    if (!STREAMING_SOUND()->m_paused && snd_GetMasterVolume(1) == 0 && snd_GetMasterVolume(0) == 0) {
        snd_PauseAllSoundsInGroup(0xFFFFFFFF);
        STREAMING_SOUND()->m_paused = 1;
        return;
    }
    if (STREAMING_SOUND()->m_paused)
        return;
    m_sfxVolume -= 60;
    if (m_sfxVolume <= 0)
        m_sfxVolume = 0;
    snd_SetMasterVolume(0, m_sfxVolume);
    snd_SetMasterVolume(3, m_sfxVolume);
    snd_SetMasterVolume(5, m_sfxVolume);
    m_musicVolume -= 50;
    if (m_musicVolume <= 0)
        m_musicVolume = 0;
    snd_SetMasterVolume(1, m_musicVolume);
}
#else
INCLUDE_ASM("asm/nonmatchings/game/Sound", updatePauseAllSounds__12SoundManager);
#endif

/* Records a sound event at a player's monster this frame (up to four per player); the trigger volume sounds read them. */
#ifdef NON_MATCHING
/* equivalent (untuned): 20/33 words, size 0x84 vs 0x84 */
void SoundManager::setSoundVolumeInfo(Monster *m, int id, _fvector *pos)
{
    if (m->m_playerNum == 1) {
        m_events[m->m_index][m_eventCount[m->m_index]].id = id;
        m_events[m->m_index][m_eventCount[m->m_index]].pos = *pos;
        m_eventCount[m->m_index]++;
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/game/Sound", setSoundVolumeInfo__12SoundManagerP7MonsteriP8_fvector);
#endif

#ifdef NON_MATCHING
/* equivalent (untuned): 8/21 words, size 0x4c vs 0x54 */
void SoundManager::unpauseAllSounds(void)
{
    snd_ContinueAllSoundsInGroup(0xFFFFFFFF);
    STREAMING_SOUND()->m_paused = 0;
    while (snd_FlushSoundCommands())
        ;
}
#else
INCLUDE_ASM("asm/nonmatchings/game/Sound", unpauseAllSounds__12SoundManager);
#endif

void SoundManager::doOneTimeInit(void)
{
    snd_StartSoundSystem();
    snd_SetGroupVoiceRange(3, 0x18, 0x2F);
    snd_SetMasterVolume(8, 0x400);
    snd_SetMasterVolume(0, 0x400);
    snd_SetMasterVolume(3, 0x400);
    snd_SetMasterVolume(1, 0x33E);
    snd_SetMasterVolume(4, 0x400);
    snd_SetMasterVolume(5, 0x400);
    snd_SetMasterVolume(6, 0x271);
}

/* Reverb only with fewer than four monster slots. */
void SoundManager::manageReverb(void)
{
    if (game->m_numSlots < 4) {
        if (!m_reverbOn) {
            snd_SetReverbType(2, 3);
            snd_SetReverbDepth(2, 0xB21, 0xB21);
            m_reverbOn = 1;
        }
    } else {
        m_reverbOn = 0;
        snd_SetReverbType(2, 0);
    }
}

#ifdef NON_MATCHING
/* equivalent (untuned): 77/80 words, size 0x140 vs 0x140 */
void SoundManager::initSoundManager(void)
{
    if (shell->m_mono)
        snd_SetPlaybackMode(1);
    else
        snd_SetPlaybackMode(0);
    snd_SetMasterVolume(8, 0x400);
    snd_SetMasterVolume(0, shell->m_sfxVolume);
    snd_SetMasterVolume(3, shell->m_sfxVolume);
    snd_SetMasterVolume(1, shell->m_musicVolume);
    snd_SetMasterVolume(4, shell->m_sfxVolume);
    snd_SetMasterVolume(5, shell->m_sfxVolume);
    snd_SetMasterVolume(6, 0x271);
    for (int i = 0; i < 4; i++)
        for (int j = 0; j < 4; j++)
            m_events[i][j].id = 0;
    for (int i = 0; i < 4; i++)
        m_eventCount[i] = 0;
    f574 = 0;
    m_loadPlantBank = 0;
    m_loadMechBank = 0;
}
#else
INCLUDE_ASM("asm/nonmatchings/game/Sound", initSoundManager__12SoundManager);
#endif

#ifdef NON_MATCHING
/* equivalent (untuned): 2/22 words, size 0x34 vs 0x58 */
void SoundManager::disableSoundForCinema(void)
{
    int mode = game->m_gameMode;
    if (mode == 1 || mode == 2 || mode == 3)
        STREAMING_SOUND()->m_enabled = 0;
    else
        STREAMING_SOUND()->m_enabled = 1;
}
#else
INCLUDE_ASM("asm/nonmatchings/game/Sound", disableSoundForCinema__12SoundManager);
#endif

/* Per frame: errant loops, background bank loads (the boss banks wait for the level banks to unload), the per-player volume
   records, and fading the master volumes back in after a pause. */
#ifdef NON_MATCHING
/* equivalent (untuned): 64/144 words, size 0x230 vs 0x240 */
void SoundManager::updateSoundManager(void)
{
    gLoopManager.updateErrantLoops();
    m_banks.updateSoundBankManager();
    if (m_loadPlantBank == 1 && !m_banks.unloadPending) {
        m_loadPlantBank = 0;
        m_banks.loadRTSoundBank(&gSoundBanks[BANK_PLANT], "Plant_s.bnk", 0);
    }
    if (m_loadMechBank == 1 && !m_banks.unloadPending) {
        m_loadMechBank = 0;
        m_banks.loadRTSoundBank(&gSoundBanks[BANK_MECH], mechBankName, 0);
    }
    for (int i = 0; i < 4; i++)
        for (int j = 0; j < 4; j++)
            m_events[i][j].id = 0;
    for (int i = 0; i < 4; i++)
        m_eventCount[i] = 0;
    if (STREAMING_SOUND()->m_paused == 1 && snd_GetMasterVolume(1) == shell->m_musicVolume &&
        snd_GetMasterVolume(0) == shell->m_sfxVolume) {
        STREAMING_SOUND()->m_paused = 0;
        return;
    }
    if (STREAMING_SOUND()->m_paused != 1)
        return;
    snd_ContinueAllSoundsInGroup(0xFFFFFFFF);
    m_sfxVolume += 60;
    if (m_sfxVolume >= shell->m_sfxVolume)
        m_sfxVolume = shell->m_sfxVolume;
    snd_SetMasterVolume(0, m_sfxVolume);
    snd_SetMasterVolume(3, m_sfxVolume);
    snd_SetMasterVolume(5, m_sfxVolume);
    m_musicVolume += 50;
    if (m_musicVolume >= shell->m_musicVolume)
        m_musicVolume = shell->m_musicVolume;
    snd_SetMasterVolume(1, m_musicVolume);
}
#else
INCLUDE_ASM("asm/nonmatchings/game/Sound", updateSoundManager__12SoundManager);
#endif

void SoundManager::initVagStreaming(void)
{
    snd_StopAllSounds();
    m_banks.unloadAllSoundBanks();
    if (!m_vagStreaming) {
        m_vagStreaming = 1;
        snd_InitVAGStreamingEx(4, 0xA000, 0, 1);
    }
    STREAMING_SOUND()->bufferIntroCinemaMusic();
}
INCLUDE_ASM("asm/nonmatchings/game/Sound", D_006EFFB0);

/* Ambience, the monsters' banks (low-res versions when more than two monster slots share the sound RAM; with exactly three the
   first player keeps the full bank) and the level's bank. */
#ifdef NON_MATCHING
/* equivalent (untuned): 7/365 words, size 0x5ac vs 0x5b4 */
void SoundManager::loadRTSoundBanks(void)
{
    printf("Sound RAM Unused Before: %d\n", snd_SRAMGetFreeMem());
    if (game->m_gameMode == 6 || game->m_gameMode == 4 || game->m_numSlots < 3)
        m_banks.loadSoundBank(&gSoundBanks[BANK_AMBIENT], "GenAmb_s.bnk", 0);
    else
        m_banks.loadSoundBank(&gSoundBanks[BANK_AMBIENT], "GenAm_ls.bnk", 0);

    if (game->m_gameMode == 4) {
        loadMonsterBank(game->m_monsters[0]->m_typeBits, 0);
        loadMonsterBank(game->m_monsters[4]->m_typeBits, 1);
    } else if (game->m_gameMode == 6 || game->m_numSlots < 3) {
        for (int i = 0; i < game->m_numMonsters; i++)
            loadMonsterBank(game->m_monsters[i]->m_typeBits, 0);
        for (int i = 0; i < game->m_numAIs; i++)
            loadMonsterBank(game->m_monsters[4 + i]->m_typeBits, 1);
    } else if (game->m_numSlots != 3) {
        for (int i = 0; i < game->m_numMonsters; i++)
            loadLoResMonsterBank(game->m_monsters[i]->m_typeBits, 0);
        for (int i = 0; i < game->m_numAIs; i++)
            loadLoResMonsterBank(game->m_monsters[4 + i]->m_typeBits, 1);
    } else {
        loadMonsterBank(game->m_monsters[0]->m_typeBits, 0);
        for (int i = 1; i < game->m_numMonsters; i++)
            loadLoResMonsterBank(game->m_monsters[i]->m_typeBits, 0);
        for (int i = 0; i < game->m_numAIs; i++)
            loadLoResMonsterBank(game->m_monsters[4 + i]->m_typeBits, 1);
    }

    switch (shell->m_levelNum) {
    case 12:
        m_banks.loadSoundBank(&gSoundBanks[BANK_LEVEL], "Miltry_s.bnk", 1);
        break;
    case 10:
        m_banks.loadSoundBank(&gSoundBanks[BANK_LEVEL], "Space_s.bnk", 1);
        break;
    case 3:
    case 13:
        m_banks.loadSoundBank(&gSoundBanks[BANK_LEVEL], "Canyon_s.bnk", 1);
        break;
    case 7:
    case 14:
        m_banks.loadSoundBank(&gSoundBanks[BANK_LEVEL], "SanFra_s.bnk", 1);
        break;
    case 9:
    case 16:
        m_banks.loadSoundBank(&gSoundBanks[BANK_LEVEL], "Tokyo_s.bnk", 1);
        break;
    case 5:
        m_banks.loadSoundBank(&gSoundBanks[BANK_LEVEL], "airprt_s.bnk", 1);
        break;
    case 6:
        m_banks.loadSoundBank(&gSoundBanks[BANK_LEVEL], "3Mile_s.bnk", 1);
        if (game->m_gameMode == 1)
            m_banks.loadSoundBank(&gSoundBanks[BANK_LEVEL2], "Count_s.bnk", 1);
        break;
    case 8:
    case 15:
        m_banks.loadSoundBank(&gSoundBanks[BANK_LEVEL], "Volcno_s.bnk", 1);
        break;
    case 2:
        m_banks.loadSoundBank(&gSoundBanks[BANK_LEVEL], "Vegas_s.bnk", 1);
        break;
    case 1:
    case 4:
        m_banks.loadSoundBank(&gSoundBanks[BANK_LEVEL], "Metro_s.bnk", 1);
        break;
    case 11:
        if (game->m_gameMode == 1)
            m_banks.loadSoundBank(&gSoundBanks[BANK_FINAL], "Final_s.bnk", 1);
        break;
    case 26:
        m_banks.loadSoundBank(&gSoundBanks[BANK_LEVEL], "BigSht_s.bnk", 1);
        break;
    }
    snd_ResolveBankXREFS();
    printf("Sound RAM Unused After: %d\n", snd_SRAMGetFreeMem());
}
#else
INCLUDE_ASM("asm/nonmatchings/game/Sound", loadRTSoundBanks__12SoundManager);
#endif

/* Blocking swap of one monster's bank (the monster changed type mid-level). */
void SoundManager::swapMonsterSoundBankBlocking(int oldType, int newType)
{
    switch (oldType) {
    case 0x20: m_banks.unloadSoundBankBlocking(gSoundBanks[0]); break;
    case 0x40: m_banks.unloadSoundBankBlocking(gSoundBanks[1]); break;
    case 0x60: m_banks.unloadSoundBankBlocking(gSoundBanks[2]); break;
    case 0x80: m_banks.unloadSoundBankBlocking(gSoundBanks[3]); break;
    case 0xA0: m_banks.unloadSoundBankBlocking(gSoundBanks[4]); break;
    case 0xE0: m_banks.unloadSoundBankBlocking(gSoundBanks[6]); break;
    case 0x100: m_banks.unloadSoundBankBlocking(gSoundBanks[7]); break;
    case 0x120: m_banks.unloadSoundBankBlocking(gSoundBanks[8]); break;
    case 0x140: m_banks.unloadSoundBankBlocking(gSoundBanks[9]); break;
    case 0x160: m_banks.unloadSoundBankBlocking(gSoundBanks[10]); break;
    }
    loadMonsterBank(newType, 0);
}

void SoundManager::loadMechBossSoundBank(void)
{
    m_banks.loadSoundBank(&gSoundBanks[BANK_MECH], mechBankName, 0);
}

/* type is Monster::m_typeBits (monster index << 5); 0xC0 has no bank. */
void SoundManager::loadMonsterBank(int type, int autoUnload)
{
    switch (type) {
    case 0x20: m_banks.loadSoundBank(&gSoundBanks[0], "Ape_s.bnk", autoUnload); break;
    case 0x40: m_banks.loadSoundBank(&gSoundBanks[1], "Robot_s.bnk", autoUnload); break;
    case 0x60: m_banks.loadSoundBank(&gSoundBanks[2], "Lizard_s.bnk", autoUnload); break;
    case 0x80: m_banks.loadSoundBank(&gSoundBanks[3], "Shogun_s.bnk", autoUnload); break;
    case 0xA0: m_banks.loadSoundBank(&gSoundBanks[4], "Mantis_s.bnk", autoUnload); break;
    case 0xE0: m_banks.loadSoundBank(&gSoundBanks[6], "Energy_s.bnk", autoUnload); break;
    case 0x100: m_banks.loadSoundBank(&gSoundBanks[7], "Lava_s.bnk", autoUnload); break;
    case 0x120: m_banks.loadSoundBank(&gSoundBanks[8], "Dragon_s.bnk", autoUnload); break;
    case 0x140: m_banks.loadSoundBank(&gSoundBanks[9], "Rokman_s.bnk", autoUnload); break;
    case 0x160: m_banks.loadSoundBank(&gSoundBanks[10], "Alien_s.bnk", autoUnload); break;
    }
}

void SoundManager::loadLoResMonsterBank(int type, int autoUnload)
{
    switch (type) {
    case 0x20: m_banks.loadSoundBank(&gSoundBanks[0], "Ape_ls.bnk", autoUnload); break;
    case 0x40: m_banks.loadSoundBank(&gSoundBanks[1], "Robot_ls.bnk", autoUnload); break;
    case 0x60: m_banks.loadSoundBank(&gSoundBanks[2], "Lizrd_ls.bnk", autoUnload); break;
    case 0x80: m_banks.loadSoundBank(&gSoundBanks[3], "Shogn_ls.bnk", autoUnload); break;
    case 0xA0: m_banks.loadSoundBank(&gSoundBanks[4], "Mants_ls.bnk", autoUnload); break;
    case 0xE0: m_banks.loadSoundBank(&gSoundBanks[6], "Energ_ls.bnk", autoUnload); break;
    case 0x100: m_banks.loadSoundBank(&gSoundBanks[7], "Lava_ls.bnk", autoUnload); break;
    case 0x120: m_banks.loadSoundBank(&gSoundBanks[8], "Dragn_ls.bnk", autoUnload); break;
    case 0x140: m_banks.loadSoundBank(&gSoundBanks[9], "Rokmn_ls.bnk", autoUnload); break;
    case 0x160: m_banks.loadSoundBank(&gSoundBanks[10], "Alien_ls.bnk", autoUnload); break;
    }
}

void SoundManager::loadShellSoundBanks(void)
{
    m_reverbOn = 0;
    snd_SetReverbType(2, 0);
    while (snd_FlushSoundCommands())
        ;
    m_banks.unloadAllSoundBanks();
    if (m_vagStreaming == 1) {
        m_vagStreaming = 0;
        snd_CloseVAGStreaming();
    }
    m_banks.loadSoundBank(&gSoundBanks[BANK_SHELLMUS], "Shellmus.bnk", 0);
    m_banks.loadSoundBank(&gSoundBanks[BANK_SHELL], "Shell_s.bnk", 0);
    snd_ResolveBankXREFS();
}
#ifdef NON_MATCHING
/* matches with NON_MATCHING enabled; kept out of the retail build because the switch jump table must come from the retail rodata */
void LevelObjectSoundManager::initLevelObjectSoundManager(void)
{
    m_startTime = timerGetFieldCount();
    m_lastCreak = timerGetFieldCount();
    m_creakDelay = mathfRandf(0.2f, 0.6f);
    switch (shell->m_levelNum) {
    case 12: m_military.initMilitaryLevelSound(); break;
    case 10: m_ufo.initUFOLevelSound(); break;
    case 3: m_canyon2.initCanyon2LevelSound(); break;
    case 8: m_island.initIslandLevelSound(); break;
    case 13: m_canyon.initCanyonLevelSound(); break;
    case 6: m_threeMile.initThreeMileLevelSound(); break;
    case 2: m_vegas.initVegasLevelSound(); break;
    case 7: m_sanFran.initSanFranLevelSound(); break;
    case 5: m_airport.initAirportLevelSound(); break;
    case 15: m_volcano.initVolcanoLevelSound(); break;
    case 9:
    case 16: m_tokyo.initTokyoLevelSound(); break;
    case 1:
    case 4: m_metro.initMetroLevelSound(); break;
    case 26: m_bigToss.initBigTossLevelSound(); break;
    case 11: m_final.initFinalLevelSound(); break;
    }
    for (int i = 0; i < 20; i++) {
        m_destruct[i].handle = 0;
        m_destruct[i].handle2 = 0;
    }
    m_trigger.initTriggerVolumeSound(0x157D, 0x51, 200, 440);
    for (int i = 0; i < 100; i++)
        m_fireHandles[i] = 0;
}
#else
INCLUDE_ASM("asm/nonmatchings/game/Sound", initLevelObjectSoundManager__23LevelObjectSoundManager);
#endif
/* The level's ambience, the collapse loops, and a loop for every active fire once any player came within 570 units (x/y) of
   one. The "near" flag is not reset between fires (in retail too), so after the first near fire all following ones play. */
#ifdef NON_MATCHING
/* equivalent (untuned): 7/225 words, size 0x37c vs 0x384 */
void LevelObjectSoundManager::updateLevelObjectSoundManager(void)
{
    _fvector *fires;
    int vol = 0;
    int pan = 0;
    if (!STREAMING_SOUND()->m_enabled)
        return;
    switch (game->m_levelId) {
    case 12: m_military.updateMilitaryLevelSound(); break;
    case 10: m_ufo.updateUFOLevelSound(); break;
    case 13: m_canyon.updateCanyonLevelSound(); break;
    case 3: m_canyon2.updateCanyon2LevelSound(); break;
    case 8: m_island.updateIslandLevelSound(); break;
    case 15: m_volcano.updateVolcanoLevelSound(); break;
    case 6: m_threeMile.updateThreeMileLevelSound(); break;
    case 2: m_vegas.updateVegasLevelSound(); break;
    case 7: m_sanFran.updateSanFranLevelSound(); break;
    case 5: m_airport.updateAirportLevelSound(); break;
    case 9:
    case 16: m_tokyo.updateTokyoLevelSound(); break;
    case 1:
    case 4: m_metro.updateMetroLevelSound(); break;
    case 26: m_bigToss.updateBigTossLevelSound(); break;
    case 11: m_final.updateFinalLevelSound(); break;
    }
    int near = 0;
    updateDestructLoops();
    m_trigger.updateTriggerVolumeSound();
    int numFires = particleActiveFires(&fires);
    int i;
    for (i = 0; i < numFires; i++) {
        for (int p = 0; p < game->m_numMonsters; p++) {
            _fvector d;
            vecSub(&d, &game->m_monsters[p]->m_cs->trans, &fires[i]);
            if (d.x * d.x + d.y * d.y < 324900.0f) {
                near = 1;
                break;
            }
        }
        if (near) {
            calcDualMonoMixVolPan(&fires[i], 150, 570, vol, pan, 0);
            if (vol > 0) {
                if (m_fireHandles[i] == 0) {
                    m_fireHandles[i] = SOUND_PENDING;
                    snd_PlaySoundVolPanPMPB_CB(gSoundBanks[BANK_AMBIENT], 5, vol, pan, 0, 0, soundPlayCB, &m_fireHandles[i]);
                } else if (m_fireHandles[i] != SOUND_PENDING && m_fireHandles[i] != 0) {
                    snd_SetSoundParams_A(m_fireHandles[i], 5, vol, pan, 0, 0);
                }
            } else {
                gLoopManager.terminateLoop(&m_fireHandles[i]);
            }
        } else {
            gLoopManager.terminateLoop(&m_fireHandles[i]);
        }
    }
    for (i = numFires; i < 100; i++)
        gLoopManager.terminateLoop(&m_fireHandles[i]);
}
#else
INCLUDE_ASM("asm/nonmatchings/game/Sound", updateLevelObjectSoundManager__23LevelObjectSoundManager);
#endif
/* After `delay` seconds the loops fade out linearly over `fade` seconds; the late one-shot plays when the fade starts. */
#ifdef NON_MATCHING
/* equivalent (untuned): 22/141 words, size 0x228 vs 0x234 */
void LevelObjectSoundManager::updateDestructLoops(void)
{
    int vol = 0x400;
    int pan = 0;
    for (int i = 0; i < 20; i++) {
        DestructLoop *d = &m_destruct[i];
        int elapsed = timerGetFieldCount() - d->start;
        if ((d->handle != SOUND_PENDING && d->handle != 0) || (d->handle2 != SOUND_PENDING && d->handle2 != 0)) {
            float t = (float)elapsed;
            if (t < (d->delay + d->fade) * 60.0f) {
                calcDualMonoMixVolPan(&d->pos, d->minDist, d->maxDist, vol, pan, 0);
                if (d->delay * 60.0f < t) {
                    vol -= (int)((t - d->delay * 60.0f) / (d->fade * 60.0f) * (float)vol);
                    if (d->hasSecond) {
                        calcDualMonoMixVolPan(&d->pos2, d->minDist, d->maxDist, vol, pan, 0);
                        snd_PlaySoundVolPanPMPB_A(gSoundBanks[BANK_AMBIENT], 0x10, vol, pan, 0, 0);
                        d->hasSecond = 0;
                    }
                }
                snd_SetSoundParams_A(d->handle, 5, vol, pan, 0, 0);
                snd_SetSoundParams_A(d->handle2, 5, vol, pan, 0, 0);
            } else {
                gLoopManager.terminateLoop(&m_destruct[i].handle);
                gLoopManager.terminateLoop(&m_destruct[i].handle2);
            }
        }
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/game/Sound", updateDestructLoops__23LevelObjectSoundManager);
#endif
#ifdef NON_MATCHING
/* equivalent (untuned): 6/99 words, size 0x17c vs 0x18c */
void LevelObjectSoundManager::triggerLevelDestructSound(_fvector *pos, int minDist, int maxDist, float delay, float fade, int sound,
                                                        ActPlaySound *act)
{
    int vol = 0x400;
    int pan = 0;
    int slot = findAvailableDestructHandle();
    if (slot == -1)
        return;
    calcDualMonoMixVolPan(pos, minDist, maxDist, vol, pan, 0);
    DestructLoop *d = &m_destruct[slot];
    d->minDist = minDist;
    d->maxDist = maxDist;
    d->delay = delay;
    d->fade = fade;
    d->pos = *pos;
    d->start = timerGetFieldCount();
    d->handle = SOUND_PENDING;
    snd_PlaySoundVolPanPMPB_CB(gSoundBanks[BANK_AMBIENT], sound, vol, pan, 0, 0, soundPlayCB, &m_destruct[slot].handle);
    snd_PlaySoundVolPanPMPB_CB(gSoundBanks[BANK_AMBIENT], 0x29, vol, pan, 0, 0, soundPlayCB, &m_destruct[slot].handle2);
    if (act) {
        _fvector p;
        vecAdd(&p, pos, &act->m_offset);
        d->hasSecond = 1;
        d->pos2 = p;
    } else {
        d->hasSecond = 0;
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/game/Sound", triggerLevelDestructSound__23LevelObjectSoundManagerP8_fvectoriiffiP12ActPlaySound);
#endif
void LevelObjectSoundManager::playDebrisImpactSound(_fvector *pos, int type)
{
    int vol = 0x400;
    int pan = 0;
    if ((float)(timerGetFieldCount() - m_startTime) > 24.0f) {
        calcDualMonoMixVolPan(pos, 200, 400, vol, pan, 0);
        if (vol > 0) {
            if ((unsigned)(type - 6) < 2)
                snd_PlaySoundVolPanPMPB_A(gSoundBanks[BANK_AMBIENT], 0x34, vol, pan, 0, 0);
            else
                snd_PlaySoundVolPanPMPB_A(gSoundBanks[BANK_AMBIENT], 0x2E, vol, pan, 0, 0);
        }
    }
}
#ifdef NON_MATCHING
/* equivalent (untuned): 17/53 words, size 0xd0 vs 0xd4 */
void LevelObjectSoundManager::playTimedBuildingDestructibleSound(ActPlaySound *act, _fvector *pos)
{
    int vol = 0x400;
    int pan = 0;
    triggerLevelDestructSound(pos, 1500, 3500, (float)(act->m_time / 60), 0.5f, 15, act);
    calcDualMonoMixVolPan(pos, 1000, 2400, vol, pan, 0);
    snd_PlaySoundVolPanPMPB_A(gSoundBanks[BANK_AMBIENT], 0xB, vol, pan, 0, 0);
}
#else
INCLUDE_ASM("asm/nonmatchings/game/Sound", playTimedBuildingDestructibleSound__23LevelObjectSoundManagerP12ActPlaySoundP8_fvector);
#endif
/* One-shot (and for the buildings, collapse loop) sound of a destructible, by its sound id. */
#ifdef NON_MATCHING
/* equivalent (untuned): 136/374 words, size 0x5d8 vs 0x5d4 */
void LevelObjectSoundManager::playDestructibleSound(unsigned id, _fvector *pos)
{
    int vol = 0x400;
    int pan = 0;
    switch (id) {
    case 1:
    case 0x78:
    case 0x79:
    case 0x7A:
        calcDualMonoMixVolPan(pos, 400, 1500, vol, pan, 0);
        if (vol > 0)
            snd_PlaySoundVolPanPMPB_A(gSoundBanks[BANK_AMBIENT], mathfRand(0, 2) + 0x4C, vol, pan, 0, 0);
        break;
    case 2:
        triggerLevelDestructSound(pos, 400, 1000, 1.0f, 0.6f, 15, 0);
        calcDualMonoMixVolPan(pos, 400, 1000, vol, pan, 0);
        snd_PlaySoundVolPanPMPB_A(gSoundBanks[BANK_AMBIENT], 0xB, vol, pan, 0, 0);
        break;
    case 3:
        triggerLevelDestructSound(pos, 1000, 2400, 1.75f, 0.9f, 15, 0);
        calcDualMonoMixVolPan(pos, 1000, 2400, vol, pan, 0);
        snd_PlaySoundVolPanPMPB_A(gSoundBanks[BANK_AMBIENT], 0xB, vol, pan, 0, 0);
        break;
    case 4:
        triggerLevelDestructSound(pos, 1500, 3500, 2.5f, 1.2f, 15, 0);
        calcDualMonoMixVolPan(pos, 1500, 3500, vol, pan, 0);
        snd_PlaySoundVolPanPMPB_A(gSoundBanks[BANK_AMBIENT], 0xB, vol, pan, 0, 0);
        break;
    case 5:
        calcDualMonoMixVolPan(pos, 1800, 5000, vol, pan, 0);
        if (vol > 0)
            snd_PlaySoundVolPanPMPB_A(gSoundBanks[BANK_AMBIENT], 4, vol, pan, 0, 0);
        break;
    case 0x64:
        calcDualMonoMixVolPan(pos, 400, 1000, vol, pan, 0);
        if (vol > 0)
            snd_PlaySoundVolPanPMPB_A(gSoundBanks[BANK_AMBIENT], 0, vol, pan, 0, 0);
        break;
    case 0x65:
        calcDualMonoMixVolPan(pos, 1000, 2400, vol, pan, 0);
        if (vol > 0)
            snd_PlaySoundVolPanPMPB_A(gSoundBanks[BANK_AMBIENT], 2, vol, pan, 0, 0);
        break;
    case 0x66:
        calcDualMonoMixVolPan(pos, 1500, 3500, vol, pan, 0);
        if (vol > 0)
            snd_PlaySoundVolPanPMPB_A(gSoundBanks[BANK_AMBIENT], 3, vol, pan, 0, 0);
        break;
    case 0x85:
        calcDualMonoMixVolPan(pos, 1500, 3500, vol, pan, 0);
        if (vol > 0)
            snd_PlaySoundVolPanPMPB_A(gSoundBanks[BANK_LEVEL], 3, vol, pan, 0, 0);
        break;
    case 0x86:
        calcDualMonoMixVolPan(pos, 1500, 3500, vol, pan, 0);
        if (vol > 0)
            snd_PlaySoundVolPanPMPB_A(gSoundBanks[BANK_LEVEL], 6, vol, pan, 0, 0);
        break;
    case 0x67:
        calcDualMonoMixVolPan(pos, 1000, 2400, vol, pan, 0);
        if (vol > 0) {
            snd_PlaySoundVolPanPMPB_A(gSoundBanks[BANK_AMBIENT], 6, vol, pan, 0, 0);
            snd_PlaySoundVolPanPMPB_A(gSoundBanks[BANK_AMBIENT], 0x22, vol, pan, 0, 0);
            snd_PlaySoundVolPanPMPB_A(gSoundBanks[BANK_AMBIENT], 0, vol, pan, 0, 0);
        }
        break;
    case 0x7E:
        /* creaks, at most one every m_creakDelay seconds */
        if (m_creakDelay * 60.0f < (float)(timerGetFieldCount() - m_lastCreak)) {
            calcDualMonoMixVolPan(pos, 400, 1000, vol, pan, 0);
            if (vol > 0) {
                m_lastCreak = timerGetFieldCount();
                m_creakDelay = mathfRandf(0.2f, 0.6f);
                snd_PlaySoundVolPanPMPB_A(gSoundBanks[BANK_AMBIENT], 0x2D, vol, pan, 0, 0);
            }
        }
        break;
    case 0x7F:
        calcDualMonoMixVolPan(pos, 400, 1000, vol, pan, 0);
        if (vol > 0)
            snd_PlaySoundVolPanPMPB_A(gSoundBanks[BANK_AMBIENT], 0x21, vol, pan, 0, 0);
        break;
    case 0x81:
        calcDualMonoMixVolPan(pos, 900, 1700, vol, pan, 0);
        if (vol > 0) {
            snd_PlaySoundVolPanPMPB_A(gSoundBanks[BANK_AMBIENT], mathfRand(0, 1) + 0xD, vol, pan, 0, 0);
            snd_PlaySoundVolPanPMPB_A(gSoundBanks[BANK_AMBIENT], 0xC, vol, pan, 0, 0);
        }
        break;
    case 0x87:
        calcDualMonoMixVolPan(pos, 700, 2500, vol, pan, 0);
        if (vol > 0) {
            snd_PlaySoundVolPanPMPB_A(gSoundBanks[BANK_AMBIENT], 8, vol, pan, 0, 0);
            snd_PlaySoundVolPanPMPB_A(gSoundBanks[BANK_AMBIENT], 2, vol, pan, 0, 0);
            snd_PlaySoundVolPanPMPB_A(gSoundBanks[BANK_FINAL], 0xC, vol, pan, 0, 0);
        }
        break;
    case 0x7B:
    case 0x7C:
    case 0x7D:
    case 0x80:
    case 0x83:
    case 0x84:
        calcDualMonoMixVolPan(pos, 900, 1700, vol, pan, 0);
        if (vol > 0) {
            snd_PlaySoundVolPanPMPB_A(gSoundBanks[BANK_AMBIENT], 0xD, vol, pan, 0, 0);
            snd_PlaySoundVolPanPMPB_A(gSoundBanks[BANK_AMBIENT], 0xC, vol, pan, 0, 0);
        }
        break;
    default:
        printf("Invalid Sound ID for Destructible!!\n");
        break;
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/game/Sound", playDestructibleSound__23LevelObjectSoundManagerUiP8_fvector);
#endif
int LevelObjectSoundManager::findAvailableDestructHandle(void)
{
    for (int i = 0; i < 20; i++) {
        if (m_destruct[i].handle == 0)
            return i;
    }
    return -1;
}
void CanyonLevelSound::initCanyonLevelSound(void)
{
    m_ambient.initLevelObjectSound(BANK_LEVEL, 0, 300, 1300, 1600, 0, 11);
}
void CanyonLevelSound::updateCanyonLevelSound(void)
{
    m_ambient.updateLevelObjectSound();
}
void VegasLevelSound::initVegasLevelSound(void)
{
    m_trigger.initTriggerVolumeSound(0x157F, 7, 400, 600);
    animationGetHandle(&m_anim, 0x27D8, 0, 0);
    f0 = 0;
}
void VegasLevelSound::updateVegasLevelSound(void)
{
    float pct = animationGetCurrentPercent(m_anim);
    if (f0 == 0 && pct < 0.5f)
        m_trigger.updateTriggerVolumeSound();
    else
        m_trigger.terminateTriggerVolumeSound();
}
void FinalLevelSound::initFinalLevelSound(void)
{
    m_ambient.initLevelObjectSound(BANK_AMBIENT, 0x56, 1450, 3050, 0, -3215, -32);
}
void FinalLevelSound::updateFinalLevelSound(void)
{
    m_ambient.updateLevelObjectSound();
}
void AirportLevelSound::initAirportLevelSound(void)
{
    m_trigger.initTriggerVolumeSound(0x157E, 4);
    m_wind.initHeightBasedSound(5, 700, -80);
}
void AirportLevelSound::updateAirportLevelSound(void)
{
    m_trigger.updateTriggerVolumeSound();
    m_wind.updateHeightBasedSound();
}
void SanFranLevelSound::initSanFranLevelSound(void)
{
    m_quaking = 0;
    m_quake.initLevelObjectSound(BANK_LEVEL, 0, 10000, 11000, -393, 2034, -514);
    m_ambient[0].initLevelObjectSound(BANK_AMBIENT, 5, 1950, 4100, -3300, 1273, 2000);
    m_ambient[1].initLevelObjectSound(BANK_AMBIENT, 5, 8500, 11650, -3710, 11155, 1908);
    m_ambient[2].initLevelObjectSound(BANK_AMBIENT, 5, 8050, 11350, 7500, 7700, 3317);
    m_trigger.initTriggerVolumeSound(0x157E, 1);
    m_wind.initHeightBasedSound(2, 600, 1);
    m_fires[0].initLevelObjectSound(BANK_LEVEL, 3, 300, 1000, 921, 775, -369);
    m_fires[1].initLevelObjectSound(BANK_LEVEL, 3, 300, 1000, 132, 669, -380);
    m_fires[2].initLevelObjectSound(BANK_LEVEL, 3, 300, 1000, -1355, 639, 978);
    m_fires[3].initLevelObjectSound(BANK_LEVEL, 3, 300, 1000, -1258, 271, 978);
    m_fires[4].initLevelObjectSound(BANK_LEVEL, 3, 300, 1000, -1683, 2519, 497);
    m_fires[5].initLevelObjectSound(BANK_LEVEL, 3, 300, 1000, -717, 3525, 858);
    m_fires[6].initLevelObjectSound(BANK_LEVEL, 3, 300, 1000, 297, 3204, 345);
    m_fires[7].initLevelObjectSound(BANK_LEVEL, 3, 300, 1000, 20, 1984, 532);
    m_fires[8].initLevelObjectSound(BANK_LEVEL, 3, 300, 1000, 576, 1984, 532);
    m_firesOn = 0;
}
/* The quake rumble lasts 1164 fields after startQuakeSound. */
void SanFranLevelSound::updateSanFranLevelSound(void)
{
    if (m_quaking) {
        if ((float)(timerGetFieldCount() - m_quakeStart) < 1164.0f)
            m_quake.updateLevelObjectSound();
        else
            m_quaking = 0;
    } else {
        m_quake.stopLevelObjectSound();
    }
    m_trigger.updateTriggerVolumeSound();
    m_wind.updateHeightBasedSound();
    if (m_firesOn) {
        for (int i = 0; i < 9; i++)
            m_fires[i].updateLevelObjectSound();
    } else {
        for (int i = 0; i < 9; i++)
            m_fires[i].stopLevelObjectSound();
    }
}
/* Computes the volume and pan but plays nothing (in retail too). */
void AirportLevelSound::playPlaneLandingSound(_fvector *pos)
{
    int vol = 0;
    int pan = 0;
    calcDualMonoMixVolPan(pos, 270, 1800, vol, pan, 0);
}
void SanFranLevelSound::startQuakeSound(void)
{
    m_quaking = 1;
    m_quakeStart = timerGetFieldCount();
}
void TokyoLevelSound::initTokyoLevelSound(void)
{
    m_movingOn = 0;
    m_moving.initLevelObjectSound(BANK_LEVEL, 0, 5000, 6000, 0, 0, 0);
    m_trigger.initTriggerVolumeSound(0x157E, 2);
    m_wind.initHeightBasedSound(3, 700, -11);
    animationGetHandle(&m_moveAnim, 0x2744, 0, 0);
    animationGetHandle(&m_anim, 0x2745, 0, 0);
    m_animSound.initLevelObjectSound(BANK_LEVEL, 5, 2000, 4000, 721, -1982, 700);
}
/* m_moving travels along y with m_moveAnim and stops at 85%. */
#ifdef NON_MATCHING
/* equivalent (untuned): 12/82 words, size 0x148 vs 0x144 */
void TokyoLevelSound::updateTokyoLevelSound(void)
{
    _fvector pos;
    pos.x = 938.0f;
    pos.y = -4770.0f;
    pos.z = 0.0f;
    if (m_movingOn) {
        float pct = animationGetCurrentPercent(m_moveAnim);
        pos.y += pct * 8070.0f;
        m_moving.m_pos = pos;
        m_moving.updateLevelObjectSound();
        if (pct > 0.85f)
            m_movingOn = 0;
    } else {
        m_moving.stopLevelObjectSound();
    }
    m_trigger.updateTriggerVolumeSound();
    m_wind.updateHeightBasedSound();
    if (animationIsRunning(m_anim))
        m_animSound.updateLevelObjectSound();
    else
        m_animSound.stopLevelObjectSound();
}
#else
INCLUDE_ASM("asm/nonmatchings/game/Sound", updateTokyoLevelSound__15TokyoLevelSound);
#endif
void BigTossLevelSound::initBigTossLevelSound(void)
{
    m_handle = 0;
    animationGetHandle(&m_anim, 0x27C3, 0, 0);
}
/* Loop 0 from frame 30 of the animation; when it ends, one-shot 1 at the same volume. */
void BigTossLevelSound::updateBigTossLevelSound(void)
{
    _fvector pos;
    int vol;
    int pan;
    pos.x = -40.0f;
    pos.y = 0.0f;
    pos.z = 0.0f;
    calcDualMonoMixVolPan(&pos, 400, 900, vol, pan, 0);
    float frame = animationGetCurrentFrame(m_anim);
    if (vol > 0 && frame > 30.0f) {
        if (m_handle == 0) {
            m_handle = SOUND_PENDING;
            snd_PlaySoundVolPanPMPB_CB(gSoundBanks[BANK_LEVEL], 0, vol, pan, 0, 0, soundPlayCB, this);
        } else if (m_handle != SOUND_PENDING && m_handle != 0) {
            snd_SetSoundParams_A(m_handle, 0xD, vol, pan, 0, 0);
        }
    } else if (m_handle != SOUND_PENDING && m_handle != 0) {
        snd_PlaySoundVolPanPMPB_A(gSoundBanks[BANK_LEVEL], 1, vol, pan, 0, 0);
        gLoopManager.terminateLoop(&m_handle);
    }
}
void ThreeMileLevelSound::initThreeMileLevelSound(void)
{
    m_countdownHandle = 0;
    f224 = 0;
    f218 = 0;
    f21C = 0;
    f220 = 0;
    m_30.initLevelObjectSound(BANK_LEVEL, 0, 950, 3400, 200, 1000, 25);
    m_1E0.initLevelObjectSound(BANK_LEVEL, 2, 500, 1200, -231, 0, 67);
    m_stacks[0].initLevelObjectSound(BANK_LEVEL, 3, 175, 900, -1054, -510, 130);
    m_stacks[1].initLevelObjectSound(BANK_LEVEL, 3, 175, 900, -1288, 18, 130);
    m_stacks[2].initLevelObjectSound(BANK_LEVEL, 3, 175, 900, 640, 543, 130);
    m_stacks[3].initLevelObjectSound(BANK_LEVEL, 3, 175, 900, 612, -578, 130);
    m_stacksDown[0].initLevelObjectSound(BANK_AMBIENT, 0x56, 175, 900, -1054, -510, 130);
    m_stacksDown[1].initLevelObjectSound(BANK_AMBIENT, 0x56, 175, 900, -1288, 18, 130);
    m_stacksDown[2].initLevelObjectSound(BANK_AMBIENT, 0x56, 175, 900, 640, 543, 130);
    m_stacksDown[3].initLevelObjectSound(BANK_AMBIENT, 0x56, 175, 900, 612, -578, 130);
    m_ambient.initLevelObjectSound(BANK_LEVEL, 5, 1100, 1900, -231, 320, 0);
    m_craterHandle = 0;
    m_trigger.initTriggerVolumeSound(0x157E, 6);
    m_wind.initHeightBasedSound(7, 800, -43);
    for (int i = 0; i < 4; i++)
        gStackDestroyed[i] = 0;
}
void ThreeMileLevelSound::setAllStacksDestroyed(void)
{
    for (int i = 0; i < 4; i++)
        gStackDestroyed[i] = 2;
}
void ThreeMileLevelSound::resetReactorSoundInfo(int i)
{
    if (i == 0)
        m_stacks[0].stopLevelObjectSound();
    else if (i == 1)
        m_stacks[1].stopLevelObjectSound();
    else if (i == 2)
        m_stacks[2].stopLevelObjectSound();
    else if (i == 3)
        m_stacks[3].stopLevelObjectSound();
}
/* An intact stack keeps its sound, a destroyed one (1) gets the other loop; 2 silences the destroyed loop. Note that the intact
   loop is not stopped when the stack goes down (resetReactorSoundInfo does that). */
void ThreeMileLevelSound::updateThreeMileLevelSound(void)
{
    if (f218)
        m_30.updateLevelObjectSound();
    else
        m_30.stopLevelObjectSound();
    if (f220)
        m_1E0.updateLevelObjectSound();
    else
        m_1E0.stopLevelObjectSound();
    if (gStackDestroyed[0] == 0)
        m_stacks[0].updateLevelObjectSound();
    else if (gStackDestroyed[0] == 1)
        m_stacksDown[0].updateLevelObjectSound();
    else
        m_stacksDown[0].stopLevelObjectSound();
    if (gStackDestroyed[1] == 0)
        m_stacks[1].updateLevelObjectSound();
    else if (gStackDestroyed[1] == 1)
        m_stacksDown[1].updateLevelObjectSound();
    else
        m_stacksDown[1].stopLevelObjectSound();
    if (gStackDestroyed[2] == 0)
        m_stacks[2].updateLevelObjectSound();
    else if (gStackDestroyed[2] == 1)
        m_stacksDown[2].updateLevelObjectSound();
    else
        m_stacksDown[2].stopLevelObjectSound();
    if (gStackDestroyed[3] == 0)
        m_stacks[3].updateLevelObjectSound();
    else if (gStackDestroyed[3] == 1)
        m_stacksDown[3].updateLevelObjectSound();
    else
        m_stacksDown[3].stopLevelObjectSound();
    if (f21C)
        m_ambient.updateLevelObjectSound();
    else
        m_ambient.stopLevelObjectSound();
    m_trigger.updateTriggerVolumeSound();
    m_wind.updateHeightBasedSound();
    if (craterSwitched && m_craterHandle == 0) {
        m_craterHandle = SOUND_PENDING;
        snd_PlaySoundVolPanPMPB_CB(gSoundBanks[BANK_AMBIENT], 0x56, 0x226, 0, 0, 0, soundPlayCB, &m_craterHandle);
    }
}
#ifdef NON_MATCHING
/* equivalent (untuned): 13/19 words, size 0x48 vs 0x4c */
void ThreeMileLevelSound::playWickedBigExplosion(void)
{
    snd_PlaySoundVolPanPMPB_A(gSoundBanks[BANK_AMBIENT], 4, 0x400, 0, 0, 0);
    STREAMING_SOUND()->m_enabled = 0;
}
#else
INCLUDE_ASM("asm/nonmatchings/game/Sound", playWickedBigExplosion__19ThreeMileLevelSound);
#endif
void ThreeMileLevelSound::playShockSound(_fvector *pos)
{
    int vol = 0;
    int pan = 0;
    calcDualMonoMixVolPan(pos, 300, 1250, vol, pan, 0);
    if (vol > 0)
        snd_PlaySoundVolPanPMPB_A(gSoundBanks[BANK_AMBIENT], 8, vol, pan, 0, 0);
}
/* Spoken countdown from Count_s.bnk: 10, 5..2 are one-shots, 1 is kept in m_countdownHandle; anything else is the level
   bank's sound 1. */
#ifdef NON_MATCHING
/* equivalent (untuned): 59/61 words, size 0xf4 vs 0xf4 */
void ThreeMileLevelSound::playThreeMileCountdown(int n)
{
    if (n == 10)
        snd_PlaySoundVolPanPMPB_A(gSoundBanks[BANK_LEVEL2], 6, 0x400, 0, 0, 0);
    else if (n == 5)
        snd_PlaySoundVolPanPMPB_A(gSoundBanks[BANK_LEVEL2], 5, 0x400, 0, 0, 0);
    else if (n == 4)
        snd_PlaySoundVolPanPMPB_A(gSoundBanks[BANK_LEVEL2], 4, 0x400, 0, 0, 0);
    else if (n == 3)
        snd_PlaySoundVolPanPMPB_A(gSoundBanks[BANK_LEVEL2], 3, 0x400, 0, 0, 0);
    else if (n == 2)
        snd_PlaySoundVolPanPMPB_A(gSoundBanks[BANK_LEVEL2], 2, 0x400, 0, 0, 0);
    else if (n == 1)
        snd_PlaySoundVolPanPMPB_CB(gSoundBanks[BANK_LEVEL2], 1, 0x400, 0, 0, 0, soundPlayCB, &m_countdownHandle);
    else
        snd_PlaySoundVolPanPMPB_A(gSoundBanks[BANK_LEVEL], 1, 0x400, 0, 0, 0);
}
#else
INCLUDE_ASM("asm/nonmatchings/game/Sound", playThreeMileCountdown__19ThreeMileLevelSoundi);
#endif
void VolcanoLevelSound::initVolcanoLevelSound(void)
{
    m_lava.initLevelObjectSound(BANK_LEVEL, 1, 900, 1650, 34, 2000, 300);
    m_wind.initHeightBasedSound(5, 600, 250);
    m_trigger.initTriggerVolumeSound(0x157E, 0);
}
void VolcanoLevelSound::updateVolcanoLevelSound(void)
{
    m_lava.updateLevelObjectSound();
    m_trigger.updateTriggerVolumeSound();
    m_wind.updateHeightBasedSound();
}
void UFOLevelSound::initUFOLevelSound(void)
{
    m_handle = 0;
    animationGetHandle(&m_anim, 1, 0, 0);
    m_left.initLevelObjectSound(BANK_LEVEL, 6, 350, 1000, 0, 833, 25);
    m_right.initLevelObjectSound(BANK_LEVEL, 6, 350, 1000, 0, -833, 25);
}
/* A constant loop, plus the two side sounds during two windows of the animation. */
void UFOLevelSound::updateUFOLevelSound(void)
{
    float pct = animationGetCurrentPercent(m_anim);
    if (m_handle == 0) {
        m_handle = SOUND_PENDING;
        snd_PlaySoundVolPanPMPB_CB(gSoundBanks[BANK_LEVEL], 5, 0x400, 0, 0, 0, soundPlayCB, this);
    } else if (m_handle != SOUND_PENDING && m_handle != 0) {
        snd_SoundIsStillPlaying_CB(m_handle, soundPlayCB, this);
    }
    if ((pct > 0.01f && pct < 0.36f) || (pct > 0.48f && pct < 0.86f)) {
        m_left.updateLevelObjectSound();
        m_right.updateLevelObjectSound();
    } else {
        m_left.stopLevelObjectSound();
        m_right.stopLevelObjectSound();
    }
}
void MetroLevelSound::initMetroLevelSound(void)
{
    m_handle = 0;
}
void MetroLevelSound::updateMetroLevelSound(void)
{
    if (m_handle == 0) {
        m_handle = SOUND_PENDING;
        snd_PlaySoundVolPanPMPB_CB(gSoundBanks[BANK_LEVEL], 5, 0x400, 0, 0, 0, soundPlayCB, this);
    }
}
void MilitaryLevelSound::initMilitaryLevelSound(void)
{
}
void MilitaryLevelSound::updateMilitaryLevelSound(void)
{
}
void IslandLevelSound::initIslandLevelSound(void)
{
    m_surf.initLevelObjectSound(BANK_LEVEL, 1, 900, 1650, -155, 1434, 350);
    m_ambient.initLevelObjectSound(BANK_LEVEL, 1, 275, 650, -1425, 500, 320);
    m_wind.initHeightBasedSound(5, 600, -60);
    m_trigger.initTriggerVolumeSound(0x157E, 0);
    fF0 = 0;
}
/* The wind is updated twice (in retail too). */
void IslandLevelSound::updateIslandLevelSound(void)
{
    m_surf.updateLevelObjectSound();
    m_wind.updateHeightBasedSound();
    m_ambient.updateLevelObjectSound();
    m_wind.updateHeightBasedSound();
    m_trigger.updateTriggerVolumeSound();
}
void Canyon2LevelSound::initCanyon2LevelSound(void)
{
    m_damDestroyed = 0;
    f68 = 0;
    f6C = 0;
}
void Canyon2LevelSound::updateCanyon2LevelSound(void)
{
}
/* Stops both loops; the volume and pan for the dam are computed but nothing is played (in retail too). */
void Canyon2LevelSound::playDamDestroyedSound(void)
{
    _fvector pos;
    int vol;
    int pan;
    m_damDestroyed = 1;
    vol = 0;
    pan = 0;
    m_0.stopLevelObjectSound();
    m_30.stopLevelObjectSound();
    pos.x = 1150.0f;
    pos.y = -1100.0f;
    pos.z = 11.0f;
    calcDualMonoMixVolPan(&pos, 1000, 2000, vol, pan, 0);
}
void HeightBasedSound::initHeightBasedSound(int sound, int top, int bottom)
{
    m_bottom = bottom;
    m_sound = sound;
    m_top = top;
    m_handle = 0;
}
void HeightBasedSound::updateHeightBasedSound(void)
{
    int vol = 0;
    int pan = 0;
    int mute[4];
    for (int i = 0; i < 4; i++)
        mute[i] = 0;
    for (int i = 0; i < 4; i++) {
        if (SOUND_MANAGER()->m_events[i][0].id == 0x157C)
            mute[i] = 1;
    }
    calcDualMonoMixHeightBasedSoundVol((float)m_top, (float)m_bottom, vol, pan, mute);
    if (vol > 0) {
        if (m_handle == 0) {
            m_handle = SOUND_PENDING;
            snd_PlaySoundVolPanPMPB_CB(gSoundBanks[BANK_LEVEL], m_sound, vol, pan, 0, 0, soundPlayCB, this);
        } else if (m_handle != SOUND_PENDING && m_handle != 0) {
            snd_SetSoundParams_A(m_handle, 5, vol, pan, 0, 0);
            snd_SoundIsStillPlaying_CB(m_handle, soundPlayCB, this);
        }
    } else {
        gLoopManager.terminateLoop(&m_handle);
    }
}
void LevelObjectSound::initLevelObjectSound(int bank, int sound, int minDist, int maxDist, int x, int y, int z)
{
    m_maxDist = maxDist;
    m_bank = bank;
    m_sound = sound;
    m_pos.x = x;
    m_pos.y = y;
    m_pos.z = z;
    m_minDist = minDist;
    m_handle = 0;
}
void LevelObjectSound::stopLevelObjectSound(void)
{
    gLoopManager.terminateLoop(&m_handle);
}
void LevelObjectSound::updateLevelObjectSound(void)
{
    int vol = 0;
    int pan = 0;
    calcDualMonoMixVolPan(&m_pos, m_minDist, m_maxDist, vol, pan, 0);
    if (vol > 0) {
        if (m_handle == 0) {
            m_handle = SOUND_PENDING;
            snd_PlaySoundVolPanPMPB_CB(gSoundBanks[m_bank], m_sound, vol, pan, 0, 0, soundPlayCB, this);
        } else if (m_handle != SOUND_PENDING && m_handle != 0) {
            snd_SetSoundParams_A(m_handle, 5, vol, pan, 0, 0);
            snd_SoundIsStillPlaying_CB(m_handle, soundPlayCB, this);
        }
    } else {
        gLoopManager.terminateLoop(&m_handle);
    }
}
void NoPanLevelObjectSound::updateLevelObjectSound(void)
{
    int vol = 0;
    int pan = 0;
    calcDualMonoMixVolPan(&m_pos, m_minDist, m_maxDist, vol, pan, 0);
    if (vol > 0) {
        if (m_handle == 0) {
            m_handle = SOUND_PENDING;
            snd_PlaySoundVolPanPMPB_CB(gSoundBanks[BANK_LEVEL], m_sound, vol, 0, 0, 0, soundPlayCB, this);
        } else if (m_handle != SOUND_PENDING && m_handle != 0) {
            snd_SetSoundParams_A(m_handle, 5, vol, 0, 0, 0);
        }
    } else {
        gLoopManager.terminateLoop(&m_handle);
    }
}
void InvertedLevelObjectSound::updateLevelObjectSound(void)
{
    int vol = 0;
    int pan = 0;
    calcDualMonoMixVolPan(&m_pos, m_minDist, m_maxDist, vol, pan, 0);
    vol = 0x400 - vol;
    if (vol > 0) {
        if (m_handle == 0) {
            m_handle = SOUND_PENDING;
            snd_PlaySoundVolPanPMPB_CB(gSoundBanks[BANK_LEVEL], m_sound, vol, 0, 0, 0, soundPlayCB, this);
        } else if (m_handle != SOUND_PENDING && m_handle != 0) {
            snd_SetSoundParams_A(m_handle, 5, vol, 0, 0, 0);
        }
    } else {
        gLoopManager.terminateLoop(&m_handle);
    }
}
void TankSound::initTankSound(void)
{
    m_handle = 0;
    f0 = 0;
}
/* Engine loop while the tank is alive, moving (or `moving`), flagged 0x10 this frame and not held. */
void TankSound::updateTankSound(TankPickup *tank, int moving)
{
    int vol = 0;
    int pan = 0;
    if (tank->health > 0.0f && (tank->speed > 0.0f || moving) && (tank->flags & 0x10) && tank->heldState == 0) {
        calcDualMonoMixVolPan(&tank->cs->trans, 170, 700, vol, pan, 0);
        if (m_handle == 0) {
            m_handle = SOUND_PENDING;
            snd_PlaySoundVolPanPMPB_CB(gSoundBanks[BANK_LEVEL], 0, vol, pan, 0, 0, soundPlayCB, &m_handle);
        } else if (m_handle != SOUND_PENDING && m_handle != 0) {
            snd_SetSoundParams_A(m_handle, 5, vol, pan, 0, 0);
        }
        tank->flags &= ~0x10;
    } else {
        gLoopManager.terminateLoop(&m_handle);
    }
}
void TankSound::terminateTankSound(void)
{
    gLoopManager.terminateLoop(&m_handle);
}
void TankSound::playTankFireSound(_fvector *pos)
{
    int vol = 0;
    int pan = 0;
    calcDualMonoMixVolPan(pos, 150, 700, vol, pan, 0);
    if (vol > 0)
        snd_PlaySoundVolPanPMPB_A(gSoundBanks[BANK_AMBIENT], 0x2A, vol, pan, 0, 0);
}
void HeliSound::initHeliSound(void)
{
    m_rotorHandle = 0;
    m_gunHandle = 0;
    m_crashHandle = 0;
}
/* Rotor loop with doppler pitch (single player only), the gun loop of type 12 in state 2, and the crash loop in state 4. */
#ifdef NON_MATCHING
/* equivalent (untuned): 20/200 words, size 0x31c vs 0x320 */
void HeliSound::updateHeliSound(HeliPickup *heli)
{
    int vol = 0;
    int pan = 0;
    int speed;
    int pitch = 0;
    if (heli->health > 0.0f && STREAMING_SOUND()->m_enabled == 1) {
        calcDualMonoMixVolPan(&heli->cs->trans, 100, 1200, vol, pan, 0);
        if (vol > 0) {
            if (shell->m_numPlayers == 1) {
                findApproachingSpeed(&heli->cs->trans, &speed, &m_prevDist);
                if (m_rotorHandle == 0)
                    speed = 0;
                pitch = snd_GetDopplerPitchMod(speed);
            }
            if (m_rotorHandle == 0) {
                m_rotorHandle = SOUND_PENDING;
                snd_PlaySoundVolPanPMPB_CB(gSoundBanks[BANK_AMBIENT], 0x1A, vol, pan, 0, 0, soundPlayCB, &m_rotorHandle);
            } else if (m_rotorHandle != SOUND_PENDING && m_rotorHandle != 0) {
                snd_SetSoundParams_A(m_rotorHandle, 0xD, vol, pan, pitch, 0);
            }
            if (heli->m_state == 2 && heli->pickupType == 0xC) {
                if (m_gunHandle == 0) {
                    m_gunHandle = SOUND_PENDING;
                    snd_PlaySoundVolPanPMPB_CB(gSoundBanks[BANK_AMBIENT], 0x54, vol, pan, 0, 0, soundPlayCB, &m_gunHandle);
                } else if (m_gunHandle != SOUND_PENDING && m_gunHandle != 0) {
                    snd_SetSoundParams_A(m_gunHandle, 5, vol, pan, 0, 0);
                }
            } else {
                gLoopManager.terminateLoop(&m_gunHandle);
            }
        } else {
            gLoopManager.terminateLoop(&m_rotorHandle);
            gLoopManager.terminateLoop(&m_gunHandle);
        }
    } else {
        gLoopManager.terminateLoop(&m_rotorHandle);
        gLoopManager.terminateLoop(&m_gunHandle);
    }
    if (heli->m_state == 4) {
        calcDualMonoMixVolPan(&heli->cs->trans, 100, 1200, vol, pan, 0);
        if (vol > 0) {
            if (m_crashHandle == 0) {
                m_crashHandle = SOUND_PENDING;
                snd_PlaySoundVolPanPMPB_CB(gSoundBanks[BANK_AMBIENT], 0x5C, vol, pan, 0, 0, soundPlayCB, &m_crashHandle);
            } else if (m_crashHandle != SOUND_PENDING && m_crashHandle != 0) {
                snd_SetSoundParams_A(m_crashHandle, 5, vol, pan, 0, 0);
            }
        } else {
            gLoopManager.terminateLoop(&m_crashHandle);
        }
    } else {
        gLoopManager.terminateLoop(&m_crashHandle);
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/game/Sound", updateHeliSound__9HeliSoundP10HeliPickup);
#endif
void HeliSound::terminateHeliSound(void)
{
    gLoopManager.terminateLoop(&m_rotorHandle);
    gLoopManager.terminateLoop(&m_gunHandle);
    gLoopManager.terminateLoop(&m_crashHandle);
}
void CarSound::initCarSound(CarPickup *car)
{
    m_car = car;
    m_handle = 0;
}
/* Engine loop of a live car whose flags are exactly 0x10 this frame (not on Three Mile once the crater switched): the level's
   car engine for type 15 on the levels that have one, the generic one for types 2 and 9. Pitch follows doppler and speed. */
#ifdef NON_MATCHING
/* equivalent (untuned): 55/175 words, size 0x2b0 vs 0x2bc */
void CarSound::updateCarSound(void)
{
    int vol = 0;
    int pan = 0;
    int speed;
    int pitch = 0;
    if (m_car->health > 0.0f && m_car->flags == 0x10 && (game->m_levelId != 6 || !craterSwitched)) {
        calcDualMonoMixVolPan(&m_car->cs->trans, 60, 700, vol, pan, 0);
        if (vol > 0) {
            if (shell->m_numPlayers == 1) {
                findApproachingSpeed(&m_car->cs->trans, &speed, &m_prevDist);
                if (m_handle == 0)
                    speed = 0;
                pitch = snd_GetDopplerPitchMod(speed);
            }
            if (m_handle == 0) {
                if (m_car->pickupType == 0xF) {
                    int level = shell->m_levelNum;
                    if (level == 4 || level == 1 || level == 2 || level == 6) {
                        m_handle = SOUND_PENDING;
                        if (shell->m_levelNum == 6)
                            snd_PlaySoundVolPanPMPB_CB(gSoundBanks[BANK_LEVEL], 4, vol, pan, 0, 0, soundPlayCB, &m_handle);
                        else if (shell->m_levelNum == 2)
                            snd_PlaySoundVolPanPMPB_CB(gSoundBanks[BANK_LEVEL], 5, vol, pan, 0, 0, soundPlayCB, &m_handle);
                        else
                            snd_PlaySoundVolPanPMPB_CB(gSoundBanks[BANK_LEVEL], 4, vol, pan, 0, 0, soundPlayCB, &m_handle);
                    }
                } else if (m_car->pickupType == 2 || m_car->pickupType == 9) {
                    m_handle = SOUND_PENDING;
                    snd_PlaySoundVolPanPMPB_CB(gSoundBanks[BANK_AMBIENT], 0x1B, vol, pan, 0, 0, soundPlayCB, &m_handle);
                }
            } else if (m_handle != SOUND_PENDING && m_handle != 0) {
                snd_SetSoundParams_A(m_handle, 0x1D, vol, pan, pitch, (int)(m_car->speed / m_car->m_maxSpeed * 32767.0f));
            }
        } else {
            gLoopManager.terminateLoop(&m_handle);
        }
        m_car->flags &= ~0x10;
    } else {
        gLoopManager.terminateLoop(&m_handle);
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/game/Sound", updateCarSound__8CarSound);
#endif
/* One chance in 31 per call, on the city levels (1, 2, 4). */
#ifdef NON_MATCHING
/* equivalent (untuned): 33/97 words, size 0x16c vs 0x184 */
void CarSound::playHornSound(void)
{
    int vol = 0;
    int pan = 0;
    if (mathfRand(0, 30) != 0)
        return;
    calcDualMonoMixVolPan(&m_car->cs->trans, 60, 700, vol, pan, 0);
    if (vol <= 0)
        return;
    int level = shell->m_levelNum;
    if (level != 4 && level != 1 && level != 2)
        return;
    int type = m_car->pickupType;
    if (type == 0xF || type == 0x1A) {
        if (shell->m_levelNum == 2)
            snd_PlaySoundVolPanPMPB_A(gSoundBanks[BANK_LEVEL], mathfRand(0, 1) + 1, vol, pan, 0, 0);
        else
            snd_PlaySoundVolPanPMPB_A(gSoundBanks[BANK_LEVEL], mathfRand(0, 1), vol, pan, 0, 0);
    } else if (type == 2 || type == 3) {
        if (shell->m_levelNum == 2)
            snd_PlaySoundVolPanPMPB_A(gSoundBanks[BANK_LEVEL], mathfRand(0, 1) + 3, vol, pan, 0, 0);
        else
            snd_PlaySoundVolPanPMPB_A(gSoundBanks[BANK_LEVEL], mathfRand(0, 1) + 2, vol, pan, 0, 0);
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/game/Sound", playHornSound__8CarSound);
#endif
void CarSound::terminateCarSound(void)
{
    gLoopManager.terminateLoop(&m_handle);
}
void PickupSound::initPickupSound(Pickup *p)
{
    m_handle = 0;
    m_turretHandle = 0;
    m_pickup = p;
}
void PickupSound::playBatImpactSound(void)
{
    int vol = 0;
    int pan = 0;
    calcDualMonoMixVolPan(&m_pickup->cs->trans, 700, 1500, vol, pan, 0);
    if (vol > 0)
        snd_PlaySoundVolPanPMPB_A(gSoundBanks[BANK_AMBIENT], 0xE, vol, pan, 0, 0);
}
/* Engine loop of the drivable pickups (types 2, 9, 27 and the dragon heads): while alive and flagged 0x10 this frame; type 27
   also while lying free, the others also while held. */
#ifdef NON_MATCHING
/* equivalent (untuned): 16/139 words, size 0x22c vs 0x21c */
void PickupSound::updatePickupSound(void)
{
    int vol = 0;
    int pan = 0;
    int type = m_pickup->pickupType;
    if (type != 2 && type != 9 && type != 0x1B && !((m_pickup->bits >> 5) & 1))
        return;
    if (m_pickup->health > 0.0f &&
        (m_pickup->flags == 0x10 || (m_pickup->heldState ? m_pickup->pickupType != 0x1B : m_pickup->pickupType == 0x1B))) {
        if (m_pickup->pickupType == 0x1B)
            calcDualMonoMixVolPan(&m_pickup->cs->trans, 350, 1200, vol, pan, 0);
        else
            calcDualMonoMixVolPan(&m_pickup->cs->trans, 60, 700, vol, pan, 0);
        if (vol > 0) {
            if (m_handle == 0) {
                m_handle = SOUND_PENDING;
                if (m_pickup->pickupType == 0x1B)
                    snd_PlaySoundVolPanPMPB_CB(gSoundBanks[BANK_LEVEL], 8, vol, pan, 0, 0, soundPlayCB, &m_handle);
                else if ((m_pickup->bits >> 5) & 1)
                    snd_PlaySoundVolPanPMPB_CB(gSoundBanks[BANK_LEVEL], 7, vol, pan, 0, 0, soundPlayCB, &m_handle);
                else
                    snd_PlaySoundVolPanPMPB_CB(gSoundBanks[BANK_AMBIENT], 0x1B, vol, pan, 0, 0, soundPlayCB, &m_handle);
            } else if (m_handle != SOUND_PENDING && m_handle != 0) {
                snd_SetSoundParams_A(m_handle, 5, vol, pan, 0, 0);
            }
        } else {
            gLoopManager.terminateLoop(&m_handle);
        }
        m_pickup->flags &= ~0x10;
    } else {
        gLoopManager.terminateLoop(&m_handle);
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/game/Sound", updatePickupSound__11PickupSound);
#endif
/* Turret rotation loop while either rate is at least 0.09. Type 1 is heard only while the first player is above z 460, type 0
   only below; type 2 (the boss turret) always. */
#ifdef NON_MATCHING
/* equivalent (untuned): 132/134 words, size 0x218 vs 0x218 */
void PickupSound::updateTurretSound(int type, float a, float b)
{
    int vol = 0;
    int pan = 0;
    if (type == 2 || (type == 1 ? game->m_monsters[0]->m_cs->trans.z > 460.0f : game->m_monsters[0]->m_cs->trans.z < 460.0f))
        calcDualMonoMixVolPan(&m_pickup->cs->trans, 250, 750, vol, pan, 0);
    else
        vol = 0;
    if (vol > 0) {
        if (a < 0.09f && b < 0.09f) {
            gLoopManager.terminateLoop(&m_turretHandle);
        } else if (m_turretHandle == 0) {
            m_turretHandle = SOUND_PENDING;
            if (type == 2) {
                if (assBossActive)
                    snd_PlaySoundVolPanPMPB_CB(gSoundBanks[BANK_MECH], 0xC, vol, pan, 0, 0, soundPlayCB, &m_turretHandle);
                else
                    snd_PlaySoundVolPanPMPB_CB(gSoundBanks[BANK_LEVEL], 5, vol, pan, 0, 0, soundPlayCB, &m_turretHandle);
            } else {
                snd_PlaySoundVolPanPMPB_CB(gSoundBanks[BANK_LEVEL], 1, vol, pan, 0, 0, soundPlayCB, &m_turretHandle);
            }
        } else if (m_turretHandle != SOUND_PENDING && m_turretHandle != 0) {
            snd_SetSoundParams_A(m_turretHandle, 5, vol, pan, 0, 0);
        }
    } else {
        gLoopManager.terminateLoop(&m_turretHandle);
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/game/Sound", updateTurretSound__11PickupSoundiff);
#endif
void PickupSound::terminateTurretSound(void)
{
    gLoopManager.terminateLoop(&m_turretHandle);
}
void PickupSound::terminatePickupSound(void)
{
    gLoopManager.terminateLoop(&m_handle);
}
void PickupSound::playTurretPickupFireSound(int type)
{
    int vol = 0;
    int pan = 0;
    calcDualMonoMixVolPan(&m_pickup->cs->trans, 350, 1000, vol, pan, 0);
    if (vol > 0) {
        if (type == 2) {
            if (assBossActive)
                snd_PlaySoundVolPanPMPB_A(gSoundBanks[BANK_MECH], 0xD, vol, pan, 0, 0);
            else
                snd_PlaySoundVolPanPMPB_A(gSoundBanks[BANK_LEVEL], 4, vol, pan, 0, 0);
        } else {
            snd_PlaySoundVolPanPMPB_A(gSoundBanks[BANK_AMBIENT], 0x1D, vol, pan, 0, 0);
        }
    }
}
ShellSound::ShellSound()
{
    m_sliderHandle = 0;
    m_musicHandle = 0;
    m_musicStopped = 0;
    m_musicOn = 0;
}
void ShellSound::resetShellSoundFlags(void)
{
    m_musicOn = 1;
    m_musicStopped = 0;
    m_musicHandle = 0;
    m_tokenHandle = 0;
    f18 = 0;
}
void ShellSound::updateShellSound(void)
{
    snd_FlushSoundCommands();
    if (m_musicStopped == 0 && f18 == 0 && m_musicOn == 1) {
        if (m_musicHandle == 0) {
            m_musicHandle = SOUND_PENDING;
            snd_PlaySoundVolPanPMPB_CB(gSoundBanks[BANK_SHELLMUS], 0, 0x400, 0, 0, 0, soundPlayCB, this);
        }
    } else {
        gLoopManager.terminateLoop(&m_musicHandle);
    }
}
#ifdef NON_MATCHING
/* equivalent (untuned): 21/28 words, size 0x6c vs 0x70 */
void ShellSound::updateTokenSound(void)
{
    if (m_tokenHandle == 0) {
        m_tokenHandle = SOUND_PENDING;
        snd_PlaySoundVolPanPMPB_CB(gSoundBanks[BANK_AMBIENT], 0x5D, 0x400, 0, 0, 0, soundPlayCB, &m_tokenHandle);
        STREAMING_SOUND()->f18 = 0;
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/game/Sound", updateTokenSound__10ShellSound);
#endif
void ShellSound::playTokenCountFinishedSound(void)
{
    snd_PlaySoundVolPanPMPB_A(gSoundBanks[BANK_AMBIENT], 0x5F, 0x400, 0, 0, 0);
}
#ifdef NON_MATCHING
/* equivalent (untuned): 6/16 words, size 0x3c vs 0x40 */
void ShellSound::stopTokenSound(void)
{
    gLoopManager.terminateLoop(&m_tokenHandle);
    STREAMING_SOUND()->f18 = 1;
}
#else
INCLUDE_ASM("asm/nonmatchings/game/Sound", stopTokenSound__10ShellSound);
#endif
void ShellSound::updateSFXSliderSound(void)
{
    if (m_sliderHandle == 0) {
        m_sliderHandle = SOUND_PENDING;
        snd_PlaySoundVolPanPMPB_CB(gSoundBanks[BANK_SHELL], 0xA, 0x400, 0, 0, 0, soundPlayCB, &m_sliderHandle);
    }
}
void ShellSound::stopSFXSliderSound(void)
{
    gLoopManager.terminateLoop(&m_sliderHandle);
}
void ShellSound::terminateShellSound(void)
{
    gLoopManager.terminateLoop(&m_musicHandle);
    m_musicStopped = 1;
}
/* Sound 1 also stops the menu music. */
void ShellSound::playShellSFXSound(int sound)
{
    if (sound == 1) {
        m_musicStopped = sound;
        gLoopManager.terminateLoop(&m_musicHandle);
    }
    snd_PlaySoundVolPanPMPB_A(gSoundBanks[BANK_SHELL], sound, 0x400, 0, 0, 0);
}
void ShellSound::playSweetUnlockedSound(void)
{
    snd_PlaySoundVolPanPMPB_A(gSoundBanks[BANK_SHELL], 0xB, 0x400, 0, 0, 0);
}
void ShellSound::playPauseMenuSound(int sound)
{
    snd_PlaySoundVolPanPMPB_A(gSoundBanks[BANK_AMBIENT], sound, 0x400, 0, 0, 0);
}
void HudSound::initHudSound(void)
{
}
void HudSound::updateHudSound(void)
{
}
void GenericSound::initGenericSound(int bank, int sound, int minDist, int maxDist)
{
    m_maxDist = maxDist;
    m_bank = bank;
    m_sound = sound;
    m_minDist = minDist;
    m_handle = 0;
}
void GenericSound::terminateGenericSound(void)
{
    gLoopManager.terminateLoop(&m_handle);
}
void GenericSound::updateGenericSound(_fvector *pos)
{
    int vol;
    int pan;
    calcDualMonoMixVolPan(pos, m_minDist, m_maxDist, vol, pan, 0);
    if (vol > 0) {
        if (m_handle == 0) {
            m_handle = SOUND_PENDING;
            snd_PlaySoundVolPanPMPB_CB(gSoundBanks[m_bank], m_sound, vol, pan, 0, 0, soundPlayCB, this);
        } else if (m_handle != SOUND_PENDING && m_handle != 0) {
            snd_SetSoundParams_A(m_handle, 0xD, vol, pan, 0, 0);
        }
    } else if (m_handle != SOUND_PENDING && m_handle != 0) {
        gLoopManager.terminateLoop(&m_handle);
    }
}
void PedSound::initPedSound(void)
{
    m_handle = 0;
}
void PedSound::terminatePedSound(void)
{
    gLoopManager.terminateLoop(&m_handle);
}
#ifdef NON_MATCHING
/* equivalent (untuned): 12/65 words, size 0x104 vs 0x104 */
void PedSound::updatePedSound(_fvector *pos, int count)
{
    int vol;
    int pan;
    if (count < 16) {
        gLoopManager.terminateLoop(&m_handle);
        return;
    }
    calcDualMonoMixVolPan(pos, gInnerPed, gOutterPed, vol, pan, 0);
    if (vol > 0) {
        if (m_handle == 0) {
            m_handle = SOUND_PENDING;
            snd_PlaySoundVolPanPMPB_CB(gSoundBanks[BANK_AMBIENT], 0x14, vol, pan, 0, 0, soundPlayCB, this);
            f4 = 0x14;
        } else if (m_handle != SOUND_PENDING && m_handle != 0) {
            snd_SetSoundParams_A(m_handle, 5, vol, pan, 0, 0);
        }
    } else if (m_handle != SOUND_PENDING && m_handle != 0) {
        gLoopManager.terminateLoop(&m_handle);
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/game/Sound", updatePedSound__8PedSoundP8_fvectori);
#endif
void FireSound::initFireSound(void)
{
    m_handle = 0;
}
void FireSound::terminateFireSound(void)
{
    gLoopManager.terminateLoop(&m_handle);
}
#ifdef NON_MATCHING
/* equivalent (untuned): 63/66 words, size 0x108 vs 0x108 */
void FireSound::updateFireSound(_fvector *pos)
{
    int vol;
    int pan;
    calcDualMonoMixVolPan(pos, 500, 1500, vol, pan, 0);
    if (vol > 0) {
        if (m_handle == 0) {
            snd_PlaySoundVolPanPMPB_A(gSoundBanks[BANK_AMBIENT], 0x1F, vol, pan, 0, 0);
            m_handle = SOUND_PENDING;
            snd_PlaySoundVolPanPMPB_CB(gSoundBanks[BANK_AMBIENT], 5, vol, pan, 0, 0, soundPlayCB, this);
        } else if (m_handle != SOUND_PENDING && m_handle != 0) {
            snd_SetSoundParams_A(m_handle, 0xD, vol, pan, 0, 0);
        }
    } else if (m_handle != SOUND_PENDING && m_handle != 0) {
        gLoopManager.terminateLoop(&m_handle);
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/game/Sound", updateFireSound__9FireSoundP8_fvector);
#endif
void PlaneSound::initPlaneSound(void)
{
    m_engineHandle = 0;
    m_handle4 = 0;
}
void PlaneSound::terminatePlaneSound(void)
{
    gLoopManager.terminateLoop(&m_engineHandle);
}
void PlaneSound::updatePlaneSound(_fvector *pos, int state, float t)
{
    int vol;
    int pan;
    calcDualMonoMixVolPan(pos, 270, 1800, vol, pan, 0);
    if (vol > 0) {
        if (m_engineHandle == 0) {
            m_engineHandle = SOUND_PENDING;
            snd_PlaySoundVolPanPMPB_CB(gSoundBanks[BANK_LEVEL], 2, vol, pan, 0, 0, soundPlayCB, this);
        } else if (m_engineHandle != SOUND_PENDING && m_engineHandle != 0) {
            snd_SetSoundParams_A(m_engineHandle, 0xD, vol, pan, 0, 0);
        }
    } else if (m_engineHandle != SOUND_PENDING && m_engineHandle != 0) {
        gLoopManager.terminateLoop(&m_engineHandle);
        gLoopManager.terminateLoop(&m_handle4);
    }
    if (state == 1 && t < 485.0f && m_handle4 == 0) {
        m_handle4 = SOUND_PENDING;
        snd_PlaySoundVolPanPMPB_CB(gSoundBanks[BANK_LEVEL], 0, vol, pan, 0, 0, soundPlayCB, &m_handle4);
    } else if (m_handle4 != SOUND_PENDING && m_handle4 != 0) {
        snd_SetSoundParams_A(m_handle4, 0xD, vol, pan, 0, 0);
        snd_SoundIsStillPlaying_CB(m_handle4, soundPlayCB, &m_handle4);
    }
    if (state == 6 && t < 1170.0f && m_handle8 == 0) {
        m_handle8 = SOUND_PENDING;
        snd_PlaySoundVolPanPMPB_CB(gSoundBanks[BANK_LEVEL], 3, vol, pan, 0, 0, soundPlayCB, &m_handle8);
    } else if (m_handle8 != SOUND_PENDING && m_handle8 != 0) {
        snd_SetSoundParams_A(m_handle8, 0xD, vol, pan, 0, 0);
        snd_SoundIsStillPlaying_CB(m_handle8, soundPlayCB, &m_handle8);
    }
}
void WeaponSound::initWeaponSound(void)
{
    m_handle = 0;
}
void WeaponSound::terminateWeaponSound(void)
{
    gLoopManager.terminateLoop(&m_handle);
}
#ifdef NON_MATCHING
/* equivalent (untuned): 48/192 words, size 0x2e8 vs 0x300; the switch jump table must come from the retail rodata */
void WeaponSound::updateWeaponSound(Weapon *w)
{
    int vol = 0;
    int pan = 0;
    int pitch = 0;
    if (!w->m_active) {
        gLoopManager.terminateLoop(&m_handle);
        return;
    }
    int type = w->m_type;
    if (type == 26 || type == 27 || type == 33 || type == 35)
        calcDualMonoMixVolPan(&w->m_cs->trans, 170, 700, vol, pan, 0);
    else if (type == 40 || type == 41)
        calcDualMonoMixVolPan(&w->m_cs->trans, 450, 1500, vol, pan, 0);
    else
        calcDualMonoMixVolPan(&w->m_cs->trans, 350, 1000, vol, pan, 0);
    if (vol == 0) {
        gLoopManager.terminateLoop(&m_handle);
        return;
    }
    if (shell->m_numPlayers == 1) {
        int speed;
        findApproachingSpeed(&w->m_cs->trans, &speed, &m_prevDist);
        if (m_handle == 0)
            speed = 0;
        pitch = snd_GetDopplerPitchMod(speed);
    }
    switch (w->m_type) {
    case 5:
        if (m_handle == 0) {
            m_handle = SOUND_PENDING;
            snd_PlaySoundVolPanPMPB_CB(gSoundBanks[8], 8, vol, pan, 0, 0, soundPlayCB, this);
        }
        break;
    case 1:
        if (m_handle == 0) {
            m_handle = SOUND_PENDING;
            snd_PlaySoundVolPanPMPB_CB(gSoundBanks[1], 0x11, vol, pan, 0, 0, soundPlayCB, this);
        }
        break;
    case 13:
        if (m_handle == 0) {
            m_handle = SOUND_PENDING;
            snd_PlaySoundVolPanPMPB_CB(gSoundBanks[1], 7, vol, pan, 0, 0, soundPlayCB, this);
        }
        break;
    case 26:
    case 27:
    case 33:
    case 35:
        if (m_handle == 0) {
            m_handle = SOUND_PENDING;
            snd_PlaySoundVolPanPMPB_CB(gSoundBanks[BANK_AMBIENT], 0x1E, vol, pan, 0, 0, soundPlayCB, this);
        }
        pitch = (int)(pitch * 0.6f);
        break;
    case 39:
    case 40:
    case 41:
        if (m_handle == 0) {
            m_handle = SOUND_PENDING;
            snd_PlaySoundVolPanPMPB_CB(gSoundBanks[BANK_LEVEL], 2, vol, pan, 0, 0, soundPlayCB, this);
        }
        break;
    }
    if (m_handle != SOUND_PENDING && m_handle != 0)
        snd_SetSoundParams_A(m_handle, 0xD, vol, pan, pitch, 0);
}
#else
INCLUDE_ASM("asm/nonmatchings/game/Sound", updateWeaponSound__11WeaponSoundP6Weapon);
#endif
#ifdef NON_MATCHING
/* equivalent (untuned): 117/183 words, size 0x2dc vs 0x2dc; the switch jump table must come from the retail rodata */
void WeaponSound::playWeaponLaunchSound(_fvector *pos, int type)
{
    int vol = 0;
    int pan = 0;
    if (type == 26 || type == 27 || type == 33 || type == 35)
        calcDualMonoMixVolPan(pos, 225, 800, vol, pan, 0);
    else if (type == 30 || type == 31)
        calcDualMonoMixVolPan(pos, 150, 700, vol, pan, 0);
    else if (type == 10 || type == 7)
        calcDualMonoMixVolPan(pos, 700, 2500, vol, pan, 0);
    else
        calcDualMonoMixVolPan(pos, 350, 1000, vol, pan, 0);
    if (vol == 0)
        return;
    switch (type) {
    case 0: snd_PlaySoundVolPanPMPB_A(gSoundBanks[0], 5, vol, pan, 0, 0); break;
    case 1: snd_PlaySoundVolPanPMPB_A(gSoundBanks[1], 0xF, vol, pan, 0, 0); break;
    case 2: snd_PlaySoundVolPanPMPB_A(gSoundBanks[4], 0xA, vol, pan, 0, 0); break;
    case 3: snd_PlaySoundVolPanPMPB_A(gSoundBanks[2], 5, vol, pan, 0, 0); break;
    case 4: snd_PlaySoundVolPanPMPB_A(gSoundBanks[9], 9, vol, pan, 0, 0); break;
    case 5: snd_PlaySoundVolPanPMPB_A(gSoundBanks[8], 9, vol, pan, 0, 0); break;
    case 6: snd_PlaySoundVolPanPMPB_A(gSoundBanks[3], 5, vol, pan, 0, 0); break;
    case 7: snd_PlaySoundVolPanPMPB_A(gSoundBanks[BANK_FINAL], 0x17, vol, pan, 0, 0); break;
    case 8: snd_PlaySoundVolPanPMPB_A(gSoundBanks[6], 5, vol, pan, 0, 0); break;
    case 9: snd_PlaySoundVolPanPMPB_A(gSoundBanks[7], 3, vol, pan, 0, 0); break;
    case 10: snd_PlaySoundVolPanPMPB_A(gSoundBanks[BANK_MECH], 3, vol, pan, 0, 0); break;
    case 11: snd_PlaySoundVolPanPMPB_A(gSoundBanks[10], 0, vol, pan, 0, 0); break;
    case 13: snd_PlaySoundVolPanPMPB_A(gSoundBanks[1], 6, vol, pan, 0, 0); break;
    case 21: snd_PlaySoundVolPanPMPB_A(gSoundBanks[7], 9, vol, pan, 0, 0); break;
    case 22: snd_PlaySoundVolPanPMPB_A(gSoundBanks[7], 0xE, vol, pan, 0, 0); break;
    case 26:
    case 27:
    case 33:
    case 35: snd_PlaySoundVolPanPMPB_A(gSoundBanks[BANK_AMBIENT], 0x1C, vol, pan, 0, 0); break;
    case 29:
        /* level 3: the ass boss's own sound when he is out, else the level's; level 10: the ambience one */
        if (shell->m_levelNum == 3) {
            if (assBossActive)
                snd_PlaySoundVolPanPMPB_A(gSoundBanks[BANK_MECH], 0xD, vol, pan, 0, 0);
            else
                snd_PlaySoundVolPanPMPB_A(gSoundBanks[BANK_LEVEL], 4, vol, pan, 0, 0);
        } else if (shell->m_levelNum == 10) {
            snd_PlaySoundVolPanPMPB_A(gSoundBanks[BANK_AMBIENT], 0x1D, vol, pan, 0, 0);
        }
        break;
    case 30:
    case 31: snd_PlaySoundVolPanPMPB_A(gSoundBanks[BANK_AMBIENT], 0x2A, vol, pan, 0, 0); break;
    case 32:
    case 34: snd_PlaySoundVolPanPMPB_A(gSoundBanks[BANK_AMBIENT], 0x55, vol, pan, 0, 0); break;
    case 36:
    case 37:
    case 38: snd_PlaySoundVolPanPMPB_A(gSoundBanks[BANK_AMBIENT], 0x1D, vol, pan, 0, 0); break;
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/game/Sound", playWeaponLaunchSound__11WeaponSoundP8_fvectori);
#endif
#ifdef NON_MATCHING
/* equivalent (untuned): 85/203 words, size 0x32c vs 0x314 (retail shares one call for all cases); the switch jump table must come from the retail rodata */
void WeaponSound::playWeaponExplosion(_fvector *pos, int type, int hit)
{
    int vol = 0;
    int pan = 0;
    if (type == 21 || type == 22 || type == 39 || type == 40 || type == 41)
        calcDualMonoMixVolPan(pos, 350, 1600, vol, pan, 0);
    else if (type == 13)
        calcDualMonoMixVolPan(pos, 850, 2500, vol, pan, 0);
    else
        calcDualMonoMixVolPan(pos, 375, 1500, vol, pan, 0);
    if (vol == 0)
        return;
    switch (type) {
    case 0: snd_PlaySoundVolPanPMPB_A(gSoundBanks[0], 6, vol, pan, 0, 0); break;
    case 1:
    case 24: snd_PlaySoundVolPanPMPB_A(gSoundBanks[1], 0x10, vol, pan, 0, 0); break;
    case 2: snd_PlaySoundVolPanPMPB_A(gSoundBanks[4], 0xB, vol, pan, 0, 0); break;
    case 3: snd_PlaySoundVolPanPMPB_A(gSoundBanks[2], 6, vol, pan, 0, 0); break;
    case 4: snd_PlaySoundVolPanPMPB_A(gSoundBanks[9], 8, vol, pan, 0, 0); break;
    case 5: snd_PlaySoundVolPanPMPB_A(gSoundBanks[8], 7, vol, pan, 0, 0); break;
    case 6: snd_PlaySoundVolPanPMPB_A(gSoundBanks[3], 6, vol, pan, 0, 0); break;
    case 7: snd_PlaySoundVolPanPMPB_A(gSoundBanks[BANK_FINAL], 0x18, vol, pan, 0, 0); break;
    case 8: snd_PlaySoundVolPanPMPB_A(gSoundBanks[6], 0x10, vol, pan, 0, 0); break;
    case 9: snd_PlaySoundVolPanPMPB_A(gSoundBanks[7], 4, vol, pan, 0, 0); break;
    case 10: snd_PlaySoundVolPanPMPB_A(gSoundBanks[BANK_AMBIENT], 0, vol, pan, 0, 0); break;
    case 11: snd_PlaySoundVolPanPMPB_A(gSoundBanks[10], 9, vol, pan, 0, 0); break;
    case 13: snd_PlaySoundVolPanPMPB_A(gSoundBanks[1], 5, vol, pan, 0, 0); break;
    case 21:
    case 22: snd_PlaySoundVolPanPMPB_A(gSoundBanks[7], 0xA, vol, pan, 0, 0); break;
    case 23:
        if (hit)
            snd_PlaySoundVolPanPMPB_A(gSoundBanks[BANK_AMBIENT], 0x2F, vol, pan, 0, 0);
        break;
    case 26:
    case 27: snd_PlaySoundVolPanPMPB_A(gSoundBanks[BANK_AMBIENT], 0, vol, pan, 0, 0); break;
    case 28:
    case 29:
        if (shell->m_levelNum == 3)
            snd_PlaySoundVolPanPMPB_A(gSoundBanks[BANK_AMBIENT], 1, vol, pan, 0, 0);
        else if (shell->m_levelNum == 10)
            snd_PlaySoundVolPanPMPB_A(gSoundBanks[BANK_LEVEL], 7, vol, pan, 0, 0);
        break;
    case 30:
    case 31:
    case 32: snd_PlaySoundVolPanPMPB_A(gSoundBanks[BANK_AMBIENT], 1, vol, pan, 0, 0); break;
    case 33:
    case 34:
    case 35: snd_PlaySoundVolPanPMPB_A(gSoundBanks[BANK_AMBIENT], 1, vol, pan, 0, 0); break;
    case 36:
    case 37:
    case 38:
        switch (shell->m_levelNum) {
        case 9:
        case 16: snd_PlaySoundVolPanPMPB_A(gSoundBanks[BANK_LEVEL], 4, vol, pan, 0, 0); break;
        case 10: snd_PlaySoundVolPanPMPB_A(gSoundBanks[BANK_LEVEL], 7, vol, pan, 0, 0); break;
        }
        break;
    case 39:
    case 40:
    case 41: snd_PlaySoundVolPanPMPB_A(gSoundBanks[BANK_AMBIENT], 3, vol, pan, 0, 0); break;
    case 42:
    case 44: snd_PlaySoundVolPanPMPB_A(gSoundBanks[BANK_PLANT], 0xA, vol, pan, 0, 0); break;
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/game/Sound", playWeaponExplosion__11WeaponSoundP8_fvectorii);
#endif
void WeaponSound::playRockSpecialImpact(_fvector *pos)
{
    int vol = 0;
    int pan = 0;
    gLoopManager.terminateLoop(&m_handle);
    calcDualMonoMixVolPan(pos, 375, 1500, vol, pan, 0);
    if (vol)
        snd_PlaySoundVolPanPMPB_A(gSoundBanks[9], 0xC, vol, pan, 0, 0);
}
MonsterSound::MonsterSound()
{
    m_anims[0].proc = 0;
    m_anims[1].proc = 0;
    m_anims[2].proc = 0;
    m_anims[3].proc = 0;
    m_monster = 0;
}
#ifdef NON_MATCHING
/* equivalent (untuned): 87/104 words, size 0x1a0 vs 0x1a0 */
void MonsterSound::initMonsterSound(Monster *m)
{
    f1E0 = timerGetFieldCount();
    m_inhaleTime = timerGetFieldCount();
    m_bodyImpactTime = timerGetFieldCount();
    m_squishTime = timerGetFieldCount();
    f1FC = timerGetFieldCount();
    f1D8 = timerGetFieldCount();
    m_monster = m;
    f1F8 = 0;
    m_zapHandle2 = 0;
    m_inhaleHandle = 0;
    m_mechLaserImpactHandle = 0;
    f204 = 0;
    m_breathHandle = 0;
    m_rotorHandle = 0;
    m_assBossSpinHandle = 0;
    m_rotorHandle2 = 0;
    m_grappleHandle = 0;
    m_jellyTazerHandle = 0;
    m_dashHandle2 = 0;
    m_dashHandle = 0;
    f3C = 0;
    m_reticleHandle = 0;
    m_zapHandle = 0;
    m_shogunSpecialHandle = 0;
    m_flyHandle = 0;
    f60 = 0;
    f64 = 0;
    f68 = 0;
    f220 = 0;
    f21C = 0;
    f2C = 0;
    f28 = 0;
    m_runHandle = 0;
    f70 = 0;
    f74 = 0;
    m_pickupHandle = 0;
    f1EC = 1000;
    m_enabled = 1;
    f44 = timerGetFieldCount();
    m_knockedBack = 0;
    f1E4 = timerGetFieldCount() - 6000;
    f218 = 0;
    f4 = 0;
    f8 = 0;
    m_knockBackTime = timerGetFieldCount();
    f48 = timerGetFieldCount();
    f1DC = timerGetFieldCount();
    m_footstepTime = timerGetFieldCount();
    m_fire.initFireSound();
    animationGetHandle(&m_anims[0], m_monster->m_typeBits, m_monster->m_dupId, 0xCF);
    animationGetHandle(&m_anims[1], m_monster->m_typeBits, m_monster->m_dupId, 0xD0);
    animationGetHandle(&m_anims[2], m_monster->m_typeBits, m_monster->m_dupId, 0x82);
    animationGetHandle(&m_anims[3], m_monster->m_typeBits, m_monster->m_dupId, 0xCE);
    m_cinemaPlayed[3] = 0;
    m_cinemaPlayed[0] = 0;
    m_cinemaPlayed[1] = 0;
    m_cinemaPlayed[2] = 0;
}
#else
INCLUDE_ASM("asm/nonmatchings/game/Sound", initMonsterSound__12MonsterSoundP7Monster);
#endif
void MonsterSound::updateMonsterSound(void)
{
    snd_FlushSoundCommands();
    if (!m_enabled)
        return;
    int type = m_monster->m_typeBits;
    if (type == 0x40)
        updateRobotSound();
    else if (type == 0x180)
        updateOgreSound();
    else if (type == 0x160)
        updateAlienSound();
    else if (type == 0x100)
        updateLavaSound();
    else if (type == 0x80)
        updateShogunSound();
    else if (type == 0xA0)
        updateMantisSound();
    updateGeneralMonsterSounds();
}
/* In cinemas, each of the four cinema animations plays its sound once when it starts running. */
#ifdef NON_MATCHING
/* equivalent (untuned): 23/127 words, size 0x1ec vs 0x1fc */
void MonsterSound::updateMonsterCinemaSound(void)
{
    if (animationIsRunning(m_anims[1]) && !m_cinemaPlayed[0]) {
        STREAMING_SOUND()->m_enabled = 1;
        m_cinemaPlayed[0] = 1;
        playSignatureSound();
        STREAMING_SOUND()->m_enabled = 0;
    }
    if (animationIsRunning(m_anims[0]) && !m_cinemaPlayed[2]) {
        STREAMING_SOUND()->m_enabled = 1;
        m_cinemaPlayed[2] = 1;
        playSignatureSound();
        STREAMING_SOUND()->m_enabled = 0;
    }
    if (animationIsRunning(m_anims[2]) && !m_cinemaPlayed[1]) {
        STREAMING_SOUND()->m_enabled = 1;
        m_cinemaPlayed[1] = 1;
        playVictorySound(0x82);
        STREAMING_SOUND()->m_enabled = 0;
    }
    if (animationIsRunning(m_anims[3]) && !m_cinemaPlayed[3]) {
        STREAMING_SOUND()->m_enabled = 1;
        m_cinemaPlayed[3] = 1;
        play206Sound();
        STREAMING_SOUND()->m_enabled = 0;
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/game/Sound", updateMonsterCinemaSound__12MonsterSound);
#endif
INCLUDE_ASM("asm/nonmatchings/game/Sound", updateGeneralMonsterSounds__12MonsterSound);
/* Animation sound events: frames below 10 are footsteps, the rest landings. */
void MonsterSound::playSoundFromAnimation(float frame)
{
    if (frame < 10.0f)
        playFootstepSound();
    else
        playLandingSound();
}
INCLUDE_ASM("asm/nonmatchings/game/Sound", playSignatureSound__12MonsterSound);
INCLUDE_ASM("asm/nonmatchings/game/Sound", playTakeHitSound__12MonsterSound);
void MonsterSound::playCloakingSound(void)
{
    int vol = 0;
    int pan = 0;
    calcDualMonoMixVolPan(&m_monster->m_cs->trans, 700, 1500, vol, pan, 0);
    if (vol > 0)
        snd_PlaySoundVolPanPMPB_A(gSoundBanks[BANK_AMBIENT], 0x28, vol, pan, 0, 0);
}
void MonsterSound::playEnterCounterBlockSound(void)
{
    int vol = 0;
    int pan = 0;
    if (m_monster->m_typeBits == 0x60) {
        calcDualMonoMixVolPan(&m_monster->m_cs->trans, 700, 1500, vol, pan, 0);
        if (vol > 0)
            snd_PlaySoundVolPanPMPB_A(gSoundBanks[2], 0x11, vol, pan, 0, 0);
    }
}
void MonsterSound::playGeneratorTazerSound(void)
{
    int vol = 0;
    int pan = 0;
    calcDualMonoMixVolPan(&m_monster->m_cs->trans, 700, 1500, vol, pan, 0);
    if (vol > 0)
        snd_PlaySoundVolPanPMPB_A(gSoundBanks[BANK_AMBIENT], 9, vol, pan, 0, 0);
}
void MonsterSound::playJellyTazerOneShot(void)
{
    int vol = 0;
    int pan = 0;
    calcDualMonoMixVolPan(&m_monster->m_cs->trans, 700, 2500, vol, pan, 0);
    if (vol > 0)
        snd_PlaySoundVolPanPMPB_A(gSoundBanks[BANK_AMBIENT], 9, vol, pan, 0, 0);
}
void MonsterSound::playShockedSound(void)
{
    int vol = 0;
    int pan = 0;
    calcDualMonoMixVolPan(&m_monster->m_cs->trans, 700, 1500, vol, pan, 0);
    if (vol > 0)
        snd_PlaySoundVolPanPMPB_A(gSoundBanks[BANK_AMBIENT], 0xA, vol, pan, 0, 0);
}
/* At most one squish every 12 fields. */
void MonsterSound::playPedSquishSound(void)
{
    int vol = 0;
    int pan = 0;
    if ((float)(timerGetFieldCount() - m_squishTime) > 12.0f) {
        calcDualMonoMixVolPan(&m_monster->m_cs->trans, 70, 70, vol, pan, 0);
        if (vol > 0) {
            snd_PlaySoundVolPanPMPB_A(gSoundBanks[BANK_AMBIENT], 0x2C, vol, pan, 0, 0);
            m_squishTime = timerGetFieldCount();
        }
    }
}
void MonsterSound::playRockmanSpecialLaunch(void)
{
    int vol = 0;
    int pan = 0;
    calcDualMonoMixVolPan(&m_monster->m_cs->trans, 350, 1000, vol, pan, 0);
    if (vol > 0)
        snd_PlaySoundVolPanPMPB_A(gSoundBanks[9], 0xA, vol, pan, 0, 0);
}
/* pos is unused: the sound plays at the monster. */
void MonsterSound::playRockmanSpecialBounce(_fvector *pos)
{
    int vol = 0;
    int pan = 0;
    calcDualMonoMixVolPan(&m_monster->m_cs->trans, 350, 1000, vol, pan, 0);
    if (vol > 0) {
        snd_PlaySoundVolPanPMPB_A(gSoundBanks[BANK_AMBIENT], 0x46, vol, pan, 0, 0);
        snd_PlaySoundVolPanPMPB_A(gSoundBanks[BANK_AMBIENT], 0xD, vol, pan, 0, 0);
    }
}
void MonsterSound::playCounterAttackSound(void)
{
    int vol = 0;
    int pan = 0;
    calcDualMonoMixVolPan(&m_monster->m_cs->trans, 700, 1500, vol, pan, 0);
    if (vol > 0)
        snd_PlaySoundVolPanPMPB_A(gSoundBanks[BANK_AMBIENT], 0x33, vol, pan, 0, 0);
}
void MonsterSound::playRockmanSpecialExplode(_fvector *pos)
{
    int vol = 0;
    int pan = 0;
    calcDualMonoMixVolPan(pos, 350, 1000, vol, pan, 0);
    if (vol > 0)
        snd_PlaySoundVolPanPMPB_A(gSoundBanks[9], 0xC, vol, pan, 0, 0);
}
void MonsterSound::updateClimbingSound(MonsterAnim anim)
{
}
void MonsterSound::updateJellyTazer(void)
{
    int vol = 0;
    int pan = 0;
    calcDualMonoMixVolPan(&m_monster->m_cs->trans, 700, 2500, vol, pan, 0);
    if (m_jellyTazerHandle == 0) {
        m_jellyTazerHandle = SOUND_PENDING;
        snd_PlaySoundVolPanPMPB_CB(gSoundBanks[BANK_FINAL], 0xE, vol, pan, 0, 0, soundPlayCB, &m_jellyTazerHandle);
    } else if (m_jellyTazerHandle != SOUND_PENDING && m_jellyTazerHandle != 0) {
        snd_SetSoundParams_A(m_jellyTazerHandle, 5, vol, pan, 0, 0);
    }
}
void MonsterSound::terminateJellyTazer(void)
{
    gLoopManager.terminateLoop(&m_jellyTazerHandle);
}
void MonsterSound::terminateDashSound(void)
{
    gLoopManager.terminateLoop(&m_dashHandle);
    gLoopManager.terminateLoop(&m_dashHandle2);
}
void MonsterSound::playRamSound(void)
{
    int vol = 0;
    int pan = 0;
    calcDualMonoMixVolPan(&m_monster->m_cs->trans, 700, 1500, vol, pan, 0);
    if (vol > 0)
        snd_PlaySoundVolPanPMPB_A(gSoundBanks[BANK_AMBIENT], 0x38, vol, pan, 0, 0);
}
void MonsterSound::updateDashSound(void)
{
    int vol = 0;
    int pan = 0;
    calcDualMonoMixVolPan(&m_monster->m_cs->trans, 700, 1500, vol, pan, 0);
    int type = m_monster->m_typeBits;
    if (type == 0x80 || type == 0x160 || type == 0x40) {
        if (vol > 0) {
            if (m_dashHandle2 == 0) {
                m_dashHandle2 = SOUND_PENDING;
                snd_PlaySoundVolPanPMPB_CB(gSoundBanks[BANK_AMBIENT], 0x5A, vol, pan, 0, 0, soundPlayCB, &m_dashHandle2);
            } else if (m_dashHandle2 != SOUND_PENDING && m_dashHandle2 != 0) {
                snd_SetSoundParams_A(m_dashHandle2, 5, vol, pan, 0, 0);
            }
        } else {
            gLoopManager.terminateLoop(&m_dashHandle2);
        }
    }
    if (m_monster->m_typeBits == 0xA0) {
        if (vol > 0) {
            if (m_dashHandle == 0) {
                m_dashHandle = SOUND_PENDING;
                snd_PlaySoundVolPanPMPB_CB(gSoundBanks[4], 0x1C, vol, pan, 0, 0, soundPlayCB, &m_dashHandle);
            } else if (m_dashHandle != SOUND_PENDING && m_dashHandle != 0) {
                snd_SetSoundParams_A(m_dashHandle, 5, vol, pan, 0, 0);
            }
        } else {
            gLoopManager.terminateLoop(&m_dashHandle);
        }
    }
}
void MonsterSound::updateOgreSound(void)
{
}
#ifdef NON_MATCHING
/* equivalent (untuned): 48/111 words, size 0x1b8 vs 0x1bc */
void MonsterSound::updateRobotSound(void)
{
    int vol = 0;
    int pan = 0;
    Monster *m = m_monster;
    int *state = m->m_state;
    if (*state == 4 && m_enabled && state[6] != 9) {
        calcDualMonoMixVolPan(&m->m_cs->trans, 700, 1500, vol, pan, 0);
        if (vol > 0) {
            if (m_runHandle == 0) {
                m_runHandle = SOUND_PENDING;
                snd_PlaySoundVolPanPMPB_CB(gSoundBanks[1], 3, vol, pan, 0, 0, soundPlayCB, &m_runHandle);
            } else if (m_runHandle != SOUND_PENDING && m_runHandle != 0) {
                snd_SetSoundParams_A(m_runHandle, 5, vol, pan, 0, 0);
            }
        } else {
            gLoopManager.terminateLoop(&m_runHandle);
        }
    } else {
        gLoopManager.terminateLoop(&m_runHandle);
    }
    m = m_monster;
    if (*m->m_state == 0x1F) {
        calcDualMonoMixVolPan(&m->m_cs->trans, 700, 1500, vol, pan, 0);
        if (vol > 0) {
            if (f74 == 0) {
            } else if (f74 != SOUND_PENDING && f74 != 0) {
                snd_SetSoundParams_A(f74, 5, vol, pan, 0, 0);
            }
        } else {
            gLoopManager.terminateLoop(&f74);
        }
    } else {
        gLoopManager.terminateLoop(&f74);
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/game/Sound", updateRobotSound__12MonsterSound);
#endif
/* Wing loop while the mantis is in state 1 (flying). */
void MonsterSound::updateMantisSound(void)
{
    int vol = 0;
    int pan = 0;
    Monster *m = m_monster;
    if (*m->m_state == 1 && MI(m, 0x6CB0) == 0) {
        calcDualMonoMixVolPan(&m->m_cs->trans, 700, 1500, vol, pan, 0);
        if (vol > 0) {
            if (m_flyHandle == 0) {
                m_flyHandle = SOUND_PENDING;
                snd_PlaySoundVolPanPMPB_CB(gSoundBanks[4], 0x13, vol, pan, 0, 0, soundPlayCB, &m_flyHandle);
            } else if (m_flyHandle != SOUND_PENDING && m_flyHandle != 0) {
                snd_SetSoundParams_A(m_flyHandle, 5, vol, pan, 0, 0);
            }
        } else {
            gLoopManager.terminateLoop(&m_flyHandle);
        }
    } else {
        gLoopManager.terminateLoop(&m_flyHandle);
    }
}
#ifdef NON_MATCHING
/* equivalent (untuned): 72/73 words, size 0x124 vs 0x124 */
void MonsterSound::updateShogunSound(void)
{
    int vol = 0;
    int pan = 0;
    Monster *m = m_monster;
    int *state = m->m_state;
    if ((*state == 4 || *state == 0x26) && m_enabled) {
        if (state[6] != 9) {
            calcDualMonoMixVolPan(&m->m_cs->trans, 700, 1500, vol, pan, 0);
            if (vol > 0) {
                if (m_runHandle == 0) {
                    m_runHandle = SOUND_PENDING;
                    snd_PlaySoundVolPanPMPB_CB(gSoundBanks[3], 1, vol, pan, 0, 0, soundPlayCB, &m_runHandle);
                } else if (m_runHandle != SOUND_PENDING && m_runHandle != 0) {
                    snd_SetSoundParams_A(m_runHandle, 5, vol, pan, 0, 0);
                }
                return;
            }
        }
        gLoopManager.terminateLoop(&m_runHandle);
    } else {
        gLoopManager.terminateLoop(&m_runHandle);
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/game/Sound", updateShogunSound__12MonsterSound);
#endif
/* The inhale loop bends up over its first 180 fields. */
void MonsterSound::updateLavaSpecialInhaleChargeup(void)
{
    int vol = 0;
    int pan = 0;
    calcDualMonoMixVolPan(&m_monster->m_cs->trans, 350, 1000, vol, pan, 0);
    if (vol > 0) {
        if (m_inhaleHandle == 0) {
            int t = timerGetFieldCount() - m_inhaleTime;
            int bend = 0x7FFF;
            if (t < 180)
                bend = (int)(t / 180.0f * 32767.0f);
            m_inhaleHandle = SOUND_PENDING;
            snd_PlaySoundVolPanPMPB_CB(gSoundBanks[7], 0xC, vol, pan, 0, bend, soundPlayCB, &m_inhaleHandle);
        } else if (m_inhaleHandle != SOUND_PENDING && m_inhaleHandle != 0) {
            int t = timerGetFieldCount() - m_inhaleTime;
            int bend;
            if (t < 180)
                bend = (int)(t / 180.0f * 32767.0f);
            else
                bend = 0x7FFF;
            snd_SetSoundParams_A(m_inhaleHandle, 0x15, vol, pan, 0, bend);
        }
    } else {
        gLoopManager.terminateLoop(&m_inhaleHandle);
    }
}
void MonsterSound::terminateLavaSpecialInhaleChargeup(void)
{
    gLoopManager.terminateLoop(&m_inhaleHandle);
}
void MonsterSound::updateLavaSound(void)
{
}
void MonsterSound::updateAlienSound(void)
{
    int vol = 0;
    int pan = 0;
    if (m_grappleHandle != SOUND_PENDING && m_grappleHandle != 0) {
        calcDualMonoMixVolPan(&m_monster->m_cs->trans, 700, 1500, vol, pan, 0);
        snd_SetSoundParams_A(m_grappleHandle, 5, vol, pan, 0, 0);
    }
    Monster *m = m_monster;
    if (*m->m_state == 1 && MI(m, 0x6CB0) == 0) {
        calcDualMonoMixVolPan(&m->m_cs->trans, 700, 1500, vol, pan, 0);
        if (vol > 0) {
            if (m_flyHandle == 0) {
                m_flyHandle = SOUND_PENDING;
                snd_PlaySoundVolPanPMPB_CB(gSoundBanks[10], 0x17, vol, pan, 0, 0, soundPlayCB, &m_flyHandle);
            } else if (m_flyHandle != SOUND_PENDING && m_flyHandle != 0) {
                snd_SetSoundParams_A(m_flyHandle, 5, vol, pan, 0, 0);
            }
        } else {
            gLoopManager.terminateLoop(&m_flyHandle);
        }
    } else {
        gLoopManager.terminateLoop(&m_flyHandle);
    }
}
#ifdef NON_MATCHING
/* equivalent (untuned): 20/81 words, size 0x140 vs 0x144 */
void MonsterSound::updateFireBreath(void)
{
    int vol = 0;
    int pan = 0;
    Monster *m = m_monster;
    if (MI(m, 0x68C0)) {
        calcDualMonoMixVolPan(&m->m_cs->trans, 100, 1200, vol, pan, 0);
        switch (m_monster->m_typeBits) {
        case 0x120:
            if (m_breathHandle == 0) {
                m_breathHandle = SOUND_PENDING;
                snd_PlaySoundVolPanPMPB_CB(gSoundBanks[8], 0xA, vol, pan, 0, 0, soundPlayCB, &m_breathHandle);
            }
            break;
        case 0x60:
            if (m_breathHandle == 0) {
                m_breathHandle = SOUND_PENDING;
                snd_PlaySoundVolPanPMPB_CB(gSoundBanks[2], 7, vol, pan, 0, 0, soundPlayCB, &m_breathHandle);
            }
            break;
        }
        if (m_breathHandle != SOUND_PENDING && m_breathHandle != 0)
            snd_SetSoundParams_A(m_breathHandle, 5, vol, pan, 0, 0);
    } else {
        gLoopManager.terminateLoop(&m_breathHandle);
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/game/Sound", updateFireBreath__12MonsterSound);
#endif
/* Computes the volume but plays nothing (the per-type cases are empty in retail). */
#ifdef NON_MATCHING
/* equivalent (untuned): 2/26 words, size 0x40 vs 0x68; retail keeps the compares of the empty switch */
void MonsterSound::playPunchSound(void)
{
    int vol = 0;
    int pan = 0;
    calcDualMonoMixVolPan(&m_monster->m_cs->trans, 250, 800, vol, pan, 0);
    switch (m_monster->m_typeBits) {
    case 0xC0:
        break;
    case 0x120:
        break;
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/game/Sound", playPunchSound__12MonsterSound);
#endif
INCLUDE_ASM("asm/nonmatchings/game/Sound", playVictorySound__12MonsterSoundi);
void MonsterSound::playTakeBatHitSound(void)
{
    int vol = 0;
    int pan = 0;
    calcDualMonoMixVolPan(&m_monster->m_cs->trans, 800, 2000, vol, pan, 0);
    Monster *m = m_monster;
    if (*m->m_state == 0x1B)
        snd_PlaySoundVolPanPMPB_A(gSoundBanks[BANK_AMBIENT], 0x32, vol, pan, 0, 0);
    else if (m->m_typeBits == 0x40)
        snd_PlaySoundVolPanPMPB_A(gSoundBanks[BANK_AMBIENT], 0x3B, vol, pan, 0, 0);
    else if (m->m_typeBits == 0x1A0)
        snd_PlaySoundVolPanPMPB_A(gSoundBanks[BANK_MECH], 7, vol, pan, 0, 0);
    else
        snd_PlaySoundVolPanPMPB_A(gSoundBanks[BANK_AMBIENT], 0x3A, vol, pan, 0, 0);
}
INCLUDE_ASM("asm/nonmatchings/game/Sound", playTakeHit__12MonsterSoundP7Monster);
void MonsterSound::playKnockBackSound(void)
{
    m_knockedBack = 1;
    m_knockBackTime = timerGetFieldCount();
}
void MonsterSound::terminateFlightGlideSound(void)
{
    gLoopManager.terminateLoop(&m_glideHandle);
}
void MonsterSound::updateFlightGlideSound(int state)
{
    int vol = 0;
    int pan = 0;
    if (state == 7) {
        calcDualMonoMixVolPan(&m_monster->m_cs->trans, 700, 1500, vol, pan, 0);
        if (m_glideHandle == 0) {
            if (m_monster->m_typeBits == 0xA0) {
                m_glideHandle = SOUND_PENDING;
                snd_PlaySoundVolPanPMPB_CB(gSoundBanks[4], 0x12, vol, pan, 0, 0, soundPlayCB, &m_glideHandle);
            } else if (m_monster->m_typeBits == 0x120) {
                m_glideHandle = SOUND_PENDING;
                snd_PlaySoundVolPanPMPB_CB(gSoundBanks[8], 0xD, vol, pan, 0, 0, soundPlayCB, &m_glideHandle);
            }
        } else if (m_glideHandle != SOUND_PENDING && m_glideHandle != 0) {
            snd_SetSoundParams_A(m_glideHandle, 5, vol, pan, 0, 0);
        }
    } else {
        gLoopManager.terminateLoop(&m_glideHandle);
    }
}
#ifdef NON_MATCHING
/* equivalent (untuned): 123/166 words, size 0x298 vs 0x298 */
void MonsterSound::playButtSlamSound(void)
{
    int vol = 0;
    int pan = 0;
    calcDualMonoMixVolPan(&m_monster->m_cs->trans, 700, 1500, vol, pan, 0);
    switch (m_monster->m_typeBits) {
    case 0x20: snd_PlaySoundVolPanPMPB_A(gSoundBanks[BANK_AMBIENT], 0x47, vol, pan, 0, 0); break;
    case 0x40:
        snd_PlaySoundVolPanPMPB_A(gSoundBanks[1], 0xE, vol, pan, 0, 0);
        snd_PlaySoundVolPanPMPB_A(gSoundBanks[BANK_AMBIENT], 0x46, vol, pan, 0, 0);
        break;
    case 0x60:
    case 0x100: snd_PlaySoundVolPanPMPB_A(gSoundBanks[BANK_AMBIENT], 0x46, vol, pan, 0, 0); break;
    case 0x80: snd_PlaySoundVolPanPMPB_A(gSoundBanks[3], 0xE, vol, pan, 0, 0); break;
    case 0xA0: snd_PlaySoundVolPanPMPB_A(gSoundBanks[4], 4, vol, pan, 0, 0); break;
    case 0xE0: snd_PlaySoundVolPanPMPB_A(gSoundBanks[6], 8, vol, pan, 0, 0); break;
    case 0x120: snd_PlaySoundVolPanPMPB_A(gSoundBanks[8], 6, vol, pan, 0, 0); break;
    case 0x140: snd_PlaySoundVolPanPMPB_A(gSoundBanks[9], 7, vol, pan, 0, 0); break;
    case 0x160: snd_PlaySoundVolPanPMPB_A(gSoundBanks[10], 5, vol, pan, 0, 0); break;
    }
    calcDualMonoMixVolPan(&m_monster->m_cs->trans, 800, 3000, vol, pan, 0);
    snd_PlaySoundVolPanPMPB_A(gSoundBanks[BANK_AMBIENT], 0x42, vol, pan, 0, 0);
}
#else
INCLUDE_ASM("asm/nonmatchings/game/Sound", playButtSlamSound__12MonsterSound);
#endif
#ifdef NON_MATCHING
/* equivalent (untuned): 80/152 words, size 0x24c vs 0x260 */
void MonsterSound::playLandingSound(void)
{
    int vol = 0;
    int pan = 0;
    if (!m_enabled)
        return;
    if (m_monster->m_typeBits == 0x1C0)
        calcDualMonoMixVolPan(&m_monster->m_cs->trans, 700, 2500, vol, pan, 0);
    else
        calcDualMonoMixVolPan(&m_monster->m_cs->trans, 700, 1500, vol, pan, 0);
    switch (m_monster->m_typeBits) {
    case 0x20: snd_PlaySoundVolPanPMPB_A(gSoundBanks[BANK_AMBIENT], 0x47, vol, pan, 0, 0); break;
    case 0x40:
        snd_PlaySoundVolPanPMPB_A(gSoundBanks[1], 0xE, vol, pan, 0, 0);
        snd_PlaySoundVolPanPMPB_A(gSoundBanks[BANK_AMBIENT], 0x46, vol, pan, 0, 0);
        break;
    case 0x60:
    case 0x100: snd_PlaySoundVolPanPMPB_A(gSoundBanks[BANK_AMBIENT], 0x46, vol, pan, 0, 0); break;
    case 0x80: snd_PlaySoundVolPanPMPB_A(gSoundBanks[3], 0xE, vol, pan, 0, 0); break;
    case 0xA0: snd_PlaySoundVolPanPMPB_A(gSoundBanks[4], 4, vol, pan, 0, 0); break;
    case 0xE0: snd_PlaySoundVolPanPMPB_A(gSoundBanks[6], 8, vol, pan, 0, 0); break;
    case 0x120: snd_PlaySoundVolPanPMPB_A(gSoundBanks[8], 6, vol, pan, 0, 0); break;
    case 0x140: snd_PlaySoundVolPanPMPB_A(gSoundBanks[9], 7, vol, pan, 0, 0); break;
    case 0x160: snd_PlaySoundVolPanPMPB_A(gSoundBanks[10], 5, vol, pan, 0, 0); break;
    case 0x1C0: snd_PlaySoundVolPanPMPB_A(gSoundBanks[BANK_FINAL], 0x10, vol, pan, 0, 0); break;
    }
    m_landed = 1;
}
#else
INCLUDE_ASM("asm/nonmatchings/game/Sound", playLandingSound__12MonsterSound);
#endif
/* Type 0xA0 (the flyer) steps at most every 12 fields; ground of type 10 under the foot adds sound 0x53. */
#ifdef NON_MATCHING
/* equivalent (untuned): 75/162 words, size 0x284 vs 0x288 */
void MonsterSound::playFootstepSound(void)
{
    int vol = 0;
    int pan = 0;
    int dt = timerGetFieldCount() - m_footstepTime;
    int type = m_monster->m_typeBits;
    if (type == 0xA0 && (float)dt < 12.0f)
        return;
    if (type == 0x1A0 || type == 0x1C0)
        calcDualMonoMixVolPan(&m_monster->m_cs->trans, 700, 2500, vol, pan, 0);
    else
        calcDualMonoMixVolPan(&m_monster->m_cs->trans, 700, 1500, vol, pan, 0);
    switch (m_monster->m_typeBits) {
    case 0x20:
    case 0x60: snd_PlaySoundVolPanPMPB_A(gSoundBanks[BANK_AMBIENT], 0x3F, vol, pan, 0, 0); break;
    case 0x40: snd_PlaySoundVolPanPMPB_A(gSoundBanks[1], 0x15, vol, pan, 0, 0); break;
    case 0x80: snd_PlaySoundVolPanPMPB_A(gSoundBanks[3], 0xB, vol, pan, 0, 0); break;
    case 0xA0: snd_PlaySoundVolPanPMPB_A(gSoundBanks[4], 6, vol, pan, 0, 0); break;
    case 0xE0: snd_PlaySoundVolPanPMPB_A(gSoundBanks[6], 0xD, vol, pan, 0, 0); break;
    case 0x100: snd_PlaySoundVolPanPMPB_A(gSoundBanks[7], 0xD, vol, pan, 0, 0); break;
    case 0x120: snd_PlaySoundVolPanPMPB_A(gSoundBanks[BANK_AMBIENT], 0x3E, vol, pan, 0, 0); break;
    case 0x140: snd_PlaySoundVolPanPMPB_A(gSoundBanks[9], 5, vol, pan, 0, 0); break;
    case 0x160: snd_PlaySoundVolPanPMPB_A(gSoundBanks[10], 0xB, vol, pan, 0, 0); break;
    case 0x1A0: snd_PlaySoundVolPanPMPB_A(gSoundBanks[BANK_MECH], 0, vol, pan, 0, 0); break;
    }
    m_footstepTime = timerGetFieldCount();
    if (((unsigned char *)m_monster->getFootHDResult())[2] == 10)
        snd_PlaySoundVolPanPMPB_A(gSoundBanks[BANK_AMBIENT], 0x53, vol, pan, 0, 0);
}
#else
INCLUDE_ASM("asm/nonmatchings/game/Sound", playFootstepSound__12MonsterSound);
#endif
#ifdef NON_MATCHING
/* equivalent (untuned): 54/57 words, size 0xe4 vs 0xe4 */
void MonsterSound::playScrape(void)
{
    int vol = 0;
    int pan = 0;
    calcDualMonoMixVolPan(&m_monster->m_cs->trans, 700, 1500, vol, pan, 0);
    if (vol > 0) {
        snd_PlaySoundVolPanPMPB_A(gSoundBanks[BANK_AMBIENT], 0xE, vol, pan, 0, 0);
        Monster *m = m_monster;
        if (m->m_typeBits == 0x40 && *m->m_state == 3 && MI(m, 0x10DFC) == 1 && MI(m, 0x10DF8) == 0)
            snd_PlaySoundVolPanPMPB_A(gSoundBanks[1], 0x1C, vol, pan, 0, 0);
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/game/Sound", playScrape__12MonsterSound);
#endif
void MonsterSound::playRobotHeadSound(void)
{
    int vol = 0;
    int pan = 0;
    calcDualMonoMixVolPan(&m_monster->m_cs->trans, 700, 1500, vol, pan, 0);
    if (vol > 0)
        snd_PlaySoundVolPanPMPB_A(gSoundBanks[1], 2, vol, pan, 0, 0);
}
void MonsterSound::playRoboJetOneShot(void)
{
}
/* At most one body impact every 12 fields. */
#ifdef NON_MATCHING
/* equivalent (untuned): 33/77 words, size 0x134 vs 0x118 (retail shares one call for the cases) */
void MonsterSound::playBodyImpactSound(void)
{
    int vol = 0;
    int pan = 0;
    if ((float)(timerGetFieldCount() - m_bodyImpactTime) > 12.0f) {
        m_bodyImpactTime = timerGetFieldCount();
        calcDualMonoMixVolPan(&m_monster->m_cs->trans, 800, 3000, vol, pan, 0);
        if (vol > 0) {
            switch (m_monster->m_typeBits) {
            case 0x40:
                snd_PlaySoundVolPanPMPB_A(gSoundBanks[1], 0xC, vol, pan, 0, 0);
                break;
            case 0x140:
                snd_PlaySoundVolPanPMPB_A(gSoundBanks[9], 0xE, vol, pan, 0, 0);
                break;
            case 0x80:
                snd_PlaySoundVolPanPMPB_A(gSoundBanks[3], 0xF, vol, pan, 0, 0);
                break;
            default:
                snd_PlaySoundVolPanPMPB_A(gSoundBanks[BANK_AMBIENT], 0x42, vol, pan, 0, 0);
                break;
            }
        }
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/game/Sound", playBodyImpactSound__12MonsterSound);
#endif
#ifdef NON_MATCHING
/* equivalent (untuned): 65/136 words, size 0x21c vs 0x220 */
void MonsterSound::playGrappleAttackSound(void)
{
    int vol = 0;
    int pan = 0;
    if (m_monster->m_typeBits != 0x1A0)
        calcDualMonoMixVolPan(&m_monster->m_cs->trans, 700, 1500, vol, pan, 0);
    else
        calcDualMonoMixVolPan(&m_monster->m_cs->trans, 700, 2500, vol, pan, 0);
    switch (m_monster->m_typeBits) {
    case 0x20:
    case 0x60:
    case 0x80:
        snd_PlaySoundVolPanPMPB_A(gSoundBanks[BANK_AMBIENT], 0x37, vol, pan, 0, 0);
        break;
    case 0x40:
        snd_PlaySoundVolPanPMPB_A(gSoundBanks[1], 2, vol, pan, 0, 0);
        snd_PlaySoundVolPanPMPB_A(gSoundBanks[1], 0x16, vol, pan, 0, 0);
        break;
    case 0xA0: snd_PlaySoundVolPanPMPB_A(gSoundBanks[4], 0x11, vol, pan, 0, 0); break;
    case 0xE0: snd_PlaySoundVolPanPMPB_A(gSoundBanks[6], 0xE, vol, pan, 0, 0); break;
    case 0x100: snd_PlaySoundVolPanPMPB_A(gSoundBanks[7], 7, vol, pan, 0, 0); break;
    case 0x120: snd_PlaySoundVolPanPMPB_A(gSoundBanks[8], 0xC, vol, pan, 0, 0); break;
    case 0x140: snd_PlaySoundVolPanPMPB_A(gSoundBanks[9], 0x10, vol, pan, 0, 0); break;
    case 0x160: snd_PlaySoundVolPanPMPB_A(gSoundBanks[10], 7, vol, pan, 0, 0); break;
    case 0x1A0: snd_PlaySoundVolPanPMPB_A(gSoundBanks[BANK_MECH], 4, vol, pan, 0, 0); break;
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/game/Sound", playGrappleAttackSound__12MonsterSound);
#endif
/* Same sounds as playObjectThrowSound. */
#ifdef NON_MATCHING
/* equivalent (untuned): 35/144 words, size 0x20c vs 0x240 */
void MonsterSound::playStompAttackSound(void)
{
    int vol = 0;
    int pan = 0;
    if (m_monster->m_typeBits != 0x1A0)
        calcDualMonoMixVolPan(&m_monster->m_cs->trans, 700, 1500, vol, pan, 0);
    else
        calcDualMonoMixVolPan(&m_monster->m_cs->trans, 700, 2500, vol, pan, 0);
    switch (m_monster->m_typeBits) {
    case 0x40: snd_PlaySoundVolPanPMPB_A(gSoundBanks[1], 9, vol, pan, 0, 0); break;
    case 0xA0: snd_PlaySoundVolPanPMPB_A(gSoundBanks[4], 7, vol, pan, 0, 0); break;
    case 0xE0: snd_PlaySoundVolPanPMPB_A(gSoundBanks[6], 0xE, vol, pan, 0, 0); break;
    case 0x100: snd_PlaySoundVolPanPMPB_A(gSoundBanks[7], 7, vol, pan, 0, 0); break;
    case 0x120: snd_PlaySoundVolPanPMPB_A(gSoundBanks[8], 0xC, vol, pan, 0, 0); break;
    case 0x140: snd_PlaySoundVolPanPMPB_A(gSoundBanks[9], 0x10, vol, pan, 0, 0); break;
    case 0x160: snd_PlaySoundVolPanPMPB_A(gSoundBanks[10], 7, vol, pan, 0, 0); break;
    case 0x1A0: snd_PlaySoundVolPanPMPB_A(gSoundBanks[BANK_MECH], 5, vol, pan, 0, 0); break;
    }
    snd_PlaySoundVolPanPMPB_A(gSoundBanks[BANK_AMBIENT], 0x4B, vol, pan, 0, 0);
}
#else
INCLUDE_ASM("asm/nonmatchings/game/Sound", playStompAttackSound__12MonsterSound);
#endif
void MonsterSound::terminateGrappleSound(void)
{
    gLoopManager.terminateLoop(&m_grappleHandle);
}
void MonsterSound::playGrappleThrowSound(void)
{
    int vol = 0;
    int pan = 0;
    calcDualMonoMixVolPan(&m_monster->m_cs->trans, 700, 1500, vol, pan, 0);
    if (m_monster->m_typeBits == 0x160)
        gLoopManager.terminateLoop(&m_grappleHandle);
    snd_PlaySoundVolPanPMPB_A(gSoundBanks[BANK_AMBIENT], 0x44, vol, pan, 0, 0);
    snd_PlaySoundVolPanPMPB_A(gSoundBanks[BANK_AMBIENT], 0x36, vol, pan, 0, 0);
}
void MonsterSound::playBatWhooshSound(void)
{
    int vol = 0;
    int pan = 0;
    calcDualMonoMixVolPan(&m_monster->m_cs->trans, 700, 1500, vol, pan, 0);
    if (vol > 0)
        snd_PlaySoundVolPanPMPB_A(gSoundBanks[BANK_AMBIENT], 0x36, vol, pan, 0, 0);
}
#ifdef NON_MATCHING
/* equivalent (untuned): 35/144 words, size 0x20c vs 0x240 */
void MonsterSound::playObjectThrowSound(void)
{
    int vol = 0;
    int pan = 0;
    if (m_monster->m_typeBits != 0x1A0)
        calcDualMonoMixVolPan(&m_monster->m_cs->trans, 700, 1500, vol, pan, 0);
    else
        calcDualMonoMixVolPan(&m_monster->m_cs->trans, 700, 2500, vol, pan, 0);
    switch (m_monster->m_typeBits) {
    case 0x40: snd_PlaySoundVolPanPMPB_A(gSoundBanks[1], 9, vol, pan, 0, 0); break;
    case 0xA0: snd_PlaySoundVolPanPMPB_A(gSoundBanks[4], 7, vol, pan, 0, 0); break;
    case 0xE0: snd_PlaySoundVolPanPMPB_A(gSoundBanks[6], 0xE, vol, pan, 0, 0); break;
    case 0x100: snd_PlaySoundVolPanPMPB_A(gSoundBanks[7], 7, vol, pan, 0, 0); break;
    case 0x120: snd_PlaySoundVolPanPMPB_A(gSoundBanks[8], 0xC, vol, pan, 0, 0); break;
    case 0x140: snd_PlaySoundVolPanPMPB_A(gSoundBanks[9], 0x10, vol, pan, 0, 0); break;
    case 0x160: snd_PlaySoundVolPanPMPB_A(gSoundBanks[10], 7, vol, pan, 0, 0); break;
    case 0x1A0: snd_PlaySoundVolPanPMPB_A(gSoundBanks[BANK_MECH], 5, vol, pan, 0, 0); break;
    }
    snd_PlaySoundVolPanPMPB_A(gSoundBanks[BANK_AMBIENT], 0x4B, vol, pan, 0, 0);
}
#else
INCLUDE_ASM("asm/nonmatchings/game/Sound", playObjectThrowSound__12MonsterSound);
#endif
INCLUDE_ASM("asm/nonmatchings/game/Sound", playCrowdControlSound__12MonsterSound);
INCLUDE_ASM("asm/nonmatchings/game/Sound", play206Sound__12MonsterSound);
INCLUDE_ASM("asm/nonmatchings/game/Sound", playTauntSound__12MonsterSound);
void MonsterSound::playPickupSound(void)
{
    int vol = 0;
    int pan = 0;
    calcDualMonoMixVolPan(&m_monster->m_cs->trans, 700, 1500, vol, pan, 0);
    switch (m_monster->m_typeBits) {
    case 0x40:
        snd_PlaySoundVolPanPMPB_CB(gSoundBanks[1], 1, vol, pan, 0, 0, soundPlayCB, &m_pickupHandle);
        break;
    case 0x140:
        snd_PlaySoundVolPanPMPB_CB(gSoundBanks[9], 3, vol, pan, 0, 0, soundPlayCB, &m_pickupHandle);
        break;
    }
    snd_PlaySoundVolPanPMPB_A(gSoundBanks[BANK_AMBIENT], 0x4A, vol, pan, 0, 0);
}
#ifdef NON_MATCHING
/* equivalent (untuned): 20/81 words, size 0x144 vs 0x138 */
void MonsterSound::playGrapplePickupSound(void)
{
    int vol = 0;
    int pan = 0;
    calcDualMonoMixVolPan(&m_monster->m_cs->trans, 700, 1500, vol, pan, 0);
    switch (m_monster->m_typeBits) {
    case 0x160:
        snd_PlaySoundVolPanPMPB_CB(gSoundBanks[10], 0x11, vol, pan, 0, 0, soundPlayCB, &m_grappleHandle);
        return;
    case 0x40:
        snd_PlaySoundVolPanPMPB_CB(gSoundBanks[1], 1, vol, pan, 0, 0, soundPlayCB, &m_pickupHandle);
        break;
    case 0x140:
        snd_PlaySoundVolPanPMPB_CB(gSoundBanks[9], 3, vol, pan, 0, 0, soundPlayCB, &m_pickupHandle);
        break;
    }
    snd_PlaySoundVolPanPMPB_A(gSoundBanks[BANK_AMBIENT], 0x4A, vol, pan, 0, 0);
}
#else
INCLUDE_ASM("asm/nonmatchings/game/Sound", playGrapplePickupSound__12MonsterSound);
#endif
void MonsterSound::stopPickupSound(void)
{
    gLoopManager.terminateLoop(&m_pickupHandle);
}
void MonsterSound::playWingFlap(void)
{
    int vol = 0;
    int pan = 0;
    if (m_enabled) {
        calcDualMonoMixVolPan(&m_monster->m_cs->trans, 700, 1500, vol, pan, 0);
        if (m_monster->m_typeBits == 0xA0)
            snd_PlaySoundVolPanPMPB_A(gSoundBanks[4], 1, vol, pan, 0, 0);
        else if (m_monster->m_typeBits == 0x120)
            snd_PlaySoundVolPanPMPB_A(gSoundBanks[8], 1, vol, pan, 0, 0);
    }
}
void MonsterSound::playImpalerRemoveSound(void)
{
    int vol = 0;
    int pan = 0;
    calcDualMonoMixVolPan(&m_monster->m_cs->trans, 700, 1500, vol, pan, 0);
    snd_PlaySoundVolPanPMPB_A(gSoundBanks[BANK_AMBIENT], 0x30, vol, pan, 0, 0);
}
/* Computes the volume but plays nothing (the per-type cases are empty in retail). */
#ifdef NON_MATCHING
/* equivalent (untuned): 2/24 words, size 0x40 vs 0x60; retail keeps the compare of the empty switch */
void MonsterSound::playGrappleSwing(int n)
{
    int vol = 0;
    int pan = 0;
    calcDualMonoMixVolPan(&m_monster->m_cs->trans, 700, 1500, vol, pan, 0);
    switch (m_monster->m_typeBits) {
    case 0xC0:
        break;
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/game/Sound", playGrappleSwing__12MonsterSoundi);
#endif
INCLUDE_ASM("asm/nonmatchings/game/Sound", playSwing__12MonsterSoundii);
#ifdef NON_MATCHING
/* equivalent (untuned): 117/129 words, size 0x204 vs 0x204 */
void MonsterSound::playJumpSound(void)
{
    int vol = 0;
    int pan = 0;
    if (m_monster->m_typeBits == 0x1C0)
        calcDualMonoMixVolPan(&m_monster->m_cs->trans, 700, 2500, vol, pan, 0);
    else
        calcDualMonoMixVolPan(&m_monster->m_cs->trans, 700, 1500, vol, pan, 0);
    switch (m_monster->m_typeBits) {
    case 0x20: snd_PlaySoundVolPanPMPB_A(gSoundBanks[0], 3, vol, pan, 0, 0); break;
    case 0x60: snd_PlaySoundVolPanPMPB_A(gSoundBanks[2], 4, vol, pan, 0, 0); break;
    case 0xA0: snd_PlaySoundVolPanPMPB_A(gSoundBanks[4], 3, vol, pan, 0, 0); break;
    case 0xE0: snd_PlaySoundVolPanPMPB_A(gSoundBanks[6], 6, vol, pan, 0, 0); break;
    case 0x100: snd_PlaySoundVolPanPMPB_A(gSoundBanks[7], 0, vol, pan, 0, 0); break;
    case 0x120: snd_PlaySoundVolPanPMPB_A(gSoundBanks[8], 0, vol, pan, 0, 0); break;
    case 0x140: snd_PlaySoundVolPanPMPB_A(gSoundBanks[9], 4, vol, pan, 0, 0); break;
    case 0x160: snd_PlaySoundVolPanPMPB_A(gSoundBanks[10], 4, vol, pan, 0, 0); break;
    case 0x1C0: snd_PlaySoundVolPanPMPB_A(gSoundBanks[BANK_FINAL], 0xF, vol, pan, 0, 0); break;
    case 0x1E0: snd_PlaySoundVolPanPMPB_A(gSoundBanks[BANK_FINAL], 0x14, vol, pan, 0, 0); break;
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/game/Sound", playJumpSound__12MonsterSound);
#endif
INCLUDE_ASM("asm/nonmatchings/game/Sound", playDeathSound__12MonsterSoundi);
void MonsterSound::playSonicRoarSound(void)
{
    int vol = 0;
    int pan = 0;
    calcDualMonoMixVolPan(&m_monster->m_cs->trans, 100, 1200, vol, pan, 0);
    if (vol)
        snd_PlaySoundVolPanPMPB_A(gSoundBanks[0], 7, vol, pan, 0, 0);
}
void MonsterSound::playWaterSplashSound(void)
{
    int vol = 0;
    int pan = 0;
    calcDualMonoMixVolPan(&m_monster->m_cs->trans, 700, 1500, vol, pan, 0);
    if (vol)
        snd_PlaySoundVolPanPMPB_A(gSoundBanks[BANK_AMBIENT], 0x45, vol, pan, 0, 0);
}
void MonsterSound::playWaterFootstepSound(void)
{
    int vol = 0;
    int pan = 0;
    calcDualMonoMixVolPan(&m_monster->m_cs->trans, 700, 1500, vol, pan, 0);
    if (vol)
        snd_PlaySoundVolPanPMPB_A(gSoundBanks[BANK_AMBIENT], 0x41, vol, pan, 0, 0);
}
/* Both rotor loops while the rotor spins, else the spin-down. */
#ifdef NON_MATCHING
/* equivalent (untuned): 16/94 words, size 0x174 vs 0x178 */
void MonsterSound::updateAssBossRotorSound(void)
{
    int vol = 0;
    int pan = 0;
    calcDualMonoMixVolPan(&m_monster->m_cs->trans, 700, 2500, vol, pan, 0);
    Monster *m = m_monster;
    if (*(float *)((char *)m + 0x3140 + (MI(m, 0x3170) * 6 + 4) * 4) != 0.0f) {
        if (m_rotorHandle == 0) {
            m_rotorHandle = SOUND_PENDING;
            snd_PlaySoundVolPanPMPB_CB(gSoundBanks[BANK_MECH], 8, vol, pan, 0, 0, soundPlayCB, &m_rotorHandle);
        } else if (m_rotorHandle != SOUND_PENDING && m_rotorHandle != 0) {
            snd_SetSoundParams_A(m_rotorHandle, 5, vol, pan, 0, 0);
        }
        if (m_rotorHandle2 == 0) {
            m_rotorHandle2 = SOUND_PENDING;
            snd_PlaySoundVolPanPMPB_CB(gSoundBanks[BANK_MECH], 9, vol, pan, 0, 0, soundPlayCB, &m_rotorHandle2);
        } else if (m_rotorHandle2 != SOUND_PENDING && m_rotorHandle2 != 0) {
            snd_SetSoundParams_A(m_rotorHandle2, 5, vol, pan, 0, 0);
        }
    } else {
        terminateAssBossRotorSound();
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/game/Sound", updateAssBossRotorSound__12MonsterSound);
#endif
/* Spin-down one-shot if the rotor loop was playing, then both rotor loops stop. */
void MonsterSound::terminateAssBossRotorSound(void)
{
    int vol = 0;
    int pan = 0;
    calcDualMonoMixVolPan(&m_monster->m_cs->trans, 700, 2500, vol, pan, 0);
    if (vol && m_rotorHandle != SOUND_PENDING && m_rotorHandle != 0)
        snd_PlaySoundVolPanPMPB_A(gSoundBanks[BANK_MECH], 0xA, vol, pan, 0, 0);
    gLoopManager.terminateLoop(&m_rotorHandle);
    gLoopManager.terminateLoop(&m_rotorHandle2);
}
void MonsterSound::updateAssBossSpinSound(void)
{
    int vol = 0;
    int pan = 0;
    calcDualMonoMixVolPan(&m_monster->m_cs->trans, 700, 2500, vol, pan, 0);
    if (m_assBossSpinHandle == 0) {
        m_assBossSpinHandle = SOUND_PENDING;
        snd_PlaySoundVolPanPMPB_CB(gSoundBanks[BANK_MECH], 0xB, vol, pan, 0, 0, soundPlayCB, &m_assBossSpinHandle);
    } else if (m_assBossSpinHandle != SOUND_PENDING && m_assBossSpinHandle != 0) {
        snd_SetSoundParams_A(m_assBossSpinHandle, 5, vol, pan, 0, 0);
    }
}
void MonsterSound::terminateAssBossSpinSound(void)
{
    gLoopManager.terminateLoop(&m_assBossSpinHandle);
}
void MonsterSound::playAssBossWeaponDrawSound(void)
{
    int vol = 0;
    int pan = 0;
    calcDualMonoMixVolPan(&m_monster->m_cs->trans, 700, 2500, vol, pan, 0);
    if (vol)
        snd_PlaySoundVolPanPMPB_A(gSoundBanks[BANK_MECH], 2, vol, pan, 0, 0);
}
void MonsterSound::updateRobotReticleSound(void)
{
    int vol = 0;
    int pan = 0;
    calcDualMonoMixVolPan(&m_monster->m_cs->trans, 700, 1500, vol, pan, 0);
    if (m_reticleHandle == 0) {
        m_reticleHandle = SOUND_PENDING;
        snd_PlaySoundVolPanPMPB_CB(gSoundBanks[1], 0x1D, vol, pan, 0, 0, soundPlayCB, &m_reticleHandle);
    } else if (m_reticleHandle != SOUND_PENDING && m_reticleHandle != 0) {
        snd_SetSoundParams_A(m_reticleHandle, 5, vol, pan, 0, 0);
    }
}
void MonsterSound::terminateRobotReticleSound(void)
{
    gLoopManager.terminateLoop(&m_reticleHandle);
}
void MonsterSound::updateElectricShockSound(void)
{
    int vol = 0;
    int pan = 0;
    calcDualMonoMixVolPan(&m_monster->m_cs->trans, 500, 2500, vol, pan, 0);
    if (m_shockHandle == 0) {
        m_shockHandle = SOUND_PENDING;
        snd_PlaySoundVolPanPMPB_CB(gSoundBanks[BANK_AMBIENT], 7, vol, pan, 0, 0, soundPlayCB, &m_shockHandle);
    } else if (m_shockHandle != SOUND_PENDING && m_shockHandle != 0) {
        snd_SetSoundParams_A(m_shockHandle, 5, vol, pan, 0, 0);
    }
}
void MonsterSound::terminateElectricShockSound(void)
{
    gLoopManager.terminateLoop(&m_shockHandle);
}
void MonsterSound::updateAlienSpecial(int state)
{
    int vol = 0;
    int pan = 0;
    if (state == 0xA0 || state == 0xA1) {
        calcDualMonoMixVolPan(&m_monster->m_cs->trans, 700, 1500, vol, pan, 0);
        if (vol > 0) {
            if (m_alienSpecialHandle == 0) {
                m_alienSpecialHandle = SOUND_PENDING;
                snd_PlaySoundVolPanPMPB_CB(gSoundBanks[10], 0xA, vol, pan, 0, 0, soundPlayCB, &m_alienSpecialHandle);
            } else if (m_alienSpecialHandle != SOUND_PENDING && m_alienSpecialHandle != 0) {
                snd_SetSoundParams_A(m_alienSpecialHandle, 5, vol, pan, 0, 0);
            }
        } else {
            gLoopManager.terminateLoop(&m_alienSpecialHandle);
        }
    } else {
        gLoopManager.terminateLoop(&m_alienSpecialHandle);
    }
}
void MonsterSound::terminateAlienSpecial(void)
{
    gLoopManager.terminateLoop(&m_alienSpecialHandle);
}
/* The zap's impact loop at the target, and the beam loop with random crackles at the monster. */
#ifdef NON_MATCHING
/* equivalent (untuned): 130/133 words, size 0x214 vs 0x214 */
void MonsterSound::updateZapAttackSound(int on, Monster *target)
{
    int vol = 0;
    int pan = 0;
    if (on && target) {
        calcDualMonoMixVolPan(&target->m_cs->trans, 700, 1500, vol, pan, 0);
        if (vol > 0) {
            if (m_zapHandle2 == 0) {
                snd_PlaySoundVolPanPMPB_A(gSoundBanks[6], 0xA, vol, pan, 0, 0);
                m_zapHandle2 = SOUND_PENDING;
                snd_PlaySoundVolPanPMPB_CB(gSoundBanks[6], 9, vol, pan, 0, 0, soundPlayCB, &m_zapHandle2);
            } else if (m_zapHandle2 != SOUND_PENDING && m_zapHandle2 != 0) {
                snd_SetSoundParams_A(m_zapHandle2, 5, vol, pan, 0, 0);
            }
        } else {
            gLoopManager.terminateLoop(&m_zapHandle2);
        }
    } else {
        gLoopManager.terminateLoop(&m_zapHandle2);
    }
    calcDualMonoMixVolPan(&m_monster->m_cs->trans, 1400, 2200, vol, pan, 0);
    if (vol > 0) {
        if (m_zapHandle == 0) {
            m_zapHandle = SOUND_PENDING;
            snd_PlaySoundVolPanPMPB_CB(gSoundBanks[6], 0xB, vol, pan, 0, 0, soundPlayCB, &m_zapHandle);
        } else if (m_zapHandle != SOUND_PENDING && m_zapHandle != 0) {
            snd_SetSoundParams_A(m_zapHandle, 5, vol, pan, 0, 0);
        }
        if (mathfRand(0, 8) == 0)
            snd_PlaySoundVolPanPMPB_A(gSoundBanks[6], 0xC, vol, pan, 0, 0);
    } else {
        gLoopManager.terminateLoop(&m_zapHandle);
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/game/Sound", updateZapAttackSound__12MonsterSoundiP7Monster);
#endif
void MonsterSound::terminateZapAttackSound(void)
{
    gLoopManager.terminateLoop(&m_zapHandle);
    gLoopManager.terminateLoop(&m_zapHandle2);
}
void MonsterSound::playShogunSpecialImpactSound(void)
{
    int vol = 0;
    int pan = 0;
    calcDualMonoMixVolPan(&m_monster->m_cs->trans, 700, 1500, vol, pan, 0);
    if (vol > 0)
        snd_PlaySoundVolPanPMPB_A(gSoundBanks[3], 9, vol, pan, 0, 0);
}
void MonsterSound::updateShogunSpecialSound(void)
{
    int vol = 0;
    int pan = 0;
    calcDualMonoMixVolPan(&m_monster->m_cs->trans, 1200, 3000, vol, pan, 0);
    if (m_shogunSpecialHandle == 0) {
        m_shogunSpecialHandle = SOUND_PENDING;
        snd_PlaySoundVolPanPMPB_CB(gSoundBanks[3], 0xA, vol, pan, 0, 0, soundPlayCB, &m_shogunSpecialHandle);
    } else if (m_shogunSpecialHandle != SOUND_PENDING && m_shogunSpecialHandle != 0) {
        snd_SetSoundParams_A(m_shogunSpecialHandle, 5, vol, pan, 0, 0);
    }
}
void MonsterSound::terminateShogunSpecialSound(void)
{
    gLoopManager.terminateLoop(&m_shogunSpecialHandle);
}
void MonsterSound::terminateEvilEye(void)
{
    gLoopManager.terminateLoop(&m_breathHandle);
}
void MonsterSound::updateEvilEye(void)
{
    int vol = 0;
    int pan = 0;
    calcDualMonoMixVolPan(&m_monster->m_cs->trans, 100, 1200, vol, pan, 0);
    if (m_breathHandle == 0) {
    } else if (m_breathHandle != SOUND_PENDING && m_breathHandle != 0) {
        snd_SetSoundParams_A(m_breathHandle, 5, vol, pan, 0, 0);
    }
}
void MonsterSound::updateSpiderMagnoSpecial(void)
{
    int vol = 0;
    int pan = 0;
    calcDualMonoMixVolPan(&m_monster->m_cs->trans, 700, 1500, vol, pan, 0);
    if (m_magnoHandle == 0) {
    } else if (m_magnoHandle != SOUND_PENDING && m_magnoHandle != 0) {
        snd_SetSoundParams_A(m_magnoHandle, 5, vol, pan, 0, 0);
    }
}
void MonsterSound::terminateSpiderMagnoSpecial(void)
{
    gLoopManager.terminateLoop(&m_magnoHandle);
}
void MonsterSound::playLavaSpecialInhale(void)
{
    int vol = 0;
    int pan = 0;
    calcDualMonoMixVolPan(&m_monster->m_cs->trans, 350, 1000, vol, pan, 0);
    m_inhaleTime = timerGetFieldCount();
    if (vol) {
        m_inhaleHandle = SOUND_PENDING;
        snd_PlaySoundVolPanPMPB_CB(gSoundBanks[7], 0xC, vol, pan, 0, 0, soundPlayCB, &m_inhaleHandle);
    }
}
void MonsterSound::playSuckerBugLaunch(void)
{
    int vol = 0;
    int pan = 0;
    calcDualMonoMixVolPan(&m_monster->m_cs->trans, 350, 1000, vol, pan, 0);
    if (vol)
        snd_PlaySoundVolPanPMPB_A(gSoundBanks[4], 0xC, vol, pan, 0, 0);
}
void MonsterSound::playShogunSpecialOneShot(void)
{
    int vol = 0;
    int pan = 0;
    calcDualMonoMixVolPan(&m_monster->m_cs->trans, 350, 1000, vol, pan, 0);
    if (vol)
        snd_PlaySoundVolPanPMPB_A(gSoundBanks[3], 8, vol, pan, 0, 0);
}
/* Computes the volume but plays nothing. */
void MonsterSound::playAlienRaygunDrawSound(void)
{
    int vol = 0;
    int pan = 0;
    calcDualMonoMixVolPan(&m_monster->m_cs->trans, 700, 1500, vol, pan, 0);
}
INCLUDE_ASM("asm/nonmatchings/game/Sound", initSuckerBugSound__14SuckerBugSound);
INCLUDE_ASM("asm/nonmatchings/game/Sound", terminateSuckerBugSound__14SuckerBugSound);
INCLUDE_ASM("asm/nonmatchings/game/Sound", updateSuckerBugSound__14SuckerBugSoundP8_fvectori);
void MonsterSound::stopAllMonsterSounds(void)
{
    gLoopManager.terminateLoop(&m_inhaleHandle);
    gLoopManager.terminateLoop(&m_rotorHandle);
    gLoopManager.terminateLoop(&m_assBossSpinHandle);
    gLoopManager.terminateLoop(&m_rotorHandle2);
    gLoopManager.terminateLoop(&m_grappleHandle);
    gLoopManager.terminateLoop(&m_jellyTazerHandle);
    gLoopManager.terminateLoop(&m_zapHandle);
    gLoopManager.terminateLoop(&m_shogunSpecialHandle);
    gLoopManager.terminateLoop(&m_flyHandle);
    gLoopManager.terminateLoop(&f60);
    gLoopManager.terminateLoop(&f220);
    gLoopManager.terminateLoop(&f21C);
    gLoopManager.terminateLoop(&f2C);
    gLoopManager.terminateLoop(&f28);
    gLoopManager.terminateLoop(&m_runHandle);
    gLoopManager.terminateLoop(&m_pickupHandle);
    gLoopManager.terminateLoop(&m_zapHandle2);
    gLoopManager.terminateLoop(&f3C);
    gLoopManager.terminateLoop(&m_alienSpecialHandle);
    gLoopManager.terminateLoop(&m_inhaleHandle);
    gLoopManager.terminateLoop(&f20C);
    gLoopManager.terminateLoop(&f210);
    gLoopManager.terminateLoop(&m_shockHandle);
    gLoopManager.terminateLoop(&m_mechLaserImpactHandle);
    gLoopManager.terminateLoop(&m_glideHandle);
    gLoopManager.terminateLoop(&m_pickupHandle);
    gLoopManager.terminateLoop(&m_magnoHandle);
    gLoopManager.terminateLoop(&m_dashHandle);
    gLoopManager.terminateLoop(&m_dashHandle2);
    gLoopManager.terminateLoop(&f20);
    gLoopManager.terminateLoop(&m_breathHandle);
}
void TriggerVolumeSound::initTriggerVolumeSound(int trigger, unsigned sound)
{
    m_sound = sound;
    m_trigger = trigger;
    m_handle = 0;
}
void TriggerVolumeSound::terminateTriggerVolumeSound(void)
{
    gLoopManager.terminateLoop(&m_handle);
}
/* Only each player's first event of the frame is looked at. */
#ifdef NON_MATCHING
/* equivalent (untuned): 48/61 words, size 0xf4 vs 0xf4 */
void TriggerVolumeSound::updateTriggerVolumeSound(void)
{
    int found = 0;
    for (int i = 0; i < game->m_numMonsters; i++) {
        if (SOUND_MANAGER()->m_events[i][0].id == m_trigger)
            found = 1;
    }
    if (found) {
        if (m_handle == 0) {
            m_handle = SOUND_PENDING;
            snd_PlaySoundVolPanPMPB_CB(gSoundBanks[BANK_LEVEL], m_sound, 0x400, 0, 0, 0, soundPlayCB, this);
        } else if (m_handle != SOUND_PENDING && m_handle != 0) {
            snd_SetSoundParams_A(m_handle, 5, 0x400, 0, 0, 0);
        }
    } else {
        gLoopManager.terminateLoop(&m_handle);
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/game/Sound", updateTriggerVolumeSound__18TriggerVolumeSound);
#endif
#ifdef NON_MATCHING
/* equivalent (untuned): 10/14 words, size 0x38 vs 0x38 */
void RangedTriggerVolumeSound::initTriggerVolumeSound(int trigger, unsigned sound, int minDist, int maxDist)
{
    for (int i = 0; i < 4; i++) {
        m_slots[i].handle = 0;
        m_slots[i].active = 0;
    }
    m_maxDist = maxDist;
    m_trigger = trigger;
    m_sound = sound;
    m_minDist = minDist;
}
#else
INCLUDE_ASM("asm/nonmatchings/game/Sound", initTriggerVolumeSound__24RangedTriggerVolumeSoundiUiii);
#endif
void RangedTriggerVolumeSound::terminateTriggerVolumeSound(void)
{
    for (int i = 0; i < 4; i++)
        gLoopManager.terminateLoop(&m_slots[i].handle);
}
/* Every m_trigger event of the frame keeps the slot already playing at exactly that position, or takes a free slot. Slots
   without an event this frame stop. Trigger 0x157F uses the level bank, the others the ambience bank. */
#ifdef NON_MATCHING
/* equivalent (untuned): 26/218 words, size 0x368 vs 0x340 */
void RangedTriggerVolumeSound::updateTriggerVolumeSound(void)
{
    int vol = 0x400;
    int pan = 0;
    for (int i = 0; i < 4; i++)
        m_slots[i].active = 0;
    for (int p = 0; p < game->m_numMonsters; p++) {
        for (int e = 0; e < SOUND_MANAGER()->m_eventCount[p]; e++) {
            if (SOUND_MANAGER()->m_events[p][e].id != m_trigger)
                continue;
            int found = 0;
            for (int s = 0; s < 4; s++) {
                unsigned h = m_slots[s].handle;
                if (h != SOUND_PENDING && h != 0) {
                    _fvector *pos = &SOUND_MANAGER()->m_events[p][e].pos;
                    if (pos->x == m_slots[s].pos.x && pos->y == m_slots[s].pos.y && pos->z == m_slots[s].pos.z) {
                        found = 1;
                        m_slots[s].active = 1;
                    }
                }
            }
            if (!found) {
                for (int s = 0; s < 4; s++) {
                    if (m_slots[s].handle == 0) {
                        m_slots[s].active = 1;
                        m_slots[s].pos = SOUND_MANAGER()->m_events[p][e].pos;
                        break;
                    }
                }
            }
        }
    }
    for (int s = 0; s < 4; s++) {
        Slot *slot = &m_slots[s];
        if (slot->active == 1) {
            calcDualMonoMixVolPan(&slot->pos, m_minDist, m_maxDist, vol, pan, 0);
            if (slot->handle == 0) {
                slot->handle = SOUND_PENDING;
                if (m_trigger == 0x157F)
                    snd_PlaySoundVolPanPMPB_CB(gSoundBanks[BANK_LEVEL], m_sound, vol, pan, 0, 0, soundPlayCB, slot);
                else
                    snd_PlaySoundVolPanPMPB_CB(gSoundBanks[BANK_AMBIENT], m_sound, vol, pan, 0, 0, soundPlayCB, slot);
            } else if (slot->handle != SOUND_PENDING && slot->handle != 0) {
                snd_SetSoundParams_A(slot->handle, 5, vol, pan, 0, 0);
            }
        } else {
            slot->active = 0;
            gLoopManager.terminateLoop(&slot->handle);
        }
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/game/Sound", updateTriggerVolumeSound__24RangedTriggerVolumeSound);
#endif
INCLUDE_ASM("asm/nonmatchings/game/Sound", soundPlayPowerUpSound__FiiiP8_fvector);
INCLUDE_ASM("asm/nonmatchings/game/Sound", playLowStaminaSound__Fi);
INCLUDE_ASM("asm/nonmatchings/game/Sound", initPlantBossRootSound__18PlantBossRootSound);
INCLUDE_ASM("asm/nonmatchings/game/Sound", updatePlantBossRootSound__18PlantBossRootSoundP8_fvector);
INCLUDE_ASM("asm/nonmatchings/game/Sound", terminatePlantBossRootSound__18PlantBossRootSound);
INCLUDE_ASM("asm/nonmatchings/game/Sound", initFinalBossSound__14FinalBossSoundP9FinalBoss);
INCLUDE_ASM("asm/nonmatchings/game/Sound", updateFinalBossSound__14FinalBossSoundP8_fvectoriP11_animHandle);
INCLUDE_ASM("asm/nonmatchings/game/Sound", playGetReadyToFireSound__14FinalBossSoundP8_fvector);
INCLUDE_ASM("asm/nonmatchings/game/Sound", playShieldPowerUpSound__14FinalBossSoundP8_fvector);
INCLUDE_ASM("asm/nonmatchings/game/Sound", playShieldPowerDownSound__14FinalBossSoundP8_fvector);
INCLUDE_ASM("asm/nonmatchings/game/Sound", playMechaPowerDownSound__14FinalBossSoundP8_fvector);
INCLUDE_ASM("asm/nonmatchings/game/Sound", playTakeHitSound__14FinalBossSoundP8_fvector);
INCLUDE_ASM("asm/nonmatchings/game/Sound", updateMechLaser__14FinalBossSoundP8_fvector);
INCLUDE_ASM("asm/nonmatchings/game/Sound", terminateMechLaser__14FinalBossSound);
void MonsterSound::stopMechaLaserImpactSound(void)
{
    gLoopManager.terminateLoop(&m_mechLaserImpactHandle);
}
INCLUDE_ASM("asm/nonmatchings/game/Sound", initPlantBossSound__14PlantBossSound);
INCLUDE_ASM("asm/nonmatchings/game/Sound", updatePlantBossSound__14PlantBossSound);
INCLUDE_ASM("asm/nonmatchings/game/Sound", playPlantBossHideSound__14PlantBossSound);
INCLUDE_ASM("asm/nonmatchings/game/Sound", playPlantBossEmergeSound__14PlantBossSound);
INCLUDE_ASM("asm/nonmatchings/game/Sound", playPlantBossCabbageSmack__14PlantBossSound);
INCLUDE_ASM("asm/nonmatchings/game/Sound", playPlantBossDeathSound__18PlantBossHeadSoundP8_fvector);
INCLUDE_ASM("asm/nonmatchings/game/Sound", playPlantBossChompSound__18PlantBossHeadSoundP8_fvector);
INCLUDE_ASM("asm/nonmatchings/game/Sound", playPlantBossHeadDeadSound__18PlantBossHeadSoundP8_fvector);
INCLUDE_ASM("asm/nonmatchings/game/Sound", playPlantBossMissChompSound__18PlantBossHeadSoundP8_fvector);
INCLUDE_ASM("asm/nonmatchings/game/Sound", playPlantBossGrowlSound__18PlantBossHeadSoundP8_fvector);
INCLUDE_ASM("asm/nonmatchings/game/Sound", playPlantBossHissSound__18PlantBossHeadSoundP8_fvector);
INCLUDE_ASM("asm/nonmatchings/game/Sound", playPlantBossSpitSound__18PlantBossHeadSoundP8_fvector);
INCLUDE_ASM("asm/nonmatchings/game/Sound", playPlantBossSporeLaunchSound__18PlantBossHeadSoundP8_fvector);
INCLUDE_ASM("asm/nonmatchings/game/Sound", updateChompSound__18PlantBossHeadSoundP8_fvector);
INCLUDE_ASM("asm/nonmatchings/game/Sound", terminateChompSound__18PlantBossHeadSound);
INCLUDE_ASM("asm/nonmatchings/game/Sound", initRigidPickupSound__16RigidPickupSound);
INCLUDE_ASM("asm/nonmatchings/game/Sound", updateRigidPickupSound__16RigidPickupSoundP8_fvectorfi);
INCLUDE_ASM("asm/nonmatchings/game/Sound", terminateRollyBallSound__16RigidPickupSound);
INCLUDE_ASM("asm/nonmatchings/game/Sound", playRigidPickupSound__16RigidPickupSoundP8_fvectorfi);
INCLUDE_ASM("asm/nonmatchings/game/Sound", initWindSound__9WindSoundiiffffiiiiiiii);
INCLUDE_ASM("asm/nonmatchings/game/Sound", terminateWindSound__9WindSound);
INCLUDE_ASM("asm/nonmatchings/game/Sound", updateWindSound__9WindSound);
INCLUDE_ASM("asm/nonmatchings/game/Sound", rampWindVolumeAndPitchUp__10LavaInhaleifi);
INCLUDE_ASM("asm/nonmatchings/game/Sound", updateWindSound__10LavaInhale);
INCLUDE_ASM("asm/nonmatchings/game/Sound", updateWindSound__11MantisGlide);
INCLUDE_ASM("asm/nonmatchings/game/Sound", rampWindVolumeDown__11MantisGlide);
INCLUDE_ASM("asm/nonmatchings/game/Sound", rampWindVolumeUp__11MantisGlide);
INCLUDE_ASM("asm/nonmatchings/game/Sound", calcDualMonoMixVolPan__FP8_fvectoriiRiT3Pi);
INCLUDE_ASM("asm/nonmatchings/game/Sound", calcCheapDualMonoMixPan__Fi);
INCLUDE_ASM("asm/nonmatchings/game/Sound", findApproachingSpeed__FP8_fvectorPiPf);
INCLUDE_ASM("asm/nonmatchings/game/Sound", calcDualMonoMixHeightBasedSoundVol__FffRiT2Pi);
INCLUDE_ASM("asm/nonmatchings/game/Sound", __static_initialization_and_destruction_0_001C1758);
INCLUDE_ASM("asm/nonmatchings/game/Sound", _GLOBAL_$I$gIntroMoviesHavePlayed);
