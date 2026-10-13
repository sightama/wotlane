#include "common.h"
#include "game/game.h"
#include "game/shell.h"
#include "game/streaming_sound.h"
#include "game/sound.h"

extern char mechBankName[] __asm__("D_006EFFB0");   /* "Mech_s.bnk" */

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

/* Records a sound a player's monster made this frame (up to four per player). */
#ifdef NON_MATCHING
/* equivalent (untuned): 20/33 words, size 0x84 vs 0x84 */
void SoundManager::setSoundVolumeInfo(Monster *m, int vol, _fvector *pos)
{
    if (m->m_playerNum == 1) {
        m_volInfo[m->m_index][m_volCount[m->m_index]].vol = vol;
        m_volInfo[m->m_index][m_volCount[m->m_index]].pos = *pos;
        m_volCount[m->m_index]++;
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
            m_volInfo[i][j].vol = 0;
    for (int i = 0; i < 4; i++)
        m_volCount[i] = 0;
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
            m_volInfo[i][j].vol = 0;
    for (int i = 0; i < 4; i++)
        m_volCount[i] = 0;
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
INCLUDE_ASM("asm/nonmatchings/game/Sound", initLevelObjectSoundManager__23LevelObjectSoundManager);
INCLUDE_ASM("asm/nonmatchings/game/Sound", updateLevelObjectSoundManager__23LevelObjectSoundManager);
INCLUDE_ASM("asm/nonmatchings/game/Sound", updateDestructLoops__23LevelObjectSoundManager);
INCLUDE_ASM("asm/nonmatchings/game/Sound", triggerLevelDestructSound__23LevelObjectSoundManagerP8_fvectoriiffiP12ActPlaySound);
INCLUDE_ASM("asm/nonmatchings/game/Sound", playDebrisImpactSound__23LevelObjectSoundManagerP8_fvectori);
INCLUDE_ASM("asm/nonmatchings/game/Sound", playTimedBuildingDestructibleSound__23LevelObjectSoundManagerP12ActPlaySoundP8_fvector);
INCLUDE_ASM("asm/nonmatchings/game/Sound", playDestructibleSound__23LevelObjectSoundManagerUiP8_fvector);
INCLUDE_ASM("asm/nonmatchings/game/Sound", findAvailableDestructHandle__23LevelObjectSoundManager);
INCLUDE_ASM("asm/nonmatchings/game/Sound", initCanyonLevelSound__16CanyonLevelSound);
INCLUDE_ASM("asm/nonmatchings/game/Sound", updateCanyonLevelSound__16CanyonLevelSound);
INCLUDE_ASM("asm/nonmatchings/game/Sound", initVegasLevelSound__15VegasLevelSound);
INCLUDE_ASM("asm/nonmatchings/game/Sound", updateVegasLevelSound__15VegasLevelSound);
INCLUDE_ASM("asm/nonmatchings/game/Sound", initFinalLevelSound__15FinalLevelSound);
INCLUDE_ASM("asm/nonmatchings/game/Sound", updateFinalLevelSound__15FinalLevelSound);
INCLUDE_ASM("asm/nonmatchings/game/Sound", initAirportLevelSound__17AirportLevelSound);
INCLUDE_ASM("asm/nonmatchings/game/Sound", updateAirportLevelSound__17AirportLevelSound);
INCLUDE_ASM("asm/nonmatchings/game/Sound", initSanFranLevelSound__17SanFranLevelSound);
INCLUDE_ASM("asm/nonmatchings/game/Sound", updateSanFranLevelSound__17SanFranLevelSound);
INCLUDE_ASM("asm/nonmatchings/game/Sound", playPlaneLandingSound__17AirportLevelSoundP8_fvector);
INCLUDE_ASM("asm/nonmatchings/game/Sound", startQuakeSound__17SanFranLevelSound);
INCLUDE_ASM("asm/nonmatchings/game/Sound", initTokyoLevelSound__15TokyoLevelSound);
INCLUDE_ASM("asm/nonmatchings/game/Sound", updateTokyoLevelSound__15TokyoLevelSound);
INCLUDE_ASM("asm/nonmatchings/game/Sound", initBigTossLevelSound__17BigTossLevelSound);
INCLUDE_ASM("asm/nonmatchings/game/Sound", updateBigTossLevelSound__17BigTossLevelSound);
INCLUDE_ASM("asm/nonmatchings/game/Sound", initThreeMileLevelSound__19ThreeMileLevelSound);
INCLUDE_ASM("asm/nonmatchings/game/Sound", setAllStacksDestroyed__19ThreeMileLevelSound);
INCLUDE_ASM("asm/nonmatchings/game/Sound", resetReactorSoundInfo__19ThreeMileLevelSoundi);
INCLUDE_ASM("asm/nonmatchings/game/Sound", updateThreeMileLevelSound__19ThreeMileLevelSound);
INCLUDE_ASM("asm/nonmatchings/game/Sound", playWickedBigExplosion__19ThreeMileLevelSound);
INCLUDE_ASM("asm/nonmatchings/game/Sound", playShockSound__19ThreeMileLevelSoundP8_fvector);
INCLUDE_ASM("asm/nonmatchings/game/Sound", playThreeMileCountdown__19ThreeMileLevelSoundi);
INCLUDE_ASM("asm/nonmatchings/game/Sound", initVolcanoLevelSound__17VolcanoLevelSound);
INCLUDE_ASM("asm/nonmatchings/game/Sound", updateVolcanoLevelSound__17VolcanoLevelSound);
INCLUDE_ASM("asm/nonmatchings/game/Sound", initUFOLevelSound__13UFOLevelSound);
INCLUDE_ASM("asm/nonmatchings/game/Sound", updateUFOLevelSound__13UFOLevelSound);
INCLUDE_ASM("asm/nonmatchings/game/Sound", initMetroLevelSound__15MetroLevelSound);
INCLUDE_ASM("asm/nonmatchings/game/Sound", updateMetroLevelSound__15MetroLevelSound);
INCLUDE_ASM("asm/nonmatchings/game/Sound", initMilitaryLevelSound__18MilitaryLevelSound);
INCLUDE_ASM("asm/nonmatchings/game/Sound", updateMilitaryLevelSound__18MilitaryLevelSound);
INCLUDE_ASM("asm/nonmatchings/game/Sound", initIslandLevelSound__16IslandLevelSound);
INCLUDE_ASM("asm/nonmatchings/game/Sound", updateIslandLevelSound__16IslandLevelSound);
INCLUDE_ASM("asm/nonmatchings/game/Sound", initCanyon2LevelSound__17Canyon2LevelSound);
INCLUDE_ASM("asm/nonmatchings/game/Sound", updateCanyon2LevelSound__17Canyon2LevelSound);
INCLUDE_ASM("asm/nonmatchings/game/Sound", playDamDestroyedSound__17Canyon2LevelSound);
INCLUDE_ASM("asm/nonmatchings/game/Sound", initHeightBasedSound__16HeightBasedSoundiii);
INCLUDE_ASM("asm/nonmatchings/game/Sound", updateHeightBasedSound__16HeightBasedSound);
INCLUDE_ASM("asm/nonmatchings/game/Sound", initLevelObjectSound__16LevelObjectSoundiiiiiii);
INCLUDE_ASM("asm/nonmatchings/game/Sound", stopLevelObjectSound__16LevelObjectSound);
INCLUDE_ASM("asm/nonmatchings/game/Sound", updateLevelObjectSound__16LevelObjectSound);
INCLUDE_ASM("asm/nonmatchings/game/Sound", updateLevelObjectSound__21NoPanLevelObjectSound);
INCLUDE_ASM("asm/nonmatchings/game/Sound", updateLevelObjectSound__24InvertedLevelObjectSound);
INCLUDE_ASM("asm/nonmatchings/game/Sound", initTankSound__9TankSound);
INCLUDE_ASM("asm/nonmatchings/game/Sound", updateTankSound__9TankSoundP10TankPickupi);
INCLUDE_ASM("asm/nonmatchings/game/Sound", terminateTankSound__9TankSound);
INCLUDE_ASM("asm/nonmatchings/game/Sound", playTankFireSound__9TankSoundP8_fvector);
INCLUDE_ASM("asm/nonmatchings/game/Sound", initHeliSound__9HeliSound);
INCLUDE_ASM("asm/nonmatchings/game/Sound", updateHeliSound__9HeliSoundP10HeliPickup);
INCLUDE_ASM("asm/nonmatchings/game/Sound", terminateHeliSound__9HeliSound);
INCLUDE_ASM("asm/nonmatchings/game/Sound", initCarSound__8CarSoundP9CarPickup);
INCLUDE_ASM("asm/nonmatchings/game/Sound", updateCarSound__8CarSound);
INCLUDE_ASM("asm/nonmatchings/game/Sound", playHornSound__8CarSound);
INCLUDE_ASM("asm/nonmatchings/game/Sound", terminateCarSound__8CarSound);
INCLUDE_ASM("asm/nonmatchings/game/Sound", initPickupSound__11PickupSoundP6Pickup);
INCLUDE_ASM("asm/nonmatchings/game/Sound", playBatImpactSound__11PickupSound);
INCLUDE_ASM("asm/nonmatchings/game/Sound", updatePickupSound__11PickupSound);
INCLUDE_ASM("asm/nonmatchings/game/Sound", updateTurretSound__11PickupSoundiff);
INCLUDE_ASM("asm/nonmatchings/game/Sound", terminateTurretSound__11PickupSound);
INCLUDE_ASM("asm/nonmatchings/game/Sound", terminatePickupSound__11PickupSound);
INCLUDE_ASM("asm/nonmatchings/game/Sound", playTurretPickupFireSound__11PickupSoundi);
INCLUDE_ASM("asm/nonmatchings/game/Sound", __10ShellSound);
INCLUDE_ASM("asm/nonmatchings/game/Sound", resetShellSoundFlags__10ShellSound);
INCLUDE_ASM("asm/nonmatchings/game/Sound", updateShellSound__10ShellSound);
INCLUDE_ASM("asm/nonmatchings/game/Sound", updateTokenSound__10ShellSound);
INCLUDE_ASM("asm/nonmatchings/game/Sound", playTokenCountFinishedSound__10ShellSound);
INCLUDE_ASM("asm/nonmatchings/game/Sound", stopTokenSound__10ShellSound);
INCLUDE_ASM("asm/nonmatchings/game/Sound", updateSFXSliderSound__10ShellSound);
INCLUDE_ASM("asm/nonmatchings/game/Sound", stopSFXSliderSound__10ShellSound);
INCLUDE_ASM("asm/nonmatchings/game/Sound", terminateShellSound__10ShellSound);
INCLUDE_ASM("asm/nonmatchings/game/Sound", playShellSFXSound__10ShellSoundi);
INCLUDE_ASM("asm/nonmatchings/game/Sound", playSweetUnlockedSound__10ShellSound);
INCLUDE_ASM("asm/nonmatchings/game/Sound", playPauseMenuSound__10ShellSoundi);
INCLUDE_ASM("asm/nonmatchings/game/Sound", initHudSound__8HudSound);
INCLUDE_ASM("asm/nonmatchings/game/Sound", updateHudSound__8HudSound);
INCLUDE_ASM("asm/nonmatchings/game/Sound", initGenericSound__12GenericSoundiiii);
INCLUDE_ASM("asm/nonmatchings/game/Sound", terminateGenericSound__12GenericSound);
INCLUDE_ASM("asm/nonmatchings/game/Sound", updateGenericSound__12GenericSoundP8_fvector);
INCLUDE_ASM("asm/nonmatchings/game/Sound", initPedSound__8PedSound);
INCLUDE_ASM("asm/nonmatchings/game/Sound", terminatePedSound__8PedSound);
INCLUDE_ASM("asm/nonmatchings/game/Sound", updatePedSound__8PedSoundP8_fvectori);
INCLUDE_ASM("asm/nonmatchings/game/Sound", initFireSound__9FireSound);
INCLUDE_ASM("asm/nonmatchings/game/Sound", terminateFireSound__9FireSound);
INCLUDE_ASM("asm/nonmatchings/game/Sound", updateFireSound__9FireSoundP8_fvector);
INCLUDE_ASM("asm/nonmatchings/game/Sound", initPlaneSound__10PlaneSound);
INCLUDE_ASM("asm/nonmatchings/game/Sound", terminatePlaneSound__10PlaneSound);
INCLUDE_ASM("asm/nonmatchings/game/Sound", updatePlaneSound__10PlaneSoundP8_fvectorif);
INCLUDE_ASM("asm/nonmatchings/game/Sound", initWeaponSound__11WeaponSound);
INCLUDE_ASM("asm/nonmatchings/game/Sound", terminateWeaponSound__11WeaponSound);
INCLUDE_ASM("asm/nonmatchings/game/Sound", updateWeaponSound__11WeaponSoundP6Weapon);
INCLUDE_ASM("asm/nonmatchings/game/Sound", playWeaponLaunchSound__11WeaponSoundP8_fvectori);
INCLUDE_ASM("asm/nonmatchings/game/Sound", playWeaponExplosion__11WeaponSoundP8_fvectorii);
INCLUDE_ASM("asm/nonmatchings/game/Sound", playRockSpecialImpact__11WeaponSoundP8_fvector);
INCLUDE_ASM("asm/nonmatchings/game/Sound", __12MonsterSound);
INCLUDE_ASM("asm/nonmatchings/game/Sound", initMonsterSound__12MonsterSoundP7Monster);
INCLUDE_ASM("asm/nonmatchings/game/Sound", updateMonsterSound__12MonsterSound);
INCLUDE_ASM("asm/nonmatchings/game/Sound", updateMonsterCinemaSound__12MonsterSound);
INCLUDE_ASM("asm/nonmatchings/game/Sound", updateGeneralMonsterSounds__12MonsterSound);
INCLUDE_ASM("asm/nonmatchings/game/Sound", playSoundFromAnimation__12MonsterSoundf);
INCLUDE_ASM("asm/nonmatchings/game/Sound", playSignatureSound__12MonsterSound);
INCLUDE_ASM("asm/nonmatchings/game/Sound", playTakeHitSound__12MonsterSound);
INCLUDE_ASM("asm/nonmatchings/game/Sound", playCloakingSound__12MonsterSound);
INCLUDE_ASM("asm/nonmatchings/game/Sound", playEnterCounterBlockSound__12MonsterSound);
INCLUDE_ASM("asm/nonmatchings/game/Sound", playGeneratorTazerSound__12MonsterSound);
INCLUDE_ASM("asm/nonmatchings/game/Sound", playJellyTazerOneShot__12MonsterSound);
INCLUDE_ASM("asm/nonmatchings/game/Sound", playShockedSound__12MonsterSound);
INCLUDE_ASM("asm/nonmatchings/game/Sound", playPedSquishSound__12MonsterSound);
INCLUDE_ASM("asm/nonmatchings/game/Sound", playRockmanSpecialLaunch__12MonsterSound);
INCLUDE_ASM("asm/nonmatchings/game/Sound", playRockmanSpecialBounce__12MonsterSoundP8_fvector);
INCLUDE_ASM("asm/nonmatchings/game/Sound", playCounterAttackSound__12MonsterSound);
INCLUDE_ASM("asm/nonmatchings/game/Sound", playRockmanSpecialExplode__12MonsterSoundP8_fvector);
INCLUDE_ASM("asm/nonmatchings/game/Sound", updateClimbingSound__12MonsterSound11MonsterAnim);
INCLUDE_ASM("asm/nonmatchings/game/Sound", updateJellyTazer__12MonsterSound);
INCLUDE_ASM("asm/nonmatchings/game/Sound", terminateJellyTazer__12MonsterSound);
INCLUDE_ASM("asm/nonmatchings/game/Sound", terminateDashSound__12MonsterSound);
INCLUDE_ASM("asm/nonmatchings/game/Sound", playRamSound__12MonsterSound);
INCLUDE_ASM("asm/nonmatchings/game/Sound", updateDashSound__12MonsterSound);
INCLUDE_ASM("asm/nonmatchings/game/Sound", updateOgreSound__12MonsterSound);
INCLUDE_ASM("asm/nonmatchings/game/Sound", updateRobotSound__12MonsterSound);
INCLUDE_ASM("asm/nonmatchings/game/Sound", updateMantisSound__12MonsterSound);
INCLUDE_ASM("asm/nonmatchings/game/Sound", updateShogunSound__12MonsterSound);
INCLUDE_ASM("asm/nonmatchings/game/Sound", updateLavaSpecialInhaleChargeup__12MonsterSound);
INCLUDE_ASM("asm/nonmatchings/game/Sound", terminateLavaSpecialInhaleChargeup__12MonsterSound);
INCLUDE_ASM("asm/nonmatchings/game/Sound", updateLavaSound__12MonsterSound);
INCLUDE_ASM("asm/nonmatchings/game/Sound", updateAlienSound__12MonsterSound);
INCLUDE_ASM("asm/nonmatchings/game/Sound", updateFireBreath__12MonsterSound);
INCLUDE_ASM("asm/nonmatchings/game/Sound", playPunchSound__12MonsterSound);
INCLUDE_ASM("asm/nonmatchings/game/Sound", playVictorySound__12MonsterSoundi);
INCLUDE_ASM("asm/nonmatchings/game/Sound", playTakeBatHitSound__12MonsterSound);
INCLUDE_ASM("asm/nonmatchings/game/Sound", playTakeHit__12MonsterSoundP7Monster);
INCLUDE_ASM("asm/nonmatchings/game/Sound", playKnockBackSound__12MonsterSound);
INCLUDE_ASM("asm/nonmatchings/game/Sound", terminateFlightGlideSound__12MonsterSound);
INCLUDE_ASM("asm/nonmatchings/game/Sound", updateFlightGlideSound__12MonsterSoundi);
INCLUDE_ASM("asm/nonmatchings/game/Sound", playButtSlamSound__12MonsterSound);
INCLUDE_ASM("asm/nonmatchings/game/Sound", playLandingSound__12MonsterSound);
INCLUDE_ASM("asm/nonmatchings/game/Sound", playFootstepSound__12MonsterSound);
INCLUDE_ASM("asm/nonmatchings/game/Sound", playScrape__12MonsterSound);
INCLUDE_ASM("asm/nonmatchings/game/Sound", playRobotHeadSound__12MonsterSound);
INCLUDE_ASM("asm/nonmatchings/game/Sound", playRoboJetOneShot__12MonsterSound);
INCLUDE_ASM("asm/nonmatchings/game/Sound", playBodyImpactSound__12MonsterSound);
INCLUDE_ASM("asm/nonmatchings/game/Sound", playGrappleAttackSound__12MonsterSound);
INCLUDE_ASM("asm/nonmatchings/game/Sound", playStompAttackSound__12MonsterSound);
INCLUDE_ASM("asm/nonmatchings/game/Sound", terminateGrappleSound__12MonsterSound);
INCLUDE_ASM("asm/nonmatchings/game/Sound", playGrappleThrowSound__12MonsterSound);
INCLUDE_ASM("asm/nonmatchings/game/Sound", playBatWhooshSound__12MonsterSound);
INCLUDE_ASM("asm/nonmatchings/game/Sound", playObjectThrowSound__12MonsterSound);
INCLUDE_ASM("asm/nonmatchings/game/Sound", playCrowdControlSound__12MonsterSound);
INCLUDE_ASM("asm/nonmatchings/game/Sound", play206Sound__12MonsterSound);
INCLUDE_ASM("asm/nonmatchings/game/Sound", playTauntSound__12MonsterSound);
INCLUDE_ASM("asm/nonmatchings/game/Sound", playPickupSound__12MonsterSound);
INCLUDE_ASM("asm/nonmatchings/game/Sound", playGrapplePickupSound__12MonsterSound);
INCLUDE_ASM("asm/nonmatchings/game/Sound", stopPickupSound__12MonsterSound);
INCLUDE_ASM("asm/nonmatchings/game/Sound", playWingFlap__12MonsterSound);
INCLUDE_ASM("asm/nonmatchings/game/Sound", playImpalerRemoveSound__12MonsterSound);
INCLUDE_ASM("asm/nonmatchings/game/Sound", playGrappleSwing__12MonsterSoundi);
INCLUDE_ASM("asm/nonmatchings/game/Sound", playSwing__12MonsterSoundii);
INCLUDE_ASM("asm/nonmatchings/game/Sound", playJumpSound__12MonsterSound);
INCLUDE_ASM("asm/nonmatchings/game/Sound", playDeathSound__12MonsterSoundi);
INCLUDE_ASM("asm/nonmatchings/game/Sound", playSonicRoarSound__12MonsterSound);
INCLUDE_ASM("asm/nonmatchings/game/Sound", playWaterSplashSound__12MonsterSound);
INCLUDE_ASM("asm/nonmatchings/game/Sound", playWaterFootstepSound__12MonsterSound);
INCLUDE_ASM("asm/nonmatchings/game/Sound", updateAssBossRotorSound__12MonsterSound);
INCLUDE_ASM("asm/nonmatchings/game/Sound", terminateAssBossRotorSound__12MonsterSound);
INCLUDE_ASM("asm/nonmatchings/game/Sound", updateAssBossSpinSound__12MonsterSound);
INCLUDE_ASM("asm/nonmatchings/game/Sound", terminateAssBossSpinSound__12MonsterSound);
INCLUDE_ASM("asm/nonmatchings/game/Sound", playAssBossWeaponDrawSound__12MonsterSound);
INCLUDE_ASM("asm/nonmatchings/game/Sound", updateRobotReticleSound__12MonsterSound);
INCLUDE_ASM("asm/nonmatchings/game/Sound", terminateRobotReticleSound__12MonsterSound);
INCLUDE_ASM("asm/nonmatchings/game/Sound", updateElectricShockSound__12MonsterSound);
INCLUDE_ASM("asm/nonmatchings/game/Sound", terminateElectricShockSound__12MonsterSound);
INCLUDE_ASM("asm/nonmatchings/game/Sound", updateAlienSpecial__12MonsterSoundi);
INCLUDE_ASM("asm/nonmatchings/game/Sound", terminateAlienSpecial__12MonsterSound);
INCLUDE_ASM("asm/nonmatchings/game/Sound", updateZapAttackSound__12MonsterSoundiP7Monster);
INCLUDE_ASM("asm/nonmatchings/game/Sound", terminateZapAttackSound__12MonsterSound);
INCLUDE_ASM("asm/nonmatchings/game/Sound", playShogunSpecialImpactSound__12MonsterSound);
INCLUDE_ASM("asm/nonmatchings/game/Sound", updateShogunSpecialSound__12MonsterSound);
INCLUDE_ASM("asm/nonmatchings/game/Sound", terminateShogunSpecialSound__12MonsterSound);
INCLUDE_ASM("asm/nonmatchings/game/Sound", terminateEvilEye__12MonsterSound);
INCLUDE_ASM("asm/nonmatchings/game/Sound", updateEvilEye__12MonsterSound);
INCLUDE_ASM("asm/nonmatchings/game/Sound", updateSpiderMagnoSpecial__12MonsterSound);
INCLUDE_ASM("asm/nonmatchings/game/Sound", terminateSpiderMagnoSpecial__12MonsterSound);
INCLUDE_ASM("asm/nonmatchings/game/Sound", playLavaSpecialInhale__12MonsterSound);
INCLUDE_ASM("asm/nonmatchings/game/Sound", playSuckerBugLaunch__12MonsterSound);
INCLUDE_ASM("asm/nonmatchings/game/Sound", playShogunSpecialOneShot__12MonsterSound);
INCLUDE_ASM("asm/nonmatchings/game/Sound", playAlienRaygunDrawSound__12MonsterSound);
INCLUDE_ASM("asm/nonmatchings/game/Sound", initSuckerBugSound__14SuckerBugSound);
INCLUDE_ASM("asm/nonmatchings/game/Sound", terminateSuckerBugSound__14SuckerBugSound);
INCLUDE_ASM("asm/nonmatchings/game/Sound", updateSuckerBugSound__14SuckerBugSoundP8_fvectori);
INCLUDE_ASM("asm/nonmatchings/game/Sound", stopAllMonsterSounds__12MonsterSound);
INCLUDE_ASM("asm/nonmatchings/game/Sound", initTriggerVolumeSound__18TriggerVolumeSoundiUi);
INCLUDE_ASM("asm/nonmatchings/game/Sound", terminateTriggerVolumeSound__18TriggerVolumeSound);
INCLUDE_ASM("asm/nonmatchings/game/Sound", updateTriggerVolumeSound__18TriggerVolumeSound);
INCLUDE_ASM("asm/nonmatchings/game/Sound", initTriggerVolumeSound__24RangedTriggerVolumeSoundiUiii);
INCLUDE_ASM("asm/nonmatchings/game/Sound", terminateTriggerVolumeSound__24RangedTriggerVolumeSound);
INCLUDE_ASM("asm/nonmatchings/game/Sound", updateTriggerVolumeSound__24RangedTriggerVolumeSound);
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
INCLUDE_ASM("asm/nonmatchings/game/Sound", stopMechaLaserImpactSound__12MonsterSound);
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
