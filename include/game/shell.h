#ifndef SHELL_H
#define SHELL_H

struct _hierhead;
enum _vramAddrs { VRAM_ADDRS_DUMMY };

/* Front-end shell: level names and file name formatting used by the point tools. */
class Shell {
public:
    char pad0[0x2938];
    int m_monsterSel[8];  /* 0x2938: per slot (players first, then AIs): (monster << 5) | variant; monster indexes MonsterLongNames */
    char pad2958[0x2A30 - 0x2958];
    int m_sfxVolume;      /* 0x2A30: options sfx volume (0..0x400), master volume groups 0/3/4/5 */
    int m_musicVolume;    /* 0x2A34: options music volume, group 1 */
    char pad2A38[0x2BA8 - 0x2A38];
    int m_restart;        /* 0x2BA8: 1 = the level is being restarted (ResetLevel, not UnpauseLevel) and needs the loading background */
    char pad2BAC[0x2BB0 - 0x2BAC];
    int m_levelNum;       /* 0x2BB0: index into GameLevelNames (0 = the level named on the command line) */
    int m_numPlayers;     /* 0x2BB4 */
    int m_numAIs;         /* 0x2BB8 */
    char pad2BBC[0x2BCC - 0x2BBC];
    int m_inSession;      /* 0x2BCC: a play session is running (rtMain loop) */
    int m_inMenus;        /* 0x2BD0: the boot -> menus -> session cycle keeps going */
    char pad2BD4[0x2BE0 - 0x2BD4];
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
    void BootInitGame(void);
    void InitBeforeUiDbLoad(void);
    void InitBeforeUserintDbLoad(void);
    void LoadUserintDB(void);
    void LoadUserintTexture(void);
    void InitRTState(void);
    void FinishLoadBar(void);
    void FadeScreen(int a, bool b, unsigned char c, unsigned char d, unsigned char e, unsigned char f, unsigned char g, unsigned char h);
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

#endif
