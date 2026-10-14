#ifndef SHELL_H
#define SHELL_H

#include "engine.h"

struct _hierhead;
struct _cs;
struct _hierswitch;
enum _vramAddrs { VRAM_ADDRS_DUMMY };

/* Front-end shell: level names and file name formatting used by the point tools. */
class Shell {
public:
    _cs *m_menuCs;        /* 0x0: root cs of the front-end menus (hidden by screenWaitForStart) */
    char pad4[0x5C - 0x4];
    _cs *m_cs5C;          /* 0x5C: drawn from uiInitScreens on */
    _cs *m_buttonCs[4];   /* 0x60: cs of the four button prompts drawn at the bottom of the menus (screen.cpp showButtons) */
    _hierswitch *m_sw70[3]; /* 0x70: all three set to 1 by the one-player modes menu, 0 by the two-player one */
    char pad7C[0x84 - 0x7C];
    _cs *m_costumeBoxCs3; /* 0x84: third costume panel of the three-player select (screenCharSelect3P: the AI, or in mode 2 the first AI) */
    _cs *m_costumeBoxCs;  /* 0x88: costume panel of the monster select (+0x7C is its alpha, faded in by screenCharSelect1P) */
    _cs *m_costumeBoxCs2; /* 0x8C: m_costumeBoxCs of player two (or the AI) in the two-player select */
    _hierswitch *m_costumeSw[4]; /* 0x90: one per costume of the highlighted monster: 0 unavailable, 1 available, 2 cursor */
    _hierswitch *m_costumeSw2[4]; /* 0xA0: m_costumeSw of player two in the two-player selects */
    _cs *m_costumeBoxCsMG[2];     /* 0xB0: costume panel of each player in the two-player minigame select */
    _hierswitch *m_costumeSwMG[2][4]; /* 0xB8: m_costumeSw of each player in the two-player minigame select */
    char padD8[0xF8 - 0xD8];
    _hierswitch *m_costumeSw3[4]; /* 0xF8: m_costumeSw of the third panel (m_costumeBoxCs3) */
    char pad108[0x160 - 0x108];
    _hierobject *m_movieScreen; /* 0x160: object the menu background movie is played on (moviesReadyScreenObject) */
    char pad164[0x168 - 0x164];
    _hierswitch *m_selectSw[10]; /* 0x168: one per monster of the select screen; [0] and [9] show 4 (hidden) while that monster is locked */
    char pad190[0x194 - 0x190];
    _hierswitch *m_selectSwMG[10]; /* 0x194: m_selectSw of the minigame monster select */
    char pad1BC[0x1C0 - 0x1BC];
    _hierswitch *m_monsterCursorSw[3]; /* 0x1C0: monster column under each cursor of the multi-player selects (player one, player two or second AI, AI) */
    char pad1CC[0x25C - 0x1CC];
    _hierswitch *m_menuItemSw[22][10]; /* 0x25C: [screen][item] highlight switch of each menu item (null ends the list); TagList::m_tags */
    _hierswitch *m_sw5CC; /* 0x5CC: set to 0 when a two-player mode is picked */
    _hierswitch *m_sw5D0; /* 0x5D0: 0 on the main and mode menus; the level select sets it to the first level of the list */
    _hierswitch *m_screenSw; /* 0x5D4: picks the panel of the current menu screen (screen - 1) */
    char pad5D8[0x5DC - 0x5D8];
    /* options screen (10) */
    _hierswitch *m_difficultySw[3]; /* 0x5DC: one lit per value of TheGame+0x1203CC */
    _hierswitch *m_vibrationSw[2][2]; /* 0x5E8: per pad, [0] lit when vibration is on, [1] when off */
    _hierswitch *m_stereoSw;  /* 0x5F8: lit when m_mono is 0 */
    _hierswitch *m_monoSw;    /* 0x5FC */
    char pad600[0x604 - 0x600];
    _cs *m_sliderCs[2];      /* 0x604: knobs of the sfx and music volume bars (setSlider moves them) */
    char pad60C[0x614 - 0x60C];
    _hierswitch *m_lockedSw; /* 0x614: 1 while the level highlighted in the level select is still locked */
    char pad618[0x61C - 0x618];
    _animHandle m_screenAnim[12]; /* 0x61C: menu transition animations 1..11 (anim 0x270E of the shell db); changeScreen plays one */
    char *m_menuObj[22][90]; /* 0x6DC: [screen][i] objects drawn on each menu screen (TagList::m_entries); +0x4C is their alpha, faded by screenTransition */
    _hierswitch *m_sw25CC; /* 0x25CC: set to 1 (with m_sw25D0) when the main menu opens screen 9 */
    _hierswitch *m_sw25D0; /* 0x25D0 */
    _animHandle m_reelSpinAnim[2]; /* 0x25D4: one per select reel (anims 0x2585 + reel) */
    char pad25F4[0x2644 - 0x25F4];
    _animHandle m_reelAnim[2][10]; /* 0x2644: per reel, the animation of each of its 10 positions (anims 0x259E + reel * 10 + i); changeReel */
    char pad2784[0x2938 - 0x2784];
    int m_monsterSel[8];  /* 0x2938: per slot (players first, then AIs): (monster << 5) | variant; monster indexes MonsterLongNames */
    char pad2958[0x29E0 - 0x2958];
    int m_levelNumAIs[17]; /* 0x29E0: per m_levelNum, the AIs of game mode 1 (first item of the one-player modes menu) on that level */
    int m_monsterFlags[3]; /* 0x2A24: one bit per monster index (onBitMonsters): [0] available, [1] locked, [2] chosen */
    int m_sfxVolume;      /* 0x2A30: options sfx volume (0..0x400), master volume groups 0/3/4/5 */
    int m_musicVolume;    /* 0x2A34: options music volume, group 1 */
    int m_sfxVolumeSaved;   /* 0x2A38: m_sfxVolume when the options screen was opened */
    int m_musicVolumeSaved; /* 0x2A3C: m_musicVolume when the options screen was opened */
    char pad2A40[0x2A44 - 0x2A40];
    int m_killTarget;     /* 0x2A44: wins needed to take the match (0 = no target, rounds just restart) */
    char pad2A48[0x2B40 - 0x2A48];
    int m_continues[2];   /* 0x2B40: rounds each player can still lose in a two-player match (from m_elimination) */
    int m_elimination;    /* 0x2B48: elimination setting picked in the menu, plus one */
    int m_wins[2];        /* 0x2B4C: rounds won by each player in the current match (shown through Monster::m_winsThisGame) */
    int m_battleMode;     /* 0x2B54: battle type picked in the menu (3, 6, ...) */
    int m_survivor;       /* 0x2B58: player left standing when the other ran out of continues */
    int m_roundsPlayed;   /* 0x2B5C */
    char pad2B60[0x2B8C - 0x2B60];
    int m_ctlrCfg[2];     /* 0x2B8C: controller configuration of each player, 1-based (0 = not set yet; Monster+0x672C gets it minus one) */
    int m_vibration[2];   /* 0x2B94: vibration option of each player (TheGame::SetVibration) */
    int m_selFirst;       /* 0x2B9C: first selectable monster column of the select screen (1 while the first monster is locked) */
    int m_selLast;        /* 0x2BA0: one past the last selectable column (9 while the last monster is locked) */
    char pad2BA4[0x2BA8 - 0x2BA4];
    int m_restart;        /* 0x2BA8: 1 = the level is being restarted (ResetLevel, not UnpauseLevel) and needs the loading background */
    char pad2BAC[0x2BB0 - 0x2BAC];
    int m_levelNum;       /* 0x2BB0: index into GameLevelNames (0 = the level named on the command line) */
    int m_numPlayers;     /* 0x2BB4 */
    int m_numAIs;         /* 0x2BB8 */
    int m_lives[4];       /* 0x2BBC: lives left per player (NUM_LIVES[mode] at the start, see InitPlayerLives) */
    int m_inSession;      /* 0x2BCC: a play session is running (rtMain loop) */
    int m_inMenus;        /* 0x2BD0: the boot -> menus -> session cycle keeps going */
    char pad2BD4[0x2BDC - 0x2BD4];
    int m_roundsDecided;  /* 0x2BDC: rounds that ended with exactly one monster standing */
    int m_costume[8];     /* 0x2BE0: costume number of the first two players, then of the AIs */
    char pad2C00[0x2C10 - 0x2C00];
    int m_mode; /* 0x2C10: 1 = normal play, 8 = bigshot, 9 = crush */
    char pad2C14[0x2C20 - 0x2C14];
    int m_mono;           /* 0x2C20: nonzero selects snd_SetPlaybackMode(1) */

    enum _shFileType { SH_FILE_0, SH_FILE_PLAYER, SH_FILE_AI };

    char *GetLevelName(void);
    void LoadLevelFiles(void);
    int MonsterExists(int i);
    int MonsterIsChosen(int i);
    void BootInitUi(void);
    void BootInitUserint(void);
    void BootInitUserint1(void);
    void BootInitGame(void);
    void InitBeforeUiDbLoad(void);
    void InitBeforeUserintDbLoad(void);
    void LoadUserintDB(void);
    void LoadUserintDB1(void);
    void LoadUserintTexture(void);
    void LoadUserintTexture1(void);
    void InitRTState(void);
    void FinishLoadBar(void);
    void FadeScreen(int a, bool b, unsigned char c, unsigned char d, unsigned char e, unsigned char f, unsigned char g, unsigned char h);
    void FadeScreen(int a, bool b, unsigned int &t0, unsigned int &t1, unsigned char c, unsigned char d, unsigned char e, unsigned char f,
                    unsigned char g, unsigned char h);
    void InitPlayers(void);
    void InitPlayerLives(void);
    void EvaluateGameStatus(int r);
    void EvaluateOnePlayerStoryStatus(int r);
    void EvaluateOnePlayerChallengeStatus(int r);
    void EvaluateTwoPlayerCoopStatus(int r);
    void EvaluateMultiPlayerBattleStatusNoAI(int r);
    void EvaluateMultiPlayerBattleStatusAI(int r);
    void EvaluateOnePlayerEnduranceStatus(int r);
    void EvaluateBigShotStatus(int r);
    void EvaluateCrushStatus(int r);
    void EvaluateDodgeBallStatus(int r);
    void EvaluateOnlineBattleStatus(int r);
    void DisplayLoadBackground(bool b);
    void GenesisMovie(void);
    void InitGS(short mode);
    int Use30HzMode(void);
    int MonsterIsLocked(int i);
    void SelectAI(void);
    void RandomlySelectAI(void);
    void SetMenuItemFlag(int menu, int item, int on);
    void EnableMonsterSelection(int sel);
    void ResetLevel(void);
    void InitialMemCardScreen(void);
    void LoadLevelDB(void);
    void LoadMonstersDB(void);
    void LoadResTexture(void);
    void LoadTexture(void);
    _vramAddrs getVramAddr(void);
    void AddEpNode(int i, _hierhead *h);
    void formatFilename1(char *dst, const char *name, int costume, const char *ext, _shFileType t);
    void formatFilename(char *dst, const char *level, const char *ext, _shFileType t);
    static void formatFilename(char *dst, const char *a, const char *b, const char *c);
};
extern Shell *shell;

#define SHELL_AT(f, off) typedef char _shell_at_##f[(unsigned)&((Shell *)0)->f == (off) ? 1 : -1]
SHELL_AT(m_selectSw, 0x168);
SHELL_AT(m_menuItemSw, 0x25C);
SHELL_AT(m_screenSw, 0x5D4);
SHELL_AT(m_monoSw, 0x5FC);
SHELL_AT(m_sliderCs, 0x604);
SHELL_AT(m_movieScreen, 0x160);
SHELL_AT(m_screenAnim, 0x61C);
SHELL_AT(m_menuObj, 0x6DC);
SHELL_AT(m_reelSpinAnim, 0x25D4);
SHELL_AT(m_reelAnim, 0x2644);
SHELL_AT(m_ctlrCfg, 0x2B8C);
SHELL_AT(m_selLast, 0x2BA0);
SHELL_AT(m_monsterFlags, 0x2A24);
SHELL_AT(m_killTarget, 0x2A44);
SHELL_AT(m_continues, 0x2B40);
SHELL_AT(m_battleMode, 0x2B54);
SHELL_AT(m_lives, 0x2BBC);
SHELL_AT(m_inSession, 0x2BCC);
SHELL_AT(m_roundsDecided, 0x2BDC);
SHELL_AT(m_mode, 0x2C10);
SHELL_AT(m_lockedSw, 0x614);
SHELL_AT(m_levelNumAIs, 0x29E0);
SHELL_AT(pad7C, 0x7C);
SHELL_AT(m_costumeBoxCs, 0x88);
SHELL_AT(m_costumeSw, 0x90);
SHELL_AT(m_costumeBoxCs2, 0x8C);
SHELL_AT(m_costumeSw2, 0xA0);
SHELL_AT(m_costumeBoxCsMG, 0xB0);
SHELL_AT(m_costumeSwMG, 0xB8);
SHELL_AT(m_costumeBoxCs3, 0x84);
SHELL_AT(m_costumeSw3, 0xF8);
SHELL_AT(m_monsterCursorSw, 0x1C0);
SHELL_AT(m_sw5D0, 0x5D0);
SHELL_AT(m_sw25D0, 0x25D0);
SHELL_AT(m_musicVolumeSaved, 0x2A3C);
#undef SHELL_AT

#endif
