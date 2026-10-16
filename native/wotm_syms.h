// Gerado por tools/native_syms.py a partir de symbol_addrs.txt (tabela de simbolos do ELF retail). Nao edite a mao.
// wotm::sym::<nome> = endereco de um dado (global, tabela, objeto); wotm::fn::<nome> = endereco de uma funcao (hooks).
// Os nomes sao os do ELF (mangling do gcc 2.95), com '$' e '.' trocados por '_'.
#pragma once
#if __has_include("wotm_syms_nm.h")
#include "wotm_syms_nm.h"
#else
#include <cstdint>

namespace wotm {
namespace sym {
constexpr uint32_t x_xlaunch = 0x0024F100;
constexpr uint32_t attackMessages = 0x0025C480;   // size 0x3C
constexpr uint32_t retreatPts = 0x0025C4C0;   // size 0x30
constexpr uint32_t x_10AiPathTool_instance = 0x0025C4F0;   // size 0x4060
constexpr uint32_t ELEVATOR_LOWER_A_2511 = 0x00260550;   // size 0x10
constexpr uint32_t ELEVATOR_LOWER_B_2512 = 0x00260560;   // size 0x10
constexpr uint32_t DROP_ZONE_A_2516 = 0x00260570;   // size 0x10
constexpr uint32_t DROP_ZONE_B_2517 = 0x00260580;   // size 0x10
constexpr uint32_t airportPts = 0x00260590;   // size 0x80
constexpr uint32_t debrisOffset_2526 = 0x00260610;   // size 0x90
constexpr uint32_t x_11AnimContact_s_locatorId = 0x002606A0;   // size 0x18
constexpr uint32_t antHill = 0x002606C0;   // size 0x10
constexpr uint32_t x_14DodgeBallLevel_instance = 0x002606D0;   // size 0x44
constexpr uint32_t x_6Debris_m_debrisPool = 0x00260720;   // size 0x28800
constexpr uint32_t finalBoss = 0x00288F20;   // size 0x490
constexpr uint32_t attractPts = 0x002893B0;   // size 0x20
constexpr uint32_t resetObj = 0x00289400;   // size 0xC0C0
constexpr uint32_t globalResetObj = 0x002954C0;   // size 0xC0C0
constexpr uint32_t monsterStack = 0x002A1580;   // size 0x4B0
constexpr uint32_t monsterNametomonsterId = 0x002A1A40;   // size 0x80
constexpr uint32_t PO_1PFreeForAll_Levels = 0x002A1AC0;   // size 0x34
constexpr uint32_t PO_MPFreeForAll_Levels = 0x002A1AF8;   // size 0x34
constexpr uint32_t PO_MPElimination_Levels = 0x002A1B30;   // size 0x18
constexpr uint32_t PO_1PEndurance_Levels = 0x002A1B48;   // size 0x24
constexpr uint32_t PO_1PFreeForAll = 0x002A1B70;   // size 0xC
constexpr uint32_t PO_MPFreeForAll = 0x002A1B80;   // size 0xC
constexpr uint32_t PO_1PEndurance = 0x002A1B90;   // size 0xC
constexpr uint32_t PO_MPElimination = 0x002A1BA0;   // size 0xC
constexpr uint32_t levelMonsters = 0x002A1BB0;   // size 0x240
constexpr uint32_t levelMonsterModels = 0x002A1DF0;   // size 0xC0
constexpr uint32_t monsterSelectMode = 0x002A1EB0;   // size 0x840
constexpr uint32_t s_gunOffset_002A26F0 = 0x002A26F0;   // size 0x10
constexpr uint32_t x_9HomingBug_s_bugPool = 0x002A2700;   // size 0x900
constexpr uint32_t x_3Hud_m_letterbox = 0x002A3000;   // size 0x30
constexpr uint32_t s_hudGraphicData = 0x002A3030;   // size 0x120
constexpr uint32_t gMessages = 0x002A3150;   // size 0x9C
constexpr uint32_t centralParkMessagesA = 0x002A31F0;   // size 0xF0
constexpr uint32_t centralParkMessagesB = 0x002A32E0;   // size 0xF0
constexpr uint32_t centralParkMessagesC = 0x002A33D0;   // size 0xF0
constexpr uint32_t canyon2Messages = 0x002A34C0;   // size 0x78
constexpr uint32_t threeMileMessages = 0x002A3538;   // size 0x60
constexpr uint32_t capitolMessages = 0x002A3598;   // size 0x18
constexpr uint32_t feelsLikeTheFirstTime = 0x002A35B0;   // size 0x14
constexpr uint32_t textBoxOffset_2724 = 0x002A35D0;   // size 0x10
constexpr uint32_t gHudPos = 0x002A35E0;   // size 0x10
constexpr uint32_t volcanoTargets = 0x002A35F0;   // size 0xB0
constexpr uint32_t s_bgcolor = 0x002A36A0;   // size 0x40
constexpr uint32_t s_lightdir = 0x002A36E0;   // size 0x30
constexpr uint32_t s_lightcol = 0x002A3710;   // size 0x30
constexpr uint32_t s_ambient = 0x002A3740;   // size 0x10
constexpr uint32_t x_6McSync_global = 0x002A3750;   // size 0x38
constexpr uint32_t x_9McPageMgr_global = 0x002A3790;   // size 0x11B0
constexpr uint32_t stNull = 0x002A4940;   // size 0x30
constexpr uint32_t stProductDirectory = 0x002A4970;   // size 0xD
constexpr uint32_t stGameName = 0x002A4980;   // size 0x14
constexpr uint32_t stAvailSuffix = 0x002A4998;   // size 0x30
constexpr uint32_t stAvailUnformatted = 0x002A49C8;   // size 0x30
constexpr uint32_t stAvail = 0x002A49F8;   // size 0x60
constexpr uint32_t stPorts = 0x002A4A58;   // size 0x60
constexpr uint32_t stMedia = 0x002A4AB8;   // size 0xC0
constexpr uint32_t stPrompt = 0x002A4B78;   // size 0x120
constexpr uint32_t stWhichUnlock = 0x002A4C98;   // size 0x120
constexpr uint32_t stInfo = 0x002A4DB8;   // size 0x270
constexpr uint32_t stToCard = 0x002A5028;   // size 0x60
constexpr uint32_t stFromCard = 0x002A5088;   // size 0x60
constexpr uint32_t stWhatsOnTheCard = 0x002A50E8;   // size 0x60
constexpr uint32_t stActivityWarning = 0x002A5148;   // size 0x60
constexpr uint32_t stSure = 0x002A51A8;   // size 0x30
constexpr uint32_t stX = 0x002A51D8;   // size 0x120
constexpr uint32_t stT = 0x002A52F8;   // size 0x120
constexpr uint32_t stC = 0x002A5418;   // size 0x120
constexpr uint32_t stS = 0x002A5538;   // size 0x120
constexpr uint32_t stTitles = 0x002A5658;   // size 0x60
constexpr uint32_t stMCUtilStatus = 0x002A56B8;   // size 0x180
constexpr uint32_t arcCLUT = 0x002A5840;   // size 0x10
constexpr uint32_t UpMatrix = 0x002A5850;   // size 0x40
constexpr uint32_t x_13AnimBlendPool_s_pool = 0x002A5890;   // size 0x13B4
constexpr uint32_t x_9MonsterMc_global = 0x002A6C80;   // size 0x284
constexpr uint32_t x_10McBootPage_global = 0x002A6F08;   // size 0x278
constexpr uint32_t x_18McCheckForSettings_global = 0x002A7180;   // size 0x278
constexpr uint32_t x_14McLoadSettings_global = 0x002A73F8;   // size 0x274
constexpr uint32_t x_12McChooseSlot_global = 0x002A7670;   // size 0x278
constexpr uint32_t x_12McCheckSpace_global = 0x002A78E8;   // size 0x27C
constexpr uint32_t x_13McCheckFormat_global = 0x002A7B68;   // size 0x274
constexpr uint32_t x_16McCheckOverwrite_global = 0x002A7DE0;   // size 0x278
constexpr uint32_t x_14McSaveSettings_global = 0x002A8058;   // size 0x28C
constexpr uint32_t x_15McSaveAdventure_global = 0x002A82E8;   // size 0x274
constexpr uint32_t x_20McSaveAdventureBlank_global = 0x002A8560;   // size 0x274
constexpr uint32_t x_16McSelectSaveSlot_global = 0x002A8800;   // size 0x540
constexpr uint32_t x_11McAskToSave_global = 0x002A8D40;   // size 0x284
constexpr uint32_t x_11McAskIfSure_global = 0x002A8FC8;   // size 0x274
constexpr uint32_t x_11McErrorPage_global = 0x002A9240;   // size 0x274
constexpr uint32_t x_20McCheckForSweetTooth_global = 0x002A94B8;   // size 0x278
constexpr uint32_t x_13McCardRemoved_global = 0x002A9730;   // size 0x274
constexpr uint32_t x_15McPrepBoxScreen_global = 0x002A99C0;   // size 0x7C0
constexpr uint32_t x_15McLoadGameBoxes_global = 0x002AA180;   // size 0x274
constexpr uint32_t x_12McSelectReel_global = 0x002AA400;   // size 0x540
constexpr uint32_t s_animQueue = 0x002AA940;   // size 0x124
constexpr uint32_t s_stStoryGameName = 0x002AAA68;   // size 0xD
constexpr uint32_t s_gameTitle = 0x002AAA78;   // size 0x14
constexpr uint32_t s_productDirectory = 0x002AAA90;   // size 0xD
constexpr uint32_t s_iconFilename = 0x002AAAA0;   // size 0x9
constexpr uint32_t ENGLISH_MEMCARD_STRINGS = 0x002AAAB0;   // size 0x110
constexpr uint32_t g_triggerFunctions = 0x002AABC0;   // size 0x40
constexpr uint32_t x_8PathTool_instance = 0x002AAC00;   // size 0x4060
constexpr uint32_t x_12LevelPickups_s_highlightPickup = 0x002AEC60;   // size 0x40
constexpr uint32_t x_12LevelPickups_s_activeList = 0x002AECA0;   // size 0xC
constexpr uint32_t x_12LevelPickups_s_inUseList = 0x002AECB0;   // size 0xC
constexpr uint32_t x_12LevelPickups_s_inFlightList = 0x002AECC0;   // size 0xC
constexpr uint32_t x_12LevelPickups_s_regenList = 0x002AECD0;   // size 0xC
constexpr uint32_t x_12LevelPickups_s_deadList = 0x002AECE0;   // size 0xC
constexpr uint32_t x_12LevelPickups_tankerShock = 0x002AECF0;   // size 0x2A4
constexpr uint32_t headAttacks = 0x002AEF98;   // size 0x38
constexpr uint32_t s_powerUpCount = 0x002AEFD0;   // size 0x50
constexpr uint32_t s_tweakMinRegenCount = 0x002AF020;   // size 0x50
constexpr uint32_t s_tweakMaxRegenCount = 0x002AF070;   // size 0x50
constexpr uint32_t x_11PowerUpTool_instance = 0x002AF0C0;   // size 0x4060
constexpr uint32_t gRtReturn = 0x002B3120;   // size 0xC
constexpr uint32_t MainMovie = 0x002B3130;   // size 0x14
constexpr uint32_t screenTitles = 0x002B3148;   // size 0x58
constexpr uint32_t screenInstructions = 0x002B31A0;   // size 0x58
constexpr uint32_t movieFileNames = 0x002B31F8;   // size 0x5C
constexpr uint32_t animQueue = 0x002B3258;   // size 0x124
constexpr uint32_t monsterIsSelected_2829 = 0x002B3380;   // size 0x10
constexpr uint32_t promptString_2840 = 0x002B3390;   // size 0x40
constexpr uint32_t promptString_2960 = 0x002B33D0;   // size 0x40
constexpr uint32_t currentSelection = 0x002B3410;   // size 0x160
constexpr uint32_t NUM_LIVES = 0x002B3570;   // size 0x30
constexpr uint32_t shellGame = 0x002B35A0;   // size 0x122800
constexpr uint32_t shellDestructibles = 0x003D5DA0;   // size 0x3EFE0
constexpr uint32_t shellInfo = 0x00414D80;   // size 0x31C0
constexpr uint32_t uiResetFilename = 0x00417F40;   // size 0x40
constexpr uint32_t whichLevel = 0x00417F80;   // size 0xF
constexpr uint32_t x_9ShockWave_m_wavePool = 0x00417F90;   // size 0x1FE0
constexpr uint32_t gLoopManager = 0x00419F70;   // size 0xCC
constexpr uint32_t gStackDestroyed = 0x0041A040;   // size 0x10
constexpr uint32_t gSoundBanks = 0x0041A050;   // size 0x64
constexpr uint32_t x_10SpecFxAnim_s_fxPool = 0x0041A0B8;   // size 0x440
constexpr uint32_t x_11StartPoints_s_pointTypes = 0x0041A4F8;   // size 0x40
constexpr uint32_t x_14StartPointTool_instance = 0x0041A540;   // size 0x4060
constexpr uint32_t x_11StickShaker_m_shakePool = 0x0041E5A0;   // size 0x4380
constexpr uint32_t s_gunOffset_00422920 = 0x00422920;   // size 0x10
constexpr uint32_t x_12TurretPickup_s_config = 0x00422930;   // size 0xA8
constexpr uint32_t stackStart = 0x004229E0;   // size 0x40
constexpr uint32_t stack1End = 0x00422A20;   // size 0x50
constexpr uint32_t stack2End = 0x00422A70;   // size 0x80
constexpr uint32_t stack3End = 0x00422AF0;   // size 0x70
constexpr uint32_t stack4End = 0x00422B60;   // size 0x90
constexpr uint32_t centerTower = 0x00422BF0;   // size 0x10
constexpr uint32_t plantBoss = 0x00422C00;   // size 0x10B0
constexpr uint32_t tidalWaveAnim = 0x00423CB0;   // size 0x10
constexpr uint32_t ufoPts = 0x00423CC0;   // size 0x20
constexpr uint32_t alienSaucers = 0x00423CE0;   // size 0x50
constexpr uint32_t myMovie = 0x00423D40;   // size 0x98
constexpr uint32_t oldScreens = 0x00423DD8;   // size 0x40
constexpr uint32_t chosenAI = 0x00423E18;   // size 0x28
constexpr uint32_t userintResetObj = 0x00423E40;   // size 0xC0C0
constexpr uint32_t ticString = 0x0042FF00;   // size 0x10
constexpr uint32_t gUserintReturn = 0x0042FF10;   // size 0xC
constexpr uint32_t bb = 0x0042FF20;   // size 0xD0
constexpr uint32_t devScreen = 0x00430000;   // size 0x1990
constexpr uint32_t netScreen = 0x00431990;   // size 0x1CF0
constexpr uint32_t whichRoleStrings = 0x00433680;   // size 0x18
constexpr uint32_t numAiStrings = 0x00433698;   // size 0xA
constexpr uint32_t whichPlayerValues = 0x004336A8;   // size 0x2C
constexpr uint32_t whichPlayerStrings = 0x004336D8;   // size 0x84
constexpr uint32_t whichAiStrings = 0x00433760;   // size 0x84
constexpr uint32_t whichModeStrings = 0x004337E8;   // size 0x30
constexpr uint32_t devUi = 0x00433818;   // size 0x34
constexpr uint32_t whichLevelStrings = 0x00433850;   // size 0x300
constexpr uint32_t whichServerStrings = 0x00433B50;   // size 0x900
constexpr uint32_t p_3 = 0x00434450;   // size 0x4
constexpr uint32_t x__new_handler = 0x00434454;   // size 0x4
constexpr uint32_t x_GLOBAL__F_nothrow = 0x00434458;
constexpr uint32_t x_GLOBAL__F___builtin_delete______src_gcc_cp_new2_cc1GeU73 = 0x004344F8;
constexpr uint32_t x_GLOBAL__F____9type_info = 0x00434540;
constexpr uint32_t x__terminate_func = 0x004348C4;   // size 0x4
constexpr uint32_t get_eh_context = 0x004348C8;   // size 0x4
constexpr uint32_t x_GLOBAL__F___default_terminate = 0x004348CC;
constexpr uint32_t x__unexpected_func = 0x00434EBC;   // size 0x4
constexpr uint32_t x_GLOBAL__F_terminate__Fv = 0x00434EC0;
constexpr uint32_t object_mutex = 0x00435250;   // size 0x4
constexpr uint32_t x__tiv = 0x00435258;   // size 0x8
constexpr uint32_t x__tix = 0x00435260;   // size 0x8
constexpr uint32_t x__til = 0x00435268;   // size 0x8
constexpr uint32_t x__tii = 0x00435270;   // size 0x8
constexpr uint32_t x__tis = 0x00435278;   // size 0x8
constexpr uint32_t x__tib = 0x00435280;   // size 0x8
constexpr uint32_t x__tic = 0x00435288;   // size 0x8
constexpr uint32_t x__tiw = 0x00435290;   // size 0x8
constexpr uint32_t x__tir = 0x00435298;   // size 0x8
constexpr uint32_t x__tid = 0x004352A0;   // size 0x8
constexpr uint32_t x__tif = 0x004352A8;   // size 0x8
constexpr uint32_t x__tiUi = 0x004352B0;   // size 0x8
constexpr uint32_t x__tiUl = 0x004352B8;   // size 0x8
constexpr uint32_t x__tiUx = 0x004352C0;   // size 0x8
constexpr uint32_t x__tiUs = 0x004352C8;   // size 0x8
constexpr uint32_t x__tiUc = 0x004352D0;   // size 0x8
constexpr uint32_t x__tiSc = 0x004352D8;   // size 0x8
constexpr uint32_t x_GLOBAL__F_before__C9type_infoRC9type_info = 0x004352E0;
constexpr uint32_t gCommBusy = 0x00435780;   // size 0x4
constexpr uint32_t gAwaitingInts = 0x00435784;   // size 0x4
constexpr uint32_t gStreamingInited = 0x00435788;   // size 0x4
constexpr uint32_t gCdCallback = 0x0043578C;   // size 0x4
constexpr uint32_t gSSRead = 0x00435790;   // size 0x4
constexpr uint32_t gSSReadDone = 0x00435794;   // size 0x4
constexpr uint32_t gLoadingFromFS = 0x00435798;   // size 0x4
constexpr uint32_t gSndCommandBuffePtr = 0x004357A0;   // size 0x8
constexpr uint32_t gCommandBuffeBytesAvail = 0x004357A8;   // size 0x8
constexpr uint32_t gSndCommandReturnDefPtr = 0x004357B0;   // size 0x8
constexpr uint32_t gReturnValuesPtr = 0x004357B8;   // size 0x8
constexpr uint32_t gCommandFillBuffer = 0x004357C0;   // size 0x4
constexpr uint32_t gCaching = 0x004357C4;   // size 0x4
constexpr uint32_t gLoadBusy = 0x004357C8;   // size 0x4
constexpr uint32_t gLoadReturnDef = 0x004357D0;   // size 0x10
constexpr uint32_t gLoadCB = 0x004357E0;   // size 0x4
constexpr uint32_t gLoadUserData = 0x004357E8;   // size 0x8
constexpr uint32_t gLoadCommand = 0x004357F0;   // size 0x4
constexpr uint32_t gLoadAwaits = 0x004357F4;   // size 0x4
constexpr uint32_t gSyncBuffer = 0x00435800;   // size 0x40
constexpr uint32_t gSyncSendBuffer = 0x00435840;   // size 0x40
constexpr uint32_t gSyncStringBuffer = 0x00435880;   // size 0x100
constexpr uint32_t gSndCommandBuffer1 = 0x00435980;   // size 0x1000
constexpr uint32_t gSndCommandBuffer2 = 0x00436980;   // size 0x1000
constexpr uint32_t gSndCommandReturnDef1 = 0x00437980;   // size 0x1000
constexpr uint32_t gSndCommandReturnDef2 = 0x00438980;   // size 0x1000
constexpr uint32_t gActualReturnValues1 = 0x00439980;   // size 0x420
constexpr uint32_t gActualReturnValues2 = 0x00439DC0;   // size 0x420
constexpr uint32_t gStats = 0x0043A200;   // size 0x20
constexpr uint32_t gLoadReturnValue = 0x0043A240;   // size 0x10
constexpr uint32_t gLoadParams = 0x0043A280;   // size 0x20
constexpr uint32_t x_6CsPool_m_activeList = 0x0043A2C0;   // size 0xC
constexpr uint32_t x_6CsPool_m_inactiveList = 0x0043A2D0;   // size 0xC
constexpr uint32_t x_6CsPool_m_HPActiveList = 0x0043A2E0;   // size 0xC
constexpr uint32_t dbsStack = 0x0043A2F0;   // size 0x3840
constexpr uint32_t defaultCLUT = 0x0043DB30;   // size 0x40
constexpr uint32_t cdFileSystemToc = 0x0043DB80;   // size 0x7000
constexpr uint32_t cdSectorBuffer = 0x00444B80;   // size 0x810
constexpr uint32_t fileStatus = 0x00445390;   // size 0x184
constexpr uint32_t gFileName = 0x00445518;   // size 0x20
constexpr uint32_t globalTimeString = 0x00445538;   // size 0x10
constexpr uint32_t fogColorCtx = 0x00445580;   // size 0x40
constexpr uint32_t gsPkt = 0x004455C0;   // size 0x660
constexpr uint32_t fixAxes_477 = 0x00445C80;   // size 0x40
constexpr uint32_t objRange = 0x00445CC0;   // size 0x10
constexpr uint32_t worldCtx = 0x00445CD0;   // size 0x9B0
constexpr uint32_t dmaVu1 = 0x00446680;   // size 0x18
constexpr uint32_t fakeAlphaVu1 = 0x00446698;   // size 0x18
constexpr uint32_t hierObjPkt = 0x00446700;   // size 0x88000
constexpr uint32_t hierAlphaPkt = 0x004CE700;   // size 0x3C000
constexpr uint32_t hierAlphaBins = 0x0050A700;   // size 0x810
constexpr uint32_t lastAlpEntry = 0x0050AF10;   // size 0x204
constexpr uint32_t hierAlpList = 0x0050B118;   // size 0x3E80
constexpr uint32_t FxToKill = 0x0050EF98;   // size 0x80
constexpr uint32_t g_cameraLosPoints = 0x0050F020;   // size 0xA0
constexpr uint32_t g_cameraLosPointsPerView = 0x0050F0C0;   // size 0x14
constexpr uint32_t texInfo = 0x0050F100;   // size 0x3E80
constexpr uint32_t texBinE = 0x00512F80;   // size 0x9600
constexpr uint32_t texAllBins = 0x0051C580;   // size 0x80
constexpr uint32_t psBlkLst = 0x0051C600;   // size 0x1E0
constexpr uint32_t psObjDma = 0x0051C800;   // size 0x28000
constexpr uint32_t psBlkDma = 0x00544800;   // size 0x8000
constexpr uint32_t psFilterDma = 0x0054C800;   // size 0x1E0
constexpr uint32_t psBorderDma = 0x0054CA00;   // size 0x180
constexpr uint32_t psObjIdx = 0x0054CB80;   // size 0x28
constexpr uint32_t psDrawEnv = 0x0054CBB0;   // size 0x80
constexpr uint32_t psClearDraw = 0x0054CC30;   // size 0x60
constexpr uint32_t psLitDirWs = 0x0054CC90;   // size 0x10
constexpr uint32_t psNumObjects = 0x0054CCA0;   // size 0x28
constexpr uint32_t gLightEnv = 0x0054CD00;   // size 0x240
constexpr uint32_t gRand = 0x0054CF40;   // size 0x9C4
constexpr uint32_t UNIT_MATRIX = 0x0054D910;   // size 0x40
constexpr uint32_t x_11MemoryStack_global = 0x0054D950;   // size 0x18
constexpr uint32_t voBuf = 0x0054D968;   // size 0x14
constexpr uint32_t videoDec = 0x0054D980;   // size 0xB8
constexpr uint32_t display = 0x0054DA38;   // size 0x38
constexpr uint32_t infile = 0x0054DA70;   // size 0x2C
constexpr uint32_t audioDec = 0x0054DAA0;   // size 0x58
constexpr uint32_t th_param01 = 0x0054DAF8;   // size 0x30
constexpr uint32_t th_param02 = 0x0054DB28;   // size 0x30
constexpr uint32_t originalTexZero = 0x0054DB60;   // size 0x10
constexpr uint32_t th_paramM = 0x0054DB70;   // size 0x30
constexpr uint32_t th_paramD = 0x0054DBA0;   // size 0x30
constexpr uint32_t miscSprt = 0x0054DBD0;   // size 0x20040
constexpr uint32_t miscLine = 0x0056DC10;   // size 0x20040
constexpr uint32_t misc2D = 0x0058DC50;   // size 0x20040
constexpr uint32_t VisualInfo = 0x005ADD00;   // size 0x12E0
constexpr uint32_t Gusts = 0x005AEFE0;   // size 0x10
constexpr uint32_t FieldInfo = 0x005AEFF0;   // size 0x2F30
constexpr uint32_t NoOffset = 0x005B1F20;   // size 0x10
constexpr uint32_t UpABit = 0x005B1F30;   // size 0x10
constexpr uint32_t FireOffset = 0x005B1F40;   // size 0x10
constexpr uint32_t Nozzle = 0x005B1F50;   // size 0x10
constexpr uint32_t Front = 0x005B1F60;   // size 0x40
constexpr uint32_t Up = 0x005B1FA0;   // size 0x40
constexpr uint32_t Back = 0x005B1FE0;   // size 0x40
constexpr uint32_t EmitterInfo = 0x005B2020;   // size 0x2F30
constexpr uint32_t fragColoring = 0x005B4F50;   // size 0x10
constexpr uint32_t trailColoring = 0x005B4F60;   // size 0x10
constexpr uint32_t ParticleInfo = 0x005B4F70;   // size 0x38A0
constexpr uint32_t monArcParticle = 0x005B8810;   // size 0x14
constexpr uint32_t monArcCLUT = 0x005B8828;   // size 0x40
constexpr uint32_t roboArcCLUT = 0x005B8868;   // size 0x40
constexpr uint32_t attractors = 0x005B88A8;   // size 0x10
constexpr uint32_t dustTexLitNormalMat = 0x005B88B8;   // size 0x30
constexpr uint32_t dustLighting = 0x005B88E8;   // size 0x20
constexpr uint32_t sweepTrailHandle = 0x005B8908;   // size 0x20
constexpr uint32_t displayList = 0x005B8980;   // size 0x1500
constexpr uint32_t TextureInfo = 0x005B9E80;   // size 0x400
constexpr uint32_t fx = 0x005BA280;   // size 0x124F80
constexpr uint32_t fxList = 0x006DF200;   // size 0x4B0
constexpr uint32_t TEX0 = 0x006DF6B0;   // size 0x400
constexpr uint32_t texId = 0x006DFAB0;   // size 0x100
constexpr uint32_t maxParticles = 0x006DFBB0;   // size 0x48
constexpr uint32_t paraL = 0x006DFC00;   // size 0x40
constexpr uint32_t paraLightR = 0x006DFC40;   // size 0xC
constexpr uint32_t paraLightG = 0x006DFC50;   // size 0xC
constexpr uint32_t paraLightB = 0x006DFC60;   // size 0xC
constexpr uint32_t paraLightH = 0x006DFC70;   // size 0xC
constexpr uint32_t paraLightP = 0x006DFC80;   // size 0xC
constexpr uint32_t paraTODLightR = 0x006DFC90;   // size 0xC
constexpr uint32_t paraTODLightG = 0x006DFCA0;   // size 0xC
constexpr uint32_t paraTODLightB = 0x006DFCB0;   // size 0xC
constexpr uint32_t paraTODLightH = 0x006DFCC0;   // size 0xC
constexpr uint32_t paraTODLightP = 0x006DFCD0;   // size 0xC
constexpr uint32_t pointLights = 0x006DFCE0;   // size 0x8A0
constexpr uint32_t s_sourceObjects = 0x006E0580;   // size 0x100
constexpr uint32_t ascii_special = 0x006E0680;   // size 0x84
constexpr uint32_t ascii_table = 0x006E0708;   // size 0xC
constexpr uint32_t x_11TaskManager_global = 0x006E0718;   // size 0x10
constexpr uint32_t x_11TaskManager_debris = 0x006E0728;   // size 0x10
constexpr uint32_t x_11TaskManager_global0 = 0x006E0738;   // size 0x10
constexpr uint32_t todLink = 0x006E0750;   // size 0xFC
constexpr uint32_t todInfo = 0x006E0850;   // size 0x600
constexpr uint32_t s_todPlightsEnabled = 0x006E0E50;   // size 0x10
constexpr uint32_t todSky = 0x006E0E60;   // size 0x460
constexpr uint32_t viewDef = 0x006E12C0;   // size 0x600
constexpr uint32_t s_viewAmbientVolBlend = 0x006E18C0;   // size 0x10
constexpr uint32_t s_viewAmbientVolRed = 0x006E18D0;   // size 0x10
constexpr uint32_t s_viewAmbientVolGreen = 0x006E18E0;   // size 0x10
constexpr uint32_t s_viewAmbientVolBlue = 0x006E18F0;   // size 0x10
constexpr uint32_t viewInfo = 0x006E1900;   // size 0x64
constexpr uint32_t rearViewPos = 0x006E1970;   // size 0x10
constexpr uint32_t rearViewRot = 0x006E1980;   // size 0x10
constexpr uint32_t viewScreenMats = 0x006E1990;   // size 0x140
constexpr uint32_t viewClipMats = 0x006E1AD0;   // size 0x140
constexpr uint32_t viewFovMats = 0x006E1C10;   // size 0x140
constexpr uint32_t viewClipView = 0x006E1D50;   // size 0x140
constexpr uint32_t viewFovView = 0x006E1E90;   // size 0x140
constexpr uint32_t worldToScreenMat = 0x006E1FD0;   // size 0x140
constexpr uint32_t viewShadRecMat = 0x006E2110;   // size 0x40
constexpr uint32_t viewShadWorldToScrMat = 0x006E2150;   // size 0x40
constexpr uint32_t viewDb = 0x006E21C0;   // size 0x10E0
constexpr uint32_t G_codeLen = 0x006E32C0;   // size 0x4F0
constexpr uint32_t ctxDef = 0x006E37B0;   // size 0x70
constexpr uint32_t q1_3 = 0x006E3820;   // size 0x10
constexpr uint32_t q2_4 = 0x006E3830;   // size 0x10
constexpr uint32_t x__ps2_libinfo___006E3840 = 0x006E3840;   // size 0x10
constexpr uint32_t gp_6 = 0x006E3850;   // size 0x10
constexpr uint32_t init_mp3_3 = 0x006E3860;   // size 0x10
constexpr uint32_t x__ps2_libinfo___006E3870 = 0x006E3870;   // size 0x10
constexpr uint32_t isInit = 0x006E3880;   // size 0x4
constexpr uint32_t isWarning = 0x006E3884;   // size 0x4
constexpr uint32_t PadStateStr = 0x006E3888;   // size 0x20
constexpr uint32_t ReqStateStr = 0x006E38A8;   // size 0xC
constexpr uint32_t x__ps2_libinfo___006E38B8 = 0x006E38B8;   // size 0x10
constexpr uint32_t mcRunCmdNo = 0x006E38C8;   // size 0x4
constexpr uint32_t semaidRegFunc = 0x006E38CC;   // size 0x4
constexpr uint32_t x_strmap = 0x006E38D0;   // size 0xA0
constexpr uint32_t x__ps2_libinfo___006E3970 = 0x006E3970;   // size 0x10
constexpr uint32_t x_defIQM = 0x006E3980;   // size 0x40
constexpr uint32_t x_defNIQM = 0x006E39C0;   // size 0x40
constexpr uint32_t x_rix = 0x006E3A00;   // size 0x20
constexpr uint32_t x_ri0 = 0x006E3A20;   // size 0x20
constexpr uint32_t x_isDirty = 0x006E3A40;   // size 0x28
constexpr uint32_t x_showCount = 0x006E3A68;   // size 0x40
constexpr uint32_t x__ps2_libinfo___006E3AB0 = 0x006E3AB0;   // size 0x10
constexpr uint32_t iqval = 0x006E3AC0;   // size 0x50
constexpr uint32_t vqval = 0x006E3B10;   // size 0x20
constexpr uint32_t x__ps2_libinfo___006E3B40 = 0x006E3B40;   // size 0x10
constexpr uint32_t SCE_CD_debug = 0x006E3B50;   // size 0x4
constexpr uint32_t cb_thid = 0x006E3B54;   // size 0x4
constexpr uint32_t scmd_sema_keep_cmd = 0x006E3B58;   // size 0x4
constexpr uint32_t ncmd_sema_keep_cmd = 0x006E3B5C;   // size 0x4
constexpr uint32_t cb_semid = 0x006E3B60;   // size 0x4
constexpr uint32_t Init_seq = 0x006E3B64;   // size 0x4
constexpr uint32_t x_sceCd_ncmd_semid = 0x006E3B68;   // size 0x4
constexpr uint32_t x_sceCd_scmd_semid = 0x006E3B6C;   // size 0x4
constexpr uint32_t x_sceCd_c_cb_sem = 0x006E3B70;   // size 0x4
constexpr uint32_t x_sceCd_ee_read_mode = 0x006E3B74;   // size 0x4
constexpr uint32_t x_ncmd_bind = 0x006E3B78;   // size 0x4
constexpr uint32_t x_icmd_bind = 0x006E3B7C;   // size 0x4
constexpr uint32_t x_sf_bind = 0x006E3B80;   // size 0x4
constexpr uint32_t x_scmd_dr_bind = 0x006E3B84;   // size 0x4
constexpr uint32_t x_scmd_bind = 0x006E3B88;   // size 0x4
constexpr uint32_t x_it_bind = 0x006E3B8C;   // size 0x4
constexpr uint32_t c_cnt_57 = 0x006E3B90;   // size 0x4
constexpr uint32_t sceCdCbfunc_num = 0x006E3B94;   // size 0x4
constexpr uint32_t sceCdCbfunc_number = 0x006E3B98;   // size 0x4
constexpr uint32_t x_sceCd_ncmdrdata = 0x006E3BC0;   // size 0x60
constexpr uint32_t x_sceCd_ncmdsdata = 0x006E3C40;   // size 0x1000
constexpr uint32_t x_sceCd_rd_intr_data = 0x006E4C40;   // size 0x90
constexpr uint32_t x_sceCd_Read_cur_pos = 0x006E4D00;   // size 0x10
constexpr uint32_t x_sceCd_cd_ncmd = 0x006E4D10;   // size 0x28
constexpr uint32_t x_sceCd_scmdrdata = 0x006E4D40;   // size 0x420
constexpr uint32_t x_sceCd_scmdsdata = 0x006E5180;   // size 0x408
constexpr uint32_t x_sceCd_cd_scmd = 0x006E5588;   // size 0x28
constexpr uint32_t stm_status = 0x006E55B0;   // size 0x4
constexpr uint32_t impure_data = 0x006E55B8;   // size 0x2EC
constexpr uint32_t x_impure_ptr = 0x006E58A4;   // size 0x4
constexpr uint32_t x__malloc_av_ = 0x006E58A8;   // size 0x408
constexpr uint32_t x__malloc_trim_threshold = 0x006E5CB0;   // size 0x8
constexpr uint32_t x__malloc_top_pad = 0x006E5CB8;   // size 0x8
constexpr uint32_t x__malloc_sbrk_base = 0x006E5CC0;   // size 0x4
constexpr uint32_t x__malloc_max_sbrked_mem = 0x006E5CC8;   // size 0x8
constexpr uint32_t x__malloc_max_total_mem = 0x006E5CD0;   // size 0x8
constexpr uint32_t x__malloc_current_mallinfo = 0x006E5CD8;   // size 0x28
constexpr uint32_t x__mb_cur_max = 0x006E5D00;   // size 0x4
constexpr uint32_t lc_ctype_3 = 0x006E5D08;   // size 0x8
constexpr uint32_t last_lc_ctype_4 = 0x006E5D10;   // size 0x8
constexpr uint32_t x__ps2_klibinfo__ = 0x006E5D18;
constexpr uint32_t ttyinit = 0x006E5D28;   // size 0x4
constexpr uint32_t heap_ptr_30 = 0x006E5D2C;   // size 0x4
constexpr uint32_t srcfile = 0x006E5D30;   // size 0x740
constexpr uint32_t eenull = 0x006E6470;   // size 0x28
constexpr uint32_t SysEntry_006E6498 = 0x006E6498;   // size 0x40
constexpr uint32_t topId = 0x006E64D8;   // size 0x4
constexpr uint32_t count_006E64DC = 0x006E64DC;   // size 0x4
constexpr uint32_t x_putchar = 0x006E64E0;   // size 0x4
constexpr uint32_t x_cmd_init_check = 0x006E64E4;   // size 0x4
constexpr uint32_t x_sceSifInitCheck = 0x006E64E8;   // size 0x4
constexpr uint32_t x_sceFs_q = 0x006E64F0;   // size 0x80
constexpr uint32_t x_fs_init = 0x006E6570;   // size 0x4
constexpr uint32_t x_fs_semid = 0x006E6574;   // size 0x4
constexpr uint32_t x_fs_iob_semid = 0x006E6578;   // size 0x4
constexpr uint32_t x_fs_fsq_semid = 0x006E657C;   // size 0x4
constexpr uint32_t x_fswildcard = 0x006E6580;   // size 0x4
constexpr uint32_t x_bind = 0x006E6584;   // size 0x4
constexpr uint32_t x_bind_check = 0x006E6588;   // size 0x4
constexpr uint32_t x_lfwildcard = 0x006E658C;   // size 0x4
constexpr uint32_t SysEntry_006E6590 = 0x006E6590;   // size 0xC
constexpr uint32_t x__sce_sema_id = 0x006E659C;   // size 0x4
constexpr uint32_t x__sce_eh_sema_id = 0x006E65A0;   // size 0x4
constexpr uint32_t x_SyscallEntry = 0x006E65A4;   // size 0x4
constexpr uint32_t osdsrc = 0x006E65A8;   // size 0x7A8
constexpr uint32_t SysExecPS2Entry = 0x006E6D50;   // size 0x18
constexpr uint32_t tlbsrc = 0x006E6D68;   // size 0x328
constexpr uint32_t x_kTLBRefillHandler = 0x006E7090;   // size 0x4
constexpr uint32_t x_kDebugHandler = 0x006E7098;   // size 0x40
constexpr uint32_t SysEntry_006E70D8 = 0x006E70D8;   // size 0x40
constexpr uint32_t kernelTLB = 0x006E7118;   // size 0xD0
constexpr uint32_t defaultTLB = 0x006E71E8;   // size 0x120
constexpr uint32_t extendTLB = 0x006E7308;   // size 0x80
constexpr uint32_t TLBInfo = 0x006E7388;   // size 0x1C
constexpr uint32_t x__CTOR_LIST__ = 0x006E73A4;   // size 0xC0
constexpr uint32_t x__DTOR_LIST__ = 0x006E7464;   // size 0x1C
constexpr uint32_t x_vt_18AiThrashActionList = 0x006E7580;   // size 0x18
constexpr uint32_t x_vt_12AiActionList = 0x006E7598;   // size 0x18
constexpr uint32_t x_vt_13AiActionTuple = 0x006E75B0;   // size 0x38
constexpr uint32_t x_vt_15AiGrappleAttack = 0x006E79A0;   // size 0x38
constexpr uint32_t x_vt_14AiGrappleThrow = 0x006E79D8;   // size 0x38
constexpr uint32_t x_vt_21AiGrapplingActionList = 0x006E7A10;   // size 0x18
constexpr uint32_t x_vt_20AiGrappledActionList = 0x006E7A28;   // size 0x18
constexpr uint32_t x_vt_10AiPathTool = 0x006E7AF0;   // size 0x28
constexpr uint32_t x_vt_12AiPutOutFire = 0x006E7C78;   // size 0x38
constexpr uint32_t x_vt_7AiTaunt = 0x006E7CB0;   // size 0x38
constexpr uint32_t x_vt_14AiDetonateHead = 0x006E7CE8;   // size 0x38
constexpr uint32_t x_vt_15AiSpecialAttack = 0x006E7D20;   // size 0x38
constexpr uint32_t x_vt_14AiCrowdControl = 0x006E7D58;   // size 0x38
constexpr uint32_t x_vt_11AiRamAttack = 0x006E7D90;   // size 0x38
constexpr uint32_t x_vt_13AiStompReflex = 0x006E7DC8;   // size 0x38
constexpr uint32_t x_vt_13AiThrowPickup = 0x006E7E00;   // size 0x38
constexpr uint32_t x_vt_14AiThrowRecover = 0x006E7E38;   // size 0x38
constexpr uint32_t x_vt_13AiSmashReflex = 0x006E7E70;   // size 0x38
constexpr uint32_t x_vt_15AiDisposeReflex = 0x006E7EA8;   // size 0x38
constexpr uint32_t x_vt_14AiThrashReflex = 0x006E7EE0;   // size 0x38
constexpr uint32_t x_vt_15AiCounterReflex = 0x006E7F18;   // size 0x38
constexpr uint32_t x_vt_13AiBlockReflex = 0x006E7F50;   // size 0x38
constexpr uint32_t x_vt_15AiGrappleReflex = 0x006E7F88;   // size 0x38
constexpr uint32_t x_vt_16AiFireProjectile = 0x006E7FC0;   // size 0x38
constexpr uint32_t x_vt_11AiBatReflex = 0x006E7FF8;   // size 0x38
constexpr uint32_t x_vt_13AiPunchReflex = 0x006E8030;   // size 0x38
constexpr uint32_t x_vt_7AiReset = 0x006E8270;   // size 0x28
constexpr uint32_t GameLevelNames_006E82A8 = 0x006E82A8;   // size 0x118
constexpr uint32_t MonsterLongNames_006E83C0 = 0x006E83C0;   // size 0x88
constexpr uint32_t x_vt_14AiDodgeSpecial = 0x006E8450;   // size 0x38
constexpr uint32_t x_vt_12AiDodgeStomp = 0x006E8488;   // size 0x38
constexpr uint32_t x_vt_10AiDodgeRam = 0x006E84C0;   // size 0x38
constexpr uint32_t x_vt_10AiBatThrow = 0x006E84F8;   // size 0x38
constexpr uint32_t x_vt_12AiCatchThrow = 0x006E8530;   // size 0x38
constexpr uint32_t x_vt_12AiDodgeThrow = 0x006E8568;   // size 0x38
constexpr uint32_t x_vt_7AiSwarm = 0x006E85A0;   // size 0x38
constexpr uint32_t x_vt_13AiSeekMonster = 0x006E85D8;   // size 0x38
constexpr uint32_t x_vt_12AiSeekPickup = 0x006E8610;   // size 0x38
constexpr uint32_t x_vt_11AiSeekCloak = 0x006E8648;   // size 0x38
constexpr uint32_t x_vt_13AiSeekSpecial = 0x006E8680;   // size 0x38
constexpr uint32_t x_vt_13AiSeekStamina = 0x006E86B8;   // size 0x38
constexpr uint32_t x_vt_12AiSeekHealth = 0x006E86F0;   // size 0x38
constexpr uint32_t x_vt_8Airplane = 0x006E8870;   // size 0x30
constexpr uint32_t x_12BigShotLevel_UPDATE_FUNK = 0x006E8920;   // size 0x10
constexpr uint32_t MonsterNewNames_006E8958 = 0x006E8958;   // size 0xCC
constexpr uint32_t x_6Camera_UPDATE_FUNK = 0x006E8A28;   // size 0x50
constexpr uint32_t x_vt_9CarPickup = 0x006E8B10;   // size 0x70
constexpr uint32_t x_vt_9charreset = 0x006E8B90;   // size 0x28
constexpr uint32_t MonsterLongNames_006E8BC8 = 0x006E8BC8;   // size 0x88
constexpr uint32_t x_10CrushLevel_UPDATE_FUNK = 0x006E8D60;   // size 0x10
constexpr uint32_t MonsterNewNames_006E8D90 = 0x006E8D90;   // size 0xCC
constexpr uint32_t x_vt_12Destructible = 0x006E8F88;   // size 0x30
constexpr uint32_t x_vt_9FinalBoss = 0x006E9060;   // size 0x30
constexpr uint32_t x_vt_11GlobalReset = 0x006E9550;   // size 0x28
constexpr uint32_t x_vt_13StateGrappled = 0x006E9590;   // size 0x50
constexpr uint32_t x_vt_12StateGrapple = 0x006E95E0;   // size 0x50
constexpr uint32_t x_vt_19StateMonkeyOHAttack = 0x006E9650;   // size 0x60
constexpr uint32_t x_vt_20StateGrappleOHAttack = 0x006E96B0;   // size 0x60
constexpr uint32_t x_vt_18StateGrappleThrow1 = 0x006E9750;   // size 0x60
constexpr uint32_t x_vt_17StateGrappleThrow = 0x006E97B0;   // size 0x60
constexpr uint32_t x_vt_16StateGrappleLift = 0x006E9810;   // size 0x50
constexpr uint32_t x_vt_10HeliPickup = 0x006E98F8;   // size 0x70
constexpr uint32_t x_vt_9HomingBug = 0x006E99B8;   // size 0x30
constexpr uint32_t MonsterNewNames_006EAA78 = 0x006EAA78;   // size 0xCC
constexpr uint32_t x_vt_14McSetupGameDir = 0x006EAC88;   // size 0x30
constexpr uint32_t x_vt_14McScrambleIcon = 0x006EACB8;   // size 0x30
constexpr uint32_t x_vt_8McFormat = 0x006EACE8;   // size 0x30
constexpr uint32_t x_vt_13McUpdateState = 0x006EAD18;   // size 0x30
constexpr uint32_t x_vt_15McDoesFileExist = 0x006EAD48;   // size 0x30
constexpr uint32_t x_vt_19McListRootDirectory = 0x006EAD78;   // size 0x30
constexpr uint32_t x_vt_10McSaveFile = 0x006EADA8;   // size 0x30
constexpr uint32_t x_vt_10McLoadFile = 0x006EADD8;   // size 0x30
constexpr uint32_t x_vt_9McMessage = 0x006EAE08;   // size 0x30
constexpr uint32_t x_vt_6McPage = 0x006EAF10;   // size 0x70
constexpr uint32_t x_vt_12BoxFormation = 0x006EAF80;   // size 0x40
constexpr uint32_t x_vt_13LineFormation = 0x006EAFC0;   // size 0x40
constexpr uint32_t x_vt_17MilitaryFormation = 0x006EB000;   // size 0x40
constexpr uint32_t x_vt_14MilitaryPickup = 0x006EB078;   // size 0x90
constexpr uint32_t x_vt_18MissileTruckPickup = 0x006EB160;   // size 0x90
constexpr uint32_t x_vt_12StateVictory = 0x006EB268;   // size 0x50
constexpr uint32_t x_vt_15StateBigTakeHit = 0x006EB2B8;   // size 0x50
constexpr uint32_t x_vt_17StateShieldAttack = 0x006EB308;   // size 0x60
constexpr uint32_t x_vt_16StateTazerAttack = 0x006EB368;   // size 0x60
constexpr uint32_t x_vt_16StateStompAttack = 0x006EB3C8;   // size 0x60
constexpr uint32_t x_vt_12StateShocked = 0x006EB428;   // size 0x50
constexpr uint32_t x_vt_19StateTwoHandedThrow = 0x006EB478;   // size 0x60
constexpr uint32_t x_vt_10StateThrow = 0x006EB4D8;   // size 0x60
constexpr uint32_t x_vt_10StateTaunt = 0x006EB538;   // size 0x50
constexpr uint32_t x_vt_12StateStunned = 0x006EB588;   // size 0x50
constexpr uint32_t x_vt_11StateRecoil = 0x006EB5D8;   // size 0x50
constexpr uint32_t x_vt_10StatePunch = 0x006EB628;   // size 0x60
constexpr uint32_t x_vt_Q210StatePunch11PunchConfig = 0x006EB688;   // size 0x20
constexpr uint32_t x_vt_11StatePickUp = 0x006EB6A8;   // size 0x50
constexpr uint32_t x_vt_12StateJavelin = 0x006EB6F8;   // size 0x60
constexpr uint32_t x_vt_12StateImpaled = 0x006EB758;   // size 0x50
constexpr uint32_t x_vt_14StateGunAttack = 0x006EB7A8;   // size 0x60
constexpr uint32_t x_vt_16StateGetupAttack = 0x006EB808;   // size 0x50
constexpr uint32_t x_vt_10StateDeath = 0x006EB858;   // size 0x50
constexpr uint32_t x_vt_14StateCountered = 0x006EB8A8;   // size 0x50
constexpr uint32_t x_vt_12StateCounter = 0x006EB8F8;   // size 0x50
constexpr uint32_t x_vt_10StateCatch = 0x006EB948;   // size 0x50
constexpr uint32_t x_vt_10StateBlock = 0x006EB998;   // size 0x50
constexpr uint32_t x_vt_13StateBatSwipe = 0x006EB9E8;   // size 0x60
constexpr uint32_t x_vt_Q213StateBatSwipe11SwipeConfig = 0x006EBA48;   // size 0x20
constexpr uint32_t x_vt_9StateIdle = 0x006EBA68;   // size 0x50
constexpr uint32_t x_vt_11AttackState = 0x006EBAB8;   // size 0x60
constexpr uint32_t x_vt_12MonsterState = 0x006EBB18;   // size 0x50
constexpr uint32_t x_vt_7Monster = 0x006EC018;   // size 0x30
constexpr uint32_t x_vt_13McCardRemoved = 0x006EC750;   // size 0x70
constexpr uint32_t x_vt_11McErrorPage = 0x006EC7C0;   // size 0x70
constexpr uint32_t x_vt_11McAskIfSure = 0x006EC830;   // size 0x70
constexpr uint32_t x_vt_11McAskToSave = 0x006EC8A0;   // size 0x70
constexpr uint32_t x_vt_16McSelectSaveSlot = 0x006EC910;   // size 0x70
constexpr uint32_t x_vt_20McSaveAdventureBlank = 0x006EC980;   // size 0x70
constexpr uint32_t x_vt_15McSaveAdventure = 0x006EC9F0;   // size 0x70
constexpr uint32_t x_vt_12McSelectReel = 0x006ECA60;   // size 0x70
constexpr uint32_t x_vt_15McLoadGameBoxes = 0x006ECAD0;   // size 0x70
constexpr uint32_t x_vt_15McPrepBoxScreen = 0x006ECB40;   // size 0x70
constexpr uint32_t x_vt_16McCheckOverwrite = 0x006ECBB0;   // size 0x70
constexpr uint32_t x_vt_13McCheckFormat = 0x006ECC20;   // size 0x70
constexpr uint32_t x_vt_12McCheckSpace = 0x006ECC90;   // size 0x70
constexpr uint32_t x_vt_12McChooseSlot = 0x006ECD00;   // size 0x70
constexpr uint32_t x_vt_14McSaveSettings = 0x006ECD70;   // size 0x70
constexpr uint32_t x_vt_14McLoadSettings = 0x006ECDE0;   // size 0x70
constexpr uint32_t x_vt_18McCheckForSettings = 0x006ECE50;   // size 0x70
constexpr uint32_t x_vt_20McCheckForSweetTooth = 0x006ECEC0;   // size 0x70
constexpr uint32_t x_vt_10McBootPage = 0x006ECF30;   // size 0x70
constexpr uint32_t GameLevelLongNames_006ED120 = 0x006ED120;   // size 0x1F8
constexpr uint32_t MonsterNewNames_006ED318 = 0x006ED318;   // size 0xCC
constexpr uint32_t x_vt_8StateRun = 0x006ED530;   // size 0x50
constexpr uint32_t x_vt_14StateRamAttack = 0x006ED580;   // size 0x60
constexpr uint32_t x_vt_9StateJump = 0x006ED5E0;   // size 0x50
constexpr uint32_t x_vt_8StateFly = 0x006ED630;   // size 0x50
constexpr uint32_t x_vt_11JumpFlyBase = 0x006ED680;   // size 0x50
constexpr uint32_t x_vt_9StateDash = 0x006ED6D0;   // size 0x50
constexpr uint32_t x_vt_10StateClimb = 0x006ED720;   // size 0x50
constexpr uint32_t x_vt_13StateButtSlam = 0x006ED770;   // size 0x60
constexpr uint32_t x_vt_8PathTool = 0x006ED870;   // size 0x28
constexpr uint32_t x_vt_6Pickup = 0x006EDB90;   // size 0x70
constexpr uint32_t x_vt_13PlantBossBase = 0x006EDDE8;   // size 0x30
constexpr uint32_t x_vt_13PlantBossRoot = 0x006EDE18;   // size 0x30
constexpr uint32_t x_vt_13PlantBossHead = 0x006EDE48;   // size 0x30
constexpr uint32_t x_vt_11PowerUpTool = 0x006EDEC0;   // size 0x28
constexpr uint32_t x_vt_5reset = 0x006EDF28;   // size 0x28
constexpr uint32_t x_11RigidDebris_COLLIS_FUNK = 0x006EDF50;   // size 0x30
constexpr uint32_t x_vt_11RigidPickup = 0x006EDFD8;   // size 0x70
constexpr uint32_t on_bit_006EEE78 = 0x006EEE78;   // size 0x80
constexpr uint32_t GameLevelLongNames_006EEEF8 = 0x006EEEF8;   // size 0x1F8
constexpr uint32_t GameLevelNames_006EF748 = 0x006EF748;   // size 0x118
constexpr uint32_t MonsterLongNames_006EF860 = 0x006EF860;   // size 0x88
constexpr uint32_t on_bit_006EF8E8 = 0x006EF8E8;   // size 0x80
constexpr uint32_t off_bit = 0x006EF968;   // size 0x80
constexpr uint32_t MonsterNewNames_006EFEC8 = 0x006EFEC8;   // size 0xCC
constexpr uint32_t x_vt_15StateUltraTazer = 0x006F0E30;   // size 0x60
constexpr uint32_t x_vt_16StateCannonHands = 0x006F0E90;   // size 0x60
constexpr uint32_t x_vt_12StateTopSpin = 0x006F0EF0;   // size 0x60
constexpr uint32_t x_vt_14StateZapAttack = 0x006F0F50;   // size 0x60
constexpr uint32_t x_vt_14StateSonicRoar = 0x006F0FB0;   // size 0x60
constexpr uint32_t x_vt_16StateRockSpecial = 0x006F1010;   // size 0x60
constexpr uint32_t x_vt_17StateRobotSpecial = 0x006F1070;   // size 0x60
constexpr uint32_t x_vt_14StateLavaBlast = 0x006F10D0;   // size 0x60
constexpr uint32_t x_vt_16StateGrappleHook = 0x006F1130;   // size 0x60
constexpr uint32_t x_vt_15StateFireBreath = 0x006F1190;   // size 0x60
constexpr uint32_t x_vt_17StateCrowdControl = 0x006F11F0;   // size 0x60
constexpr uint32_t x_vt_14StateBugAttack = 0x006F1250;   // size 0x60
constexpr uint32_t x_vt_14StateAirStrike = 0x006F12B0;   // size 0x60
constexpr uint32_t x_vt_14StartPointTool = 0x006F1498;   // size 0x28
constexpr uint32_t x_vt_12SubwayPickup = 0x006F14D8;   // size 0x70
constexpr uint32_t x_vt_10TankPickup = 0x006F15A0;   // size 0x90
constexpr uint32_t x_vt_12TurretPickup = 0x006F1678;   // size 0x70
constexpr uint32_t x_vt_13DbInteractive = 0x006F1768;   // size 0x30
constexpr uint32_t x_vt_14StateKnockBack = 0x006F1878;   // size 0x50
constexpr uint32_t x_vt_17StateGrappleBreak = 0x006F18C8;   // size 0x50
constexpr uint32_t x_vt_16StateBeingThrown = 0x006F1918;   // size 0x50
constexpr uint32_t x_vt_17StateBeingSlammed = 0x006F1968;   // size 0x50
constexpr uint32_t x_vt_14StateThrowBack = 0x006F19B8;   // size 0x50
constexpr uint32_t x_vt_8UfoTokyo = 0x006F1B08;   // size 0x30
constexpr uint32_t x_vt_9UfoPickup = 0x006F1BE0;   // size 0x70
constexpr uint32_t GameLevelNames_006F2208 = 0x006F2208;   // size 0x118
constexpr uint32_t x_vt_14CNetworkScreen = 0x006F2470;   // size 0x38
constexpr uint32_t x_vt_10CDevScreen = 0x006F24A8;   // size 0x38
constexpr uint32_t x_vt_7CScreen = 0x006F24E0;   // size 0x38
constexpr uint32_t x_vt_16CVerticalSection = 0x006F2518;   // size 0x60
constexpr uint32_t x_vt_18CHorizontalSection = 0x006F2578;   // size 0x58
constexpr uint32_t x_vt_18CFunctionalSection = 0x006F25D0;   // size 0x58
constexpr uint32_t x_vt_14CScreenSection = 0x006F2628;   // size 0x28
constexpr uint32_t UIDEV_P = 0x006F26E8;   // size 0x20
constexpr uint32_t GameLevelNames_006F2708 = 0x006F2708;   // size 0x118
constexpr uint32_t nothrow = 0x006F2B90;   // size 0x1
constexpr uint32_t x_vt_9bad_alloc = 0x006F2B98;   // size 0x18
constexpr uint32_t x_vt_17__class_type_info = 0x006F2BE0;   // size 0x18
constexpr uint32_t x_vt_14__si_type_info = 0x006F2BF8;   // size 0x18
constexpr uint32_t x_vt_16__user_type_info = 0x006F2C10;   // size 0x18
constexpr uint32_t x_vt_10bad_typeid = 0x006F2C28;   // size 0x18
constexpr uint32_t x_vt_8bad_cast = 0x006F2C40;   // size 0x18
constexpr uint32_t x_vt_9type_info = 0x006F2C58;   // size 0x10
constexpr uint32_t x_vt_13bad_exception = 0x006F2CE0;   // size 0x18
constexpr uint32_t x_vt_9exception = 0x006F2CF8;   // size 0x18
constexpr uint32_t x_vt_17__array_type_info = 0x006F2E98;   // size 0x10
constexpr uint32_t x_vt_16__ptmd_type_info = 0x006F2EA8;   // size 0x10
constexpr uint32_t x_vt_16__ptmf_type_info = 0x006F2EB8;   // size 0x10
constexpr uint32_t x_vt_16__func_type_info = 0x006F2EC8;   // size 0x10
constexpr uint32_t x_vt_19__builtin_type_info = 0x006F2ED8;   // size 0x10
constexpr uint32_t x_vt_16__attr_type_info = 0x006F2EE8;   // size 0x10
constexpr uint32_t x_vt_19__pointer_type_info = 0x006F2EF8;   // size 0x10
constexpr uint32_t x_vt_12PointToolKit = 0x006F47D8;   // size 0x28
constexpr uint32_t x_vt_8resetcom = 0x006F4A30;   // size 0x28
constexpr uint32_t mask_bits = 0x006F4E28;   // size 0x22
constexpr uint32_t c_border = 0x006F4E50;   // size 0x4C
constexpr uint32_t c_plens = 0x006F4EA0;   // size 0x3E
constexpr uint32_t c_plext = 0x006F4EE0;   // size 0x3E
constexpr uint32_t c_pdist = 0x006F4F20;   // size 0x3C
constexpr uint32_t c_pdext = 0x006F4F60;   // size 0x3C
constexpr uint32_t crc_32_tab = 0x006F4FA0;   // size 0x800
constexpr uint32_t atanhi = 0x006F57E8;   // size 0x10
constexpr uint32_t atanlo = 0x006F57F8;   // size 0x10
constexpr uint32_t aT = 0x006F5808;   // size 0x2C
constexpr uint32_t one_006F5834 = 0x006F5834;   // size 0x4
constexpr uint32_t huge_006F5838 = 0x006F5838;   // size 0x4
constexpr uint32_t huge_006F583C = 0x006F583C;   // size 0x4
constexpr uint32_t one_006F5840 = 0x006F5840;   // size 0x4
constexpr uint32_t one_006F5890 = 0x006F5890;   // size 0x4
constexpr uint32_t pi_006F5894 = 0x006F5894;   // size 0x4
constexpr uint32_t pio2_hi_006F5898 = 0x006F5898;   // size 0x4
constexpr uint32_t pio2_lo_006F589C = 0x006F589C;   // size 0x4
constexpr uint32_t pS0_006F58A0 = 0x006F58A0;   // size 0x4
constexpr uint32_t pS1_006F58A4 = 0x006F58A4;   // size 0x4
constexpr uint32_t pS2_006F58A8 = 0x006F58A8;   // size 0x4
constexpr uint32_t pS3_006F58AC = 0x006F58AC;   // size 0x4
constexpr uint32_t pS4_006F58B0 = 0x006F58B0;   // size 0x4
constexpr uint32_t pS5_006F58B4 = 0x006F58B4;   // size 0x4
constexpr uint32_t qS1_006F58B8 = 0x006F58B8;   // size 0x4
constexpr uint32_t qS2_006F58BC = 0x006F58BC;   // size 0x4
constexpr uint32_t qS3_006F58C0 = 0x006F58C0;   // size 0x4
constexpr uint32_t qS4_006F58C4 = 0x006F58C4;   // size 0x4
constexpr uint32_t one_006F58C8 = 0x006F58C8;   // size 0x4
constexpr uint32_t huge_006F58CC = 0x006F58CC;   // size 0x4
constexpr uint32_t pio2_hi_006F58D0 = 0x006F58D0;   // size 0x4
constexpr uint32_t pio2_lo_006F58D4 = 0x006F58D4;   // size 0x4
constexpr uint32_t pio4_hi = 0x006F58D8;   // size 0x4
constexpr uint32_t pS0_006F58DC = 0x006F58DC;   // size 0x4
constexpr uint32_t pS1_006F58E0 = 0x006F58E0;   // size 0x4
constexpr uint32_t pS2_006F58E4 = 0x006F58E4;   // size 0x4
constexpr uint32_t pS3_006F58E8 = 0x006F58E8;   // size 0x4
constexpr uint32_t pS4_006F58EC = 0x006F58EC;   // size 0x4
constexpr uint32_t pS5_006F58F0 = 0x006F58F0;   // size 0x4
constexpr uint32_t qS1_006F58F4 = 0x006F58F4;   // size 0x4
constexpr uint32_t qS2_006F58F8 = 0x006F58F8;   // size 0x4
constexpr uint32_t qS3_006F58FC = 0x006F58FC;   // size 0x4
constexpr uint32_t qS4_006F5900 = 0x006F5900;   // size 0x4
constexpr uint32_t tiny_006F5904 = 0x006F5904;   // size 0x4
constexpr uint32_t zero_006F5908 = 0x006F5908;   // size 0x4
constexpr uint32_t pi_o_4 = 0x006F590C;   // size 0x4
constexpr uint32_t pi_o_2 = 0x006F5910;   // size 0x4
constexpr uint32_t pi_006F5914 = 0x006F5914;   // size 0x4
constexpr uint32_t pi_lo = 0x006F5918;   // size 0x4
constexpr uint32_t bp = 0x006F5920;   // size 0x8
constexpr uint32_t dp_h = 0x006F5928;   // size 0x8
constexpr uint32_t dp_l = 0x006F5930;   // size 0x8
constexpr uint32_t zero_006F5938 = 0x006F5938;   // size 0x4
constexpr uint32_t one_006F593C = 0x006F593C;   // size 0x4
constexpr uint32_t two = 0x006F5940;   // size 0x4
constexpr uint32_t two24 = 0x006F5944;   // size 0x4
constexpr uint32_t huge_006F5948 = 0x006F5948;   // size 0x4
constexpr uint32_t tiny_006F594C = 0x006F594C;   // size 0x4
constexpr uint32_t L1 = 0x006F5950;   // size 0x4
constexpr uint32_t L2 = 0x006F5954;   // size 0x4
constexpr uint32_t L3 = 0x006F5958;   // size 0x4
constexpr uint32_t L4 = 0x006F595C;   // size 0x4
constexpr uint32_t L5 = 0x006F5960;   // size 0x4
constexpr uint32_t L6 = 0x006F5964;   // size 0x4
constexpr uint32_t P1 = 0x006F5968;   // size 0x4
constexpr uint32_t P2 = 0x006F596C;   // size 0x4
constexpr uint32_t P3 = 0x006F5970;   // size 0x4
constexpr uint32_t P4 = 0x006F5974;   // size 0x4
constexpr uint32_t P5 = 0x006F5978;   // size 0x4
constexpr uint32_t lg2 = 0x006F597C;   // size 0x4
constexpr uint32_t lg2_h = 0x006F5980;   // size 0x4
constexpr uint32_t lg2_l = 0x006F5984;   // size 0x4
constexpr uint32_t ovt = 0x006F5988;   // size 0x4
constexpr uint32_t cp = 0x006F598C;   // size 0x4
constexpr uint32_t cp_h = 0x006F5990;   // size 0x4
constexpr uint32_t cp_l = 0x006F5994;   // size 0x4
constexpr uint32_t ivln2 = 0x006F5998;   // size 0x4
constexpr uint32_t ivln2_h = 0x006F599C;   // size 0x4
constexpr uint32_t ivln2_l = 0x006F59A0;   // size 0x4
constexpr uint32_t two_over_pi = 0x006F59A8;   // size 0x318
constexpr uint32_t npio2_hw = 0x006F5CC0;   // size 0x80
constexpr uint32_t zero_006F5D40 = 0x006F5D40;   // size 0x4
constexpr uint32_t half_006F5D44 = 0x006F5D44;   // size 0x4
constexpr uint32_t two8_006F5D48 = 0x006F5D48;   // size 0x4
constexpr uint32_t invpio2 = 0x006F5D4C;   // size 0x4
constexpr uint32_t pio2_1 = 0x006F5D50;   // size 0x4
constexpr uint32_t pio2_1t = 0x006F5D54;   // size 0x4
constexpr uint32_t pio2_2 = 0x006F5D58;   // size 0x4
constexpr uint32_t pio2_2t = 0x006F5D5C;   // size 0x4
constexpr uint32_t pio2_3 = 0x006F5D60;   // size 0x4
constexpr uint32_t pio2_3t = 0x006F5D64;   // size 0x4
constexpr uint32_t one_006F5D68 = 0x006F5D68;   // size 0x4
constexpr uint32_t tiny_006F5D6C = 0x006F5D6C;   // size 0x4
constexpr uint32_t one_006F5D70 = 0x006F5D70;   // size 0x4
constexpr uint32_t C1 = 0x006F5D74;   // size 0x4
constexpr uint32_t C2 = 0x006F5D78;   // size 0x4
constexpr uint32_t C3 = 0x006F5D7C;   // size 0x4
constexpr uint32_t C4 = 0x006F5D80;   // size 0x4
constexpr uint32_t C5 = 0x006F5D84;   // size 0x4
constexpr uint32_t C6 = 0x006F5D88;   // size 0x4
constexpr uint32_t init_jk = 0x006F5D90;   // size 0xC
constexpr uint32_t PIo2 = 0x006F5DA0;   // size 0x2C
constexpr uint32_t zero_006F5DCC = 0x006F5DCC;   // size 0x4
constexpr uint32_t one_006F5DD0 = 0x006F5DD0;   // size 0x4
constexpr uint32_t two8_006F5DD4 = 0x006F5DD4;   // size 0x4
constexpr uint32_t twon8 = 0x006F5DD8;   // size 0x4
constexpr uint32_t half_006F5DDC = 0x006F5DDC;   // size 0x4
constexpr uint32_t S1 = 0x006F5DE0;   // size 0x4
constexpr uint32_t S2 = 0x006F5DE4;   // size 0x4
constexpr uint32_t S3 = 0x006F5DE8;   // size 0x4
constexpr uint32_t S4 = 0x006F5DEC;   // size 0x4
constexpr uint32_t S5 = 0x006F5DF0;   // size 0x4
constexpr uint32_t S6 = 0x006F5DF4;   // size 0x4
constexpr uint32_t one_006F5DF8 = 0x006F5DF8;   // size 0x4
constexpr uint32_t pio4 = 0x006F5DFC;   // size 0x4
constexpr uint32_t pio4lo = 0x006F5E00;   // size 0x4
constexpr uint32_t T = 0x006F5E08;   // size 0x34
constexpr uint32_t x__infinity = 0x006F5E40;   // size 0x8
constexpr uint32_t x__fdlib_version = 0x006F5E48;   // size 0x4
constexpr uint32_t TWO52 = 0x006F5E50;   // size 0x10
constexpr uint32_t huge_006F5E60 = 0x006F5E60;   // size 0x4
constexpr uint32_t two25 = 0x006F5E64;   // size 0x4
constexpr uint32_t twom25 = 0x006F5E68;   // size 0x4
constexpr uint32_t huge_006F5E6C = 0x006F5E6C;   // size 0x4
constexpr uint32_t tiny_006F5E70 = 0x006F5E70;   // size 0x4
constexpr uint32_t x_ctype_ = 0x006F6BE8;   // size 0x101
constexpr uint32_t blanks_12 = 0x006F6D00;   // size 0x10
constexpr uint32_t zeroes_13 = 0x006F6D10;   // size 0x10
constexpr uint32_t lconv = 0x006F6F68;   // size 0x30
constexpr uint32_t p05_27 = 0x006F6FB0;   // size 0xC
constexpr uint32_t x__mprec_tens = 0x006F6FC0;   // size 0xC8
constexpr uint32_t x__mprec_bigtens = 0x006F7088;   // size 0x28
constexpr uint32_t x__mprec_tinytens = 0x006F70B0;   // size 0x28
constexpr uint32_t x__clz_tab_006F7400 = 0x006F7400;   // size 0x100
constexpr uint32_t x__clz_tab_006F7500 = 0x006F7500;   // size 0x100
constexpr uint32_t x__clz_tab_006F7600 = 0x006F7600;   // size 0x100
constexpr uint32_t x__clz_tab_006F7700 = 0x006F7700;   // size 0x100
constexpr uint32_t moviePlaying = 0x006F7900;   // size 0x4
constexpr uint32_t movieAborted = 0x006F7904;   // size 0x4
constexpr uint32_t s_genericEventCB = 0x006F7908;   // size 0x4
constexpr uint32_t s_numUserDefinedScripts = 0x006F790C;   // size 0x4
constexpr uint32_t s_fadeOut = 0x006F7910;   // size 0x4
constexpr uint32_t s_endingActions = 0x006F7914;   // size 0x4
constexpr uint32_t s_pathNet = 0x006F7918;   // size 0x4
constexpr uint32_t s_mode = 0x006F791C;   // size 0x4
constexpr uint32_t s_startupDelayMin = 0x006F7920;   // size 0x4
constexpr uint32_t s_startupDelayMax = 0x006F7924;   // size 0x4
constexpr uint32_t x_2Ai_s_attackOtherAi = 0x006F7928;   // size 0x4
constexpr uint32_t x_2Ai_s_ccAi = 0x006F792C;   // size 0x4
constexpr uint32_t x_2Ai_s_ignoreZ = 0x006F7930;   // size 0x4
constexpr uint32_t TYPICAL_SLOPE_TAN_2481 = 0x006F7934;   // size 0x4
constexpr uint32_t CERTAINTY_FACTOR_2482 = 0x006F7938;   // size 0x4
constexpr uint32_t nodeToUpdate_2504 = 0x006F793C;   // size 0x2
constexpr uint32_t assBossTier1Health = 0x006F7960;   // size 0x4
constexpr uint32_t assBossTier2Health = 0x006F7964;   // size 0x4
constexpr uint32_t cherryBombThrown_2466 = 0x006F7968;   // size 0x4
constexpr uint32_t cherryBombAssigned_2467 = 0x006F796C;   // size 0x4
constexpr uint32_t assBoss = 0x006F7970;   // size 0x4
constexpr uint32_t attackState = 0x006F7974;   // size 0x4
constexpr uint32_t attackSuccessful = 0x006F7978;   // size 0x4
constexpr uint32_t g_mem = 0x006F7988;   // size 0x4
constexpr uint32_t s_impossibleCost = 0x006F798C;   // size 0x4
constexpr uint32_t s_zWeight = 0x006F7990;   // size 0x4
constexpr uint32_t LOOK_AHEAD_T = 0x006F79A8;   // size 0x4
constexpr uint32_t GET_UP_ATTACK_RANGE = 0x006F79AC;   // size 0x4
constexpr uint32_t minDepth_2436 = 0x006F79B0;   // size 0x4
constexpr uint32_t maxDepth_2437 = 0x006F79B4;   // size 0x4
constexpr uint32_t LOW_GRAPPLE_STAMINA = 0x006F79B8;   // size 0x4
constexpr uint32_t s_loadFromRef = 0x006F79C0;   // size 0x4
constexpr uint32_t POWERUP_TIME = 0x006F79D8;   // size 0x4
constexpr uint32_t FIRE_TIME = 0x006F79DC;   // size 0x4
constexpr uint32_t SWARM_TIME = 0x006F79E0;   // size 0x4
constexpr uint32_t PICK_UP_GRAB_RANGE = 0x006F79E4;   // size 0x4
constexpr uint32_t high_2424 = 0x006F79E8;   // size 0x4
constexpr uint32_t med_2425 = 0x006F79EC;   // size 0x4
constexpr uint32_t med_2444 = 0x006F79F0;   // size 0x4
constexpr uint32_t s_rangeCorrectFactor_2565 = 0x006F79FC;   // size 0x4
constexpr uint32_t s_catchLookAhead_2605 = 0x006F7A00;   // size 0x4
constexpr uint32_t s_catchRange_2606 = 0x006F7A04;   // size 0x4
constexpr uint32_t s_lookAhead_2625 = 0x006F7A08;   // size 0x4
constexpr uint32_t s_hitRange_2626 = 0x006F7A0C;   // size 0x4
constexpr uint32_t s_ai = 0x006F7A10;   // size 0x4
constexpr uint32_t s_net = 0x006F7A14;   // size 0x4
constexpr uint32_t ELEVATOR_Z_2513 = 0x006F7A18;   // size 0x4
constexpr uint32_t ELEVATOR_RADIUS_2514 = 0x006F7A1C;   // size 0x4
constexpr uint32_t UPPER_RING_RADIUS_2515 = 0x006F7A20;   // size 0x4
constexpr uint32_t minSpeed_2534 = 0x006F7A24;   // size 0x4
constexpr uint32_t sPinTestFOV = 0x006F7A3C;   // size 0x4
constexpr uint32_t sShakeEnabled = 0x006F7A40;   // size 0x4
constexpr uint32_t zChangeTime_2523 = 0x006F7A44;   // size 0x4
constexpr uint32_t canyon2LevelProgression = 0x006F7A48;   // size 0x4
constexpr uint32_t startMovieCount = 0x006F7A4C;   // size 0x4
constexpr uint32_t finalFade_2571 = 0x006F7A50;   // size 0x4
constexpr uint32_t toggleCount_2572 = 0x006F7A54;   // size 0x4
constexpr uint32_t assBossActive = 0x006F7A58;   // size 0x4
constexpr uint32_t antHillPlugged = 0x006F7A5C;   // size 0x4
constexpr uint32_t hintPowerUpIndex = 0x006F7A60;   // size 0x4
constexpr uint32_t MAX_ITERATIONS_2532 = 0x006F7A64;   // size 0x4
constexpr uint32_t COLLIS_EPSILON_2533 = 0x006F7A68;   // size 0x4
constexpr uint32_t s_snapDelta_2546 = 0x006F7A6C;   // size 0x4
constexpr uint32_t snapOffTorqueThresh_2547 = 0x006F7A70;   // size 0x4
constexpr uint32_t backoutStrength_2554 = 0x006F7A74;   // size 0x4
constexpr uint32_t losTestA_2427 = 0x006F7AA4;   // size 0x4
constexpr uint32_t losTestB_2428 = 0x006F7AA8;   // size 0x4
constexpr uint32_t mechaShadowRadius = 0x006F7AAC;   // size 0x4
constexpr uint32_t bossMph = 0x006F7AB0;   // size 0x4
constexpr uint32_t toggleCount_2484 = 0x006F7AB4;   // size 0x4
constexpr uint32_t smoothFactor_2488 = 0x006F7AB8;   // size 0x4
constexpr uint32_t degreeOffset_2489 = 0x006F7ABC;   // size 0x4
constexpr uint32_t inc_2490 = 0x006F7AC0;   // size 0x4
constexpr uint32_t MAX_FORCE_MAG = 0x006F7AC8;   // size 0x4
constexpr uint32_t MAX_SPEED = 0x006F7ACC;   // size 0x4
constexpr uint32_t NEIGHBOR_RANGE = 0x006F7AD0;   // size 0x4
constexpr uint32_t SIDE_CORRECTION = 0x006F7AD4;   // size 0x4
constexpr uint32_t END_CORRECTION = 0x006F7AD8;   // size 0x4
constexpr uint32_t separateDistSq = 0x006F7ADC;   // size 0x4
constexpr uint32_t separateStrength = 0x006F7AE0;   // size 0x4
constexpr uint32_t cohereDistSq = 0x006F7AE4;   // size 0x4
constexpr uint32_t cohereStrength = 0x006F7AE8;   // size 0x4
constexpr uint32_t alignDistSq = 0x006F7AEC;   // size 0x4
constexpr uint32_t alignStrength = 0x006F7AF0;   // size 0x4
constexpr uint32_t fleeDist = 0x006F7AF4;   // size 0x4
constexpr uint32_t fleeStrength = 0x006F7AF8;   // size 0x4
constexpr uint32_t squashRadius = 0x006F7AFC;   // size 0x4
constexpr uint32_t x_8PedGroup_s_numPedGroups = 0x006F7B00;   // size 0x4
constexpr uint32_t stalling_2812 = 0x006F7B14;   // size 0x4
constexpr uint32_t count_2813 = 0x006F7B18;   // size 0x4
constexpr uint32_t monsterStackIdx = 0x006F7B24;   // size 0x4
constexpr uint32_t m_minUpdateRate = 0x006F7B28;   // size 0x4
constexpr uint32_t g_levelList = 0x006F7B30;   // size 0x4
constexpr uint32_t s_shoulderAnalogSensitivity_156 = 0x006F7B34;   // size 0x4
constexpr uint32_t s_turnAnalogSensitivity_157 = 0x006F7B38;   // size 0x4
constexpr uint32_t s_len_158 = 0x006F7B3C;   // size 0x4
constexpr uint32_t s_tMod_2460 = 0x006F7B40;   // size 0x4
constexpr uint32_t s_padControl_006F7B44 = 0x006F7B44;   // size 0x4
constexpr uint32_t s_seekRange = 0x006F7B48;   // size 0x4
constexpr uint32_t s_fireRange = 0x006F7B4C;   // size 0x4
constexpr uint32_t s_fleeRange = 0x006F7B50;   // size 0x4
constexpr uint32_t s_safeRange = 0x006F7B54;   // size 0x4
constexpr uint32_t BURST_DELAY_MIN = 0x006F7B58;   // size 0x4
constexpr uint32_t BURST_DELAY_MAX = 0x006F7B5C;   // size 0x4
constexpr uint32_t BURST_COUNT_MIN = 0x006F7B60;   // size 0x4
constexpr uint32_t BURST_COUNT_MAX = 0x006F7B64;   // size 0x4
constexpr uint32_t FIRE_DELAY_MIN = 0x006F7B68;   // size 0x4
constexpr uint32_t FIRE_DELAY_MAX = 0x006F7B6C;   // size 0x4
constexpr uint32_t HOVER_DELAY_MIN = 0x006F7B70;   // size 0x4
constexpr uint32_t HOVER_DELAY_MAX = 0x006F7B74;   // size 0x4
constexpr uint32_t FIRE_DELAY_006F7B78 = 0x006F7B78;   // size 0x4
constexpr uint32_t FIRE_DELAY_RANDOMNESS_006F7B7C = 0x006F7B7C;   // size 0x4
constexpr uint32_t s_drag = 0x006F7B80;   // size 0x4
constexpr uint32_t s_elevatorThrust = 0x006F7B84;   // size 0x4
constexpr uint32_t s_headToVel = 0x006F7B88;   // size 0x4
constexpr uint32_t s_tiltThrust = 0x006F7B8C;   // size 0x4
constexpr uint32_t s_tiltToVel = 0x006F7B90;   // size 0x4
constexpr uint32_t s_slowRange_759 = 0x006F7B94;   // size 0x4
constexpr uint32_t CAR_RADIUS_763 = 0x006F7B98;   // size 0x4
constexpr uint32_t x__tmp_0_765 = 0x006F7B9C;   // size 0x4
constexpr uint32_t LOOK_AHEAD_DT_766 = 0x006F7BA0;   // size 0x4
constexpr uint32_t COLLECT_MAX_767 = 0x006F7BA4;   // size 0x4
constexpr uint32_t s_bubbleRad_771 = 0x006F7BA8;   // size 0x4
constexpr uint32_t STILL_VEL = 0x006F7BAC;   // size 0x4
constexpr uint32_t STILL_FIELDS = 0x006F7BB0;   // size 0x4
constexpr uint32_t x_9HomingBug_s_numBugs = 0x006F7BB4;   // size 0x4
constexpr uint32_t gLowStaminaThreshold = 0x006F7BB8;   // size 0x4
constexpr uint32_t x_12Letterboxing_letterBoxDist = 0x006F7BBC;   // size 0x4
constexpr uint32_t x_12Letterboxing_letterBoxVert = 0x006F7BC0;   // size 0x4
constexpr uint32_t gMessageBuf = 0x006F7BE0;   // size 0x4
constexpr uint32_t islandMessages = 0x006F7BF0;   // size 0x8
constexpr uint32_t message_2673 = 0x006F7BF8;   // size 0x4
constexpr uint32_t message_2674 = 0x006F7BFC;   // size 0x4
constexpr uint32_t message_2675 = 0x006F7C00;   // size 0x4
constexpr uint32_t message_2676 = 0x006F7C04;   // size 0x4
constexpr uint32_t hintCount_2677 = 0x006F7C08;   // size 0x4
constexpr uint32_t curMessage_2678 = 0x006F7C0C;   // size 0x4
constexpr uint32_t increment_2725 = 0x006F7C18;   // size 0x4
constexpr uint32_t gHudEnable = 0x006F7C1C;   // size 0x4
constexpr uint32_t gRangeToNearPlane = 0x006F7C20;   // size 0x4
constexpr uint32_t gHudTextureObjNum = 0x006F7C24;   // size 0x4
constexpr uint32_t m_textBoxVertMax = 0x006F7C28;   // size 0x4
constexpr uint32_t g_numCharsInTitleBeforeLineBreak = 0x006F7C30;   // size 0x4
constexpr uint32_t funcNo_2597 = 0x006F7C34;   // size 0x4
constexpr uint32_t funcNo_2612 = 0x006F7C38;   // size 0x4
constexpr uint32_t funcNo_2625 = 0x006F7C44;   // size 0x4
constexpr uint32_t funcNo_2640 = 0x006F7C48;   // size 0x4
constexpr uint32_t funcNo_2651 = 0x006F7C4C;   // size 0x4
constexpr uint32_t funcNo_2659 = 0x006F7C50;   // size 0x4
constexpr uint32_t fileDesc_2666 = 0x006F7C60;   // size 0x4
constexpr uint32_t result_2667 = 0x006F7C64;   // size 0x4
constexpr uint32_t iconFileSize_2703 = 0x006F7C78;   // size 0x4
constexpr uint32_t s_charMap = 0x006F7C90;   // size 0x5
constexpr uint32_t xoffset_2775 = 0x006F7C98;   // size 0x4
constexpr uint32_t yoffset_2776 = 0x006F7C9C;   // size 0x4
constexpr uint32_t xscale_2777 = 0x006F7CA0;   // size 0x4
constexpr uint32_t yscale_2778 = 0x006F7CA4;   // size 0x4
constexpr uint32_t xLetterScale_2779 = 0x006F7CA8;   // size 0x4
constexpr uint32_t buttonDist_2780 = 0x006F7CAC;   // size 0x4
constexpr uint32_t x__tmp_0_2902 = 0x006F7CB0;   // size 0x4
constexpr uint32_t x__tmp_1_2904 = 0x006F7CB4;   // size 0x4
constexpr uint32_t bgDist_2914 = 0x006F7CB8;   // size 0x4
constexpr uint32_t s_padControl_006F7CC8 = 0x006F7CC8;   // size 0x4
constexpr uint32_t x_18MissileTruckPickup_s_windUpDelayMin = 0x006F7CCC;   // size 0x4
constexpr uint32_t x_18MissileTruckPickup_s_windUpDelayMax = 0x006F7CD0;   // size 0x4
constexpr uint32_t x_18MissileTruckPickup_s_burstCountMin = 0x006F7CD4;   // size 0x4
constexpr uint32_t x_18MissileTruckPickup_s_burstCountMax = 0x006F7CD8;   // size 0x4
constexpr uint32_t x_18MissileTruckPickup_s_fireDelayMin = 0x006F7CDC;   // size 0x4
constexpr uint32_t x_18MissileTruckPickup_s_fireDelayMax = 0x006F7CE0;   // size 0x4
constexpr uint32_t x_18MissileTruckPickup_s_windDownDelayMin = 0x006F7CE4;   // size 0x4
constexpr uint32_t x_18MissileTruckPickup_s_windDownDelayMax = 0x006F7CE8;   // size 0x4
constexpr uint32_t x_18MissileTruckPickup_s_seekRange = 0x006F7CEC;   // size 0x4
constexpr uint32_t x_18MissileTruckPickup_s_fireRange = 0x006F7CF0;   // size 0x4
constexpr uint32_t x_18MissileTruckPickup_s_fireAngleTol = 0x006F7CF4;   // size 0x4
constexpr uint32_t x_11StatePickUp_s_attemptVelFactor = 0x006F7CF8;   // size 0x4
constexpr uint32_t s_bigRecoilTrans_2826 = 0x006F7CFC;   // size 0x4
constexpr uint32_t WaterLevel = 0x006F7D00;   // size 0x4
constexpr uint32_t cloaker_2691 = 0x006F7D04;   // size 0x4
constexpr uint32_t s_heldRad_2704 = 0x006F7D08;   // size 0x4
constexpr uint32_t s_lookAtReleaseSmoother_2753 = 0x006F7D0C;   // size 0x4
constexpr uint32_t s_maxVel_2799 = 0x006F7D10;   // size 0x4
constexpr uint32_t x_15MonsterDynamics_s_turnModZeroAtSpeed = 0x006F7D1C;   // size 0x4
constexpr uint32_t x_15MonsterDynamics_s_turnModMin = 0x006F7D20;   // size 0x4
constexpr uint32_t s_aiTurnFactor_2457 = 0x006F7D24;   // size 0x4
constexpr uint32_t s_pushMag_2461 = 0x006F7D28;   // size 0x4
constexpr uint32_t g_monsterToUnlock = 0x006F7D30;   // size 0x4
constexpr uint32_t g_skinToUnlock = 0x006F7D34;   // size 0x4
constexpr uint32_t hasValidCard_2808 = 0x006F7D60;   // size 0x4
constexpr uint32_t hasEnoughSpace_2809 = 0x006F7D64;   // size 0x4
constexpr uint32_t s_tmDirName_2834 = 0x006F7D70;   // size 0x4
constexpr uint32_t s_tmGlobalName_2835 = 0x006F7D74;   // size 0x4
constexpr uint32_t s_mon = 0x006F7D98;   // size 0x4
constexpr uint32_t clingSlack_2496 = 0x006F7D9C;   // size 0x4
constexpr uint32_t slack_2503 = 0x006F7DA0;   // size 0x4
constexpr uint32_t TOE_IN_2513 = 0x006F7DA4;   // size 0x4
constexpr uint32_t FACE_ANGLE_TOL_2517 = 0x006F7DA8;   // size 0x4
constexpr uint32_t TURN_ANGLE_TOL_2518 = 0x006F7DAC;   // size 0x4
constexpr uint32_t bankRange_2582 = 0x006F7DB0;   // size 0x4
constexpr uint32_t turnFactor_2583 = 0x006F7DB4;   // size 0x4
constexpr uint32_t s_impactVel_2590 = 0x006F7DB8;   // size 0x4
constexpr uint32_t s_turnCap_2669 = 0x006F7DBC;   // size 0x4
constexpr uint32_t x_8OgreMace_s_ballEp = 0x006F7DC0;   // size 0x4
constexpr uint32_t numDeadHeads = 0x006F7DE8;   // size 0x4
constexpr uint32_t numLoops_2562 = 0x006F7DEC;   // size 0x4
constexpr uint32_t finalFade_2563 = 0x006F7DF0;   // size 0x4
constexpr uint32_t junk_2582 = 0x006F7DF4;   // size 0x4
constexpr uint32_t playerThrown_2619 = 0x006F7DF8;   // size 0x4
constexpr uint32_t hitPowerUps = 0x006F7DFC;   // size 0x4
constexpr uint32_t s_grabHealthWhenFull = 0x006F7E00;   // size 0x4
constexpr uint32_t s_grabStaminaWhenFull = 0x006F7E04;   // size 0x4
constexpr uint32_t s_regenMultiplier = 0x006F7E08;   // size 0x4
constexpr uint32_t s_cloakTime = 0x006F7E0C;   // size 0x4
constexpr uint32_t gFieldCounter = 0x006F7E20;   // size 0x4
constexpr uint32_t doTweaks = 0x006F7E24;   // size 0x4
constexpr uint32_t grappleLastFrame = 0x006F7E28;   // size 0x4
constexpr uint32_t tweakLastFrame = 0x006F7E2C;   // size 0x4
constexpr uint32_t rtOldSp = 0x006F7E30;   // size 0x8
constexpr uint32_t g_frame = 0x006F7E38;   // size 0x4
constexpr uint32_t MAX_COLLIS_ITERATIONS = 0x006F7E48;   // size 0x4
constexpr uint32_t size_2487 = 0x006F7E4C;   // size 0x4
constexpr uint32_t number_2488 = 0x006F7E50;   // size 0x4
constexpr uint32_t jaggedness_2489 = 0x006F7E54;   // size 0x4
constexpr uint32_t whichAction_006F7E58 = 0x006F7E58;   // size 0x4
constexpr uint32_t quakeDone = 0x006F7E68;   // size 0x4
constexpr uint32_t NamecardDistance = 0x006F7E70;   // size 0x4
constexpr uint32_t targetAlpha = 0x006F7E74;   // size 0x4
constexpr uint32_t numModsLeft = 0x006F7E78;   // size 0x4
constexpr uint32_t UnlockTitle = 0x006F7E7C;   // size 0x4
constexpr uint32_t screenFirstPass = 0x006F7E80;   // size 0x4
constexpr uint32_t PlayerConfig = 0x006F7E84;   // size 0x4
constexpr uint32_t currAnimationIndex = 0x006F7E88;   // size 0x4
constexpr uint32_t betweenScreens = 0x006F7E8C;   // size 0x4
constexpr uint32_t numSelectablesH = 0x006F7E90;   // size 0x8
constexpr uint32_t AINumLeft = 0x006F7E98;   // size 0x4
constexpr uint32_t AINum = 0x006F7E9C;   // size 0x4
constexpr uint32_t mSelectWins = 0x006F7EA0;   // size 0x4
constexpr uint32_t charSelectStringOne = 0x006F7EA4;   // size 0x4
constexpr uint32_t charSelectStringTwo = 0x006F7EA8;   // size 0x4
constexpr uint32_t numeralString = 0x006F7EB0;   // size 0x4
constexpr uint32_t animq = 0x006F7EBC;   // size 0x4
constexpr uint32_t changingReel = 0x006F7EC0;   // size 0x4
constexpr uint32_t numSelectables_2784 = 0x006F7EC4;   // size 0x4
constexpr uint32_t first_2788 = 0x006F7EC8;   // size 0x4
constexpr uint32_t last_2789 = 0x006F7ECC;   // size 0x4
constexpr uint32_t first_2793 = 0x006F7ED0;   // size 0x4
constexpr uint32_t last_2794 = 0x006F7ED4;   // size 0x4
constexpr uint32_t first_2798 = 0x006F7ED8;   // size 0x4
constexpr uint32_t last_2799 = 0x006F7EDC;   // size 0x4
constexpr uint32_t first_2803 = 0x006F7EE0;   // size 0x4
constexpr uint32_t last_2804 = 0x006F7EE4;   // size 0x4
constexpr uint32_t first_2808 = 0x006F7EE8;   // size 0x4
constexpr uint32_t last_2809 = 0x006F7EEC;   // size 0x4
constexpr uint32_t first_2813 = 0x006F7EF0;   // size 0x4
constexpr uint32_t last_2814 = 0x006F7EF4;   // size 0x4
constexpr uint32_t first_2818 = 0x006F7EF8;   // size 0x4
constexpr uint32_t last_2819 = 0x006F7EFC;   // size 0x4
constexpr uint32_t numSelectables_2823 = 0x006F7F00;   // size 0x4
constexpr uint32_t numSelectables_2827 = 0x006F7F04;   // size 0x4
constexpr uint32_t m_textBoxAlpha_2828 = 0x006F7F08;   // size 0x4
constexpr uint32_t numAvail1_2830 = 0x006F7F0C;   // size 0x4
constexpr uint32_t first_2831 = 0x006F7F10;   // size 0x4
constexpr uint32_t last_2832 = 0x006F7F14;   // size 0x4
constexpr uint32_t curCostume1_2833 = 0x006F7F18;   // size 0x4
constexpr uint32_t stoppedMovie_2834 = 0x006F7F1C;   // size 0x4
constexpr uint32_t showTexCounter_2835 = 0x006F7F20;   // size 0x4
constexpr uint32_t numSelectables_2839 = 0x006F7F24;   // size 0x4
constexpr uint32_t numAvail1_2841 = 0x006F7F28;   // size 0x4
constexpr uint32_t stoppedMovie_2842 = 0x006F7F2C;   // size 0x4
constexpr uint32_t showTexCounter_2843 = 0x006F7F30;   // size 0x4
constexpr uint32_t selectingIsComplete_2844 = 0x006F7F34;   // size 0x4
constexpr uint32_t x__tmp_0_2846 = 0x006F7F38;   // size 0x4
constexpr uint32_t curCostume1_2847 = 0x006F7F3C;   // size 0x4
constexpr uint32_t first_2848 = 0x006F7F40;   // size 0x4
constexpr uint32_t last_2849 = 0x006F7F44;   // size 0x4
constexpr uint32_t numSelectables_2853 = 0x006F7F48;   // size 0x4
constexpr uint32_t numSelectables_2857 = 0x006F7F4C;   // size 0x4
constexpr uint32_t beginning_2861 = 0x006F7F50;   // size 0x4
constexpr uint32_t ending_2862 = 0x006F7F54;   // size 0x4
constexpr uint32_t m_textBoxAlpha_2863 = 0x006F7F58;   // size 0x4
constexpr uint32_t m_textBoxAlpha2_2864 = 0x006F7F5C;   // size 0x4
constexpr uint32_t m_textBoxAlpha3_2865 = 0x006F7F60;   // size 0x4
constexpr uint32_t numSelectablesOne_2866 = 0x006F7F64;   // size 0x4
constexpr uint32_t numSelectablesTwo_2867 = 0x006F7F68;   // size 0x4
constexpr uint32_t curCostume1_2868 = 0x006F7F6C;   // size 0x4
constexpr uint32_t curCostume2_2869 = 0x006F7F70;   // size 0x4
constexpr uint32_t curCostume3_2870 = 0x006F7F74;   // size 0x4
constexpr uint32_t playerOneIsDone_2871 = 0x006F7F78;   // size 0x4
constexpr uint32_t playerTwoIsDone_2872 = 0x006F7F7C;   // size 0x4
constexpr uint32_t aiOneIsDone_2873 = 0x006F7F80;   // size 0x4
constexpr uint32_t aiTwoIsDone_2874 = 0x006F7F84;   // size 0x4
constexpr uint32_t AINum_2875 = 0x006F7F88;   // size 0x4
constexpr uint32_t watchForASecond_2876 = 0x006F7F8C;   // size 0x4
constexpr uint32_t numAvail1_2877 = 0x006F7F90;   // size 0x4
constexpr uint32_t numAvail2_2878 = 0x006F7F94;   // size 0x4
constexpr uint32_t numAvail3_2879 = 0x006F7F98;   // size 0x4
constexpr uint32_t num1_2880 = 0x006F7F9C;   // size 0x4
constexpr uint32_t num2_2881 = 0x006F7FA0;   // size 0x4
constexpr uint32_t num3_2882 = 0x006F7FA4;   // size 0x4
constexpr uint32_t aiOneMon_2883 = 0x006F7FA8;   // size 0x4
constexpr uint32_t aiOneCost_2884 = 0x006F7FAC;   // size 0x4
constexpr uint32_t beginning_2888 = 0x006F7FB0;   // size 0x4
constexpr uint32_t ending_2889 = 0x006F7FB4;   // size 0x4
constexpr uint32_t m_textBoxAlpha_2890 = 0x006F7FB8;   // size 0x4
constexpr uint32_t m_textBoxAlpha2_2891 = 0x006F7FBC;   // size 0x4
constexpr uint32_t numSelectablesOne_2892 = 0x006F7FC0;   // size 0x4
constexpr uint32_t numSelectablesTwo_2893 = 0x006F7FC4;   // size 0x4
constexpr uint32_t curCostume1_2894 = 0x006F7FC8;   // size 0x4
constexpr uint32_t curCostume2_2895 = 0x006F7FCC;   // size 0x4
constexpr uint32_t playerOneIsDone_2896 = 0x006F7FD0;   // size 0x4
constexpr uint32_t playerTwoIsDone_2897 = 0x006F7FD4;   // size 0x4
constexpr uint32_t AIplayerIsDone_2898 = 0x006F7FD8;   // size 0x4
constexpr uint32_t watchForASecond_2899 = 0x006F7FDC;   // size 0x4
constexpr uint32_t numAvail1_2900 = 0x006F7FE0;   // size 0x4
constexpr uint32_t numAvail2_2901 = 0x006F7FE4;   // size 0x4
constexpr uint32_t num1_2902 = 0x006F7FE8;   // size 0x4
constexpr uint32_t num2_2903 = 0x006F7FEC;   // size 0x4
constexpr uint32_t AiLeft_2904 = 0x006F7FF0;   // size 0x4
constexpr uint32_t beginning_2908 = 0x006F7FF4;   // size 0x4
constexpr uint32_t ending_2909 = 0x006F7FF8;   // size 0x4
constexpr uint32_t numSelectablesOne_2910 = 0x006F7FFC;   // size 0x4
constexpr uint32_t numSelectablesTwo_2911 = 0x006F8000;   // size 0x4
constexpr uint32_t curCostume1_2912 = 0x006F8004;   // size 0x4
constexpr uint32_t curCostume2_2913 = 0x006F8008;   // size 0x4
constexpr uint32_t playerOneIsDone_2914 = 0x006F800C;   // size 0x4
constexpr uint32_t playerTwoIsDone_2915 = 0x006F8010;   // size 0x4
constexpr uint32_t watchForASecond_2916 = 0x006F8014;   // size 0x4
constexpr uint32_t numAvail1_2917 = 0x006F8018;   // size 0x4
constexpr uint32_t numAvail2_2918 = 0x006F801C;   // size 0x4
constexpr uint32_t num1_2919 = 0x006F8020;   // size 0x4
constexpr uint32_t num2_2920 = 0x006F8024;   // size 0x4
constexpr uint32_t numSelectables_2924 = 0x006F8028;   // size 0x4
constexpr uint32_t beginning_2925 = 0x006F802C;   // size 0x4
constexpr uint32_t ending_2926 = 0x006F8030;   // size 0x4
constexpr uint32_t stoppedMovie_2927 = 0x006F8034;   // size 0x4
constexpr uint32_t showTexCounter_2928 = 0x006F8038;   // size 0x4
constexpr uint32_t gSliderBrightness = 0x006F803C;   // size 0x4
constexpr uint32_t numSelectables_2938 = 0x006F8040;   // size 0x4
constexpr uint32_t Difficulty_2939 = 0x006F8044;   // size 0x4
constexpr uint32_t audio_2940 = 0x006F8048;   // size 0x4
constexpr uint32_t fxVol_2941 = 0x006F804C;   // size 0x4
constexpr uint32_t musVol_2942 = 0x006F8050;   // size 0x4
constexpr uint32_t vib_2943 = 0x006F8054;   // size 0x4
constexpr uint32_t vib2_2944 = 0x006F8058;   // size 0x4
constexpr uint32_t numSelectables_2948 = 0x006F805C;   // size 0x4
constexpr uint32_t numSelectables_2952 = 0x006F8060;   // size 0x4
constexpr uint32_t numSelectables_2959 = 0x006F8064;   // size 0x4
constexpr uint32_t numSelectables_2964 = 0x006F8068;   // size 0x4
constexpr uint32_t numWins_2965 = 0x006F806C;   // size 0x4
constexpr uint32_t needUpdate_2972 = 0x006F8070;   // size 0x4
constexpr uint32_t waitingForAnimation_2977 = 0x006F8074;   // size 0x4
constexpr uint32_t waitingForAlphaFade_2978 = 0x006F8078;   // size 0x4
constexpr uint32_t fadingIn_2979 = 0x006F807C;   // size 0x4
constexpr uint32_t fadingOut_2980 = 0x006F8080;   // size 0x4
constexpr uint32_t halfWayMark_2981 = 0x006F8084;   // size 0x4
constexpr uint32_t currentMenuSelection_2985 = 0x006F8088;   // size 0x4
constexpr uint32_t visibleStart_2986 = 0x006F808C;   // size 0x4
constexpr uint32_t x__tmp_1_2988 = 0x006F8090;   // size 0x4
constexpr uint32_t m_2989 = 0x006F8094;   // size 0x4
constexpr uint32_t s_2990 = 0x006F8098;   // size 0x4
constexpr uint32_t fade_2991 = 0x006F809C;   // size 0x4
constexpr uint32_t items_2992 = 0x006F80A0;   // size 0x4
constexpr uint32_t loop_2993 = 0x006F80A4;   // size 0x4
constexpr uint32_t loop2_2994 = 0x006F80A8;   // size 0x4
constexpr uint32_t fadingin_2995 = 0x006F80AC;   // size 0x4
constexpr uint32_t m_textBoxAlpha_2999 = 0x006F80CC;   // size 0x4
constexpr uint32_t m_textBoxAlpha2_3000 = 0x006F80D0;   // size 0x4
constexpr uint32_t m_textBoxAlpha3_3001 = 0x006F80D4;   // size 0x4
constexpr uint32_t m_textBoxAlpha4_3002 = 0x006F80D8;   // size 0x4
constexpr uint32_t beginning_3003 = 0x006F80DC;   // size 0x4
constexpr uint32_t ending_3004 = 0x006F80E0;   // size 0x4
constexpr uint32_t numSelectablesOne_3005 = 0x006F80E4;   // size 0x4
constexpr uint32_t numSelectablesTwo_3006 = 0x006F80E8;   // size 0x4
constexpr uint32_t curCostume1_3007 = 0x006F80EC;   // size 0x4
constexpr uint32_t curCostume2_3008 = 0x006F80F0;   // size 0x4
constexpr uint32_t curCostume3_3009 = 0x006F80F4;   // size 0x4
constexpr uint32_t curCostume4_3010 = 0x006F80F8;   // size 0x4
constexpr uint32_t playerOneIsDone_3011 = 0x006F80FC;   // size 0x4
constexpr uint32_t playerTwoIsDone_3012 = 0x006F8100;   // size 0x4
constexpr uint32_t aiOneIsDone_3013 = 0x006F8104;   // size 0x4
constexpr uint32_t aiTwoIsDone_3014 = 0x006F8108;   // size 0x4
constexpr uint32_t aiThreeIsDone_3015 = 0x006F810C;   // size 0x4
constexpr uint32_t watchForASecond_3016 = 0x006F8110;   // size 0x4
constexpr uint32_t numAvail1_3017 = 0x006F8114;   // size 0x4
constexpr uint32_t numAvail2_3018 = 0x006F8118;   // size 0x4
constexpr uint32_t numAvail3_3019 = 0x006F811C;   // size 0x4
constexpr uint32_t numAvail4_3020 = 0x006F8120;   // size 0x4
constexpr uint32_t num1_3021 = 0x006F8124;   // size 0x4
constexpr uint32_t num2_3022 = 0x006F8128;   // size 0x4
constexpr uint32_t num3_3023 = 0x006F812C;   // size 0x4
constexpr uint32_t num4_3024 = 0x006F8130;   // size 0x4
constexpr uint32_t aiOneMon_3025 = 0x006F8134;   // size 0x4
constexpr uint32_t aiOneCost_3026 = 0x006F8138;   // size 0x4
constexpr uint32_t aiTwoMon_3027 = 0x006F813C;   // size 0x4
constexpr uint32_t aiTwoCost_3028 = 0x006F8140;   // size 0x4
constexpr uint32_t loop_3044 = 0x006F8144;   // size 0x4
constexpr uint32_t high_3045 = 0x006F8148;   // size 0x4
constexpr uint32_t low_3046 = 0x006F814C;   // size 0x4
constexpr uint32_t alpha_3047 = 0x006F8150;   // size 0x4
constexpr uint32_t fadingIn_3048 = 0x006F8154;   // size 0x4
constexpr uint32_t val1_x_3061 = 0x006F8158;   // size 0x4
constexpr uint32_t val_y_3062 = 0x006F815C;   // size 0x4
constexpr uint32_t val_z_3063 = 0x006F8160;   // size 0x4
constexpr uint32_t val2_x_3064 = 0x006F8164;   // size 0x4
constexpr uint32_t unlocked_3092 = 0x006F8168;   // size 0x4
constexpr uint32_t numSelectablesOne_3096 = 0x006F816C;   // size 0x4
constexpr uint32_t numSelectablesTwo_3097 = 0x006F8170;   // size 0x4
constexpr uint32_t beginning_3098 = 0x006F8174;   // size 0x4
constexpr uint32_t ending_3099 = 0x006F8178;   // size 0x4
constexpr uint32_t g_noScripts = 0x006F8180;   // size 0x4
constexpr uint32_t UseCommandLineLevel = 0x006F8184;   // size 0x4
constexpr uint32_t NonStandardNameSelected = 0x006F8188;   // size 0x4
constexpr uint32_t altAI = 0x006F818C;   // size 0x4
constexpr uint32_t exitFromAdvStory = 0x006F8190;   // size 0x4
constexpr uint32_t clearBeforeFontDMA = 0x006F8194;   // size 0x4
constexpr uint32_t temp_2832 = 0x006F8198;   // size 0x4
constexpr uint32_t mov_2987 = 0x006F81EC;   // size 0x4
constexpr uint32_t needIntro = 0x006F81F0;   // size 0x4
constexpr uint32_t needOutro = 0x006F81F4;   // size 0x4
constexpr uint32_t game = 0x006F81F8;   // size 0x4
constexpr uint32_t destructibles = 0x006F81FC;   // size 0x4
constexpr uint32_t shell = 0x006F8200;   // size 0x4
constexpr uint32_t calledFrom = 0x006F8208;   // size 0x4
constexpr uint32_t gCurrentMenuSelection = 0x006F820C;   // size 0x4
constexpr uint32_t gLastMenuSelection = 0x006F8210;   // size 0x4
constexpr uint32_t gExitDialog = 0x006F8214;   // size 0x4
constexpr uint32_t gDrawOverlay = 0x006F8218;   // size 0x4
constexpr uint32_t currentMenuSelection_2733 = 0x006F8230;   // size 0x4
constexpr uint32_t reset_2734 = 0x006F8234;   // size 0x4
constexpr uint32_t foundSlot_2735 = 0x006F8238;   // size 0x4
constexpr uint32_t firsttime_2736 = 0x006F823C;   // size 0x4
constexpr uint32_t starting_2737 = 0x006F8240;   // size 0x4
constexpr uint32_t gPlayerThatPaused = 0x006F82B8;   // size 0x4
constexpr uint32_t g_onStartupFrame = 0x006F82C0;   // size 0x4
constexpr uint32_t gIntroMoviesHavePlayed = 0x006F82C4;   // size 0x4
constexpr uint32_t gInnerPed = 0x006F82C8;   // size 0x4
constexpr uint32_t gOutterPed = 0x006F82CC;   // size 0x4
constexpr uint32_t gUseEarsOnMonster = 0x006F82D0;   // size 0x4
constexpr uint32_t COLLECT_MAX_3000 = 0x006F82D4;   // size 0x4
constexpr uint32_t mTimeOfLastFinalBossStep = 0x006F82D8;   // size 0x4
constexpr uint32_t mPlayingBossMusic = 0x006F82F4;   // size 0x4
constexpr uint32_t mTimeOfMusicFadeOut = 0x006F82F8;   // size 0x4
constexpr uint32_t mOrigBattleFadeOutVol = 0x006F82FC;   // size 0x4
constexpr uint32_t mOrigAmbientFadeOutVol = 0x006F8300;   // size 0x4
constexpr uint32_t size_2853 = 0x006F8308;   // size 0x4
constexpr uint32_t number_2854 = 0x006F830C;   // size 0x4
constexpr uint32_t jaggedness_2855 = 0x006F8310;   // size 0x4
constexpr uint32_t x_10SpecFxAnim_s_numFxs = 0x006F8314;   // size 0x4
constexpr uint32_t quantizer = 0x006F8318;   // size 0x4
constexpr uint32_t s_padControl_006F8330 = 0x006F8330;   // size 0x4
constexpr uint32_t x_10TankPickup_s_windUpDelayMin = 0x006F8334;   // size 0x4
constexpr uint32_t x_10TankPickup_s_windUpDelayMax = 0x006F8338;   // size 0x4
constexpr uint32_t x_10TankPickup_s_burstCountMin = 0x006F833C;   // size 0x4
constexpr uint32_t x_10TankPickup_s_burstCountMax = 0x006F8340;   // size 0x4
constexpr uint32_t x_10TankPickup_s_fireDelayMin = 0x006F8344;   // size 0x4
constexpr uint32_t x_10TankPickup_s_fireDelayMax = 0x006F8348;   // size 0x4
constexpr uint32_t x_10TankPickup_s_windDownDelayMin = 0x006F834C;   // size 0x4
constexpr uint32_t x_10TankPickup_s_windDownDelayMax = 0x006F8350;   // size 0x4
constexpr uint32_t x_10TankPickup_s_seekRange = 0x006F8354;   // size 0x4
constexpr uint32_t x_10TankPickup_s_fireRange = 0x006F8358;   // size 0x4
constexpr uint32_t x_10TankPickup_s_fireAngleTol = 0x006F835C;   // size 0x4
constexpr uint32_t x_10TankPickup_s_fleeRange = 0x006F8360;   // size 0x4
constexpr uint32_t x_10TankPickup_s_safeRange = 0x006F8364;   // size 0x4
constexpr uint32_t RECOIL_SPEED = 0x006F8368;   // size 0x4
constexpr uint32_t FIRE_DELAY_006F836C = 0x006F836C;   // size 0x4
constexpr uint32_t FIRE_DELAY_RANDOMNESS_006F8370 = 0x006F8370;   // size 0x4
constexpr uint32_t x_12TurretPickup_s_fireAngleTol = 0x006F8374;   // size 0x4
constexpr uint32_t x_12TurretPickup_s_weapType = 0x006F8378;   // size 0x4
constexpr uint32_t monstersRespawned = 0x006F8380;   // size 0x4
constexpr uint32_t lastCountdownSecs_2663 = 0x006F8390;   // size 0x4
constexpr uint32_t craterSwitched = 0x006F839C;   // size 0x4
constexpr uint32_t craterTransitionInProgress = 0x006F83A0;   // size 0x4
constexpr uint32_t stackCountDownActive = 0x006F83A4;   // size 0x4
constexpr uint32_t stackCountDown = 0x006F83A8;   // size 0x4
constexpr uint32_t s_vFactor = 0x006F83B0;   // size 0x4
constexpr uint32_t s_counterUpright_2448 = 0x006F83B8;   // size 0x4
constexpr uint32_t s_maxCounterAttempts_2449 = 0x006F83BC;   // size 0x4
constexpr uint32_t s_snapAlignThresh_2477 = 0x006F83C0;   // size 0x4
constexpr uint32_t s_snapW_2478 = 0x006F83C4;   // size 0x4
constexpr uint32_t s_collisX_2479 = 0x006F83C8;   // size 0x4
constexpr uint32_t s_collisY_2480 = 0x006F83CC;   // size 0x4
constexpr uint32_t s_collisZ_2481 = 0x006F83D0;   // size 0x4
constexpr uint32_t s_collisForce_2482 = 0x006F83D4;   // size 0x4
constexpr uint32_t s_pushMagX_2483 = 0x006F83D8;   // size 0x4
constexpr uint32_t s_pushMagY_2484 = 0x006F83DC;   // size 0x4
constexpr uint32_t pedsTurnedOff = 0x006F83E0;   // size 0x4
constexpr uint32_t whichAction_2505 = 0x006F83E4;   // size 0x4
constexpr uint32_t finalFade_2506 = 0x006F83E8;   // size 0x4
constexpr uint32_t toggleCount_2507 = 0x006F83EC;   // size 0x4
constexpr uint32_t tidalWaveActive = 0x006F83F0;   // size 0x4
constexpr uint32_t tidalWaveCount = 0x006F83F4;   // size 0x4
constexpr uint32_t tidalSource = 0x006F83F8;   // size 0x4
constexpr uint32_t movieActive = 0x006F83FC;   // size 0x4
constexpr uint32_t movieCount = 0x006F8400;   // size 0x4
constexpr uint32_t finalFade_2448 = 0x006F8404;   // size 0x4
constexpr uint32_t toggleCount_2449 = 0x006F8408;   // size 0x4
constexpr uint32_t s_moveFract_2475 = 0x006F840C;   // size 0x4
constexpr uint32_t s_seekMoveFract_2476 = 0x006F8410;   // size 0x4
constexpr uint32_t s_specialSeekMoveFract_2477 = 0x006F8414;   // size 0x4
constexpr uint32_t s_moveStepMin_2481 = 0x006F8418;   // size 0x4
constexpr uint32_t s_moveStepMax_2482 = 0x006F841C;   // size 0x4
constexpr uint32_t x_9UfoPickup_s_windUpDelayMin = 0x006F8420;   // size 0x4
constexpr uint32_t x_9UfoPickup_s_windUpDelayMax = 0x006F8424;   // size 0x4
constexpr uint32_t x_9UfoPickup_s_burstCountMin = 0x006F8428;   // size 0x4
constexpr uint32_t x_9UfoPickup_s_burstCountMax = 0x006F842C;   // size 0x4
constexpr uint32_t x_9UfoPickup_s_fireDelayMin = 0x006F8430;   // size 0x4
constexpr uint32_t x_9UfoPickup_s_fireDelayMax = 0x006F8434;   // size 0x4
constexpr uint32_t x_9UfoPickup_s_seekRange = 0x006F8438;   // size 0x4
constexpr uint32_t x_9UfoPickup_s_fireRange = 0x006F843C;   // size 0x4
constexpr uint32_t x_9UfoPickup_s_windDownDelayMin = 0x006F8440;   // size 0x4
constexpr uint32_t x_9UfoPickup_s_windDownDelayMax = 0x006F8444;   // size 0x4
constexpr uint32_t specialBurstCount = 0x006F8448;   // size 0x4
constexpr uint32_t specialFireDelay = 0x006F844C;   // size 0x4
constexpr uint32_t specialWindDelay = 0x006F8450;   // size 0x4
constexpr uint32_t numSaucers = 0x006F8454;   // size 0x4
constexpr uint32_t continueDecoding = 0x006F8458;   // size 0x4
constexpr uint32_t pMode = 0x006F845C;   // size 0x4
constexpr uint32_t prevScreen = 0x006F8460;   // size 0x4
constexpr uint32_t currScreen = 0x006F8464;   // size 0x4
constexpr uint32_t lastScreen = 0x006F8468;   // size 0x4
constexpr uint32_t nextScreen = 0x006F846C;   // size 0x4
constexpr uint32_t nextMovie = 0x006F8470;   // size 0x4
constexpr uint32_t whichPlayerIsPicking = 0x006F8474;   // size 0x4
constexpr uint32_t incogTimer = 0x006F8478;   // size 0x4
constexpr uint32_t currentMonster = 0x006F847C;   // size 0x4
constexpr uint32_t lastMonster = 0x006F8480;   // size 0x4
constexpr uint32_t currentLevel = 0x006F8484;   // size 0x4
constexpr uint32_t uiWhichPad = 0x006F8488;   // size 0x4
constexpr uint32_t clutterFrame = 0x006F848C;   // size 0x4
constexpr uint32_t clutterDelay = 0x006F8490;   // size 0x4
constexpr uint32_t clutterDelayCnt = 0x006F8494;   // size 0x4
constexpr uint32_t testPattern = 0x006F8498;   // size 0x4
constexpr uint32_t AItoChoose = 0x006F849C;   // size 0x4
constexpr uint32_t initialAnimation = 0x006F84A0;   // size 0x4
constexpr uint32_t movieEnable = 0x006F84A4;   // size 0x4
constexpr uint32_t select = 0x006F84A8;   // size 0x4
constexpr uint32_t goLoadLevel = 0x006F84AC;   // size 0x4
constexpr uint32_t exitUi = 0x006F84B0;   // size 0x4
constexpr uint32_t gManualMonsterChangeHappened = 0x006F84B4;   // size 0x4
constexpr uint32_t gRealWorldEpNode = 0x006F84B8;   // size 0x4
constexpr uint32_t lastTicCount = 0x006F84BC;   // size 0x4
constexpr uint32_t thisTicCount = 0x006F84C0;   // size 0x4
constexpr uint32_t startTime = 0x006F84C4;   // size 0x4
constexpr uint32_t frameTime = 0x006F84C8;   // size 0x4
constexpr uint32_t movieTime = 0x006F84CC;   // size 0x4
constexpr uint32_t innerFrameTime = 0x006F84D0;   // size 0x4
constexpr uint32_t g_fadeFrameOffset = 0x006F84D4;   // size 0x4
constexpr uint32_t movieHasStarted_2745 = 0x006F84E0;   // size 0x4
constexpr uint32_t movieHasStarted_2749 = 0x006F84E4;   // size 0x4
constexpr uint32_t g_shellOddEven = 0x006F84E8;   // size 0x4
constexpr uint32_t userintOldSp = 0x006F84F0;   // size 0x8
constexpr uint32_t numPlayerStrings = 0x006F84F8;   // size 0x8
constexpr uint32_t z_510 = 0x006F8530;   // size 0x4
constexpr uint32_t exitDevUi = 0x006F8548;   // size 0x4
constexpr uint32_t whichAction_006F8550 = 0x006F8550;   // size 0x4
constexpr uint32_t toggleCount_2518 = 0x006F8554;   // size 0x4
constexpr uint32_t vegasSpawnMilitary = 0x006F8558;   // size 0x4
constexpr uint32_t s_hatField = 0x006F855C;   // size 0x4
constexpr uint32_t s_hatCounter = 0x006F8560;   // size 0x4
constexpr uint32_t CAR_RADIUS_792 = 0x006F8564;   // size 0x4
constexpr uint32_t x__tmp_0_794 = 0x006F8568;   // size 0x4
constexpr uint32_t LOOK_AHEAD_DT_795 = 0x006F856C;   // size 0x4
constexpr uint32_t COLLECT_MAX_796 = 0x006F8570;   // size 0x4
constexpr uint32_t gNumViews = 0x006F8578;   // size 0x4
constexpr uint32_t s_enabled = 0x006F857C;   // size 0x4
constexpr uint32_t stat_enlarge = 0x006F8580;   // size 0x4
constexpr uint32_t stat_intensity = 0x006F8584;   // size 0x4
constexpr uint32_t fb0U0 = 0x006F8588;   // size 0x4
constexpr uint32_t fb0V0 = 0x006F858C;   // size 0x4
constexpr uint32_t fb0U1 = 0x006F8590;   // size 0x4
constexpr uint32_t fb0V1 = 0x006F8594;   // size 0x4
constexpr uint32_t fb0X0 = 0x006F8598;   // size 0x4
constexpr uint32_t fb0Y0 = 0x006F859C;   // size 0x4
constexpr uint32_t fb0X1 = 0x006F85A0;   // size 0x4
constexpr uint32_t fb0Y1 = 0x006F85A4;   // size 0x4
constexpr uint32_t fb1U0 = 0x006F85A8;   // size 0x4
constexpr uint32_t fb1V0 = 0x006F85AC;   // size 0x4
constexpr uint32_t fb1U1 = 0x006F85B0;   // size 0x4
constexpr uint32_t fb1V1 = 0x006F85B4;   // size 0x4
constexpr uint32_t fb1X0 = 0x006F85B8;   // size 0x4
constexpr uint32_t fb1Y0 = 0x006F85BC;   // size 0x4
constexpr uint32_t fb1X1 = 0x006F85C0;   // size 0x4
constexpr uint32_t fb1Y1 = 0x006F85C4;   // size 0x4
constexpr uint32_t okToBlend = 0x006F85C8;   // size 0x4
constexpr uint32_t s_blendCurve = 0x006F85CC;   // size 0x4
constexpr uint32_t linearChar = 0x006F85D0;   // size 0x4
constexpr uint32_t smoothChar = 0x006F85D4;   // size 0x4
constexpr uint32_t g_dbsDisableNodeMask = 0x006F85E8;   // size 0x2
constexpr uint32_t dbsStackIdx = 0x006F8630;   // size 0x4
constexpr uint32_t isCountVblank = 0x006F8638;   // size 0x4
constexpr uint32_t isFrameEnd = 0x006F863C;   // size 0x4
constexpr uint32_t isUp = 0x006F8640;   // size 0x4
constexpr uint32_t handler_error = 0x006F8644;   // size 0x4
constexpr uint32_t VramAddrForMovies = 0x006F8648;   // size 0x4
constexpr uint32_t VramWidthForMovies = 0x006F864C;   // size 0x4
constexpr uint32_t movieIsPlaying = 0x006F8650;   // size 0x4
constexpr uint32_t vbhEnabled = 0x006F8654;   // size 0x4
constexpr uint32_t movieHasPlayed_42 = 0x006F8658;   // size 0x4
constexpr uint32_t lastValidTag = 0x006F865C;   // size 0x4
constexpr uint32_t tempflag_58 = 0x006F8660;   // size 0x4
constexpr uint32_t lastWriteValue = 0x006F8664;   // size 0x4
constexpr uint32_t localSeed = 0x006F8668;   // size 0x4
constexpr uint32_t cdHasBeenInitialized = 0x006F8670;   // size 0x4
constexpr uint32_t FileReadStatus_9 = 0x006F8674;   // size 0x4
constexpr uint32_t cdReadMode = 0x006F86C0;   // size 0x4
constexpr uint32_t fogMinRange = 0x006F86C8;   // size 0x4
constexpr uint32_t fogMaxRange = 0x006F86CC;   // size 0x4
constexpr uint32_t fogMaxVal = 0x006F86D0;   // size 0x4
constexpr uint32_t fogFarClip = 0x006F86D4;   // size 0x4
constexpr uint32_t fogFA = 0x006F86D8;   // size 0x4
constexpr uint32_t fogFB = 0x006F86DC;   // size 0x4
constexpr uint32_t fogColorRed = 0x006F86E0;   // size 0x4
constexpr uint32_t fogColorGreen = 0x006F86E4;   // size 0x4
constexpr uint32_t fogColorBlue = 0x006F86E8;   // size 0x4
constexpr uint32_t fogBackgroundColorIsSlaved = 0x006F86EC;   // size 0x4
constexpr uint32_t fogChanged = 0x006F86F0;   // size 0x4
constexpr uint32_t fogTOD = 0x006F86F4;   // size 0x4
constexpr uint32_t fogTODMinRange = 0x006F86F8;   // size 0x4
constexpr uint32_t fogTODMaxRange = 0x006F86FC;   // size 0x4
constexpr uint32_t fogTODMaxVal = 0x006F8700;   // size 0x4
constexpr uint32_t fogTODFarClip = 0x006F8704;   // size 0x4
constexpr uint32_t fogTODFA = 0x006F8708;   // size 0x4
constexpr uint32_t fogTODFB = 0x006F870C;   // size 0x4
constexpr uint32_t fogTODColorRed = 0x006F8710;   // size 0x4
constexpr uint32_t fogTODColorGreen = 0x006F8714;   // size 0x4
constexpr uint32_t fogTODColorBlue = 0x006F8718;   // size 0x4
constexpr uint32_t fontFirstFrame = 0x006F871C;   // size 0x4
constexpr uint32_t spriteTESTlocation = 0x006F8720;   // size 0x4
constexpr uint32_t spriteFBAlocation = 0x006F8724;   // size 0x4
constexpr uint32_t spritePRIMlocation = 0x006F8728;   // size 0x4
constexpr uint32_t colGrid = 0x006F872C;   // size 0x4
constexpr uint32_t hdTraceId = 0x006F8730;   // size 0x4
constexpr uint32_t csEpNodeNull = 0x006F8734;   // size 0x4
constexpr uint32_t hdDefaultHeight = 0x006F8738;   // size 0x4
constexpr uint32_t g_traceId = 0x006F873C;   // size 0x4
constexpr uint32_t s_checkBackface = 0x006F874C;   // size 0x4
constexpr uint32_t gTraversalCallback = 0x006F8768;   // size 0x4
constexpr uint32_t g_dontTraverseObjects = 0x006F876C;   // size 0x4
constexpr uint32_t g_doFOVGroups = 0x006F8770;   // size 0x4
constexpr uint32_t dirty_476 = 0x006F8774;   // size 0x4
constexpr uint32_t matStack = 0x006F8778;   // size 0x4
constexpr uint32_t fovNorms = 0x006F877C;   // size 0x4
constexpr uint32_t fovNorms1 = 0x006F8780;   // size 0x4
constexpr uint32_t lightDir = 0x006F8784;   // size 0x4
constexpr uint32_t skelMatStack = 0x006F8788;   // size 0x4
constexpr uint32_t skelStack = 0x006F878C;   // size 0x4
constexpr uint32_t eoCnt = 0x006F8790;   // size 0x4
constexpr uint32_t lastEoCnt = 0x006F8794;   // size 0x4
constexpr uint32_t lastGsCtx = 0x006F8798;   // size 0x4
constexpr uint32_t objectCount = 0x006F879C;   // size 0x4
constexpr uint32_t objsInPacket = 0x006F87A0;   // size 0x4
constexpr uint32_t objsInAlphaPacket = 0x006F87A4;   // size 0x4
constexpr uint32_t vertexCount = 0x006F87A8;   // size 0x4
constexpr uint32_t hierClipRange = 0x006F87AC;   // size 0x4
constexpr uint32_t firstDmaObj = 0x006F87B0;   // size 0x4
constexpr uint32_t ldmaVu1 = 0x006F87B4;   // size 0x4
constexpr uint32_t dmaAlphaVu1 = 0x006F87B8;   // size 0x4
constexpr uint32_t tracingShadow = 0x006F87BC;   // size 0x4
constexpr uint32_t shadowLodRangeSq = 0x006F87C0;   // size 0x4
constexpr uint32_t world = 0x006F87C4;   // size 0x4
constexpr uint32_t gCs = 0x006F87C8;   // size 0x4
constexpr uint32_t g_CameraLos = 0x006F87CC;   // size 0x4
constexpr uint32_t g_CameraLosPerView = 0x006F87D0;   // size 0x4
constexpr uint32_t interactiveState = 0x006F87D4;   // size 0x4
constexpr uint32_t lensFlareListPtr = 0x006F87D8;   // size 0x4
constexpr uint32_t lensFlareListIndex = 0x006F87DC;   // size 0x4
constexpr uint32_t iBookmark = 0x006F87E0;   // size 0x4
constexpr uint32_t texmNumTextures = 0x006F87E4;   // size 0x4
constexpr uint32_t texBins = 0x006F87E8;   // size 0x4
constexpr uint32_t texLastUsedCnt = 0x006F87EC;   // size 0x4
constexpr uint32_t nextVramAddr = 0x006F87F0;   // size 0x4
constexpr uint32_t startVramAddr = 0x006F87F4;   // size 0x4
constexpr uint32_t displayBuffer = 0x006F87F8;   // size 0x4
constexpr uint32_t movieBuffer = 0x006F87FC;   // size 0x4
constexpr uint32_t lastVramTexAddr = 0x006F8800;   // size 0x4
constexpr uint32_t loadingShellTex = 0x006F8804;   // size 0x4
constexpr uint32_t texmK = 0x006F8808;   // size 0x4
constexpr uint32_t texmL = 0x006F880C;   // size 0x4
constexpr uint32_t texmAdjustId = 0x006F8810;   // size 0x4
constexpr uint32_t texmFilter = 0x006F8814;   // size 0x4
constexpr uint32_t tempVramTexAddr = 0x006F8818;   // size 0x4
constexpr uint32_t psZoomT = 0x006F881C;   // size 0x4
constexpr uint32_t psShadowIntens = 0x006F8820;   // size 0x4
constexpr uint32_t psShowObjects = 0x006F8824;   // size 0x4
constexpr uint32_t psBlkIdx = 0x006F8828;   // size 0x4
constexpr uint32_t psViewCs = 0x006F882C;   // size 0x4
constexpr uint32_t psBufNum = 0x006F8830;   // size 0x4
constexpr uint32_t psObjectCnt = 0x006F8834;   // size 0x4
constexpr uint32_t sg_do2Circuit = 0x006F8838;   // size 0x4
constexpr uint32_t sg_doHalfTex = 0x006F883C;   // size 0x4
constexpr uint32_t movieRequested = 0x006F8840;   // size 0x4
constexpr uint32_t movieIsCompletelyFinished = 0x006F8844;   // size 0x4
constexpr uint32_t AlmostInVram = 0x006F8848;   // size 0x4
constexpr uint32_t IsInVram = 0x006F884C;   // size 0x4
constexpr uint32_t voBufData = 0x006F8850;   // size 0x4
constexpr uint32_t voBufTag = 0x006F8854;   // size 0x4
constexpr uint32_t viBufData = 0x006F8858;   // size 0x4
constexpr uint32_t readBuf = 0x006F885C;   // size 0x4
constexpr uint32_t mpegWork = 0x006F8860;   // size 0x4
constexpr uint32_t isWithAudio = 0x006F8864;   // size 0x4
constexpr uint32_t readrest = 0x006F8868;   // size 0x4
constexpr uint32_t writerest = 0x006F8870;   // size 0x4
constexpr uint32_t isStarted = 0x006F8874;   // size 0x4
constexpr uint32_t isPaused = 0x006F8878;   // size 0x4
constexpr uint32_t flushedBuffers = 0x006F887C;   // size 0x4
constexpr uint32_t videoDecTh = 0x006F8880;   // size 0x4
constexpr uint32_t defaultTh = 0x006F8884;   // size 0x4
constexpr uint32_t bank = 0x006F8888;   // size 0x4
constexpr uint32_t put_ptr = 0x006F888C;   // size 0x4
constexpr uint32_t get_ptr = 0x006F8890;   // size 0x4
constexpr uint32_t putsize = 0x006F8894;   // size 0x4
constexpr uint32_t getsize = 0x006F8898;   // size 0x4
constexpr uint32_t controller_val = 0x006F889C;   // size 0x4
constexpr uint32_t originalTexId = 0x006F88A0;   // size 0x4
constexpr uint32_t originalNumTextures = 0x006F88A4;   // size 0x4
constexpr uint32_t gO = 0x006F88A8;   // size 0x4
constexpr uint32_t alreadyPlaying = 0x006F88AC;   // size 0x4
constexpr uint32_t ScreenObjectPending = 0x006F88B0;   // size 0x4
constexpr uint32_t gMovObj = 0x006F88B4;   // size 0x4
constexpr uint32_t gMovWid = 0x006F88B8;   // size 0x4
constexpr uint32_t gMovHei = 0x006F88BC;   // size 0x4
constexpr uint32_t movieFrameCounter = 0x006F88C0;   // size 0x4
constexpr uint32_t moviePreFrameCounter = 0x006F88C4;   // size 0x4
constexpr uint32_t CurrentMovieScreen = 0x006F88C8;   // size 0x4
constexpr uint32_t seed = 0x006F88D0;   // size 0x8
constexpr uint32_t firstTimeAround = 0x006F88D8;   // size 0x4
constexpr uint32_t texIndex = 0x006F88DC;   // size 0x4
constexpr uint32_t count_006F88E0 = 0x006F88E0;   // size 0x4
constexpr uint32_t numActiveFires = 0x006F88E4;   // size 0x4
constexpr uint32_t displayListTop = 0x006F88E8;   // size 0x4
constexpr uint32_t fxActiveTop = 0x006F88EC;   // size 0x4
constexpr uint32_t fxInactiveTop = 0x006F88F0;   // size 0x4
constexpr uint32_t lensFlareIndex = 0x006F88F8;   // size 0x8
constexpr uint32_t lensFlareList = 0x006F8900;   // size 0x8
constexpr uint32_t lensFlarePacket = 0x006F8908;   // size 0x8
constexpr uint32_t splats = 0x006F8910;   // size 0x4
constexpr uint32_t paraLightColor1 = 0x006F8918;   // size 0x4
constexpr uint32_t paraLightColor2 = 0x006F8920;   // size 0x4
constexpr uint32_t paraLightColor3 = 0x006F8928;   // size 0x4
constexpr uint32_t doParaLights = 0x006F892C;   // size 0x4
constexpr uint32_t visualizeLights = 0x006F8930;   // size 0x4
constexpr uint32_t visLength = 0x006F8934;   // size 0x4
constexpr uint32_t specularColor = 0x006F8938;   // size 0x4
constexpr uint32_t s_allocatedBufferSpace = 0x006F8958;   // size 0x4
constexpr uint32_t s_numSourceObjects = 0x006F895C;   // size 0x4
constexpr uint32_t nextSector = 0x006F8968;   // size 0x4
constexpr uint32_t finalSector = 0x006F896C;   // size 0x4
constexpr uint32_t isStrFileInit = 0x006F8970;   // size 0x4
constexpr uint32_t partialMovie = 0x006F8974;   // size 0x4
constexpr uint32_t mode = 0x006F8990;   // size 0x4
constexpr uint32_t pkt = 0x006F8998;   // size 0x4
constexpr uint32_t GIFstart = 0x006F899C;   // size 0x4
constexpr uint32_t GIFeopBit = 0x006F89A0;   // size 0x4
constexpr uint32_t GIFnumRegs = 0x006F89A4;   // size 0x4
constexpr uint32_t DMAbegin = 0x006F89A8;   // size 0x4
constexpr uint32_t DMAlink = 0x006F89AC;   // size 0x4
constexpr uint32_t DIRECTstart = 0x006F89B0;   // size 0x4
constexpr uint32_t TestReg = 0x006F89B4;   // size 0x4
constexpr uint32_t todTimeOfDayOn = 0x006F89B8;   // size 0x4
constexpr uint32_t todSetTimeOfDay = 0x006F89BC;   // size 0x4
constexpr uint32_t todTimeOfDayMinutesPerSecond = 0x006F89C0;   // size 0x4
constexpr uint32_t todTimeOfDay = 0x006F89C4;   // size 0x4
constexpr uint32_t todFrame = 0x006F89C8;   // size 0x4
constexpr uint32_t todAddFrame = 0x006F89CC;   // size 0x4
constexpr uint32_t todDeleteFrame = 0x006F89D0;   // size 0x4
constexpr uint32_t startTOD_144 = 0x006F89D4;   // size 0x4
constexpr uint32_t x__tmp_0_146 = 0x006F89D8;   // size 0x4
constexpr uint32_t todAmbientRed = 0x006F89DC;   // size 0x4
constexpr uint32_t todAmbientGreen = 0x006F89E0;   // size 0x4
constexpr uint32_t todAmbientBlue = 0x006F89E4;   // size 0x4
constexpr uint32_t todDirHeading = 0x006F89E8;   // size 0x4
constexpr uint32_t todDirPitch = 0x006F89EC;   // size 0x4
constexpr uint32_t todDirRed = 0x006F89F0;   // size 0x4
constexpr uint32_t todDirGreen = 0x006F89F4;   // size 0x4
constexpr uint32_t todDirBlue = 0x006F89F8;   // size 0x4
constexpr uint32_t todBackHeading = 0x006F89FC;   // size 0x4
constexpr uint32_t todBackPitch = 0x006F8A00;   // size 0x4
constexpr uint32_t todBackIntensity = 0x006F8A04;   // size 0x4
constexpr uint32_t todBackRed = 0x006F8A08;   // size 0x4
constexpr uint32_t todBackGreen = 0x006F8A0C;   // size 0x4
constexpr uint32_t todBackBlue = 0x006F8A10;   // size 0x4
constexpr uint32_t todFogMinRange = 0x006F8A14;   // size 0x4
constexpr uint32_t todFogMaxRange = 0x006F8A18;   // size 0x4
constexpr uint32_t todFogMaxVal = 0x006F8A1C;   // size 0x4
constexpr uint32_t todFogFarClip = 0x006F8A20;   // size 0x4
constexpr uint32_t todFogRed = 0x006F8A24;   // size 0x4
constexpr uint32_t todFogGreen = 0x006F8A28;   // size 0x4
constexpr uint32_t todFogBlue = 0x006F8A2C;   // size 0x4
constexpr uint32_t todBackgroundRed = 0x006F8A30;   // size 0x4
constexpr uint32_t todBackgroundGreen = 0x006F8A34;   // size 0x4
constexpr uint32_t todBackgroundBlue = 0x006F8A38;   // size 0x4
constexpr uint32_t todSkyTarget = 0x006F8A3C;   // size 0x4
constexpr uint32_t todSkyUMod = 0x006F8A40;   // size 0x4
constexpr uint32_t todSkyVMod = 0x006F8A44;   // size 0x4
constexpr uint32_t todSkyFogBurnThru = 0x006F8A48;   // size 0x4
constexpr uint32_t todSkyRedMod = 0x006F8A4C;   // size 0x4
constexpr uint32_t todSkyGreenMod = 0x006F8A50;   // size 0x4
constexpr uint32_t todSkyBlueMod = 0x006F8A54;   // size 0x4
constexpr uint32_t todCloudRedMod = 0x006F8A58;   // size 0x4
constexpr uint32_t todCloudGreenMod = 0x006F8A5C;   // size 0x4
constexpr uint32_t todCloudBlueMod = 0x006F8A60;   // size 0x4
constexpr uint32_t todCloudAlphaMod = 0x006F8A64;   // size 0x4
constexpr uint32_t todNumSkys = 0x006F8A68;   // size 0x4
constexpr uint32_t todSkySetFound = 0x006F8A6C;   // size 0x4
constexpr uint32_t whichBuildBar = 0x006F8A70;   // size 0x4
constexpr uint32_t gWhichMacroSection = 0x006F8A74;   // size 0x4
constexpr uint32_t gWhichMicroSection = 0x006F8A78;   // size 0x4
constexpr uint32_t gOkToDrawProgressBar = 0x006F8A7C;   // size 0x4
constexpr uint32_t viewTimeOfDay = 0x006F8A88;   // size 0x4
constexpr uint32_t gUseUnifiedView = 0x006F8A8C;   // size 0x4
constexpr uint32_t viewFovH = 0x006F8A90;   // size 0x4
constexpr uint32_t viewDegrees = 0x006F8A94;   // size 0x4
constexpr uint32_t viewBgR = 0x006F8A98;   // size 0x4
constexpr uint32_t viewBgG = 0x006F8A9C;   // size 0x4
constexpr uint32_t viewBgB = 0x006F8AA0;   // size 0x4
constexpr uint32_t viewBgA = 0x006F8AA4;   // size 0x4
constexpr uint32_t viewTODBgR = 0x006F8AA8;   // size 0x4
constexpr uint32_t viewTODBgG = 0x006F8AAC;   // size 0x4
constexpr uint32_t viewTODBgB = 0x006F8AB0;   // size 0x4
constexpr uint32_t viewTODBgA = 0x006F8AB4;   // size 0x4
constexpr uint32_t viewAmbientRed = 0x006F8AB8;   // size 0x4
constexpr uint32_t viewAmbientGreen = 0x006F8ABC;   // size 0x4
constexpr uint32_t viewAmbientBlue = 0x006F8AC0;   // size 0x4
constexpr uint32_t viewTODAmbientRed = 0x006F8AC4;   // size 0x4
constexpr uint32_t viewTODAmbientGreen = 0x006F8AC8;   // size 0x4
constexpr uint32_t viewTODAmbientBlue = 0x006F8ACC;   // size 0x4
constexpr uint32_t viewAnimAmbientRed = 0x006F8AD0;   // size 0x4
constexpr uint32_t viewAnimAmbientGreen = 0x006F8AD4;   // size 0x4
constexpr uint32_t viewAnimAmbientBlue = 0x006F8AD8;   // size 0x4
constexpr uint32_t viewAmbientChanged = 0x006F8ADC;   // size 0x4
constexpr uint32_t viewNumViews = 0x006F8AE0;   // size 0x4
constexpr uint32_t viewCurView = 0x006F8AE4;   // size 0x4
constexpr uint32_t viewRearViewConfig = 0x006F8AE8;   // size 0x4
constexpr uint32_t viewRearLargeActive = 0x006F8AEC;   // size 0x4
constexpr uint32_t whichHalfMeg = 0x006F8AF0;   // size 0x4
constexpr uint32_t G_windowPos = 0x006F8AF4;   // size 0x4
constexpr uint32_t G_bitCount = 0x006F8AF8;   // size 0x4
constexpr uint32_t G_bitBucket = 0x006F8AFC;   // size 0x4
constexpr uint32_t G_crc32val = 0x006F8B00;   // size 0x8
constexpr uint32_t G_fixedTlen = 0x006F8B08;   // size 0x4
constexpr uint32_t G_FileName = 0x006F8B0C;   // size 0x4
constexpr uint32_t G_FileAddr = 0x006F8B10;   // size 0x4
constexpr uint32_t outFileWindow = 0x006F8B18;   // size 0x4
constexpr uint32_t tmpLong = 0x006F8B1C;   // size 0x4
constexpr uint32_t G_outSize = 0x006F8B20;   // size 0x8
constexpr uint32_t G_fixedTdist = 0x006F8B28;   // size 0x4
constexpr uint32_t G_fixedBlen = 0x006F8B2C;   // size 0x4
constexpr uint32_t G_fixedBdist = 0x006F8B30;   // size 0x4
constexpr uint32_t howManyBlocks = 0x006F8B34;   // size 0x4
constexpr uint32_t zipFileBuf = 0x006F8B38;   // size 0x4
constexpr uint32_t outFileBuf = 0x006F8B3C;   // size 0x4
constexpr uint32_t gRet = 0x006F8B40;   // size 0x4
constexpr uint32_t x_fbss = 0x006F8B80;
constexpr uint32_t x_12Interactives_s_numInteractives = 0x006F8DF8;   // size 0x4
constexpr uint32_t x__ti13AiActionTuple = 0x006F8E00;   // size 0x8
constexpr uint32_t x__ti12AiActionList = 0x006F8E08;   // size 0x8
constexpr uint32_t x__ti12PointToolKit = 0x006F8E10;   // size 0x8
constexpr uint32_t x__ti8resetcom = 0x006F8E18;   // size 0x8
constexpr uint32_t x__ti13DbInteractive = 0x006F8E20;   // size 0x8
constexpr uint32_t x_7Cameras_m_ticsAtUnifiedInit = 0x006F8E28;   // size 0x4
constexpr uint32_t x_7Cameras_m_leaveTriggered = 0x006F8E2C;   // size 0x4
constexpr uint32_t x_7Cameras_m_numCameras = 0x006F8E30;   // size 0x4
constexpr uint32_t x_7Cameras_m_currentFadeAlpha = 0x006F8E34;   // size 0x4
constexpr uint32_t x_7Cameras_m_leaveReason = 0x006F8E38;   // size 0x4
constexpr uint32_t x_7Cameras_m_animDisableSwitch = 0x006F8E3C;   // size 0x4
constexpr uint32_t x_9HomingBug_s_homingFactor = 0x006F8E40;   // size 0x4
constexpr uint32_t x_9HomingBug_s_avoidanceFactor = 0x006F8E44;   // size 0x4
constexpr uint32_t x_9HomingBug_s_maxForce = 0x006F8E48;   // size 0x4
constexpr uint32_t x_9HomingBug_s_maxVel = 0x006F8E4C;   // size 0x4
constexpr uint32_t x_9HomingBug_s_initialVel = 0x006F8E50;   // size 0x4
constexpr uint32_t x_9HomingBug_s_initialHealth = 0x006F8E54;   // size 0x4
constexpr uint32_t x_9HomingBug_s_attachRange = 0x006F8E58;   // size 0x4
constexpr uint32_t x_9HomingBug_s_attachMinAngle = 0x006F8E5C;   // size 0x4
constexpr uint32_t x_9HomingBug_s_attachFields = 0x006F8E60;   // size 0x4
constexpr uint32_t x_9HomingBug_s_maxSeekFields = 0x006F8E64;   // size 0x4
constexpr uint32_t x_9HomingBug_s_headRadius = 0x006F8E68;   // size 0x4
constexpr uint32_t x_9HomingBug_s_feelerRadius = 0x006F8E6C;   // size 0x4
constexpr uint32_t x_9HomingBug_s_collisRadius = 0x006F8E70;   // size 0x4
constexpr uint32_t x_9HomingBug_s_damageRadius = 0x006F8E74;   // size 0x4
constexpr uint32_t x_9HomingBug_s_damagePerSecond = 0x006F8E78;   // size 0x4
constexpr uint32_t x_9HomingBug_s_maxMonsterDamage = 0x006F8E7C;   // size 0x4
constexpr uint32_t x_9HomingBug_s_targetLockMaxRange = 0x006F8E80;   // size 0x4
constexpr uint32_t x_9HomingBug_s_targetLockMinAngle = 0x006F8E84;   // size 0x4
constexpr uint32_t x_6Debris_m_damageMod = 0x006F8E88;   // size 0x4
constexpr uint32_t x_6Debris_m_damageThreshold = 0x006F8E8C;   // size 0x4
constexpr uint32_t x__ti12MonsterState = 0x006F8E90;   // size 0x8
constexpr uint32_t x_12LevelPickups_s_militaryStillAlive = 0x006F8E98;   // size 0x4
constexpr uint32_t x_9HomingBug_s_targetLockDotMin = 0x006F8E9C;   // size 0x4
constexpr uint32_t x__ti6McTask = 0x006F8EA0;   // size 0x8
constexpr uint32_t x__ti6McPage = 0x006F8EA8;   // size 0x8
constexpr uint32_t x__ti17MilitaryFormation = 0x006F8EB0;   // size 0x8
constexpr uint32_t x__ti20PunchSwipeConfigBase = 0x006F8EB8;   // size 0x8
constexpr uint32_t x_12LevelPickups_s_colorInc = 0x006F8EC0;   // size 0x1
constexpr uint32_t x_12LevelPickups_s_color = 0x006F8EC2;   // size 0x2
constexpr uint32_t x_12LevelPickups_s_green = 0x006F8EC4;   // size 0x2
constexpr uint32_t x_12LevelPickups_s_highlight = 0x006F8EC8;   // size 0x8
constexpr uint32_t x_11ShellRtLoop_m_oddEven = 0x006F8ED0;   // size 0x4
constexpr uint32_t x__ti14CScreenSection = 0x006F8ED8;   // size 0x8
constexpr uint32_t x__ti7CScreen = 0x006F8EE0;   // size 0x8
constexpr uint32_t x__ti9exception = 0x006F8EE8;   // size 0x8
constexpr uint32_t x__ti9type_info = 0x006F8EF0;   // size 0x8
constexpr uint32_t x_errno = 0x006F8EF8;   // size 0x4
constexpr uint32_t viBufTag = 0x0077A280;   // size 0x1010
constexpr uint32_t defStack = 0x0077B2C0;   // size 0x800
constexpr uint32_t videoDecStack = 0x0077BAC0;   // size 0x4000
constexpr uint32_t audioBuff = 0x0077FAC0;   // size 0xC000
constexpr uint32_t x_11DelayAction_m_pool = 0x007BBED0;   // size 0x1C00
constexpr uint32_t x_8PowerUps_instance = 0x007BDAD0;   // size 0x2718
constexpr uint32_t x_7Cameras_m_cameras = 0x007C01F0;   // size 0x3AC0
constexpr uint32_t x_9AiPathNet_monster = 0x007C3CB0;   // size 0x5B80
constexpr uint32_t x__ti18AiThrashActionList = 0x007C9830;   // size 0xC
constexpr uint32_t x__ti20AiGrappledActionList = 0x007C9840;   // size 0xC
constexpr uint32_t x__ti21AiGrapplingActionList = 0x007C9850;   // size 0xC
constexpr uint32_t x__ti14AiGrappleThrow = 0x007C9860;   // size 0xC
constexpr uint32_t x__ti15AiGrappleAttack = 0x007C9870;   // size 0xC
constexpr uint32_t x__ti10AiPathTool = 0x007C9880;   // size 0xC
constexpr uint32_t x__ti13AiPunchReflex = 0x007C9890;   // size 0xC
constexpr uint32_t x__ti11AiBatReflex = 0x007C98A0;   // size 0xC
constexpr uint32_t x__ti16AiFireProjectile = 0x007C98B0;   // size 0xC
constexpr uint32_t x__ti15AiGrappleReflex = 0x007C98C0;   // size 0xC
constexpr uint32_t x__ti13AiBlockReflex = 0x007C98D0;   // size 0xC
constexpr uint32_t x__ti15AiCounterReflex = 0x007C98E0;   // size 0xC
constexpr uint32_t x__ti14AiThrashReflex = 0x007C98F0;   // size 0xC
constexpr uint32_t x__ti15AiDisposeReflex = 0x007C9900;   // size 0xC
constexpr uint32_t x__ti13AiSmashReflex = 0x007C9910;   // size 0xC
constexpr uint32_t x__ti14AiThrowRecover = 0x007C9920;   // size 0xC
constexpr uint32_t x__ti13AiThrowPickup = 0x007C9930;   // size 0xC
constexpr uint32_t x__ti13AiStompReflex = 0x007C9940;   // size 0xC
constexpr uint32_t x__ti11AiRamAttack = 0x007C9950;   // size 0xC
constexpr uint32_t x__ti14AiCrowdControl = 0x007C9960;   // size 0xC
constexpr uint32_t x__ti15AiSpecialAttack = 0x007C9970;   // size 0xC
constexpr uint32_t x__ti14AiDetonateHead = 0x007C9980;   // size 0xC
constexpr uint32_t x__ti7AiTaunt = 0x007C9990;   // size 0xC
constexpr uint32_t x__ti12AiPutOutFire = 0x007C99A0;   // size 0xC
constexpr uint32_t x__ti7AiReset = 0x007C99B0;   // size 0xC
constexpr uint32_t x__ti12AiSeekHealth = 0x007C99C0;   // size 0xC
constexpr uint32_t x__ti13AiSeekStamina = 0x007C99D0;   // size 0xC
constexpr uint32_t x__ti13AiSeekSpecial = 0x007C99E0;   // size 0xC
constexpr uint32_t x__ti11AiSeekCloak = 0x007C99F0;   // size 0xC
constexpr uint32_t x__ti12AiSeekPickup = 0x007C9A00;   // size 0xC
constexpr uint32_t x__ti13AiSeekMonster = 0x007C9A10;   // size 0xC
constexpr uint32_t x__ti7AiSwarm = 0x007C9A20;   // size 0xC
constexpr uint32_t x__ti12AiDodgeThrow = 0x007C9A30;   // size 0xC
constexpr uint32_t x__ti12AiCatchThrow = 0x007C9A40;   // size 0xC
constexpr uint32_t x__ti10AiBatThrow = 0x007C9A50;   // size 0xC
constexpr uint32_t x__ti10AiDodgeRam = 0x007C9A60;   // size 0xC
constexpr uint32_t x__ti12AiDodgeStomp = 0x007C9A70;   // size 0xC
constexpr uint32_t x__ti14AiDodgeSpecial = 0x007C9A80;   // size 0xC
constexpr uint32_t x__ti8Airplane = 0x007C9A90;   // size 0xC
constexpr uint32_t x_12BigShotLevel_instance = 0x007C9AA0;   // size 0xEC
constexpr uint32_t x_12LevelPickups_s_info = 0x007C9B90;   // size 0x930
constexpr uint32_t x__ti9CarPickup = 0x007CA4C0;   // size 0xC
constexpr uint32_t x_7PathNet_fodder = 0x007CA4D0;   // size 0x3010
constexpr uint32_t x__ti6Pickup = 0x007CD4E0;   // size 0xC
constexpr uint32_t x__ti9charreset = 0x007CD4F0;   // size 0xC
constexpr uint32_t x_10CrushLevel_instance = 0x007CD500;   // size 0x14
constexpr uint32_t x__ti12Destructible = 0x007CD518;   // size 0xC
constexpr uint32_t x_8HitEvent_s_takeHitInfo = 0x007CD528;   // size 0x40
constexpr uint32_t x_9EnemyInfo_s_info = 0x007CD570;   // size 0x2A30
constexpr uint32_t x__ti9FinalBoss = 0x007CFFA0;   // size 0xC
constexpr uint32_t x_8PedGroup_s_allPedGroups = 0x007CFFB0;   // size 0x50
constexpr uint32_t x_7PathNet_ped = 0x007D0000;   // size 0x3010
constexpr uint32_t x_11StartPoints_m_instance = 0x007D3010;   // size 0x560
constexpr uint32_t x__ti11GlobalReset = 0x007D3570;   // size 0xC
constexpr uint32_t x__ti12StateGrapple = 0x007D3580;   // size 0xC
constexpr uint32_t x__ti13StateGrappled = 0x007D3590;   // size 0xC
constexpr uint32_t x__ti20StateGrappleOHAttack = 0x007D35A0;   // size 0xC
constexpr uint32_t x__ti19StateMonkeyOHAttack = 0x007D35B0;   // size 0xC
constexpr uint32_t x__ti11AttackState = 0x007D35C0;   // size 0xC
constexpr uint32_t x__ti16StateGrappleLift = 0x007D35D0;   // size 0xC
constexpr uint32_t x__ti17StateGrappleThrow = 0x007D35E0;   // size 0xC
constexpr uint32_t x__ti18StateGrappleThrow1 = 0x007D35F0;   // size 0xC
constexpr uint32_t x__ti10HeliPickup = 0x007D3600;   // size 0xC
constexpr uint32_t x__ti9HomingBug = 0x007D3610;   // size 0xC
constexpr uint32_t x_12Interactives_s_dbInteractive = 0x007D3620;   // size 0x1000
constexpr uint32_t x__ti9McMessage = 0x007D4620;   // size 0xC
constexpr uint32_t x__ti10McLoadFile = 0x007D4630;   // size 0xC
constexpr uint32_t x__ti10McSaveFile = 0x007D4640;   // size 0xC
constexpr uint32_t x__ti19McListRootDirectory = 0x007D4650;   // size 0xC
constexpr uint32_t x__ti15McDoesFileExist = 0x007D4660;   // size 0xC
constexpr uint32_t x__ti13McUpdateState = 0x007D4670;   // size 0xC
constexpr uint32_t x__ti8McFormat = 0x007D4680;   // size 0xC
constexpr uint32_t x__ti14McScrambleIcon = 0x007D4690;   // size 0xC
constexpr uint32_t x__ti14McSetupGameDir = 0x007D46A0;   // size 0xC
constexpr uint32_t x_6McPage_s_selectedFont = 0x007D46B0;   // size 0x28
constexpr uint32_t x_6McPage_s_unselectedFont = 0x007D46D8;   // size 0x28
constexpr uint32_t x_6McPage_s_giantFont = 0x007D4700;   // size 0x28
constexpr uint32_t x_6McPage_s_menuFont = 0x007D4728;   // size 0x28
constexpr uint32_t x_6McPage_s_titleFont = 0x007D4750;   // size 0x28
constexpr uint32_t x_6McPage_s_bottomFont = 0x007D4778;   // size 0x28
constexpr uint32_t x_6McPage_s_font = 0x007D47A0;   // size 0x28
constexpr uint32_t x__ti13LineFormation = 0x007D47C8;   // size 0xC
constexpr uint32_t x__ti12BoxFormation = 0x007D47D8;   // size 0xC
constexpr uint32_t x__ti14MilitaryPickup = 0x007D47E8;   // size 0xC
constexpr uint32_t x__ti18MissileTruckPickup = 0x007D47F8;   // size 0xC
constexpr uint32_t x__ti9StateIdle = 0x007D4808;   // size 0xC
constexpr uint32_t x__tiQ213StateBatSwipe11SwipeConfig = 0x007D4818;   // size 0xC
constexpr uint32_t x__ti13StateBatSwipe = 0x007D4828;   // size 0xC
constexpr uint32_t x__ti10StateBlock = 0x007D4838;   // size 0xC
constexpr uint32_t x__ti10StateCatch = 0x007D4848;   // size 0xC
constexpr uint32_t x__ti12StateCounter = 0x007D4858;   // size 0xC
constexpr uint32_t x__ti14StateCountered = 0x007D4868;   // size 0xC
constexpr uint32_t x__ti10StateDeath = 0x007D4878;   // size 0xC
constexpr uint32_t x__ti16StateGetupAttack = 0x007D4888;   // size 0xC
constexpr uint32_t x__ti14StateGunAttack = 0x007D4898;   // size 0xC
constexpr uint32_t x__ti12StateImpaled = 0x007D48A8;   // size 0xC
constexpr uint32_t x__ti12StateJavelin = 0x007D48B8;   // size 0xC
constexpr uint32_t x__ti11StatePickUp = 0x007D48C8;   // size 0xC
constexpr uint32_t x__tiQ210StatePunch11PunchConfig = 0x007D48D8;   // size 0xC
constexpr uint32_t x__ti10StatePunch = 0x007D48E8;   // size 0xC
constexpr uint32_t x__ti11StateRecoil = 0x007D48F8;   // size 0xC
constexpr uint32_t x__ti12StateStunned = 0x007D4908;   // size 0xC
constexpr uint32_t x__ti10StateTaunt = 0x007D4918;   // size 0xC
constexpr uint32_t x__ti10StateThrow = 0x007D4928;   // size 0xC
constexpr uint32_t x__ti19StateTwoHandedThrow = 0x007D4938;   // size 0xC
constexpr uint32_t x__ti12StateShocked = 0x007D4948;   // size 0xC
constexpr uint32_t x__ti16StateStompAttack = 0x007D4958;   // size 0xC
constexpr uint32_t x__ti16StateTazerAttack = 0x007D4968;   // size 0xC
constexpr uint32_t x__ti17StateShieldAttack = 0x007D4978;   // size 0xC
constexpr uint32_t x__ti15StateBigTakeHit = 0x007D4988;   // size 0xC
constexpr uint32_t x__ti12StateVictory = 0x007D4998;   // size 0xC
constexpr uint32_t x__ti7Monster = 0x007D49A8;   // size 0xC
constexpr uint32_t x__ti10McBootPage = 0x007D49B8;   // size 0xC
constexpr uint32_t x__ti20McCheckForSweetTooth = 0x007D49C8;   // size 0xC
constexpr uint32_t x__ti18McCheckForSettings = 0x007D49D8;   // size 0xC
constexpr uint32_t x__ti14McLoadSettings = 0x007D49E8;   // size 0xC
constexpr uint32_t x__ti14McSaveSettings = 0x007D49F8;   // size 0xC
constexpr uint32_t x__ti12McChooseSlot = 0x007D4A08;   // size 0xC
constexpr uint32_t x__ti12McCheckSpace = 0x007D4A18;   // size 0xC
constexpr uint32_t x__ti13McCheckFormat = 0x007D4A28;   // size 0xC
constexpr uint32_t x__ti16McCheckOverwrite = 0x007D4A38;   // size 0xC
constexpr uint32_t x__ti15McPrepBoxScreen = 0x007D4A48;   // size 0xC
constexpr uint32_t x__ti15McLoadGameBoxes = 0x007D4A58;   // size 0xC
constexpr uint32_t x__ti12McSelectReel = 0x007D4A68;   // size 0xC
constexpr uint32_t x__ti15McSaveAdventure = 0x007D4A78;   // size 0xC
constexpr uint32_t x__ti20McSaveAdventureBlank = 0x007D4A88;   // size 0xC
constexpr uint32_t x__ti16McSelectSaveSlot = 0x007D4A98;   // size 0xC
constexpr uint32_t x__ti11McAskToSave = 0x007D4AA8;   // size 0xC
constexpr uint32_t x__ti11McAskIfSure = 0x007D4AB8;   // size 0xC
constexpr uint32_t x__ti11McErrorPage = 0x007D4AC8;   // size 0xC
constexpr uint32_t x__ti13McCardRemoved = 0x007D4AD8;   // size 0xC
constexpr uint32_t x__ti13StateButtSlam = 0x007D4AE8;   // size 0xC
constexpr uint32_t x__ti10StateClimb = 0x007D4AF8;   // size 0xC
constexpr uint32_t x__ti9StateDash = 0x007D4B08;   // size 0xC
constexpr uint32_t x__ti11JumpFlyBase = 0x007D4B18;   // size 0xC
constexpr uint32_t x__ti8StateFly = 0x007D4B28;   // size 0xC
constexpr uint32_t x__ti9StateJump = 0x007D4B38;   // size 0xC
constexpr uint32_t x__ti14StateRamAttack = 0x007D4B48;   // size 0xC
constexpr uint32_t x__ti8StateRun = 0x007D4B58;   // size 0xC
constexpr uint32_t x__ti8PathTool = 0x007D4B68;   // size 0xC
constexpr uint32_t x__ti13PlantBossHead = 0x007D4B78;   // size 0xC
constexpr uint32_t x__ti13PlantBossRoot = 0x007D4B88;   // size 0xC
constexpr uint32_t x__ti13PlantBossBase = 0x007D4B98;   // size 0xC
constexpr uint32_t x__ti11PowerUpTool = 0x007D4BA8;   // size 0xC
constexpr uint32_t x__ti5reset = 0x007D4BB8;   // size 0xC
constexpr uint32_t x__ti11RigidPickup = 0x007D4BC8;   // size 0xC
constexpr uint32_t x__ti14StateAirStrike = 0x007D4BD8;   // size 0xC
constexpr uint32_t x__ti14StateBugAttack = 0x007D4BE8;   // size 0xC
constexpr uint32_t x__ti17StateCrowdControl = 0x007D4BF8;   // size 0xC
constexpr uint32_t x__ti15StateFireBreath = 0x007D4C08;   // size 0xC
constexpr uint32_t x__ti16StateGrappleHook = 0x007D4C18;   // size 0xC
constexpr uint32_t x__ti14StateLavaBlast = 0x007D4C28;   // size 0xC
constexpr uint32_t x__ti17StateRobotSpecial = 0x007D4C38;   // size 0xC
constexpr uint32_t x__ti16StateRockSpecial = 0x007D4C48;   // size 0xC
constexpr uint32_t x__ti14StateSonicRoar = 0x007D4C58;   // size 0xC
constexpr uint32_t x__ti14StateZapAttack = 0x007D4C68;   // size 0xC
constexpr uint32_t x__ti12StateTopSpin = 0x007D4C78;   // size 0xC
constexpr uint32_t x__ti16StateCannonHands = 0x007D4C88;   // size 0xC
constexpr uint32_t x__ti15StateUltraTazer = 0x007D4C98;   // size 0xC
constexpr uint32_t x__ti14StartPointTool = 0x007D4CA8;   // size 0xC
constexpr uint32_t x__ti12SubwayPickup = 0x007D4CB8;   // size 0xC
constexpr uint32_t x__ti10TankPickup = 0x007D4CC8;   // size 0xC
constexpr uint32_t x__ti12TurretPickup = 0x007D4CD8;   // size 0xC
constexpr uint32_t x__ti14StateThrowBack = 0x007D4CE8;   // size 0xC
constexpr uint32_t x__ti17StateBeingSlammed = 0x007D4CF8;   // size 0xC
constexpr uint32_t x__ti16StateBeingThrown = 0x007D4D08;   // size 0xC
constexpr uint32_t x__ti17StateGrappleBreak = 0x007D4D18;   // size 0xC
constexpr uint32_t x__ti14StateKnockBack = 0x007D4D28;   // size 0xC
constexpr uint32_t x__ti8UfoTokyo = 0x007D4D38;   // size 0xC
constexpr uint32_t x__ti9UfoPickup = 0x007D4D48;   // size 0xC
constexpr uint32_t x__ti18CFunctionalSection = 0x007D4D58;   // size 0xC
constexpr uint32_t x__ti18CHorizontalSection = 0x007D4D68;   // size 0xC
constexpr uint32_t x__ti16CVerticalSection = 0x007D4D78;   // size 0xC
constexpr uint32_t x__ti10CDevScreen = 0x007D4D88;   // size 0xC
constexpr uint32_t x__ti14CNetworkScreen = 0x007D4D98;   // size 0xC
constexpr uint32_t x__ti9bad_alloc = 0x007D4DA8;   // size 0xC
constexpr uint32_t x__ti17__class_type_info = 0x007D4DB8;   // size 0xC
constexpr uint32_t x__ti16__user_type_info = 0x007D4DC8;   // size 0xC
constexpr uint32_t x__ti14__si_type_info = 0x007D4DD8;   // size 0xC
constexpr uint32_t x__ti10bad_typeid = 0x007D4DE8;   // size 0xC
constexpr uint32_t x__ti8bad_cast = 0x007D4DF8;   // size 0xC
constexpr uint32_t x__ti13bad_exception = 0x007D4E08;   // size 0xC
constexpr uint32_t x__ti17__array_type_info = 0x007D4E18;   // size 0xC
constexpr uint32_t x__ti16__ptmd_type_info = 0x007D4E28;   // size 0xC
constexpr uint32_t x__ti16__ptmf_type_info = 0x007D4E38;   // size 0xC
constexpr uint32_t x__ti16__func_type_info = 0x007D4E48;   // size 0xC
constexpr uint32_t x__ti19__builtin_type_info = 0x007D4E58;   // size 0xC
constexpr uint32_t x__ti16__attr_type_info = 0x007D4E68;   // size 0xC
constexpr uint32_t x__ti19__pointer_type_info = 0x007D4E78;   // size 0xC
constexpr uint32_t x_6CsPool_m_csPool = 0x007D4E90;   // size 0x2C000
constexpr uint32_t x_11MemoryStack_globalPool = 0x00800E90;   // size 0x80000
constexpr uint32_t x_9DebugDraw_s_ep = 0x00880E90;   // size 0x80
} // namespace sym

namespace fn {
constexpr uint32_t x_start = 0x00100008;   // size 0xB4
constexpr uint32_t x_exit = 0x001000B8;   // size 0x8
constexpr uint32_t x_root = 0x001000C0;   // size 0x8
constexpr uint32_t stopAllActiveActions__14ActionDispatchv = 0x00100100;   // size 0x38
constexpr uint32_t initBefore__14ActionDispatchv = 0x00100138;   // size 0x5C
constexpr uint32_t initGenericEvent__14ActionDispatchPFUi_v = 0x00100198;   // size 0x8
constexpr uint32_t addUserDefinedScipt__14ActionDispatchP11_hierScript = 0x001001A0;   // size 0x24
constexpr uint32_t callScriptFromId__14ActionDispatchUsUsi = 0x001001C8;   // size 0x98
constexpr uint32_t doRunScript__FP12ActRunScript = 0x00100260;   // size 0x48
constexpr uint32_t doGroup__FP8_fvectorPA3_fP8ActGroupP12Destructible = 0x001002A8;   // size 0x270
constexpr uint32_t doTransform__FP8_fvectorPA3_fP12ActTransformP12Destructible = 0x00100518;   // size 0x28
constexpr uint32_t triggerAnimation__FP11DelayActionP7ActAnim = 0x00100540;   // size 0x1BC
constexpr uint32_t endAnimation__FP7ActAnim = 0x00100700;   // size 0x180
constexpr uint32_t doCamShake__FP8_fvectorP11ActCamShake = 0x00100880;   // size 0x1C
constexpr uint32_t doDomino__FP9ActDomino = 0x001008A0;   // size 0x48
constexpr uint32_t doPickupHat__FP8_fvectorP12ActHatPickup = 0x001008E8;   // size 0x54
constexpr uint32_t doCinema__FP11DelayActionP9ActCinema = 0x00100940;   // size 0x1DC
constexpr uint32_t endCinema__FP11DelayActionP9ActCinema = 0x00100B20;   // size 0x1C0
constexpr uint32_t doSound__FP8_fvectorP12ActPlaySound = 0x00100CE0;   // size 0x5C
constexpr uint32_t doSetPlayerPos__FP8_fvectorRA3_A3_fP12ActPlayerPos = 0x00100D40;   // size 0x98
constexpr uint32_t doAiNavigate__FP15ActAiNavigation = 0x00100DD8;   // size 0x4C
constexpr uint32_t doGenericEvent__FP15ActGenericEvent = 0x00100E28;   // size 0x24
constexpr uint32_t addAction__14ActionDispatchP7ActHeadPA3_fP8_fvectorP12Destructible = 0x00100E50;   // size 0xC8
constexpr uint32_t init__11DelayAction = 0x00100F18;   // size 0x34
constexpr uint32_t getInstance__11DelayAction = 0x00100F50;   // size 0x54
constexpr uint32_t doIt__11DelayAction = 0x00100FA8;   // size 0x1A4
constexpr uint32_t endIt__11DelayAction = 0x00101150;   // size 0x50
constexpr uint32_t fade__11DelayAction = 0x001011A0;   // size 0x30
constexpr uint32_t x__static_initialization_and_destruction_0_001011D0 = 0x001011D0;   // size 0x30
constexpr uint32_t update__11TaskManagerPv = 0x00101200;   // size 0x1C
constexpr uint32_t doIt__11DelayActionPv = 0x00101220;   // size 0x1C
constexpr uint32_t endIt__11DelayActionPv = 0x00101240;   // size 0x1C
constexpr uint32_t fade__11DelayActionPv = 0x00101260;   // size 0x1C
constexpr uint32_t x_GLOBAL__I_moviePlaying = 0x00101280;   // size 0x20
constexpr uint32_t globalInit__2Ai = 0x001012A0;   // size 0x114
constexpr uint32_t globalUpdate__2Ai = 0x001013B8;   // size 0x40
constexpr uint32_t toggleMode__2Ai = 0x001013F8;   // size 0x8
constexpr uint32_t x__2AiR7MonsterR7GamePad = 0x00101400;   // size 0x6C
constexpr uint32_t init__2Ai = 0x00101470;   // size 0x318
constexpr uint32_t updateInputs__2Ai = 0x00101788;   // size 0x130
constexpr uint32_t getGroundHeight__2Ai = 0x001018B8;   // size 0x10
constexpr uint32_t getCommonGroundProbability__2AiR7Monster = 0x001018C8;   // size 0x68
constexpr uint32_t getFovRelevance__2Aiff = 0x00101930;   // size 0x24
constexpr uint32_t getClosestApproach__2AiR13DbInteractivef = 0x00101958;   // size 0x110
constexpr uint32_t getClosestStillApproach__2AiR13DbInteractivef = 0x00101A68;   // size 0xFC
constexpr uint32_t inFlight__2AiR13DbInteractive = 0x00101B68;   // size 0x68
constexpr uint32_t pinningAi__2Ai = 0x00101BD0;   // size 0x38
constexpr uint32_t monitorProjectiles__2Ai = 0x00101C08;   // size 0x8
constexpr uint32_t updatePathNet__2Ai = 0x00101C10;   // size 0x188
constexpr uint32_t targetPin__2Ai = 0x00101D98;   // size 0x18
constexpr uint32_t lightPunch__2Ai = 0x00101DB0;   // size 0x10
constexpr uint32_t heavyPunch__2Ai = 0x00101DC0;   // size 0x10
constexpr uint32_t block__2Ai = 0x00101DD0;   // size 0x10
constexpr uint32_t counter__2Ai = 0x00101DE0;   // size 0x18
constexpr uint32_t fireProjectile__2Ai = 0x00101DF8;   // size 0x10
constexpr uint32_t grab__2Ai = 0x00101E08;   // size 0x10
constexpr uint32_t toss__2Ai = 0x00101E18;   // size 0x10
constexpr uint32_t thrash__2Ai = 0x00101E28;   // size 0x10
constexpr uint32_t buttStomp__2Ai = 0x00101E38;   // size 0x18
constexpr uint32_t jump__2Ai = 0x00101E50;   // size 0x10
constexpr uint32_t crowdControl__2Ai = 0x00101E60;   // size 0x18
constexpr uint32_t specialAttack__2Ai = 0x00101E78;   // size 0x18
constexpr uint32_t taunt__2Ai = 0x00101E90;   // size 0x10
constexpr uint32_t getReflexDelay__2Ai = 0x00101EA0;   // size 0x20
constexpr uint32_t getButtonMashDelay__2Ai = 0x00101EC0;   // size 0x20
constexpr uint32_t getPowerUpGrabRange__2Ai = 0x00101EE0;   // size 0x18
constexpr uint32_t isMinion__2Ai = 0x00101EF8;   // size 0x44
constexpr uint32_t overPit__2AiR7Monster = 0x00101F40;   // size 0xC
constexpr uint32_t inTub__2AiR8_fvector = 0x00101F50;   // size 0xD0
constexpr uint32_t setFocus__2AiP13DbInteractive = 0x00102020;   // size 0x18
constexpr uint32_t creditRelevance__2AiP7PowerUpf = 0x00102038;   // size 0x28
constexpr uint32_t creditRelevance__2AiP6Pickupf = 0x00102060;   // size 0x34
constexpr uint32_t okayToCC__2AiP7Monsterf = 0x00102098;   // size 0xC8
constexpr uint32_t updateRelevances__2Ai = 0x00102160;   // size 0x18C
constexpr uint32_t updateOpponentList__2Ai = 0x001022F0;   // size 0x184
constexpr uint32_t getBestPowerupByType__2AiQ27PowerUp4TypeRff = 0x00102478;   // size 0x320
constexpr uint32_t getBestHealthPowerup__2AiRff = 0x00102798;   // size 0xE8
constexpr uint32_t getBestStaminaPowerup__2AiRff = 0x00102880;   // size 0xE8
constexpr uint32_t getBestSpecialPowerup__2AiRff = 0x00102968;   // size 0x20
constexpr uint32_t getBestPickup__2AiRff = 0x00102988;   // size 0x22C
constexpr uint32_t x__13AiActionTuplef = 0x00102BB8;   // size 0x50
constexpr uint32_t init__13AiActionTuplef = 0x00102C08;   // size 0x60
constexpr uint32_t reset__13AiActionTuple = 0x00102C68;   // size 0x70
constexpr uint32_t updateIValueMod__13AiActionTuple = 0x00102CD8;   // size 0xB4
constexpr uint32_t getRelevance__13AiActionTupleR2Ai = 0x00102D90;   // size 0x158
constexpr uint32_t getFieldsSinceEval__13AiActionTuple = 0x00102EE8;   // size 0x2C
constexpr uint32_t execute__13AiActionTupleR2Ai = 0x00102F18;   // size 0xC0
constexpr uint32_t deactivate__13AiActionTupleR2Ai = 0x00102FD8;   // size 0x68
constexpr uint32_t x__12AiActionList = 0x00103040;   // size 0x14
constexpr uint32_t init__12AiActionListQ212AiActionList8Strategy = 0x00103058;   // size 0x20
constexpr uint32_t reset__12AiActionList = 0x00103078;   // size 0x50
constexpr uint32_t setStrategy__12AiActionListQ212AiActionList8Strategy = 0x001030C8;   // size 0x54
constexpr uint32_t addTuple__12AiActionListR13AiActionTuple = 0x00103120;   // size 0x20
constexpr uint32_t addTuple__12AiActionListR13AiActionTuplef = 0x00103140;   // size 0x40
constexpr uint32_t runFirstPositive__12AiActionListR2Ai = 0x00103180;   // size 0xE4
constexpr uint32_t runMaxPositive__12AiActionListR2Ai = 0x00103268;   // size 0x10C
constexpr uint32_t clearIValues__12AiActionList = 0x00103378;   // size 0x34
constexpr uint32_t update__18AiThrashActionListR2Ai = 0x001033B0;   // size 0x70
constexpr uint32_t x__13AiActionGroup = 0x00103420;   // size 0x28
constexpr uint32_t init__13AiActionGroup = 0x00103448;   // size 0xC
constexpr uint32_t reset__13AiActionGroup = 0x00103458;   // size 0x58
constexpr uint32_t addList__13AiActionGroupR12AiActionList = 0x001034B0;   // size 0x1C
constexpr uint32_t update__13AiActionGroupR2Ai = 0x001034D0;   // size 0xCC
constexpr uint32_t clearIValues__13AiActionGroup = 0x001035A0;   // size 0x50
constexpr uint32_t x__tf13AiActionTuple = 0x001035F0;   // size 0x3C
constexpr uint32_t enterAction__13AiActionTupleR2Ai = 0x00103630;   // size 0x8
constexpr uint32_t exitAction__13AiActionTupleR2Ai = 0x00103638;   // size 0x8
constexpr uint32_t x__tf12AiActionList = 0x00103640;   // size 0x3C
constexpr uint32_t update__12AiActionListR2Ai = 0x00103680;   // size 0x70
constexpr uint32_t x__tf18AiThrashActionList = 0x001036F0;   // size 0x6C
constexpr uint32_t initAssBoss__2AiP7Monster = 0x00103760;   // size 0x88
constexpr uint32_t getAssBossState__2Ai = 0x001037E8;   // size 0x8
constexpr uint32_t setAssBossState__2Aii = 0x001037F0;   // size 0x43C
constexpr uint32_t updateAssBoss__2Ai = 0x00103C30;   // size 0x224
constexpr uint32_t updateAssBossPatternZero__2Ai = 0x00103E58;   // size 0x2A0
constexpr uint32_t updateAssBossPatternOne__2Ai = 0x001040F8;   // size 0xBBC
constexpr uint32_t updateAssBossPatternTwo__2Ai = 0x00104CB8;   // size 0x9A4
constexpr uint32_t x__7AiBrain = 0x00105660;   // size 0x1A0
constexpr uint32_t init__7AiBrainR2Ai = 0x00105800;   // size 0x36C
constexpr uint32_t reset__7AiBrainR2Ai = 0x00105B70;   // size 0x50
constexpr uint32_t update__7AiBrainR2Ai = 0x00105BC0;   // size 0x1C
constexpr uint32_t update__20AiGrappledActionListR2Ai = 0x00105BE0;   // size 0x88
constexpr uint32_t update__21AiGrapplingActionListR2Ai = 0x00105C68;   // size 0x88
constexpr uint32_t x__14AiGrappleThrow = 0x00105CF0;   // size 0x44
constexpr uint32_t getEntryRelevance__14AiGrappleThrowR2Ai = 0x00105D38;   // size 0x6C
constexpr uint32_t getExitRelevance__14AiGrappleThrowR2Ai = 0x00105DA8;   // size 0x3C
constexpr uint32_t enterAction__14AiGrappleThrowR2Ai = 0x00105DE8;   // size 0x80
constexpr uint32_t updateAction__14AiGrappleThrowR2Ai = 0x00105E68;   // size 0x60
constexpr uint32_t exitAction__14AiGrappleThrowR2Ai = 0x00105EC8;   // size 0x20
constexpr uint32_t x__15AiGrappleAttack = 0x00105EE8;   // size 0x44
constexpr uint32_t getEntryRelevance__15AiGrappleAttackR2Ai = 0x00105F30;   // size 0x10
constexpr uint32_t getExitRelevance__15AiGrappleAttackR2Ai = 0x00105F40;   // size 0x44
constexpr uint32_t enterAction__15AiGrappleAttackR2Ai = 0x00105F88;   // size 0x64
constexpr uint32_t updateAction__15AiGrappleAttackR2Ai = 0x00105FF0;   // size 0xD8
constexpr uint32_t exitAction__15AiGrappleAttackR2Ai = 0x001060C8;   // size 0x20
constexpr uint32_t x__tf20AiGrappledActionList = 0x001060E8;   // size 0x50
constexpr uint32_t x__tf21AiGrapplingActionList = 0x00106138;   // size 0x50
constexpr uint32_t x__tf14AiGrappleThrow = 0x00106188;   // size 0x50
constexpr uint32_t x__tf15AiGrappleAttack = 0x001061D8;   // size 0x50
constexpr uint32_t getMaxLevel__12StaminaMeter = 0x00106228;   // size 0x8
constexpr uint32_t getTaunt__10AiPadClipsv = 0x001062B0;   // size 0xC
constexpr uint32_t getDash__10AiPadClipsv = 0x001062C0;   // size 0xC
constexpr uint32_t getThrow__10AiPadClipsv = 0x001062D0;   // size 0xC
constexpr uint32_t x__static_initialization_and_destruction_0_001062E0 = 0x001062E0;   // size 0x218
constexpr uint32_t x_GLOBAL__I_getTaunt__10AiPadClipsv = 0x001064F8;   // size 0x20
constexpr uint32_t getRandPath__10AiPathNodeUs = 0x00106518;   // size 0xA0
constexpr uint32_t getRandNode__9AiPathNet = 0x001065B8;   // size 0x40
constexpr uint32_t getRandPath__9AiPathNet = 0x001065F8;   // size 0x40
constexpr uint32_t getClosestNode__9AiPathNetR8_fvector = 0x00106638;   // size 0x74
constexpr uint32_t getClosestPath__9AiPathNetR8_fvectorUc = 0x001066B0;   // size 0x12C
constexpr uint32_t getPath__9AiPathNetR8_fvector = 0x001067E0;   // size 0x74
constexpr uint32_t isIn__9AiPathNetR8_fvectorR6AiPath = 0x00106858;   // size 0x1E0
constexpr uint32_t getRangeSq__9AiPathNetR8_fvectorR6AiPath = 0x00106A38;   // size 0x250
constexpr uint32_t x__12AiPathFinderR9AiPathNetR7Monster = 0x00106C88;   // size 0x5C
constexpr uint32_t computePath__12AiPathFinderPP6AiPathR8_fvectorR6AiPath = 0x00106CE8;   // size 0x264
constexpr uint32_t traceSuccessor__12AiPathFinderR6AiPathR10AiPathNodePQ212AiPathFinder4NodeT1 = 0x00106F50;   // size 0x1C0
constexpr uint32_t computeCostEstimate__12AiPathFinderR6AiPathR10AiPathNodeT1 = 0x00107110;   // size 0x58
constexpr uint32_t computeCostActual__12AiPathFinderR6AiPathR10AiPathNodeT1 = 0x00107168;   // size 0x164
constexpr uint32_t compileResultForReturn__12AiPathFinderPP6AiPathPQ212AiPathFinder4Node = 0x001072D0;   // size 0x60
constexpr uint32_t add__Q212AiPathFinder8OpenListPQ212AiPathFinder4Node = 0x00107330;   // size 0x74
constexpr uint32_t adjust__Q212AiPathFinder8OpenListPQ212AiPathFinder4Node = 0x001073A8;   // size 0x50
constexpr uint32_t popBest__Q212AiPathFinder8OpenList = 0x001073F8;   // size 0x44
constexpr uint32_t find__Q212AiPathFinder8OpenListP6AiPath = 0x00107440;   // size 0x34
constexpr uint32_t init__Q212AiPathFinder10GlobalListi = 0x00107478;   // size 0x48
constexpr uint32_t init__9BidirLink = 0x001074C0;   // size 0xC
constexpr uint32_t init__10AiPathTooli = 0x001074D0;   // size 0x54
constexpr uint32_t loadPoints__10AiPathToolPcb = 0x00107528;   // size 0x70
constexpr uint32_t exportPoints__10AiPathTool = 0x00107598;   // size 0x3C4
constexpr uint32_t x__static_initialization_and_destruction_0_00107960 = 0x00107960;   // size 0x50
constexpr uint32_t x__tf10AiPathTool = 0x001079B0;   // size 0x50
constexpr uint32_t x__10AiPathToolP9AiPathNetPcT2 = 0x00107A00;   // size 0x20
constexpr uint32_t x_GLOBAL__I__10AiPathTool_instance = 0x00107A20;   // size 0x20
constexpr uint32_t x__13AiPunchReflex = 0x00107A40;   // size 0x58
constexpr uint32_t getEntryRelevance__13AiPunchReflexR2Ai = 0x00107A98;   // size 0xD8
constexpr uint32_t getBestTarget__13AiPunchReflexR2AiRf = 0x00107B70;   // size 0xA0
constexpr uint32_t getTargetScore__13AiPunchReflexR2AiP7Monster = 0x00107C10;   // size 0x160
constexpr uint32_t getExitRelevance__13AiPunchReflexR2Ai = 0x00107D70;   // size 0x12C
constexpr uint32_t enterAction__13AiPunchReflexR2Ai = 0x00107EA0;   // size 0xE8
constexpr uint32_t updateAction__13AiPunchReflexR2Ai = 0x00107F88;   // size 0x118
constexpr uint32_t selectPunch__13AiPunchReflexR2Ai = 0x001080A0;   // size 0x3E0
constexpr uint32_t punch__13AiPunchReflexR2Aii = 0x00108480;   // size 0xC0
constexpr uint32_t exitAction__13AiPunchReflexR2Ai = 0x00108540;   // size 0x44
constexpr uint32_t x__11AiBatReflex = 0x00108588;   // size 0x58
constexpr uint32_t getEntryRelevance__11AiBatReflexR2Ai = 0x001085E0;   // size 0x110
constexpr uint32_t getBestTarget__11AiBatReflexR2AiRf = 0x001086F0;   // size 0xA0
constexpr uint32_t getTargetScore__11AiBatReflexR2AiP7Monster = 0x00108790;   // size 0x120
constexpr uint32_t getExitRelevance__11AiBatReflexR2Ai = 0x001088B0;   // size 0x110
constexpr uint32_t enterAction__11AiBatReflexR2Ai = 0x001089C0;   // size 0x9C
constexpr uint32_t updateAction__11AiBatReflexR2Ai = 0x00108A60;   // size 0x124
constexpr uint32_t selectSwipe__11AiBatReflexR2Ai = 0x00108B88;   // size 0x37C
constexpr uint32_t swipe__11AiBatReflexR2Aii = 0x00108F08;   // size 0xC0
constexpr uint32_t exitAction__11AiBatReflexR2Ai = 0x00108FC8;   // size 0x44
constexpr uint32_t x__16AiFireProjectile = 0x00109010;   // size 0x64
constexpr uint32_t getEntryRelevance__16AiFireProjectileR2Ai = 0x00109078;   // size 0x174
constexpr uint32_t getBestTarget__16AiFireProjectileR2AiRf = 0x001091F0;   // size 0xA0
constexpr uint32_t getTargetRelevance__16AiFireProjectileR2AiP7Monster = 0x00109290;   // size 0x110
constexpr uint32_t getExitRelevance__16AiFireProjectileR2Ai = 0x001093A0;   // size 0xB4
constexpr uint32_t enterAction__16AiFireProjectileR2Ai = 0x00109458;   // size 0x74
constexpr uint32_t updateAction__16AiFireProjectileR2Ai = 0x001094D0;   // size 0x1CC
constexpr uint32_t exitAction__16AiFireProjectileR2Ai = 0x001096A0;   // size 0x50
constexpr uint32_t x__15AiGrappleReflex = 0x001096F0;   // size 0x58
constexpr uint32_t getEntryRelevance__15AiGrappleReflexR2Ai = 0x00109748;   // size 0xC8
constexpr uint32_t getBestTarget__15AiGrappleReflexR2AiRf = 0x00109810;   // size 0x164
constexpr uint32_t getExitRelevance__15AiGrappleReflexR2Ai = 0x00109978;   // size 0xB8
constexpr uint32_t enterAction__15AiGrappleReflexR2Ai = 0x00109A30;   // size 0xA8
constexpr uint32_t updateAction__15AiGrappleReflexR2Ai = 0x00109AD8;   // size 0xCC
constexpr uint32_t exitAction__15AiGrappleReflexR2Ai = 0x00109BA8;   // size 0x44
constexpr uint32_t x__13AiBlockReflex = 0x00109BF0;   // size 0x50
constexpr uint32_t getEntryRelevance__13AiBlockReflexR2Ai = 0x00109C40;   // size 0x598
constexpr uint32_t getExitRelevance__13AiBlockReflexR2Ai = 0x0010A1D8;   // size 0xA0
constexpr uint32_t enterAction__13AiBlockReflexR2Ai = 0x0010A278;   // size 0xCC
constexpr uint32_t updateAction__13AiBlockReflexR2Ai = 0x0010A348;   // size 0x94
constexpr uint32_t exitAction__13AiBlockReflexR2Ai = 0x0010A3E0;   // size 0x44
constexpr uint32_t x__15AiCounterReflex = 0x0010A428;   // size 0x50
constexpr uint32_t getEntryRelevance__15AiCounterReflexR2Ai = 0x0010A478;   // size 0x3B8
constexpr uint32_t getExitRelevance__15AiCounterReflexR2Ai = 0x0010A830;   // size 0x50
constexpr uint32_t enterAction__15AiCounterReflexR2Ai = 0x0010A880;   // size 0x80
constexpr uint32_t updateAction__15AiCounterReflexR2Ai = 0x0010A900;   // size 0xA0
constexpr uint32_t exitAction__15AiCounterReflexR2Ai = 0x0010A9A0;   // size 0x44
constexpr uint32_t x__14AiThrashReflex = 0x0010A9E8;   // size 0x4C
constexpr uint32_t getEntryRelevance__14AiThrashReflexR2Ai = 0x0010AA38;   // size 0x50
constexpr uint32_t getExitRelevance__14AiThrashReflexR2Ai = 0x0010AA88;   // size 0x28
constexpr uint32_t enterAction__14AiThrashReflexR2Ai = 0x0010AAB0;   // size 0x64
constexpr uint32_t updateAction__14AiThrashReflexR2Ai = 0x0010AB18;   // size 0x118
constexpr uint32_t exitAction__14AiThrashReflexR2Ai = 0x0010AC30;   // size 0x20
constexpr uint32_t x__15AiDisposeReflex = 0x0010AC50;   // size 0x4C
constexpr uint32_t getEntryRelevance__15AiDisposeReflexR2Ai = 0x0010ACA0;   // size 0xD4
constexpr uint32_t getExitRelevance__15AiDisposeReflexR2Ai = 0x0010AD78;   // size 0x4C
constexpr uint32_t enterAction__15AiDisposeReflexR2Ai = 0x0010ADC8;   // size 0xF4
constexpr uint32_t updateAction__15AiDisposeReflexR2Ai = 0x0010AEC0;   // size 0x60
constexpr uint32_t exitAction__15AiDisposeReflexR2Ai = 0x0010AF20;   // size 0x44
constexpr uint32_t x__13AiSmashReflex = 0x0010AF68;   // size 0x4C
constexpr uint32_t getEntryRelevance__13AiSmashReflexR2Ai = 0x0010AFB8;   // size 0xF4
constexpr uint32_t getExitRelevance__13AiSmashReflexR2Ai = 0x0010B0B0;   // size 0x50
constexpr uint32_t enterAction__13AiSmashReflexR2Ai = 0x0010B100;   // size 0x50
constexpr uint32_t updateAction__13AiSmashReflexR2Ai = 0x0010B150;   // size 0x38
constexpr uint32_t exitAction__13AiSmashReflexR2Ai = 0x0010B188;   // size 0x8
constexpr uint32_t x__14AiThrowRecover = 0x0010B190;   // size 0x4C
constexpr uint32_t getEntryRelevance__14AiThrowRecoverR2Ai = 0x0010B1E0;   // size 0xBC
constexpr uint32_t getExitRelevance__14AiThrowRecoverR2Ai = 0x0010B2A0;   // size 0x30
constexpr uint32_t enterAction__14AiThrowRecoverR2Ai = 0x0010B2D0;   // size 0x60
constexpr uint32_t updateAction__14AiThrowRecoverR2Ai = 0x0010B330;   // size 0x2C
constexpr uint32_t exitAction__14AiThrowRecoverR2Ai = 0x0010B360;   // size 0x8
constexpr uint32_t x__13AiThrowPickup = 0x0010B368;   // size 0x58
constexpr uint32_t getEntryRelevance__13AiThrowPickupR2Ai = 0x0010B3C0;   // size 0xA8
constexpr uint32_t getBestTarget__13AiThrowPickupR2AiRf = 0x0010B468;   // size 0x1C8
constexpr uint32_t getExitRelevance__13AiThrowPickupR2Ai = 0x0010B630;   // size 0x68
constexpr uint32_t enterAction__13AiThrowPickupR2Ai = 0x0010B698;   // size 0x94
constexpr uint32_t updateAction__13AiThrowPickupR2Ai = 0x0010B730;   // size 0xDC
constexpr uint32_t exitAction__13AiThrowPickupR2Ai = 0x0010B810;   // size 0x20
constexpr uint32_t x__13AiStompReflex = 0x0010B830;   // size 0x58
constexpr uint32_t getEntryRelevance__13AiStompReflexR2Ai = 0x0010B888;   // size 0x170
constexpr uint32_t getExitRelevance__13AiStompReflexR2Ai = 0x0010B9F8;   // size 0x34
constexpr uint32_t enterAction__13AiStompReflexR2Ai = 0x0010BA30;   // size 0x78
constexpr uint32_t updateAction__13AiStompReflexR2Ai = 0x0010BAA8;   // size 0x2C
constexpr uint32_t exitAction__13AiStompReflexR2Ai = 0x0010BAD8;   // size 0x20
constexpr uint32_t x__11AiRamAttack = 0x0010BAF8;   // size 0x68
constexpr uint32_t getEntryRelevance__11AiRamAttackR2Ai = 0x0010BB60;   // size 0xA0
constexpr uint32_t getBestTarget__11AiRamAttackR2AiRf = 0x0010BC00;   // size 0x2C4
constexpr uint32_t getExitRelevance__11AiRamAttackR2Ai = 0x0010BEC8;   // size 0x124
constexpr uint32_t enterAction__11AiRamAttackR2Ai = 0x0010BFF0;   // size 0x9C
constexpr uint32_t updateAction__11AiRamAttackR2Ai = 0x0010C090;   // size 0xEC
constexpr uint32_t exitAction__11AiRamAttackR2Ai = 0x0010C180;   // size 0x20
constexpr uint32_t x__14AiCrowdControl = 0x0010C1A0;   // size 0x58
constexpr uint32_t getEntryRelevance__14AiCrowdControlR2Ai = 0x0010C1F8;   // size 0x68
constexpr uint32_t getBestTarget__14AiCrowdControlR2AiRf = 0x0010C260;   // size 0x1D0
constexpr uint32_t getExitRelevance__14AiCrowdControlR2Ai = 0x0010C430;   // size 0x30
constexpr uint32_t enterAction__14AiCrowdControlR2Ai = 0x0010C460;   // size 0x60
constexpr uint32_t updateAction__14AiCrowdControlR2Ai = 0x0010C4C0;   // size 0x2C
constexpr uint32_t exitAction__14AiCrowdControlR2Ai = 0x0010C4F0;   // size 0x20
constexpr uint32_t x__15AiSpecialAttack = 0x0010C510;   // size 0x68
constexpr uint32_t getEntryRelevance__15AiSpecialAttackR2Ai = 0x0010C578;   // size 0xAC
constexpr uint32_t getBestTarget__15AiSpecialAttackR2AiRf = 0x0010C628;   // size 0x230
constexpr uint32_t getExitRelevance__15AiSpecialAttackR2Ai = 0x0010C858;   // size 0x1AC
constexpr uint32_t enterAction__15AiSpecialAttackR2Ai = 0x0010CA08;   // size 0x98
constexpr uint32_t updateAction__15AiSpecialAttackR2Ai = 0x0010CAA0;   // size 0xF4
constexpr uint32_t exitAction__15AiSpecialAttackR2Ai = 0x0010CB98;   // size 0x44
constexpr uint32_t x__14AiDetonateHead = 0x0010CBE0;   // size 0x58
constexpr uint32_t getEntryRelevance__14AiDetonateHeadR2Ai = 0x0010CC38;   // size 0x84
constexpr uint32_t getBestTarget__14AiDetonateHeadR2AiRf = 0x0010CCC0;   // size 0x290
constexpr uint32_t getExitRelevance__14AiDetonateHeadR2Ai = 0x0010CF50;   // size 0x30
constexpr uint32_t enterAction__14AiDetonateHeadR2Ai = 0x0010CF80;   // size 0x9C
constexpr uint32_t updateAction__14AiDetonateHeadR2Ai = 0x0010D020;   // size 0x2C
constexpr uint32_t exitAction__14AiDetonateHeadR2Ai = 0x0010D050;   // size 0x44
constexpr uint32_t x__7AiTaunt = 0x0010D098;   // size 0x50
constexpr uint32_t getEntryRelevance__7AiTauntR2Ai = 0x0010D0E8;   // size 0xA8
constexpr uint32_t getExitRelevance__7AiTauntR2Ai = 0x0010D190;   // size 0x4C
constexpr uint32_t enterAction__7AiTauntR2Ai = 0x0010D1E0;   // size 0x80
constexpr uint32_t updateAction__7AiTauntR2Ai = 0x0010D260;   // size 0xB0
constexpr uint32_t exitAction__7AiTauntR2Ai = 0x0010D310;   // size 0x44
constexpr uint32_t x__12AiPutOutFire = 0x0010D358;   // size 0x44
constexpr uint32_t getEntryRelevance__12AiPutOutFireR2Ai = 0x0010D3A0;   // size 0x114
constexpr uint32_t getExitRelevance__12AiPutOutFireR2Ai = 0x0010D4B8;   // size 0x70
constexpr uint32_t enterAction__12AiPutOutFireR2Ai = 0x0010D528;   // size 0x6C
constexpr uint32_t updateAction__12AiPutOutFireR2Ai = 0x0010D598;   // size 0x50
constexpr uint32_t exitAction__12AiPutOutFireR2Ai = 0x0010D5E8;   // size 0x8
constexpr uint32_t x__tf13AiPunchReflex = 0x0010D5F0;   // size 0x50
constexpr uint32_t x__tf11AiBatReflex = 0x0010D640;   // size 0x50
constexpr uint32_t x__tf16AiFireProjectile = 0x0010D690;   // size 0x50
constexpr uint32_t x__tf15AiGrappleReflex = 0x0010D6E0;   // size 0x50
constexpr uint32_t x__tf13AiBlockReflex = 0x0010D730;   // size 0x50
constexpr uint32_t x__tf15AiCounterReflex = 0x0010D780;   // size 0x50
constexpr uint32_t x__tf14AiThrashReflex = 0x0010D7D0;   // size 0x50
constexpr uint32_t x__tf15AiDisposeReflex = 0x0010D820;   // size 0x50
constexpr uint32_t x__tf13AiSmashReflex = 0x0010D870;   // size 0x50
constexpr uint32_t x__tf14AiThrowRecover = 0x0010D8C0;   // size 0x50
constexpr uint32_t x__tf13AiThrowPickup = 0x0010D910;   // size 0x50
constexpr uint32_t x__tf13AiStompReflex = 0x0010D960;   // size 0x50
constexpr uint32_t x__tf11AiRamAttack = 0x0010D9B0;   // size 0x50
constexpr uint32_t x__tf14AiCrowdControl = 0x0010DA00;   // size 0x50
constexpr uint32_t x__tf15AiSpecialAttack = 0x0010DA50;   // size 0x50
constexpr uint32_t x__tf14AiDetonateHead = 0x0010DAA0;   // size 0x50
constexpr uint32_t x__tf7AiTaunt = 0x0010DAF0;   // size 0x50
constexpr uint32_t x__tf12AiPutOutFire = 0x0010DB40;   // size 0x50
constexpr uint32_t getMaxLevel__11HealthMeter = 0x0010DB90;   // size 0x8
constexpr uint32_t inFov__9EnemyInfof = 0x0010DBA0;   // size 0x20
constexpr uint32_t inLos__9EnemyInfo = 0x0010DBC0;   // size 0x8
constexpr uint32_t x__7AiReset = 0x0010DC08;   // size 0x44
constexpr uint32_t setAi__7AiResetP2Ai = 0x0010DC50;   // size 0xB8
constexpr uint32_t loadFromRef__7AiResetb = 0x0010DD08;   // size 0x8
constexpr uint32_t writeKeys__7AiResetPc = 0x0010DD10;   // size 0xAD8
constexpr uint32_t readKey__7AiResetPc = 0x0010E7E8;   // size 0x788
constexpr uint32_t x__tf7AiReset = 0x0010EF70;   // size 0x50
constexpr uint32_t x__13AiScript3Mile = 0x0010EFC0;   // size 0x6C
constexpr uint32_t update__13AiScript3Mile = 0x0010F030;   // size 0x278
constexpr uint32_t enterState__13AiScript3MileQ213AiScript3Mile5State = 0x0010F2A8;   // size 0x334
constexpr uint32_t x__13AiScriptTokyo = 0x0010F5E0;   // size 0x58
constexpr uint32_t update__13AiScriptTokyo = 0x0010F638;   // size 0x6C
constexpr uint32_t update__13AiScript3MilePv = 0x0010F6A8;   // size 0x1C
constexpr uint32_t update__13AiScriptTokyoPv = 0x0010F6C8;   // size 0x1C
constexpr uint32_t x__12AiSeekHealth = 0x0010F6F0;   // size 0x50
constexpr uint32_t getEntryRelevance__12AiSeekHealthR2Ai = 0x0010F740;   // size 0x180
constexpr uint32_t getExitRelevance__12AiSeekHealthR2Ai = 0x0010F8C0;   // size 0x154
constexpr uint32_t enterAction__12AiSeekHealthR2Ai = 0x0010FA18;   // size 0x60
constexpr uint32_t updateAction__12AiSeekHealthR2Ai = 0x0010FA78;   // size 0xEC
constexpr uint32_t exitAction__12AiSeekHealthR2Ai = 0x0010FB68;   // size 0xCC
constexpr uint32_t x__13AiSeekStamina = 0x0010FC38;   // size 0x50
constexpr uint32_t getEntryRelevance__13AiSeekStaminaR2Ai = 0x0010FC88;   // size 0x1B8
constexpr uint32_t getExitRelevance__13AiSeekStaminaR2Ai = 0x0010FE40;   // size 0xC0
constexpr uint32_t enterAction__13AiSeekStaminaR2Ai = 0x0010FF00;   // size 0x60
constexpr uint32_t updateAction__13AiSeekStaminaR2Ai = 0x0010FF60;   // size 0xEC
constexpr uint32_t exitAction__13AiSeekStaminaR2Ai = 0x00110050;   // size 0xD0
constexpr uint32_t x__13AiSeekSpecial = 0x00110120;   // size 0x50
constexpr uint32_t getEntryRelevance__13AiSeekSpecialR2Ai = 0x00110170;   // size 0x5C
constexpr uint32_t getExitRelevance__13AiSeekSpecialR2Ai = 0x001101D0;   // size 0xC0
constexpr uint32_t enterAction__13AiSeekSpecialR2Ai = 0x00110290;   // size 0x60
constexpr uint32_t updateAction__13AiSeekSpecialR2Ai = 0x001102F0;   // size 0xEC
constexpr uint32_t exitAction__13AiSeekSpecialR2Ai = 0x001103E0;   // size 0xC4
constexpr uint32_t x__11AiSeekCloak = 0x001104A8;   // size 0x50
constexpr uint32_t getEntryRelevance__11AiSeekCloakR2Ai = 0x001104F8;   // size 0x50
constexpr uint32_t getExitRelevance__11AiSeekCloakR2Ai = 0x00110548;   // size 0xC0
constexpr uint32_t enterAction__11AiSeekCloakR2Ai = 0x00110608;   // size 0x60
constexpr uint32_t updateAction__11AiSeekCloakR2Ai = 0x00110668;   // size 0xEC
constexpr uint32_t exitAction__11AiSeekCloakR2Ai = 0x00110758;   // size 0xC4
constexpr uint32_t x__12AiSeekPickup = 0x00110820;   // size 0x54
constexpr uint32_t getEntryRelevance__12AiSeekPickupR2Ai = 0x00110878;   // size 0x74
constexpr uint32_t getExitRelevance__12AiSeekPickupR2Ai = 0x001108F0;   // size 0x100
constexpr uint32_t enterAction__12AiSeekPickupR2Ai = 0x001109F0;   // size 0xB8
constexpr uint32_t updateAction__12AiSeekPickupR2Ai = 0x00110AA8;   // size 0x104
constexpr uint32_t exitAction__12AiSeekPickupR2Ai = 0x00110BB0;   // size 0xB8
constexpr uint32_t x__13AiSeekMonster = 0x00110C68;   // size 0x60
constexpr uint32_t getEntryRelevance__13AiSeekMonsterR2Ai = 0x00110CC8;   // size 0xB0
constexpr uint32_t getBestTarget__13AiSeekMonsterR2AiRf = 0x00110D78;   // size 0x314
constexpr uint32_t getExitRelevance__13AiSeekMonsterR2Ai = 0x00111090;   // size 0xDC
constexpr uint32_t enterAction__13AiSeekMonsterR2Ai = 0x00111170;   // size 0x40
constexpr uint32_t updateAction__13AiSeekMonsterR2Ai = 0x001111B0;   // size 0xE4
constexpr uint32_t exitAction__13AiSeekMonsterR2Ai = 0x00111298;   // size 0x44
constexpr uint32_t x__7AiSwarm = 0x001112E0;   // size 0x6C
constexpr uint32_t getEntryRelevance__7AiSwarmR2Ai = 0x00111350;   // size 0xA0
constexpr uint32_t getBestTarget__7AiSwarmR2AiRf = 0x001113F0;   // size 0x174
constexpr uint32_t getExitRelevance__7AiSwarmR2Ai = 0x00111568;   // size 0x9C
constexpr uint32_t enterAction__7AiSwarmR2Ai = 0x00111608;   // size 0x74
constexpr uint32_t updateAction__7AiSwarmR2Ai = 0x00111680;   // size 0x2D8
constexpr uint32_t enterState__7AiSwarmR2AiQ27AiSwarm5State = 0x00111958;   // size 0x148
constexpr uint32_t exitAction__7AiSwarmR2Ai = 0x00111AA0;   // size 0x54
constexpr uint32_t x__12AiDodgeThrow = 0x00111AF8;   // size 0x5C
constexpr uint32_t getEntryRelevance__12AiDodgeThrowR2Ai = 0x00111B58;   // size 0x264
constexpr uint32_t getExitRelevance__12AiDodgeThrowR2Ai = 0x00111DC0;   // size 0x160
constexpr uint32_t enterAction__12AiDodgeThrowR2Ai = 0x00111F20;   // size 0x64
constexpr uint32_t updateAction__12AiDodgeThrowR2Ai = 0x00111F88;   // size 0x21C
constexpr uint32_t exitAction__12AiDodgeThrowR2Ai = 0x001121A8;   // size 0x44
constexpr uint32_t x__12AiCatchThrow = 0x001121F0;   // size 0x68
constexpr uint32_t getEntryRelevance__12AiCatchThrowR2Ai = 0x00112258;   // size 0x1D4
constexpr uint32_t getExitRelevance__12AiCatchThrowR2Ai = 0x00112430;   // size 0x11C
constexpr uint32_t enterAction__12AiCatchThrowR2Ai = 0x00112550;   // size 0x64
constexpr uint32_t updateAction__12AiCatchThrowR2Ai = 0x001125B8;   // size 0x100
constexpr uint32_t exitAction__12AiCatchThrowR2Ai = 0x001126B8;   // size 0x44
constexpr uint32_t x__10AiBatThrow = 0x00112700;   // size 0x68
constexpr uint32_t getEntryRelevance__10AiBatThrowR2Ai = 0x00112768;   // size 0x1F4
constexpr uint32_t getExitRelevance__10AiBatThrowR2Ai = 0x00112960;   // size 0x11C
constexpr uint32_t enterAction__10AiBatThrowR2Ai = 0x00112A80;   // size 0x64
constexpr uint32_t updateAction__10AiBatThrowR2Ai = 0x00112AE8;   // size 0xDC
constexpr uint32_t exitAction__10AiBatThrowR2Ai = 0x00112BC8;   // size 0x44
constexpr uint32_t x__10AiDodgeRam = 0x00112C10;   // size 0x50
constexpr uint32_t getEntryRelevance__10AiDodgeRamR2Ai = 0x00112C60;   // size 0x1FC
constexpr uint32_t getExitRelevance__10AiDodgeRamR2Ai = 0x00112E60;   // size 0x90
constexpr uint32_t enterAction__10AiDodgeRamR2Ai = 0x00112EF0;   // size 0x124
constexpr uint32_t updateAction__10AiDodgeRamR2Ai = 0x00113018;   // size 0x78
constexpr uint32_t exitAction__10AiDodgeRamR2Ai = 0x00113090;   // size 0x44
constexpr uint32_t x__12AiDodgeStomp = 0x001130D8;   // size 0x58
constexpr uint32_t getEntryRelevance__12AiDodgeStompR2Ai = 0x00113130;   // size 0xBC
constexpr uint32_t getExitRelevance__12AiDodgeStompR2Ai = 0x001131F0;   // size 0x5C
constexpr uint32_t enterAction__12AiDodgeStompR2Ai = 0x00113250;   // size 0xC0
constexpr uint32_t updateAction__12AiDodgeStompR2Ai = 0x00113310;   // size 0x78
constexpr uint32_t exitAction__12AiDodgeStompR2Ai = 0x00113388;   // size 0x44
constexpr uint32_t x__14AiDodgeSpecial = 0x001133D0;   // size 0x58
constexpr uint32_t getEntryRelevance__14AiDodgeSpecialR2Ai = 0x00113428;   // size 0x134
constexpr uint32_t getExitRelevance__14AiDodgeSpecialR2Ai = 0x00113560;   // size 0x84
constexpr uint32_t enterAction__14AiDodgeSpecialR2Ai = 0x001135E8;   // size 0x44
constexpr uint32_t updateFleeSpot__14AiDodgeSpecialR2Ai = 0x00113630;   // size 0x128
constexpr uint32_t updateAction__14AiDodgeSpecialR2Ai = 0x00113758;   // size 0x9C
constexpr uint32_t exitAction__14AiDodgeSpecialR2Ai = 0x001137F8;   // size 0x44
constexpr uint32_t x__tf12AiSeekHealth = 0x00113840;   // size 0x50
constexpr uint32_t x__tf13AiSeekStamina = 0x00113890;   // size 0x50
constexpr uint32_t x__tf13AiSeekSpecial = 0x001138E0;   // size 0x50
constexpr uint32_t x__tf11AiSeekCloak = 0x00113930;   // size 0x50
constexpr uint32_t x__tf12AiSeekPickup = 0x00113980;   // size 0x50
constexpr uint32_t setRange__12AiSeekPickupf = 0x001139D0;   // size 0x8
constexpr uint32_t x__tf13AiSeekMonster = 0x001139D8;   // size 0x50
constexpr uint32_t x__tf7AiSwarm = 0x00113A28;   // size 0x50
constexpr uint32_t x__tf12AiDodgeThrow = 0x00113A78;   // size 0x50
constexpr uint32_t x__tf12AiCatchThrow = 0x00113AC8;   // size 0x50
constexpr uint32_t x__tf10AiBatThrow = 0x00113B18;   // size 0x50
constexpr uint32_t x__tf10AiDodgeRam = 0x00113B68;   // size 0x50
constexpr uint32_t x__tf12AiDodgeStomp = 0x00113BB8;   // size 0x50
constexpr uint32_t x__tf14AiDodgeSpecial = 0x00113C08;   // size 0x50
constexpr uint32_t setActiveAi__8AiTweaksP2Aib = 0x00113CA8;   // size 0x40
constexpr uint32_t x__static_initialization_and_destruction_0_00113CE8 = 0x00113CE8;   // size 0x30
constexpr uint32_t x_GLOBAL__I_setActiveAi__8AiTweaksP2Aib = 0x00113D18;   // size 0x20
constexpr uint32_t x__11AiNavigatorR7Monster = 0x00113D38;   // size 0x50
constexpr uint32_t init__11AiNavigator = 0x00113D88;   // size 0x5C
constexpr uint32_t update__11AiNavigatorR7GamePad = 0x00113DE8;   // size 0x1CC
constexpr uint32_t updateSteering__11AiNavigatorR7GamePad = 0x00113FB8;   // size 0x1B4
constexpr uint32_t disable__11AiNavigatorQ211AiNavigator6StatusQ211AiNavigator11FailureHint = 0x00114170;   // size 0x18
constexpr uint32_t wander__11AiNavigatorR8_fvector = 0x00114188;   // size 0x14
constexpr uint32_t seek__11AiNavigatorR8_fvectorf = 0x001141A0;   // size 0x80
constexpr uint32_t seek__11AiNavigatorR13DbInteractivef = 0x00114220;   // size 0x50
constexpr uint32_t target__11AiNavigatorR8_fvectorf = 0x00114270;   // size 0x1C
constexpr uint32_t arrive__11AiNavigatorR8_fvectorfff = 0x00114290;   // size 0x90
constexpr uint32_t arrive__11AiNavigatorR13DbInteractivefff = 0x00114320;   // size 0x50
constexpr uint32_t tag__11AiNavigatorR8_fvectorf = 0x00114370;   // size 0x80
constexpr uint32_t flee__11AiNavigatorR8_fvectorf = 0x001143F0;   // size 0x30
constexpr uint32_t updateSeek__11AiNavigator = 0x00114420;   // size 0x28C
constexpr uint32_t updateTarget__11AiNavigator = 0x001146B0;   // size 0x4C
constexpr uint32_t updateArrive__11AiNavigator = 0x00114700;   // size 0x1C
constexpr uint32_t updateTag__11AiNavigator = 0x00114720;   // size 0x270
constexpr uint32_t updateFlee__11AiNavigator = 0x00114990;   // size 0x88
constexpr uint32_t targetPin__11AiNavigatorR8_fvector = 0x00114A18;   // size 0x84
constexpr uint32_t pathSeekIntercept__11AiNavigator = 0x00114AA0;   // size 0x334
constexpr uint32_t pathSeek__11AiNavigator = 0x00114DD8;   // size 0x2C0
constexpr uint32_t avoid__11AiNavigatorR8_fvector = 0x00115098;   // size 0x134
constexpr uint32_t avoidClimbing__11AiNavigatorR8_fvectorT1 = 0x001151D0;   // size 0x214
constexpr uint32_t avoidJumping__11AiNavigatorR8_fvectorT1 = 0x001153E8;   // size 0x494
constexpr uint32_t avoidFlying__11AiNavigatorR8_fvectorT1 = 0x00115880;   // size 0x53C
constexpr uint32_t avoidGeneral__11AiNavigatorR8_fvectorT1 = 0x00115DC0;   // size 0x464
constexpr uint32_t attemptClimb__11AiNavigatorR8_fvector = 0x00116228;   // size 0xE4
constexpr uint32_t handleNoJumpOrClimb__11AiNavigatorP9_hdResultR8_fvectorT2 = 0x00116310;   // size 0x200
constexpr uint32_t orientTo__11AiNavigatorR8_fvectorf = 0x00116510;   // size 0xB0
constexpr uint32_t strafeTo__11AiNavigatorR8_fvectorff = 0x001165C0;   // size 0xB0
constexpr uint32_t followPath__11AiNavigatorRQ211AiNavigator8PathInfoT1R8_fvector = 0x00116670;   // size 0x3B0
constexpr uint32_t nextPathPoint__11AiNavigator = 0x00116A20;   // size 0x7C
constexpr uint32_t getFleePath__11AiNavigatorR8_fvector = 0x00116AA0;   // size 0xC
constexpr uint32_t inPathTransition__11AiNavigatorRQ211AiNavigator8PathInfoR8_fvectorb = 0x00116AB0;   // size 0xD0
constexpr uint32_t getMaxJumpHeight__11AiNavigatorb = 0x00116B80;   // size 0x194
constexpr uint32_t getMaxStandingJumpHeight__11AiNavigatorb = 0x00116D18;   // size 0xE4
constexpr uint32_t x__Q211AiNavigator14ObstacleSensorR11AiNavigator = 0x00116E00;   // size 0x40
constexpr uint32_t init__Q211AiNavigator14ObstacleSensor = 0x00116E40;   // size 0x14
constexpr uint32_t test__Q211AiNavigator14ObstacleSensor = 0x00116E58;   // size 0x618
constexpr uint32_t getNextCollis__Q211AiNavigator14ObstacleSensorP9_hdResultiT1 = 0x00117470;   // size 0x9C
constexpr uint32_t getObstacle__Q211AiNavigator14ObstacleSensor = 0x00117510;   // size 0x1C
constexpr uint32_t getObstacleRange__Q211AiNavigator14ObstacleSensor = 0x00117530;   // size 0x8
constexpr uint32_t getObstacleHeight__Q211AiNavigator14ObstacleSensor = 0x00117538;   // size 0x14
constexpr uint32_t getCeilHeight__Q211AiNavigator14ObstacleSensor = 0x00117550;   // size 0x28
constexpr uint32_t getFloorHeight__Q211AiNavigator14ObstacleSensor = 0x00117578;   // size 0x28
constexpr uint32_t shouldAvoid__Q211AiNavigator14ObstacleSensorR8_fvector = 0x001175A0;   // size 0x2F4
constexpr uint32_t isObstacleClimbable__Q211AiNavigator14ObstacleSensor = 0x00117898;   // size 0x58
constexpr uint32_t isObstacleClimbable__Q211AiNavigator14ObstacleSensorR9_hdResult = 0x001178F0;   // size 0xA4
constexpr uint32_t init__Q211AiNavigator8PathInfoP6AiPathi = 0x00117998;   // size 0xBC
constexpr uint32_t init__Q211AiNavigator8PathInfoP6AiPathT1b = 0x00117A58;   // size 0x48
constexpr uint32_t init__Q211AiNavigator8PathInfoP6AiPathP8_fvector = 0x00117AA0;   // size 0xB4
constexpr uint32_t airportInitBefore__Fv = 0x00117B58;   // size 0x44
constexpr uint32_t airportInitAfter__Fv = 0x00117BA0;   // size 0x20
constexpr uint32_t airportUpdate__Fv = 0x00117BC0;   // size 0x40
constexpr uint32_t airportAddEpNode__FP9_hierhead = 0x00117C00;   // size 0x34
constexpr uint32_t initAfter__8Airplane = 0x00117C38;   // size 0x154
constexpr uint32_t update__8Airplane = 0x00117D90;   // size 0xB08
constexpr uint32_t updateDeath__8Airplane = 0x00118898;   // size 0x644
constexpr uint32_t takeHit__8AirplaneP8_fvectorfi = 0x00118EE0;   // size 0x18C
constexpr uint32_t respawn__8Airplane = 0x00119070;   // size 0x98
constexpr uint32_t blowDebris__8Airplane = 0x00119108;   // size 0x420
constexpr uint32_t getVel__8Airplane = 0x00119528;   // size 0x58
constexpr uint32_t updateDebris__8Airplane = 0x00119580;   // size 0x230
constexpr uint32_t x__static_initialization_and_destruction_0_001197B0 = 0x001197B0;   // size 0x38
constexpr uint32_t getPinTrans__13DbInteractive = 0x001197E8;   // size 0xC
constexpr uint32_t getLookAtTrans__13DbInteractive = 0x001197F8;   // size 0x28
constexpr uint32_t x__tf8Airplane = 0x00119820;   // size 0x50
constexpr uint32_t updateDebris__8AirplanePv = 0x00119870;   // size 0x1C
constexpr uint32_t x__tf13DbInteractive = 0x00119890;   // size 0x3C
constexpr uint32_t x_GLOBAL__I_airportInitBefore__Fv = 0x001198D0;   // size 0x20
constexpr uint32_t init__11AnimContactR11_animHandle = 0x001198F0;   // size 0xD8
constexpr uint32_t update__11AnimContact = 0x001199C8;   // size 0x68
constexpr uint32_t getLimb__11AnimContacti = 0x00119A30;   // size 0x60
constexpr uint32_t numContacts__11AnimContact = 0x00119A90;   // size 0x50
constexpr uint32_t numHandContacts__11AnimContact = 0x00119AE0;   // size 0x50
constexpr uint32_t numFootContacts__11AnimContact = 0x00119B30;   // size 0x50
constexpr uint32_t numNewContacts__11AnimContact = 0x00119B80;   // size 0x80
constexpr uint32_t numNewHandContacts__11AnimContact = 0x00119C00;   // size 0x80
constexpr uint32_t numNewFootContacts__11AnimContact = 0x00119C80;   // size 0x80
constexpr uint32_t initBeforeDbLoad__12BigShotLevel = 0x00119D00;   // size 0x114
constexpr uint32_t initAfterDbLoad__12BigShotLevel = 0x00119E18;   // size 0xD8
constexpr uint32_t initForReplay__12BigShotLevel = 0x00119EF0;   // size 0x34C
constexpr uint32_t addEpNode__12BigShotLevelP9_hierhead = 0x0011A240;   // size 0xD8
constexpr uint32_t swapPlayers__12BigShotLevel = 0x0011A318;   // size 0x288
constexpr uint32_t selectTarget__12BigShotLeveli = 0x0011A5A0;   // size 0x294
constexpr uint32_t selectNextTarget__12BigShotLevel = 0x0011A838;   // size 0x28
constexpr uint32_t targetMiss__12BigShotLevelP7Monster = 0x0011A860;   // size 0x174
constexpr uint32_t targetHit__12BigShotLeveli = 0x0011A9D8;   // size 0x80
constexpr uint32_t takeTurns__12BigShotLevel = 0x0011AA58;   // size 0x150
constexpr uint32_t mineHit__12BigShotLevelP9_hdResult = 0x0011ABA8;   // size 0x164
constexpr uint32_t updateNormal__12BigShotLevel = 0x0011AD10;   // size 0x3F8
constexpr uint32_t updateSuddenDeath__12BigShotLevel = 0x0011B108;   // size 0x78
constexpr uint32_t fade__12BigShotLeveli = 0x0011B180;   // size 0x130
constexpr uint32_t initSuddenDeath__12BigShotLevel = 0x0011B2B0;   // size 0x22C
constexpr uint32_t takeTurns__12BigShotLevelPv = 0x0011B4E0;   // size 0x1C
constexpr uint32_t init__10BoneSpringP7Monsterii = 0x0011B500;   // size 0xBC
constexpr uint32_t update__10BoneSpring = 0x0011B5C0;   // size 0x1D0
constexpr uint32_t x__as__11PitchChangeR11PitchChange = 0x0011B790;   // size 0x18
constexpr uint32_t x__as__9ShakeDataR9ShakeData = 0x0011B7A8;   // size 0x4C
constexpr uint32_t x__as__17MonsterCameraDataR17MonsterCameraData = 0x0011B7F8;   // size 0x11C
constexpr uint32_t GetUnifiedTime__7Cameras = 0x0011B918;   // size 0x7C
constexpr uint32_t GetCsCameraFollows__7Camerasi = 0x0011B998;   // size 0x20
constexpr uint32_t Init__7Cameras = 0x0011B9B8;   // size 0x1BC
constexpr uint32_t SetCameraToFollowMonster__7CamerasiP7Monster = 0x0011BB78;   // size 0x8C
constexpr uint32_t SetCameraMonster__7CamerasiP7Monster = 0x0011BC08;   // size 0x48
constexpr uint32_t SetCameraPOV__7CamerasiQ26Camera9CameraPOV = 0x0011BC50;   // size 0x254
constexpr uint32_t ResetPOVChange__7Camerasif = 0x0011BEA8;   // size 0x4C
constexpr uint32_t AlignCsWithCamera__7CamerasiP3_csfff = 0x0011BEF8;   // size 0xB0
constexpr uint32_t AlignCsWithView__7CamerasiP3_csfff = 0x0011BFA8;   // size 0xC4
constexpr uint32_t TogglePOV__7Camerasi = 0x0011C070;   // size 0x58
constexpr uint32_t DetachCamera__7CamerasiP8_fvector = 0x0011C0C8;   // size 0x9C
constexpr uint32_t ReattachCamera__7Camerasi = 0x0011C168;   // size 0x40
constexpr uint32_t StartShake__7CamerasR8_fvectorf = 0x0011C1A8;   // size 0x88
constexpr uint32_t StartShake__7Camerasf = 0x0011C230;   // size 0x3C
constexpr uint32_t ShakeFalloff__7Camerasf = 0x0011C270;   // size 0x3C
constexpr uint32_t InitCCCamera__7CamerasP7MonsterT1 = 0x0011C2B0;   // size 0x144
constexpr uint32_t InitCCCameraFromThrow__7CamerasP7MonsterT1 = 0x0011C3F8;   // size 0x1E8
constexpr uint32_t InitCCMonsters__7CamerasP7MonsterT1 = 0x0011C5E0;   // size 0x84
constexpr uint32_t InitCrushMonsters__7CamerasP7MonsterT1 = 0x0011C668;   // size 0x24
constexpr uint32_t CCFadeIn__7Cameras = 0x0011C690;   // size 0x68
constexpr uint32_t CCFadeOut__7Cameras = 0x0011C6F8;   // size 0x78
constexpr uint32_t Update__7Cameras = 0x0011C770;   // size 0xF4
constexpr uint32_t LeaveUnifiedView__7CamerasUi = 0x0011C868;   // size 0x14C
constexpr uint32_t LeaveUnifiedTask__7Cameras = 0x0011C9B8;   // size 0x10C
constexpr uint32_t ccMonsNoLongerInRange__7Camerasf = 0x0011CAC8;   // size 0x208
constexpr uint32_t ccMonsMovingTogether__7Cameras = 0x0011CCD0;   // size 0x1F8
constexpr uint32_t InitPlantBossCamera__7Cameras = 0x0011CEC8;   // size 0x154
constexpr uint32_t closeCombatUpdate__6Camera = 0x0011D020;   // size 0x1058
constexpr uint32_t roamUpdate__6Camera = 0x0011E078;   // size 0x910
constexpr uint32_t staticUpdate__6Camera = 0x0011E988;   // size 0x140
constexpr uint32_t getupUpdate__6Camera = 0x0011EAC8;   // size 0x1EC
constexpr uint32_t knockbackUpdate__6Camera = 0x0011ECB8;   // size 0x310
constexpr uint32_t topdownUpdate__6Camera = 0x0011EFC8;   // size 0x1E8
constexpr uint32_t victoryUpdate__6Camera = 0x0011F1B0;   // size 0x17C
constexpr uint32_t crushUpdate__6Camera = 0x0011F330;   // size 0x300
constexpr uint32_t normalUpdate__6Camera = 0x0011F630;   // size 0x10F0
constexpr uint32_t checkCollision__6Camerai = 0x00120720;   // size 0x234
constexpr uint32_t checkNewPosition__6Camera = 0x00120958;   // size 0x1F4
constexpr uint32_t posInFOV__6CameraR8_fvectorib = 0x00120B50;   // size 0x380
constexpr uint32_t translateCamera__6CameraR8_fvectorfb = 0x00120ED0;   // size 0x3D0
constexpr uint32_t shakeCamera__6Camera = 0x001212A0;   // size 0xEC
constexpr uint32_t CCFadeIn__7CamerasPv = 0x00121390;   // size 0x1C
constexpr uint32_t CCFadeOut__7CamerasPv = 0x001213B0;   // size 0x1C
constexpr uint32_t LeaveUnifiedTask__7CamerasPv = 0x001213D0;   // size 0x1C
constexpr uint32_t canyon2InitBefore__Fv = 0x001213F0;   // size 0x8C
constexpr uint32_t canyon2InitAfter__Fv = 0x00121480;   // size 0xB4
constexpr uint32_t canyon2Update__Fv = 0x00121538;   // size 0x7EC
constexpr uint32_t updateRespawn__Fv = 0x00121D28;   // size 0x1AC
constexpr uint32_t canyon2AddEpNode__FP9_hierhead = 0x00121ED8;   // size 0x8
constexpr uint32_t canyon2RespawnAi__Fi = 0x00121EE0;   // size 0xCC
constexpr uint32_t canyon2SaveCherryBomb__FP11RigidPickup = 0x00121FB0;   // size 0xF0
constexpr uint32_t canyon2GetCherryBomb__Fv = 0x001220A0;   // size 0x280
constexpr uint32_t canyon2BossIntro__Fv = 0x00122320;   // size 0x114
constexpr uint32_t canyon2EnableBoss__Fv = 0x00122438;   // size 0x38
constexpr uint32_t canyon2FinalBattle__Fv = 0x00122470;   // size 0x74
constexpr uint32_t x__static_initialization_and_destruction_0_001224E8 = 0x001224E8;   // size 0x60
constexpr uint32_t x_GLOBAL__I_canyon2LevelProgression = 0x00122548;   // size 0x20
constexpr uint32_t x__9CarPickup = 0x00122568;   // size 0xA8
constexpr uint32_t initAfterDbLoad__9CarPickup = 0x00122610;   // size 0x90
constexpr uint32_t takeHit__9CarPickupP8_fvectorfi = 0x001226A0;   // size 0x10
constexpr uint32_t kill__9CarPickup = 0x001226B0;   // size 0x40
constexpr uint32_t grab__9CarPickupi = 0x001226F0;   // size 0x30
constexpr uint32_t drop__9CarPickup = 0x00122720;   // size 0x3C
constexpr uint32_t update__9CarPickup = 0x00122760;   // size 0xD4
constexpr uint32_t regen__9CarPickup = 0x00122838;   // size 0x40
constexpr uint32_t updateInputs__9CarPickupR7GamePad = 0x00122878;   // size 0x4C
constexpr uint32_t x__tf9CarPickup = 0x00122900;   // size 0x50
constexpr uint32_t getVehicle__9CarPickup = 0x00122950;   // size 0x8
constexpr uint32_t centralInitBefore__Fv = 0x00122958;   // size 0x8
constexpr uint32_t centralInitAfter__Fv = 0x00122960;   // size 0x118
constexpr uint32_t centralUpdate__Fv = 0x00122A78;   // size 0x8
constexpr uint32_t x__9charreset11CREATOR_KEY = 0x00122A80;   // size 0x40
constexpr uint32_t setCharacter__9charresetP7Monster = 0x00122AC0;   // size 0x64
constexpr uint32_t writeKeys__9charresetPc = 0x00122B28;   // size 0x3AA4
constexpr uint32_t x__tf9charreset = 0x001265D0;   // size 0x50
constexpr uint32_t readKey__9charresetPc = 0x00126620;   // size 0x4B34
constexpr uint32_t collisInitPoints__7Monster = 0x0012B158;   // size 0x738
constexpr uint32_t collisTestForCollisions__7Monster = 0x0012B890;   // size 0x330
constexpr uint32_t getNextCollis__FRiP19_hdSphereBundleTestiP9_hdResult = 0x0012BBC0;   // size 0xAC
constexpr uint32_t collisTestEnvironmentCollisions__7Monster = 0x0012BC70;   // size 0x1598
constexpr uint32_t collisResolveEnvCollis__7MonsterR8_fvectorR9_hdResultib = 0x0012D208;   // size 0x658
constexpr uint32_t collisResolveCsToCsCollisions__7Monster = 0x0012D860;   // size 0x748
constexpr uint32_t initBeforeDbLoad__10CrushLevel = 0x0012DFA8;   // size 0x8
constexpr uint32_t initAfterDbLoad__10CrushLevel = 0x0012DFB0;   // size 0x90
constexpr uint32_t updateNormal__10CrushLevel = 0x0012E040;   // size 0x2D8
constexpr uint32_t initSuddenDeath__10CrushLevel = 0x0012E318;   // size 0x84
constexpr uint32_t updateSuddenDeath__10CrushLevel = 0x0012E3A0;   // size 0x78
constexpr uint32_t dbInitDb__FP9_dbheader10_vramAddrs = 0x0012E418;   // size 0x1E0
constexpr uint32_t dbProcInteractive__FP9_hierheadP8_fvectorPA3_f = 0x0012E5F8;   // size 0x440
constexpr uint32_t dbGetCSForModel__FP9_hierhead = 0x0012EA38;   // size 0x48
constexpr uint32_t dbGetHPCSForModel__FP9_hierhead = 0x0012EA80;   // size 0x48
constexpr uint32_t initAfterDbLoad__14DodgeBallLevel = 0x0012EAC8;   // size 0x10C
constexpr uint32_t update__14DodgeBallLeveli = 0x0012EBD8;   // size 0x494
constexpr uint32_t ResetCountdown__14DodgeBallLevel = 0x0012F070;   // size 0xD4
constexpr uint32_t x__static_initialization_and_destruction_0_0012F148 = 0x0012F148;   // size 0x4C
constexpr uint32_t x_GLOBAL__I__14DodgeBallLevel_instance = 0x0012F198;   // size 0x20
constexpr uint32_t InitBefore__6Debris = 0x0012F1B8;   // size 0x60
constexpr uint32_t InitAfter__6Debris = 0x0012F218;   // size 0x58
constexpr uint32_t Activate__6DebrisP9ActDebrisPA3_fP8_fvectorPA3_A3_ffT3 = 0x0012F270;   // size 0x59C
constexpr uint32_t CullView__6Debrisi = 0x0012F810;   // size 0x130
constexpr uint32_t update__6Debris = 0x0012F940;   // size 0x574
constexpr uint32_t ChunkHD__6Debrisi = 0x0012FEB8;   // size 0x160
constexpr uint32_t ChunkHat__6DebrisPQ26Debris9ChunkData = 0x00130018;   // size 0x198
constexpr uint32_t x__static_initialization_and_destruction_0_001301B0 = 0x001301B0;   // size 0x90
constexpr uint32_t update__6DebrisPv = 0x00130240;   // size 0x1C
constexpr uint32_t x_GLOBAL__I_InitBefore__6Debris = 0x00130260;   // size 0x20
constexpr uint32_t Init__13Destructibles = 0x00130280;   // size 0x20
constexpr uint32_t AddDestruct__13DestructiblesiP9_hierheadP8_fvectorPA3_f = 0x001302A0;   // size 0x328
constexpr uint32_t Update__13Destructibles = 0x001305C8;   // size 0x8
constexpr uint32_t rebuildCanyon2Pillars__13Destructibles = 0x001305D0;   // size 0x84
constexpr uint32_t rebuildCapitolPillars__13Destructibles = 0x00130658;   // size 0x84
constexpr uint32_t takeHit__12DestructibleP8_fvectorfi = 0x001306E0;   // size 0x3EC
constexpr uint32_t GenericTakeHit__12DestructibleP8_fvectorfii = 0x00130AD0;   // size 0xC0
constexpr uint32_t ChangeState__12DestructibleibT2 = 0x00130B90;   // size 0x1EC
constexpr uint32_t SwitchState__12Destructible = 0x00130D80;   // size 0x14
constexpr uint32_t DispatchAction__12DestructibleP7ActHeadPA3_f = 0x00130D98;   // size 0x7C
constexpr uint32_t doDebris__12DestructibleP9ActDebrisPA3_f = 0x00130E18;   // size 0x120
constexpr uint32_t shedMonsters__12Destructible = 0x00130F38;   // size 0xFC
constexpr uint32_t getVel__13DbInteractive = 0x00131070;   // size 0xC
constexpr uint32_t x__tf12Destructible = 0x00131080;   // size 0x50
constexpr uint32_t SwitchState__12DestructiblePv = 0x001310D0;   // size 0x1C
constexpr uint32_t getHealth__12Destructible = 0x001310F0;   // size 0x8
constexpr uint32_t CurrentState__12Destructible = 0x001310F8;   // size 0xC
constexpr uint32_t getHitType__12Destructible = 0x00131108;   // size 0x8
constexpr uint32_t getId1__12Destructible = 0x00131110;   // size 0x10
constexpr uint32_t getInfo__9EnemyInfoR7MonsterT1 = 0x00131160;   // size 0x4C
constexpr uint32_t init__9EnemyInfo = 0x001311B0;   // size 0xA0
constexpr uint32_t update__9EnemyInfo = 0x00131250;   // size 0x6C4
constexpr uint32_t initBefore__9FinalBoss = 0x00131918;   // size 0x7C
constexpr uint32_t initAfter__9FinalBoss = 0x00131998;   // size 0x324
constexpr uint32_t update__9FinalBoss = 0x00131CC0;   // size 0x414
constexpr uint32_t updateMecha__9FinalBoss = 0x001320D8;   // size 0x145C
constexpr uint32_t updateJelly__9FinalBoss = 0x00133538;   // size 0xA7C
constexpr uint32_t takeHit__9FinalBossP8_fvectorfi = 0x00133FB8;   // size 0x118
constexpr uint32_t addEpNode__9FinalBossP9_hierhead = 0x001340D0;   // size 0x68
constexpr uint32_t setLocator__9FinalBossiRA3_A3_fP8_fvector = 0x00134138;   // size 0xEC
constexpr uint32_t x__static_initialization_and_destruction_0_00134228 = 0x00134228;   // size 0xCC
constexpr uint32_t x__tf9FinalBoss = 0x00134330;   // size 0x50
constexpr uint32_t getPinTrans__9FinalBoss = 0x00134380;   // size 0x8
constexpr uint32_t x_GLOBAL__I_finalBoss = 0x001343C8;   // size 0x20
constexpr uint32_t x__10FireBreathR7Monster = 0x001343E8;   // size 0x20
constexpr uint32_t Init__10FireBreath = 0x00134408;   // size 0x30
constexpr uint32_t Activate__10FireBreathfffffff = 0x00134438;   // size 0x160
constexpr uint32_t Update__10FireBreath = 0x00134598;   // size 0x94
constexpr uint32_t ApplyMint__10FireBreath = 0x00134630;   // size 0x48
constexpr uint32_t TestCollis__10FireBreath = 0x00134678;   // size 0x374
constexpr uint32_t Update__10FireBreathPv = 0x001349F0;   // size 0x1C
constexpr uint32_t TurnOffAllPeds__8PedGroup = 0x00134A10;   // size 0x84
constexpr uint32_t startCinema__8PedGroup = 0x00134A98;   // size 0x34
constexpr uint32_t endCinema__8PedGroup = 0x00134AD0;   // size 0x40
constexpr uint32_t squishGlobal__8PedGroupR8_fvectorfP7Monster = 0x00134B10;   // size 0x88
constexpr uint32_t fleeGlobal__8PedGroupR8_fvectorf = 0x00134B98;   // size 0x7C
constexpr uint32_t x__8PedGroupUcR8_fvector = 0x00134C18;   // size 0x26C
constexpr uint32_t initPath__8PedGroupPQ28PedGroup7PathDat = 0x00134E88;   // size 0x100
constexpr uint32_t scatter__8PedGroup = 0x00134F88;   // size 0x158
constexpr uint32_t sortList__8PedGroup = 0x001350E0;   // size 0x1B0
constexpr uint32_t sortIndex__8PedGroupUiUiPUc = 0x00135290;   // size 0x190
constexpr uint32_t findInRange__8PedGroupPUcUiR8_fvectorf = 0x00135420;   // size 0x2A0
constexpr uint32_t findListIndex__8PedGroupUiUiPUc = 0x001356C0;   // size 0x78
constexpr uint32_t update__8PedGroup = 0x00135738;   // size 0x8CC
constexpr uint32_t handleEnv__8PedGroup = 0x00136008;   // size 0x1F8
constexpr uint32_t isVisible__8PedGroup = 0x00136200;   // size 0x70
constexpr uint32_t squish__8PedGroupR8_fvectorfP7Monster = 0x00136270;   // size 0x13C
constexpr uint32_t flee__8PedGroupR8_fvectorfT1 = 0x001363B0;   // size 0x188
constexpr uint32_t update__8PedGroupPv = 0x00136538;   // size 0x1C
constexpr uint32_t Init__7TheGame = 0x00136558;   // size 0x8
constexpr uint32_t InitBeforeDbLoad__7TheGame = 0x00136560;   // size 0x720
constexpr uint32_t traversalCallback__7TheGameP3_csUiUiRA3_A3_fP8_fvector = 0x00136C80;   // size 0x100
constexpr uint32_t genericEventHandler__7TheGameUi = 0x00136D80;   // size 0x1D8
constexpr uint32_t InitAfterDbLoad__7TheGame = 0x00136F58;   // size 0x35C
constexpr uint32_t Update__7TheGame = 0x001372B8;   // size 0x3F0
constexpr uint32_t Update2__7TheGame = 0x001376A8;   // size 0x60
constexpr uint32_t MonsterParse__7TheGameP9_hierheadP8_fvector = 0x00137708;   // size 0x104
constexpr uint32_t AddMonster__7TheGameP3_csii = 0x00137810;   // size 0x124
constexpr uint32_t GetMonsterFromName__7TheGameii = 0x00137938;   // size 0x84
constexpr uint32_t SetPlayerMonster__7TheGameiiiii = 0x001379C0;   // size 0x14C
constexpr uint32_t SetAIMonster__7TheGameiiii = 0x00137B10;   // size 0x334
constexpr uint32_t gameInitCamera__7TheGameii = 0x00137E48;   // size 0x4C
constexpr uint32_t GetCameraMaxHeight__7TheGameP8_fvector = 0x00137E98;   // size 0x14
constexpr uint32_t gameResolveCollisions__7TheGame = 0x00137EB0;   // size 0x1B4
constexpr uint32_t gameResolveLifeAndDeath__7TheGame = 0x00138068;   // size 0x7D8
constexpr uint32_t gameCheckForCloseCombat__7TheGame = 0x00138840;   // size 0x4C8
constexpr uint32_t SetGravity__7TheGamef = 0x00138D08;   // size 0x10
constexpr uint32_t SetOkToUnify__7TheGame = 0x00138D18;   // size 0x4C
constexpr uint32_t gameGetStartPoint__7TheGameP7Monster = 0x00138D68;   // size 0x2F4
constexpr uint32_t getClosestMonster__7TheGameR8_fvectorfRf = 0x00139060;   // size 0x94
constexpr uint32_t getClosestPlayer__7TheGameR8_fvectorfRf = 0x001390F8;   // size 0xA4
constexpr uint32_t GetNumAIsAlive__7TheGame = 0x001391A0;   // size 0x5C
constexpr uint32_t ResetLevel__7TheGame = 0x00139200;   // size 0x16C
constexpr uint32_t UnpauseLevel__7TheGame = 0x00139370;   // size 0xC4
constexpr uint32_t UpdatePadTweaks__7TheGame = 0x00139438;   // size 0xF8
constexpr uint32_t gameReestablishViews__7TheGame = 0x00139530;   // size 0x170
constexpr uint32_t monsterPush__7TheGamePP9_hierheadUi = 0x001396A0;   // size 0x3C
constexpr uint32_t monsterPop__7TheGameP13_monsterstackPP9_hierhead = 0x001396E0;   // size 0x6C
constexpr uint32_t fadeOutAndIn__7TheGamei = 0x00139750;   // size 0xC8
constexpr uint32_t updateFadeOutAndIn__7TheGame = 0x00139818;   // size 0x320
constexpr uint32_t fadeOut__7TheGamei = 0x00139B38;   // size 0x7C
constexpr uint32_t updateFadeOut__7TheGame = 0x00139BB8;   // size 0x244
constexpr uint32_t SetVibration__7TheGameib = 0x00139E00;   // size 0x70
constexpr uint32_t AddCrushMessage__7TheGamei = 0x00139E70;   // size 0x8C
constexpr uint32_t x__static_initialization_and_destruction_0_00139F00 = 0x00139F00;   // size 0x74
constexpr uint32_t updateFadeOutAndIn__7TheGamePv = 0x00139FA0;   // size 0x1C
constexpr uint32_t updateFadeOut__7TheGamePv = 0x00139FC0;   // size 0x1C
constexpr uint32_t x_GLOBAL__I_resetObj = 0x00139FE0;   // size 0x20
constexpr uint32_t clearInputs__7GamePad = 0x0013A000;   // size 0x1C
constexpr uint32_t loadPadInputs__7GamePadi = 0x0013A020;   // size 0x514
constexpr uint32_t rewind__17GamePadClipPlayer = 0x0013A538;   // size 0x44
constexpr uint32_t update__17GamePadClipPlayerR7GamePad = 0x0013A580;   // size 0xA8
constexpr uint32_t x__11GlobalReset = 0x0013A628;   // size 0x54
constexpr uint32_t writeKeys__11GlobalResetPc = 0x0013A680;   // size 0x40C
constexpr uint32_t readKey__11GlobalResetPc = 0x0013AA90;   // size 0x930
constexpr uint32_t x__tf11GlobalReset = 0x0013B3C0;   // size 0x50
constexpr uint32_t x__12StateGrapple = 0x0013B410;   // size 0xF4
constexpr uint32_t transitionOK__12StateGrapple = 0x0013B508;   // size 0x74
constexpr uint32_t transitionFeasible__12StateGrapple = 0x0013B580;   // size 0x194
constexpr uint32_t transitionInto__12StateGrapple = 0x0013B718;   // size 0x38
constexpr uint32_t update__12StateGrapple = 0x0013B750;   // size 0x390
constexpr uint32_t handlePreemption__12StateGrappleP12MonsterState = 0x0013BAE0;   // size 0x30
constexpr uint32_t enterSubState__12StateGrappleQ212StateGrapple8SubState = 0x0013BB10;   // size 0x154
constexpr uint32_t detach__12StateGrapple = 0x0013BC68;   // size 0x3C
constexpr uint32_t release__12StateGrappleb = 0x0013BCA8;   // size 0x16C
constexpr uint32_t x__13StateGrappled = 0x0013BE18;   // size 0xAC
constexpr uint32_t transitionOK__13StateGrappledi = 0x0013BEC8;   // size 0x418
constexpr uint32_t transitionInto__13StateGrappled = 0x0013C2E0;   // size 0xCC
constexpr uint32_t update__13StateGrappled = 0x0013C3B0;   // size 0x404
constexpr uint32_t healthDeltaAchieved__13StateGrappledf = 0x0013C7B8;   // size 0x34
constexpr uint32_t enterSubState__13StateGrappledQ213StateGrappled8SubState = 0x0013C7F0;   // size 0x1A0
constexpr uint32_t detach__13StateGrappled = 0x0013C990;   // size 0xE0
constexpr uint32_t breakFree__13StateGrappled = 0x0013CA70;   // size 0x180
constexpr uint32_t isOverhead__13StateGrappled = 0x0013CBF0;   // size 0xC
constexpr uint32_t handlePreemption__13StateGrappledP12MonsterState = 0x0013CC00;   // size 0x54
constexpr uint32_t x__tf12StateGrapple = 0x0013CC58;   // size 0x50
constexpr uint32_t getSubState__12StateGrapple = 0x0013CCA8;   // size 0x8
constexpr uint32_t getRecoilFields__12StateGrapple = 0x0013CCB0;   // size 0x8
constexpr uint32_t getAttachWindowStart__12StateGrapple = 0x0013CCB8;   // size 0x8
constexpr uint32_t getAttachWindowEnd__12StateGrapple = 0x0013CCC0;   // size 0x8
constexpr uint32_t x__tf13StateGrappled = 0x0013CCC8;   // size 0x50
constexpr uint32_t setStruggleDisable__13StateGrappledi = 0x0013CD18;   // size 0x14
constexpr uint32_t getSubState__13StateGrappled = 0x0013CD30;   // size 0x8
constexpr uint32_t x__20StateGrappleOHAttack = 0x0013CD38;   // size 0x98
constexpr uint32_t transitionOK__20StateGrappleOHAttack = 0x0013CDD0;   // size 0x50
constexpr uint32_t transitionFeasible__20StateGrappleOHAttack = 0x0013CE20;   // size 0x100
constexpr uint32_t transitionInto__20StateGrappleOHAttack = 0x0013CF20;   // size 0x1A4
constexpr uint32_t update__20StateGrappleOHAttack = 0x0013D0C8;   // size 0x2BC
constexpr uint32_t x__19StateMonkeyOHAttack = 0x0013D388;   // size 0xA8
constexpr uint32_t transitionOK__19StateMonkeyOHAttack = 0x0013D430;   // size 0x50
constexpr uint32_t transitionFeasible__19StateMonkeyOHAttack = 0x0013D480;   // size 0x104
constexpr uint32_t transitionInto__19StateMonkeyOHAttack = 0x0013D588;   // size 0x60
constexpr uint32_t update__19StateMonkeyOHAttack = 0x0013D5E8;   // size 0x508
constexpr uint32_t startAttack__19StateMonkeyOHAttack11MonsterAnim = 0x0013DAF0;   // size 0x110
constexpr uint32_t inWindUp__11AttackState = 0x0013DC00;   // size 0x8
constexpr uint32_t inWindDown__11AttackState = 0x0013DC08;   // size 0x8
constexpr uint32_t x__tf20StateGrappleOHAttack = 0x0013DC10;   // size 0x50
constexpr uint32_t x__tf19StateMonkeyOHAttack = 0x0013DC60;   // size 0x50
constexpr uint32_t x__tf11AttackState = 0x0013DCB0;   // size 0x50
constexpr uint32_t x__16StateGrappleLift = 0x0013DD00;   // size 0x54
constexpr uint32_t transitionOK__16StateGrappleLift = 0x0013DD58;   // size 0x7C
constexpr uint32_t transitionInto__16StateGrappleLift = 0x0013DDD8;   // size 0x248
constexpr uint32_t update__16StateGrappleLift = 0x0013E020;   // size 0x164
constexpr uint32_t x__17StateGrappleThrow = 0x0013E188;   // size 0x70
constexpr uint32_t transitionOK__17StateGrappleThrow = 0x0013E1F8;   // size 0x48
constexpr uint32_t transitionInto__17StateGrappleThrow = 0x0013E240;   // size 0x54
constexpr uint32_t update__17StateGrappleThrow = 0x0013E298;   // size 0xE8
constexpr uint32_t throwToTarget__17StateGrappleThrowR8_fvector = 0x0013E380;   // size 0x14C
constexpr uint32_t handlePreemption__17StateGrappleThrowP12MonsterState = 0x0013E4D0;   // size 0x20
constexpr uint32_t x__18StateGrappleThrow1 = 0x0013E4F0;   // size 0x70
constexpr uint32_t transitionOK__18StateGrappleThrow1 = 0x0013E560;   // size 0x48
constexpr uint32_t transitionInto__18StateGrappleThrow1 = 0x0013E5A8;   // size 0x54
constexpr uint32_t update__18StateGrappleThrow1 = 0x0013E600;   // size 0x1D4
constexpr uint32_t x__tf16StateGrappleLift = 0x0013E7E8;   // size 0x50
constexpr uint32_t x__tf17StateGrappleThrow = 0x0013E838;   // size 0x50
constexpr uint32_t x__tf18StateGrappleThrow1 = 0x0013E888;   // size 0x50
constexpr uint32_t x__10HeliPickupb = 0x0013E928;   // size 0x9C
constexpr uint32_t initAfterDbLoad__10HeliPickup = 0x0013E9C8;   // size 0xAC
constexpr uint32_t takeHit__10HeliPickupP8_fvectorfi = 0x0013EA78;   // size 0x128
constexpr uint32_t grab__10HeliPickupi = 0x0013EBA0;   // size 0x38
constexpr uint32_t drop__10HeliPickup = 0x0013EBD8;   // size 0x3C
constexpr uint32_t kill__10HeliPickup = 0x0013EC18;   // size 0xA8
constexpr uint32_t update__10HeliPickup = 0x0013ECC0;   // size 0x120
constexpr uint32_t regen__10HeliPickup = 0x0013EDE0;   // size 0x50
constexpr uint32_t updateAttackBehavior__10HeliPickupR7GamePad = 0x0013EE30;   // size 0x5B8
constexpr uint32_t enterState__10HeliPickupQ210HeliPickup5State = 0x0013F3E8;   // size 0x224
constexpr uint32_t x__tf10HeliPickup = 0x0013F648;   // size 0x50
constexpr uint32_t getVel__10HeliPickup = 0x0013F698;   // size 0x8
constexpr uint32_t x__11HeliVehicle = 0x0013F6A0;   // size 0x60
constexpr uint32_t setCs__11HeliVehicleP3_cs = 0x0013F700;   // size 0x34
constexpr uint32_t setPos__11HeliVehicleR8_fvector = 0x0013F738;   // size 0x18
constexpr uint32_t updateControls__11HeliVehicleR7GamePad = 0x0013F750;   // size 0x1E4
constexpr uint32_t updatePosition__11HeliVehicle = 0x0013F938;   // size 0xBC
constexpr uint32_t updateCollision__11HeliVehicle = 0x0013F9F8;   // size 0x50
constexpr uint32_t updateElevator__11HeliVehiclef = 0x0013FA48;   // size 0x60
constexpr uint32_t updateHeading__11HeliVehiclef = 0x0013FAA8;   // size 0xC4
constexpr uint32_t updateTilt__11HeliVehicleff = 0x0013FB70;   // size 0xE4
constexpr uint32_t orientUpToNormal__11HeliVehicleR8_fvectorf = 0x0013FC58;   // size 0xE4
constexpr uint32_t x__14HeliVehicleNavR11HeliVehicleP7PathNet = 0x0013FD40;   // size 0x34
constexpr uint32_t init__14HeliVehicleNav = 0x0013FD78;   // size 0x68
constexpr uint32_t update__14HeliVehicleNavR7GamePad = 0x0013FDE0;   // size 0x1B8
constexpr uint32_t updateSteering__14HeliVehicleNavR7GamePad = 0x0013FF98;   // size 0x1E0
constexpr uint32_t disable__14HeliVehicleNav = 0x00140178;   // size 0x8
constexpr uint32_t wander__14HeliVehicleNav = 0x00140180;   // size 0x74
constexpr uint32_t pinWander__14HeliVehicleNavR8_fvectorf = 0x001401F8;   // size 0x8C
constexpr uint32_t seek__14HeliVehicleNavR8_fvectorf = 0x00140288;   // size 0x1C
constexpr uint32_t target__14HeliVehicleNavR8_fvector = 0x001402A8;   // size 0x10
constexpr uint32_t updateSeek__14HeliVehicleNav = 0x001402B8;   // size 0x16C
constexpr uint32_t targetPin__14HeliVehicleNavR8_fvector = 0x00140428;   // size 0x108
constexpr uint32_t flee__14HeliVehicleNavR8_fvectorff = 0x00140530;   // size 0x80
constexpr uint32_t updateFlee__14HeliVehicleNav = 0x001405B0;   // size 0xC8
constexpr uint32_t tiltTo__14HeliVehicleNavR8_fvector = 0x00140678;   // size 0x44
constexpr uint32_t faceTo__14HeliVehicleNavR8_fvectorf = 0x001406C0;   // size 0xA0
constexpr uint32_t faceVel__14HeliVehicleNav = 0x00140760;   // size 0x64
constexpr uint32_t elevateTo__14HeliVehicleNavff = 0x001407C8;   // size 0xA8
constexpr uint32_t unalignedAvoid__14HeliVehicleNav = 0x00140870;   // size 0x4E0
constexpr uint32_t avoidBubble__14HeliVehicleNav = 0x00140D50;   // size 0x170
constexpr uint32_t followPath__14HeliVehicleNav = 0x00140EC0;   // size 0x288
constexpr uint32_t nextPathPoint__14HeliVehicleNav = 0x00141148;   // size 0xBC
constexpr uint32_t computePathInfo__14HeliVehicleNavP8PathNodeP4Path = 0x00141208;   // size 0x84
constexpr uint32_t getRandPath__14HeliVehicleNav = 0x00141290;   // size 0xB8
constexpr uint32_t getSeekPath__14HeliVehicleNavR8_fvector = 0x00141348;   // size 0x160
constexpr uint32_t getFleePath__14HeliVehicleNavR8_fvector = 0x001414A8;   // size 0x160
constexpr uint32_t init__9HomingBug = 0x00141608;   // size 0x114
constexpr uint32_t addInstance__9HomingBugP9_hierhead = 0x00141720;   // size 0x3C
constexpr uint32_t getInstance__9HomingBugP7Monster = 0x00141760;   // size 0x4C
constexpr uint32_t isAvailable__9HomingBugP7Monster = 0x001417B0;   // size 0x40
constexpr uint32_t initAfterDbLoad__9HomingBug = 0x001417F0;   // size 0x50
constexpr uint32_t x__9HomingBug = 0x00141840;   // size 0x5C
constexpr uint32_t registerModel__9HomingBugP9_hierhead = 0x001418A0;   // size 0xA4
constexpr uint32_t initAfter__9HomingBug = 0x00141948;   // size 0x7C
constexpr uint32_t spawn__9HomingBug = 0x001419C8;   // size 0xD4
constexpr uint32_t launch__9HomingBugP7Monster = 0x00141AA0;   // size 0x1F4
constexpr uint32_t update__9HomingBug = 0x00141C98;   // size 0x17C
constexpr uint32_t updateOrientFromDir__9HomingBugR8_fvector = 0x00141E18;   // size 0xEC
constexpr uint32_t takeHit__9HomingBugP8_fvectorfi = 0x00141F08;   // size 0x68
constexpr uint32_t updateSeek__9HomingBug = 0x00141F70;   // size 0x714
constexpr uint32_t checkAttach__9HomingBug = 0x00142688;   // size 0x80
constexpr uint32_t attach__9HomingBug = 0x00142708;   // size 0x204
constexpr uint32_t updateAttached__9HomingBug = 0x00142910;   // size 0x198
constexpr uint32_t updateFalling__9HomingBug = 0x00142AA8;   // size 0x154
constexpr uint32_t disable__9HomingBug = 0x00142C00;   // size 0xCC
constexpr uint32_t findTarget__9HomingBugR8_fvectorT1ff = 0x00142CD0;   // size 0x11C
constexpr uint32_t inView__9HomingBugP7Monsterffb = 0x00142DF0;   // size 0xC8
constexpr uint32_t x__static_initialization_and_destruction_0_00142EB8 = 0x00142EB8;   // size 0x64
constexpr uint32_t x__tf9HomingBug = 0x00142F68;   // size 0x50
constexpr uint32_t update__9HomingBugPv = 0x00142FB8;   // size 0x1C
constexpr uint32_t x_GLOBAL__I__9HomingBug_s_numBugs = 0x00143018;   // size 0x20
constexpr uint32_t initBefore__3Hudi = 0x00143038;   // size 0x14C
constexpr uint32_t initAfter__3Hudi = 0x00143188;   // size 0x40C
constexpr uint32_t initForReplay__3Hud = 0x00143598;   // size 0x100
constexpr uint32_t update__3Hud = 0x00143698;   // size 0xE04
constexpr uint32_t parseHudElements__3HudP9_hierhead = 0x001444A0;   // size 0x200
constexpr uint32_t print__3Hud = 0x001446A0;   // size 0x8
constexpr uint32_t buildPacketHead__3HudP6QwData = 0x001446A8;   // size 0x78
constexpr uint32_t dmaPacket__3Hud = 0x00144720;   // size 0x74
constexpr uint32_t buildSeparator__3Hud = 0x00144798;   // size 0x1B0
constexpr uint32_t addHealthIndicators__3Hud = 0x00144948;   // size 0x460
constexpr uint32_t drawHealthIcon__3HudP7Monsteri = 0x00144DA8;   // size 0x4F8
constexpr uint32_t drawBossHealthBar__3Hudf = 0x001452A0;   // size 0x240
constexpr uint32_t registerComboHit__3Hud = 0x001454E0;   // size 0xE0
constexpr uint32_t registerStaminaCredit__3Hudi = 0x001455C0;   // size 0x134
constexpr uint32_t registerHealthCredit__3Hudi = 0x001456F8;   // size 0xD4
constexpr uint32_t addMessage__3Hudii = 0x001457D0;   // size 0x218
constexpr uint32_t updateMessage__3Hud = 0x001459E8;   // size 0x58C
constexpr uint32_t addTextBoxMessage__3Hudi = 0x00145F78;   // size 0xB8
constexpr uint32_t updateTextBoxMessage__3Hud = 0x00146030;   // size 0xE08
constexpr uint32_t initBefore__12Letterboxing = 0x00146E38;   // size 0x10
constexpr uint32_t initAfter__12Letterboxing = 0x00146E48;   // size 0x58
constexpr uint32_t setEnabled__12Letterboxingb = 0x00146EA0;   // size 0x138
constexpr uint32_t setLetterboxCS__12LetterboxingP9_hierhead = 0x00146FD8;   // size 0x30
constexpr uint32_t update__12Letterboxing = 0x00147008;   // size 0x10C
constexpr uint32_t x__static_initialization_and_destruction_0_00147118 = 0x00147118;   // size 0x28
constexpr uint32_t x_GLOBAL__I_gLowStaminaThreshold = 0x00147140;   // size 0x20
constexpr uint32_t init__12Interactives = 0x00147160;   // size 0x38
constexpr uint32_t addInteractive__12InteractivesP13DbInteractive = 0x00147198;   // size 0x2C
constexpr uint32_t setInteractive__12InteractivesiP13DbInteractive = 0x001471C8;   // size 0x18
constexpr uint32_t getInteractive__12Interactivesi = 0x001471E0;   // size 0x60
constexpr uint32_t dispatchHit__12InteractivesP8_fvectorfii = 0x00147240;   // size 0x5C
constexpr uint32_t create__8HitEvent = 0x001472A0;   // size 0x38
constexpr uint32_t x__10HitHistory = 0x001472D8;   // size 0x10
constexpr uint32_t reset__10HitHistory = 0x001472E8;   // size 0xC
constexpr uint32_t newHit__10HitHistoryib = 0x001472F8;   // size 0xD0
constexpr uint32_t creditHit__11HurtHistoryif = 0x001473C8;   // size 0x84
constexpr uint32_t islandInitBefore__Fv = 0x00147450;   // size 0x30
constexpr uint32_t islandInitAfter__Fv = 0x00147480;   // size 0x110
constexpr uint32_t islandUpdate__Fv = 0x00147590;   // size 0x290
constexpr uint32_t islandEruptVolcano__Fii = 0x00147820;   // size 0x1C4
constexpr uint32_t islandFireLava__FP8_fvectori = 0x001479E8;   // size 0x12C
constexpr uint32_t x__static_initialization_and_destruction_0_00147B18 = 0x00147B18;   // size 0x30
constexpr uint32_t x_GLOBAL__I_islandInitBefore__Fv = 0x00147B48;   // size 0x20
constexpr uint32_t x__9McMessage = 0x00147B68;   // size 0x3C
constexpr uint32_t updateSync__9McMessageR7McState = 0x00147BA8;   // size 0x3C
constexpr uint32_t updateAsync__9McMessageR7McState = 0x00147BE8;   // size 0x3C
constexpr uint32_t x__10McLoadFile = 0x00147C28;   // size 0x24
constexpr uint32_t setFile__10McLoadFilePcPvi = 0x00147C50;   // size 0x10
constexpr uint32_t updateSync__10McLoadFileR7McState = 0x00147C60;   // size 0xE8
constexpr uint32_t updateAsync__10McLoadFileR7McState = 0x00147D48;   // size 0x148
constexpr uint32_t x__10McSaveFile = 0x00147E90;   // size 0x24
constexpr uint32_t setFile__10McSaveFilePcPvi = 0x00147EB8;   // size 0x10
constexpr uint32_t updateSync__10McSaveFileR7McState = 0x00147EC8;   // size 0xD0
constexpr uint32_t updateAsync__10McSaveFileR7McState = 0x00147F98;   // size 0x148
constexpr uint32_t x__19McListRootDirectory = 0x001480E0;   // size 0x1C
constexpr uint32_t updateSync__19McListRootDirectoryR7McState = 0x00148100;   // size 0xFC
constexpr uint32_t updateAsync__19McListRootDirectoryR7McState = 0x00148200;   // size 0x12C
constexpr uint32_t findFile__19McListRootDirectoryPc = 0x00148330;   // size 0x7C
constexpr uint32_t x__15McDoesFileExist = 0x001483B0;   // size 0x20
constexpr uint32_t updateSync__15McDoesFileExistR7McState = 0x001483D0;   // size 0x68
constexpr uint32_t updateAsync__15McDoesFileExistR7McState = 0x00148438;   // size 0x98
constexpr uint32_t updateSync__13McUpdateStateR7McState = 0x001484D0;   // size 0xB8
constexpr uint32_t updateAsync__13McUpdateStateR7McState = 0x00148588;   // size 0xE8
constexpr uint32_t updateSync__8McFormatR7McState = 0x00148670;   // size 0x60
constexpr uint32_t updateAsync__8McFormatR7McState = 0x001486D0;   // size 0x98
constexpr uint32_t createIconSysSync__FR7McState = 0x00148768;   // size 0x21C
constexpr uint32_t createIconSysAsync__FR7McStateRi = 0x00148988;   // size 0x30C
constexpr uint32_t updateSync__14McScrambleIconR7McState = 0x00148C98;   // size 0x114
constexpr uint32_t updateAsync__14McScrambleIconR7McState = 0x00148DB0;   // size 0x1A0
constexpr uint32_t checkForFilesSync__14McSetupGameDirR7McState = 0x00148F50;   // size 0xE8
constexpr uint32_t checkForFilesAsync__14McSetupGameDirR7McStateRi = 0x00149038;   // size 0x11C
constexpr uint32_t doesFileExist__14McSetupGameDirPc = 0x00149158;   // size 0x7C
constexpr uint32_t updateSync__14McSetupGameDirR7McState = 0x001491D8;   // size 0x10C
constexpr uint32_t updateAsync__14McSetupGameDirR7McState = 0x001492E8;   // size 0x1CC
constexpr uint32_t createIconFileAsync__14McSetupGameDirR7McStateRi = 0x001494B8;   // size 0x210
constexpr uint32_t createIconFile__14McSetupGameDirR7McState = 0x001496C8;   // size 0x1F8
constexpr uint32_t x__6McSync = 0x001498C0;   // size 0x28
constexpr uint32_t clear__6McSync = 0x001498E8;   // size 0xC
constexpr uint32_t addTask__6McSyncP6McTask = 0x001498F8;   // size 0x20
constexpr uint32_t sync__6McSyncR7McState = 0x00149918;   // size 0x25C
constexpr uint32_t x__static_initialization_and_destruction_0_00149B78 = 0x00149B78;   // size 0x30
constexpr uint32_t getCardNum__7McState = 0x00149BA8;   // size 0x8
constexpr uint32_t clear__6McTask = 0x00149BB0;   // size 0x8
constexpr uint32_t updateIdle__6McTaskR7McState = 0x00149BB8;   // size 0x8
constexpr uint32_t x__tf9McMessage = 0x00149BC0;   // size 0x50
constexpr uint32_t addString__9McMessagei = 0x00149C10;   // size 0x30
constexpr uint32_t clearStrings__9McMessage = 0x00149C40;   // size 0x1C
constexpr uint32_t replaceMessage__9McMessageii = 0x00149C60;   // size 0x1C
constexpr uint32_t updateIdle__9McMessageR7McState = 0x00149C80;   // size 0x28
constexpr uint32_t getNumItems__9McMessage = 0x00149CA8;   // size 0x8
constexpr uint32_t x__tf10McLoadFile = 0x00149CB0;   // size 0x50
constexpr uint32_t x__tf10McSaveFile = 0x00149D00;   // size 0x50
constexpr uint32_t x__tf19McListRootDirectory = 0x00149D50;   // size 0x50
constexpr uint32_t x__tf15McDoesFileExist = 0x00149DA0;   // size 0x50
constexpr uint32_t x__tf13McUpdateState = 0x00149DF0;   // size 0x50
constexpr uint32_t x__tf8McFormat = 0x00149E40;   // size 0x50
constexpr uint32_t x__tf14McScrambleIcon = 0x00149E90;   // size 0x50
constexpr uint32_t x__14McScrambleIcon = 0x00149EE0;   // size 0x1C
constexpr uint32_t x__tf14McSetupGameDir = 0x00149F00;   // size 0x50
constexpr uint32_t clear__14McSetupGameDir = 0x00149F50;   // size 0xC
constexpr uint32_t x__tf6McTask = 0x00149F60;   // size 0x3C
constexpr uint32_t x_GLOBAL__I_g_numCharsInTitleBeforeLineBreak = 0x00149FA0;   // size 0x20
constexpr uint32_t init__9McButtonsb = 0x00149FC0;   // size 0x2C
constexpr uint32_t initButton__9McButtonsP9_hierhead = 0x00149FF0;   // size 0xCC
constexpr uint32_t startFrame__9McButtonsv = 0x0014A0C0;   // size 0x64
constexpr uint32_t release__9McButtonsv = 0x0014A128;   // size 0x88
constexpr uint32_t drawButtonsForString__9McButtonsPciib = 0x0014A1B0;   // size 0x378
constexpr uint32_t init__7McStatei = 0x0014A528;   // size 0x54
constexpr uint32_t setPlayer__7McStateib = 0x0014A580;   // size 0x54
constexpr uint32_t setError__7McStateQ27McState6Errors = 0x0014A5D8;   // size 0x10
constexpr uint32_t x__6McPage = 0x0014A5E8;   // size 0x70
constexpr uint32_t getString__6McPagei = 0x0014A658;   // size 0x18
constexpr uint32_t makeCurrentFont__Q26McPage8FontInfoi = 0x0014A670;   // size 0x58
constexpr uint32_t printString__6McPageiiRQ26McPage9MenuItemsbN54 = 0x0014A6C8;   // size 0xF0
constexpr uint32_t printStringCentered__6McPageiiRQ26McPage9MenuItemsbN54 = 0x0014A7B8;   // size 0xF0
constexpr uint32_t onEnter__6McPage = 0x0014A8A8;   // size 0x8
constexpr uint32_t reset__6McPage = 0x0014A8B0;   // size 0xC
constexpr uint32_t stateIsOk__6McPageR7McState = 0x0014A8C0;   // size 0x8
constexpr uint32_t update__6McPageR7McState = 0x0014A8C8;   // size 0x50
constexpr uint32_t updateMenuPos__6McPageR7McState = 0x0014A918;   // size 0x208
constexpr uint32_t checkForSelection__6McPageR7McState = 0x0014AB20;   // size 0x1C8
constexpr uint32_t setSelectStart__6McPage = 0x0014ACE8;   // size 0xC
constexpr uint32_t onBack__6McPage = 0x0014ACF8;   // size 0x24
constexpr uint32_t addString__6McPageibN42 = 0x0014AD20;   // size 0x84
constexpr uint32_t addString__6McPagePcbN42 = 0x0014ADA8;   // size 0x84
constexpr uint32_t clearStrings__6McPage = 0x0014AE30;   // size 0x74
constexpr uint32_t replaceMessage__6McPageii = 0x0014AEA8;   // size 0x2C
constexpr uint32_t drawMenu__6McPage = 0x0014AED8;   // size 0x11C
constexpr uint32_t setupShellFont__6McPage = 0x0014AFF8;   // size 0x224
constexpr uint32_t setupInGameFont__6McPage = 0x0014B220;   // size 0x1C
constexpr uint32_t onEnter__9McPageMgr = 0x0014B240;   // size 0x70
constexpr uint32_t onExit__9McPageMgr = 0x0014B2B0;   // size 0x98
constexpr uint32_t exitMgr__9McPageMgr = 0x0014B348;   // size 0x10
constexpr uint32_t globalInit__9McPageMgr = 0x0014B358;   // size 0x8
constexpr uint32_t initButton__9McPageMgrP9_hierhead = 0x0014B360;   // size 0x1C
constexpr uint32_t update__9McPageMgr = 0x0014B380;   // size 0x204
constexpr uint32_t doStateUpdate__9McPageMgr = 0x0014B588;   // size 0x130
constexpr uint32_t doMonitorCardCheck__9McPageMgr = 0x0014B6B8;   // size 0xE4
constexpr uint32_t addPage__9McPageMgrP6McPage = 0x0014B7A0;   // size 0x70
constexpr uint32_t getCurPage__9McPageMgr = 0x0014B810;   // size 0x30
constexpr uint32_t getPrevPage__9McPageMgr = 0x0014B840;   // size 0x30
constexpr uint32_t nextPage__9McPageMgr = 0x0014B870;   // size 0x88
constexpr uint32_t updateState__9McPageMgr = 0x0014B8F8;   // size 0x1C
constexpr uint32_t prevPage__9McPageMgrb = 0x0014B918;   // size 0xDC
constexpr uint32_t setStackIdx__9McPageMgrib = 0x0014B9F8;   // size 0x90
constexpr uint32_t init__12McBackground = 0x0014BA88;   // size 0x130
constexpr uint32_t onExit__12McBackground = 0x0014BBB8;   // size 0x94
constexpr uint32_t setDestDims__12McBackgroundfff = 0x0014BC50;   // size 0x10
constexpr uint32_t update__12McBackground = 0x0014BC60;   // size 0x3E4
constexpr uint32_t updatewithTextBox__12McBackgroundf = 0x0014C048;   // size 0x21C
constexpr uint32_t init__9McTextBox = 0x0014C268;   // size 0x1C
constexpr uint32_t update__9McTextBox = 0x0014C288;   // size 0x348
constexpr uint32_t addTextString__9McTextBoxPciibPQ26McPage8FontInfo = 0x0014C5D0;   // size 0x11C
constexpr uint32_t setDontFade__9McTextBoxb = 0x0014C6F0;   // size 0x8
constexpr uint32_t x__Q29McPageMgr5Input = 0x0014C6F8;   // size 0x58
constexpr uint32_t update__Q29McPageMgr5Input = 0x0014C750;   // size 0x138
constexpr uint32_t clear__Q29McPageMgr5Input = 0x0014C888;   // size 0x70
constexpr uint32_t x__static_initialization_and_destruction_0_0014C8F8 = 0x0014C8F8;   // size 0x68
constexpr uint32_t x__tf6McPage = 0x0014C968;   // size 0x3C
constexpr uint32_t onExit__6McPage = 0x0014C9A8;   // size 0x8
constexpr uint32_t onSelection__6McPagei = 0x0014C9B0;   // size 0x8
constexpr uint32_t shouldSkipOnReturn__6McPage = 0x0014C9B8;   // size 0x8
constexpr uint32_t shouldMonitorCard__6McPage = 0x0014C9C0;   // size 0x8
constexpr uint32_t whichBackgroundPoly__6McPage = 0x0014C9C8;   // size 0x8
constexpr uint32_t shouldFadeBackground__6McPage = 0x0014C9D0;   // size 0x8
constexpr uint32_t shellBackgroundIsVisible__6McPage = 0x0014C9D8;   // size 0x8
constexpr uint32_t setBaseText__6McPagei = 0x0014C9E0;   // size 0x8
constexpr uint32_t resetMenuPos__6McPage = 0x0014C9E8;   // size 0xC
constexpr uint32_t getWasInitialized__6McPage = 0x0014C9F8;   // size 0x8
constexpr uint32_t setWasInitialized__6McPageb = 0x0014CA00;   // size 0x8
constexpr uint32_t getNumItems__6McPage = 0x0014CA08;   // size 0x8
constexpr uint32_t x_GLOBAL__I__9McPageMgr_global = 0x0014CA10;   // size 0x20
constexpr uint32_t x__17MilitaryFormation = 0x0014CA30;   // size 0x3C
constexpr uint32_t setLeader__17MilitaryFormationP14MilitaryPickup = 0x0014CA70;   // size 0x30
constexpr uint32_t addMember__17MilitaryFormationP14MilitaryPickup = 0x0014CAA0;   // size 0x68
constexpr uint32_t remove__17MilitaryFormationP14MilitaryPickup = 0x0014CB08;   // size 0x148
constexpr uint32_t initFormation__17MilitaryFormation = 0x0014CC50;   // size 0x98
constexpr uint32_t updatePositions__13LineFormation = 0x0014CCE8;   // size 0x74
constexpr uint32_t setLeader__12BoxFormationP14MilitaryPickup = 0x0014CD60;   // size 0x44
constexpr uint32_t addMember__12BoxFormationP14MilitaryPickup = 0x0014CDA8;   // size 0xFC
constexpr uint32_t remove__12BoxFormationP14MilitaryPickup = 0x0014CEA8;   // size 0x244
constexpr uint32_t updatePositions__12BoxFormation = 0x0014D0F0;   // size 0x164
constexpr uint32_t reverseFormation__12BoxFormation = 0x0014D258;   // size 0x240
constexpr uint32_t initFormation__12BoxFormation = 0x0014D498;   // size 0x1D4
constexpr uint32_t find__12BoxFormationP14MilitaryPickupRiT2 = 0x0014D670;   // size 0x94
constexpr uint32_t x__tf17MilitaryFormation = 0x0014D708;   // size 0x3C
constexpr uint32_t reverseFormation__17MilitaryFormation = 0x0014D748;   // size 0x8
constexpr uint32_t getLeader__17MilitaryFormation = 0x0014D750;   // size 0x8
constexpr uint32_t x__tf13LineFormation = 0x0014D758;   // size 0x6C
constexpr uint32_t x__13LineFormation = 0x0014D7C8;   // size 0x34
constexpr uint32_t setSpacing__13LineFormationf = 0x0014D800;   // size 0x8
constexpr uint32_t x__tf12BoxFormation = 0x0014D808;   // size 0x6C
constexpr uint32_t x__12BoxFormation = 0x0014D878;   // size 0x34
constexpr uint32_t setWidth__12BoxFormationi = 0x0014D8B0;   // size 0x8
constexpr uint32_t setSpacing__12BoxFormationff = 0x0014D8B8;   // size 0xC
constexpr uint32_t get__12BoxFormationii = 0x0014D8C8;   // size 0x1C
constexpr uint32_t grab__14MilitaryPickupi = 0x0014D8E8;   // size 0x40
constexpr uint32_t kill__14MilitaryPickup = 0x0014D928;   // size 0x48
constexpr uint32_t breakFormation__14MilitaryPickup = 0x0014D970;   // size 0x34
constexpr uint32_t x__tf14MilitaryPickup = 0x0014D9E0;   // size 0x50
constexpr uint32_t x__14MilitaryPickup = 0x0014DA30;   // size 0x34
constexpr uint32_t getState__14MilitaryPickup = 0x0014DA68;   // size 0x8
constexpr uint32_t getFocus__14MilitaryPickup = 0x0014DA70;   // size 0x8
constexpr uint32_t leadFormation__14MilitaryPickup = 0x0014DA78;   // size 0x8
constexpr uint32_t followFormation__14MilitaryPickup = 0x0014DA80;   // size 0x8
constexpr uint32_t resignFormation__14MilitaryPickup = 0x0014DA88;   // size 0x8
constexpr uint32_t setFormation__14MilitaryPickupP17MilitaryFormation = 0x0014DA90;   // size 0x8
constexpr uint32_t setFormationPos__14MilitaryPickupR8_fvector = 0x0014DA98;   // size 0x10
constexpr uint32_t getFormationPos__14MilitaryPickup = 0x0014DAA8;   // size 0x8
constexpr uint32_t x__18MissileTruckPickup = 0x0014DAB0;   // size 0x5C
constexpr uint32_t initAfterDbLoad__18MissileTruckPickup = 0x0014DB10;   // size 0xCC
constexpr uint32_t setTrans__18MissileTruckPickupR8_fvector = 0x0014DBE0;   // size 0x48
constexpr uint32_t takeHit__18MissileTruckPickupP8_fvectorfi = 0x0014DC28;   // size 0x50
constexpr uint32_t drop__18MissileTruckPickup = 0x0014DC78;   // size 0x3C
constexpr uint32_t kill__18MissileTruckPickup = 0x0014DCB8;   // size 0x7C
constexpr uint32_t fire__18MissileTruckPickupR7GamePad = 0x0014DD38;   // size 0x74
constexpr uint32_t update__18MissileTruckPickup = 0x0014DDB0;   // size 0x13C
constexpr uint32_t updateLeadBehavior__18MissileTruckPickupR7GamePad = 0x0014DEF0;   // size 0xC4
constexpr uint32_t updateFollowBehavior__18MissileTruckPickupR7GamePad = 0x0014DFB8;   // size 0x1C
constexpr uint32_t updateAttackBehavior__18MissileTruckPickupR7GamePad = 0x0014DFD8;   // size 0x304
constexpr uint32_t enterState__18MissileTruckPickupQ214MilitaryPickup5State = 0x0014E2E0;   // size 0x164
constexpr uint32_t leadFormation__18MissileTruckPickup = 0x0014E448;   // size 0x48
constexpr uint32_t followFormation__18MissileTruckPickup = 0x0014E490;   // size 0x4C
constexpr uint32_t resignFormation__18MissileTruckPickup = 0x0014E4E0;   // size 0x24
constexpr uint32_t x__tf18MissileTruckPickup = 0x0014E540;   // size 0x50
constexpr uint32_t init__12MonkeyChainsP7Monster = 0x0014E590;   // size 0xF4
constexpr uint32_t update__12MonkeyChains = 0x0014E688;   // size 0x60
constexpr uint32_t init__Q212MonkeyChains5ChainP7MonsterP13_ParticleTypei = 0x0014E6E8;   // size 0xBC
constexpr uint32_t update__Q212MonkeyChains5ChainP7Monster = 0x0014E7A8;   // size 0x2E4
constexpr uint32_t collisTestCloseRangeAttack__20PunchSwipeConfigBaseiP7MonsterR10HitHistoryi = 0x0014EA90;   // size 0x5C4
constexpr uint32_t x__12MonsterStateQ212MonsterState2IdUi = 0x0014F058;   // size 0x24
constexpr uint32_t update__12MonsterState = 0x0014F080;   // size 0x30
constexpr uint32_t handleCollis__12MonsterStateR9_hdResult = 0x0014F0B0;   // size 0xE4
constexpr uint32_t x__11AttackStateQ212MonsterState2IdUi = 0x0014F198;   // size 0x40
constexpr uint32_t creditStaminaForAttack__11AttackStatef = 0x0014F1D8;   // size 0x88
constexpr uint32_t x__9StateIdle = 0x0014F260;   // size 0x54
constexpr uint32_t transitionInto__9StateIdle = 0x0014F2B8;   // size 0x30
constexpr uint32_t chooseBaseIdle__9StateIdle = 0x0014F2E8;   // size 0xB0
constexpr uint32_t chooseFancyIdle__9StateIdle = 0x0014F398;   // size 0x80
constexpr uint32_t update__9StateIdle = 0x0014F418;   // size 0x450
constexpr uint32_t startBaseAnim__9StateIdle11MonsterAnim = 0x0014F868;   // size 0x108
constexpr uint32_t handlePreemption__9StateIdleP12MonsterState = 0x0014F970;   // size 0xC
constexpr uint32_t x__13StateBatSwipe = 0x0014F980;   // size 0x260
constexpr uint32_t transitionOK__13StateBatSwipe = 0x0014FBE0;   // size 0x1A4
constexpr uint32_t transitionInto__13StateBatSwipe = 0x0014FD88;   // size 0xDC
constexpr uint32_t update__13StateBatSwipe = 0x0014FE68;   // size 0xDAC
constexpr uint32_t startChain__13StateBatSwipeiiRQ213StateBatSwipe11SwipeConfig = 0x00150C18;   // size 0x3B0
constexpr uint32_t handleCollis__13StateBatSwipeR9_hdResult = 0x00150FC8;   // size 0x1C
constexpr uint32_t handlePreemption__13StateBatSwipeP12MonsterState = 0x00150FE8;   // size 0x5C
constexpr uint32_t acceptHit__13StateBatSwipeR8HitEvent = 0x00151048;   // size 0x198
constexpr uint32_t x__10StateBlock = 0x001511E0;   // size 0x7C
constexpr uint32_t transitionOK__10StateBlock = 0x00151260;   // size 0x50
constexpr uint32_t transitionInto__10StateBlock = 0x001512B0;   // size 0xE0
constexpr uint32_t chooseBlock__10StateBlock = 0x00151390;   // size 0x9C
constexpr uint32_t update__10StateBlock = 0x00151430;   // size 0x168
constexpr uint32_t handleCollis__10StateBlockR9_hdResult = 0x00151598;   // size 0x28
constexpr uint32_t handlePreemption__10StateBlockP12MonsterState = 0x001515C0;   // size 0x14
constexpr uint32_t autoBlock__10StateBlock = 0x001515D8;   // size 0x70
constexpr uint32_t isBlockable__10StateBlockR8HitEvent = 0x00151648;   // size 0x44
constexpr uint32_t x__10StateCatch = 0x00151690;   // size 0x8C
constexpr uint32_t transitionOK__10StateCatch = 0x00151720;   // size 0x7C
constexpr uint32_t transitionFeasible__10StateCatch = 0x001517A0;   // size 0x388
constexpr uint32_t transitionInto__10StateCatch = 0x00151B28;   // size 0xA4
constexpr uint32_t update__10StateCatch = 0x00151BD0;   // size 0x3A0
constexpr uint32_t handlePreemption__10StateCatchP12MonsterState = 0x00151F70;   // size 0xC
constexpr uint32_t x__12StateCounter = 0x00151F80;   // size 0xA0
constexpr uint32_t transitionOK__12StateCounter = 0x00152020;   // size 0x6C
constexpr uint32_t transitionFeasible__12StateCounter = 0x00152090;   // size 0x54
constexpr uint32_t transitionInto__12StateCounter = 0x001520E8;   // size 0xE8
constexpr uint32_t update__12StateCounter = 0x001521D0;   // size 0x264
constexpr uint32_t acceptHit__12StateCounterR8HitEvent = 0x00152438;   // size 0x8
constexpr uint32_t counterSuccess__12StateCounterR7MonsterR8_fvector = 0x00152440;   // size 0x204
constexpr uint32_t handleCollis__12StateCounterR9_hdResult = 0x00152648;   // size 0x28
constexpr uint32_t handlePreemption__12StateCounterP12MonsterState = 0x00152670;   // size 0x14
constexpr uint32_t x__14StateCountered = 0x00152688;   // size 0x54
constexpr uint32_t transitionOK__14StateCountered = 0x001526E0;   // size 0x10
constexpr uint32_t transitionInto__14StateCountered = 0x001526F0;   // size 0x74
constexpr uint32_t update__14StateCountered = 0x00152768;   // size 0x94
constexpr uint32_t handlePreemption__14StateCounteredP12MonsterState = 0x00152800;   // size 0x10
constexpr uint32_t x__10StateDeath = 0x00152810;   // size 0x48
constexpr uint32_t transitionOK__10StateDeath = 0x00152858;   // size 0x180
constexpr uint32_t transitionInto__10StateDeath = 0x001529D8;   // size 0x68C
constexpr uint32_t update__10StateDeath = 0x00153068;   // size 0x2E8
constexpr uint32_t findVictor__10StateDeath = 0x00153350;   // size 0x9C
constexpr uint32_t handlePreemption__10StateDeathP12MonsterState = 0x001533F0;   // size 0x40
constexpr uint32_t setKiller__10StateDeathP7Monster = 0x00153430;   // size 0x14
constexpr uint32_t x__14StateGunAttack = 0x00153448;   // size 0x60
constexpr uint32_t transitionOK__14StateGunAttack = 0x001534A8;   // size 0x10
constexpr uint32_t transitionInto__14StateGunAttack = 0x001534B8;   // size 0x70
constexpr uint32_t update__14StateGunAttack = 0x00153528;   // size 0x1B0
constexpr uint32_t x__16StateGetupAttack = 0x001536D8;   // size 0xC8
constexpr uint32_t transitionOK__16StateGetupAttack = 0x001537A0;   // size 0x5C
constexpr uint32_t transitionInto__16StateGetupAttack = 0x00153800;   // size 0xDC
constexpr uint32_t update__16StateGetupAttack = 0x001538E0;   // size 0x604
constexpr uint32_t enterSubState__16StateGetupAttackQ216StateGetupAttack8SubState = 0x00153EE8;   // size 0xB0
constexpr uint32_t acceptHit__16StateGetupAttackR8HitEvent = 0x00153F98;   // size 0x158
constexpr uint32_t x__12StateImpaled = 0x001540F0;   // size 0x74
constexpr uint32_t transitionOK__12StateImpaled = 0x00154168;   // size 0x38
constexpr uint32_t transitionInto__12StateImpaled = 0x001541A0;   // size 0x13C
constexpr uint32_t update__12StateImpaled = 0x001542E0;   // size 0x370
constexpr uint32_t handleCollis__12StateImpaledR9_hdResult = 0x00154650;   // size 0x28
constexpr uint32_t handlePreemption__12StateImpaledP12MonsterState = 0x00154678;   // size 0x58
constexpr uint32_t x__12StateJavelin = 0x001546D0;   // size 0x54
constexpr uint32_t transitionOK__12StateJavelin = 0x00154728;   // size 0x94
constexpr uint32_t transitionInto__12StateJavelin = 0x001547C0;   // size 0x54
constexpr uint32_t update__12StateJavelin = 0x00154818;   // size 0x104
constexpr uint32_t x__10StatePunch = 0x00154920;   // size 0x29C
constexpr uint32_t transitionOK__10StatePunch = 0x00154BC0;   // size 0x224
constexpr uint32_t transitionInto__10StatePunch = 0x00154DE8;   // size 0xBC
constexpr uint32_t update__10StatePunch = 0x00154EA8;   // size 0x944
constexpr uint32_t startChain__10StatePunchiiRQ210StatePunch11PunchConfig = 0x001557F0;   // size 0x3A4
constexpr uint32_t playPunchEffect__10StatePunchR8_fvector = 0x00155B98;   // size 0x1D4
constexpr uint32_t handleCollis__10StatePunchR9_hdResult = 0x00155D70;   // size 0x12C
constexpr uint32_t handlePreemption__10StatePunchP12MonsterState = 0x00155EA0;   // size 0x5C
constexpr uint32_t acceptHit__10StatePunchR8HitEvent = 0x00155F00;   // size 0x1AC
constexpr uint32_t getFistSize__10StatePunch = 0x001560B0;   // size 0x44
constexpr uint32_t x__11StatePickUp = 0x001560F8;   // size 0xD0
constexpr uint32_t transitionOK__11StatePickUp = 0x001561C8;   // size 0x7C
constexpr uint32_t transitionFeasible__11StatePickUp = 0x00156248;   // size 0xA8
constexpr uint32_t transitionInto__11StatePickUp = 0x001562F0;   // size 0x1B0
constexpr uint32_t update__11StatePickUp = 0x001564A0;   // size 0x1A8
constexpr uint32_t getAttachRegion__11StatePickUpR8_fvectorT1 = 0x00156648;   // size 0x148
constexpr uint32_t handlePreemption__11StatePickUpP12MonsterState = 0x00156790;   // size 0xC
constexpr uint32_t x__11StateRecoil = 0x001567A0;   // size 0x9C
constexpr uint32_t init__11StateRecoilP7Monster = 0x00156840;   // size 0x38
constexpr uint32_t getRecoilAnim__11StateRecoil = 0x00156878;   // size 0x200
constexpr uint32_t transitionOK__11StateRecoil = 0x00156A78;   // size 0x48
constexpr uint32_t transitionInto__11StateRecoil = 0x00156AC0;   // size 0x13C
constexpr uint32_t update__11StateRecoil = 0x00156C00;   // size 0x4A4
constexpr uint32_t acceptHit__11StateRecoilR8HitEvent = 0x001570A8;   // size 0x44
constexpr uint32_t handlePreemption__11StateRecoilP12MonsterState = 0x001570F0;   // size 0x44
constexpr uint32_t x__12StateStunned = 0x00157138;   // size 0x74
constexpr uint32_t transitionOK__12StateStunned = 0x001571B0;   // size 0x30
constexpr uint32_t transitionInto__12StateStunned = 0x001571E0;   // size 0x1E8
constexpr uint32_t update__12StateStunned = 0x001573C8;   // size 0x2A8
constexpr uint32_t handlePreemption__12StateStunnedP12MonsterState = 0x00157670;   // size 0x30
constexpr uint32_t handleCollis__12StateStunnedR9_hdResult = 0x001576A0;   // size 0x124
constexpr uint32_t setStarsCs__12StateStunnedP3_cs = 0x001577C8;   // size 0xC
constexpr uint32_t updateStars__12StateStunned = 0x001577D8;   // size 0x30
constexpr uint32_t x__10StateTaunt = 0x00157808;   // size 0x48
constexpr uint32_t transitionOK__10StateTaunt = 0x00157850;   // size 0x58
constexpr uint32_t transitionInto__10StateTaunt = 0x001578A8;   // size 0x94
constexpr uint32_t update__10StateTaunt = 0x00157940;   // size 0x190
constexpr uint32_t x__10StateThrow = 0x00157AD0;   // size 0x8C
constexpr uint32_t transitionOK__10StateThrow = 0x00157B60;   // size 0xC8
constexpr uint32_t transitionInto__10StateThrow = 0x00157C28;   // size 0x94
constexpr uint32_t update__10StateThrow = 0x00157CC0;   // size 0x10C
constexpr uint32_t handlePreemption__10StateThrowP12MonsterState = 0x00157DD0;   // size 0x1C
constexpr uint32_t cancelOverride__10StateThrow = 0x00157DF0;   // size 0x44
constexpr uint32_t x__19StateTwoHandedThrow = 0x00157E38;   // size 0x84
constexpr uint32_t transitionOK__19StateTwoHandedThrow = 0x00157EC0;   // size 0x54
constexpr uint32_t transitionFeasible__19StateTwoHandedThrow = 0x00157F18;   // size 0x88
constexpr uint32_t transitionInto__19StateTwoHandedThrow = 0x00157FA0;   // size 0x124
constexpr uint32_t update__19StateTwoHandedThrow = 0x001580C8;   // size 0x1E8
constexpr uint32_t handlePreemption__19StateTwoHandedThrowP12MonsterState = 0x001582B0;   // size 0x8
constexpr uint32_t x__12StateShocked = 0x001582B8;   // size 0x6C
constexpr uint32_t transitionOK__12StateShocked = 0x00158328;   // size 0x3C
constexpr uint32_t transitionInto__12StateShocked = 0x00158368;   // size 0x118
constexpr uint32_t update__12StateShocked = 0x00158480;   // size 0x114
constexpr uint32_t handlePreemption__12StateShockedP12MonsterState = 0x00158598;   // size 0x20
constexpr uint32_t x__16StateStompAttack = 0x001585B8;   // size 0xC0
constexpr uint32_t transitionOK__16StateStompAttack = 0x00158678;   // size 0x50
constexpr uint32_t transitionFeasible__16StateStompAttack = 0x001586C8;   // size 0x12C
constexpr uint32_t transitionInto__16StateStompAttack = 0x001587F8;   // size 0x54
constexpr uint32_t update__16StateStompAttack = 0x00158850;   // size 0x308
constexpr uint32_t startChain__16StateStompAttack = 0x00158B58;   // size 0x16C
constexpr uint32_t x__16StateTazerAttack = 0x00158CC8;   // size 0x68
constexpr uint32_t transitionOK__16StateTazerAttack = 0x00158D30;   // size 0x10
constexpr uint32_t transitionInto__16StateTazerAttack = 0x00158D40;   // size 0x7C
constexpr uint32_t update__16StateTazerAttack = 0x00158DC0;   // size 0x200
constexpr uint32_t launchProjectile__16StateTazerAttack = 0x00158FC0;   // size 0xA4
constexpr uint32_t x__17StateShieldAttack = 0x00159068;   // size 0xD0
constexpr uint32_t transitionOK__17StateShieldAttack = 0x00159138;   // size 0x80
constexpr uint32_t transitionFeasible__17StateShieldAttack = 0x001591B8;   // size 0x10
constexpr uint32_t transitionInto__17StateShieldAttack = 0x001591C8;   // size 0x8C
constexpr uint32_t update__17StateShieldAttack = 0x00159258;   // size 0x7A4
constexpr uint32_t x__12StateVictory = 0x00159A00;   // size 0x3C
constexpr uint32_t transitionOK__12StateVictory = 0x00159A40;   // size 0x170
constexpr uint32_t transitionInto__12StateVictory = 0x00159BB0;   // size 0x16C
constexpr uint32_t update__12StateVictory = 0x00159D20;   // size 0x114
constexpr uint32_t fade__12StateVictory = 0x00159E38;   // size 0x70
constexpr uint32_t x__15StateBigTakeHit = 0x00159EA8;   // size 0x3C
constexpr uint32_t transitionOK__15StateBigTakeHit = 0x00159EE8;   // size 0x10
constexpr uint32_t transitionInto__15StateBigTakeHit = 0x00159EF8;   // size 0x58
constexpr uint32_t update__15StateBigTakeHit = 0x00159F50;   // size 0x98
constexpr uint32_t damageToPickup__20PunchSwipeConfigBase = 0x00159FE8;   // size 0xC
constexpr uint32_t damageCaused__20PunchSwipeConfigBase = 0x00159FF8;   // size 0xC
constexpr uint32_t x__tf12MonsterState = 0x0015A008;   // size 0x3C
constexpr uint32_t init__12MonsterStateP7Monster = 0x0015A048;   // size 0x8
constexpr uint32_t transitionInto__12MonsterState = 0x0015A050;   // size 0x8
constexpr uint32_t transitionOK__12MonsterState = 0x0015A058;   // size 0x28
constexpr uint32_t transitionFeasible__12MonsterState = 0x0015A080;   // size 0x8
constexpr uint32_t acceptHit__12MonsterStateR8HitEvent = 0x0015A088;   // size 0x8
constexpr uint32_t handlePreemption__12MonsterStateP12MonsterState = 0x0015A090;   // size 0x8
constexpr uint32_t getFieldsInState__12MonsterState = 0x0015A098;   // size 0x8
constexpr uint32_t getStateId__12MonsterState = 0x0015A0A0;   // size 0x8
constexpr uint32_t getCaps__12MonsterState = 0x0015A0A8;   // size 0x8
constexpr uint32_t setCaps__12MonsterStateUs = 0x0015A0B0;   // size 0x14
constexpr uint32_t clearCaps__12MonsterStateUs = 0x0015A0C8;   // size 0x18
constexpr uint32_t hasCaps__12MonsterStateUi = 0x0015A0E0;   // size 0x14
constexpr uint32_t getMon__12MonsterState = 0x0015A0F8;   // size 0x8
constexpr uint32_t x__tf9StateIdle = 0x0015A180;   // size 0x6C
constexpr uint32_t x__tfQ213StateBatSwipe11SwipeConfig = 0x0015A1F0;   // size 0x50
constexpr uint32_t x__tf13StateBatSwipe = 0x0015A240;   // size 0x98
constexpr uint32_t damageToPickup__Q213StateBatSwipe11SwipeConfig = 0x0015A2D8;   // size 0x8
constexpr uint32_t getAttackTrans__13StateBatSwipe = 0x0015A2E0;   // size 0x2C
constexpr uint32_t getRecvrTransMod__13StateBatSwipe = 0x0015A310;   // size 0x28
constexpr uint32_t getRecoilFields__13StateBatSwipe = 0x0015A338;   // size 0x28
constexpr uint32_t getHitPause__13StateBatSwipe = 0x0015A360;   // size 0x28
constexpr uint32_t getSphereRadius__13StateBatSwipe = 0x0015A388;   // size 0x8
constexpr uint32_t getDamageMod__13StateBatSwipe = 0x0015A390;   // size 0x28
constexpr uint32_t getCurrentAnim__13StateBatSwipe = 0x0015A3B8;   // size 0x28
constexpr uint32_t getCurrentConfig__13StateBatSwipe = 0x0015A3E0;   // size 0x28
constexpr uint32_t getCurrentSwipe__13StateBatSwipe = 0x0015A408;   // size 0x8
constexpr uint32_t getFieldsInConfig__13StateBatSwipe = 0x0015A410;   // size 0x8
constexpr uint32_t getCounterStartField__13StateBatSwipe = 0x0015A418;   // size 0x8
constexpr uint32_t getCounterEndField__13StateBatSwipe = 0x0015A420;   // size 0x8
constexpr uint32_t getTier__13StateBatSwipe = 0x0015A428;   // size 0x8
constexpr uint32_t isWindingDown__13StateBatSwipe = 0x0015A430;   // size 0x8
constexpr uint32_t inCollisionWindow__13StateBatSwipe = 0x0015A438;   // size 0x8
constexpr uint32_t inCounterableWindow__13StateBatSwipe = 0x0015A440;   // size 0x8
constexpr uint32_t inWindUp__13StateBatSwipe = 0x0015A448;   // size 0x28
constexpr uint32_t inWindDown__13StateBatSwipe = 0x0015A470;   // size 0x8
constexpr uint32_t isStunHit__13StateBatSwipe = 0x0015A478;   // size 0x4C
constexpr uint32_t isKnockbackHit__13StateBatSwipe = 0x0015A4C8;   // size 0x5C
constexpr uint32_t x__tf10StateBlock = 0x0015A528;   // size 0x6C
constexpr uint32_t isBlocking__10StateBlock = 0x0015A598;   // size 0x8
constexpr uint32_t x__tf10StateCatch = 0x0015A5A0;   // size 0x6C
constexpr uint32_t x__tf12StateCounter = 0x0015A610;   // size 0x6C
constexpr uint32_t getCounterStartField__12StateCounter = 0x0015A680;   // size 0x8
constexpr uint32_t getCounterEndField__12StateCounter = 0x0015A688;   // size 0x8
constexpr uint32_t x__tf14StateCountered = 0x0015A690;   // size 0x6C
constexpr uint32_t x__tf10StateDeath = 0x0015A700;   // size 0x6C
constexpr uint32_t getKiller__10StateDeath = 0x0015A770;   // size 0x8
constexpr uint32_t x__tf16StateGetupAttack = 0x0015A778;   // size 0x6C
constexpr uint32_t x__tf14StateGunAttack = 0x0015A7E8;   // size 0x98
constexpr uint32_t x__tf12StateImpaled = 0x0015A880;   // size 0x6C
constexpr uint32_t x__tf12StateJavelin = 0x0015A8F0;   // size 0x98
constexpr uint32_t x__tf11StatePickUp = 0x0015A988;   // size 0x6C
constexpr uint32_t x__tfQ210StatePunch11PunchConfig = 0x0015A9F8;   // size 0x50
constexpr uint32_t x__tf10StatePunch = 0x0015AA48;   // size 0x98
constexpr uint32_t damageCaused__Q210StatePunch11PunchConfig = 0x0015AAE0;   // size 0x8
constexpr uint32_t getAttackTrans__10StatePunch = 0x0015AAE8;   // size 0x2C
constexpr uint32_t getRecvrTransMod__10StatePunch = 0x0015AB18;   // size 0x28
constexpr uint32_t getRecoilFields__10StatePunch = 0x0015AB40;   // size 0x28
constexpr uint32_t getHitPause__10StatePunch = 0x0015AB68;   // size 0x28
constexpr uint32_t getSphereRadius__10StatePunch = 0x0015AB90;   // size 0x8
constexpr uint32_t getTier__10StatePunch = 0x0015AB98;   // size 0x8
constexpr uint32_t getCurrentAnim__10StatePunch = 0x0015ABA0;   // size 0x28
constexpr uint32_t getCurrentPunch__10StatePunch = 0x0015ABC8;   // size 0x8
constexpr uint32_t getFieldsInConfig__10StatePunch = 0x0015ABD0;   // size 0x8
constexpr uint32_t getCounterStartField__10StatePunch = 0x0015ABD8;   // size 0x8
constexpr uint32_t getCounterEndField__10StatePunch = 0x0015ABE0;   // size 0x8
constexpr uint32_t setPunchEffect__10StatePunch4FxId = 0x0015ABE8;   // size 0x8
constexpr uint32_t getPunchEffect__10StatePunch = 0x0015ABF0;   // size 0x8
constexpr uint32_t isStunHit__10StatePunch = 0x0015ABF8;   // size 0x48
constexpr uint32_t isKnockbackHit__10StatePunch = 0x0015AC40;   // size 0x74
constexpr uint32_t isWindingDown__10StatePunch = 0x0015ACB8;   // size 0x8
constexpr uint32_t inCollisionWindow__10StatePunch = 0x0015ACC0;   // size 0x8
constexpr uint32_t inCounterableWindow__10StatePunch = 0x0015ACC8;   // size 0x8
constexpr uint32_t inWindUp__10StatePunch = 0x0015ACD0;   // size 0x28
constexpr uint32_t inWindDown__10StatePunch = 0x0015ACF8;   // size 0x8
constexpr uint32_t getCurrentConfig__10StatePunch = 0x0015AD00;   // size 0x28
constexpr uint32_t x__tf11StateRecoil = 0x0015AD28;   // size 0x6C
constexpr uint32_t setTransDirection__11StateRecoilR8_fvector = 0x0015AD98;   // size 0x20
constexpr uint32_t setTransDelta__11StateRecoilf = 0x0015ADB8;   // size 0x8
constexpr uint32_t setHitDirection__11StateRecoilR8_fvector = 0x0015ADC0;   // size 0x10
constexpr uint32_t setRecoilFields__11StateRecoilf = 0x0015ADD0;   // size 0x8
constexpr uint32_t setHitPause__11StateRecoilf = 0x0015ADD8;   // size 0x8
constexpr uint32_t setHitSource__11StateRecoili = 0x0015ADE0;   // size 0x8
constexpr uint32_t setHitType__11StateRecoili = 0x0015ADE8;   // size 0x8
constexpr uint32_t isBlocking__11StateRecoil = 0x0015ADF0;   // size 0x8
constexpr uint32_t isStaggering__11StateRecoil = 0x0015ADF8;   // size 0x10
constexpr uint32_t getBlendTime__11StateRecoil = 0x0015AE08;   // size 0x8
constexpr uint32_t x__tf12StateStunned = 0x0015AE10;   // size 0x6C
constexpr uint32_t getStarsCs__12StateStunned = 0x0015AE80;   // size 0x8
constexpr uint32_t setRecoilFirst__12StateStunnedb = 0x0015AE88;   // size 0x8
constexpr uint32_t x__tf10StateTaunt = 0x0015AE90;   // size 0x6C
constexpr uint32_t x__tf10StateThrow = 0x0015AF00;   // size 0x98
constexpr uint32_t x__tf19StateTwoHandedThrow = 0x0015AF98;   // size 0x98
constexpr uint32_t x__tf12StateShocked = 0x0015B030;   // size 0x6C
constexpr uint32_t setDamage__12StateShockedf = 0x0015B0A0;   // size 0x8
constexpr uint32_t x__tf16StateStompAttack = 0x0015B0A8;   // size 0x98
constexpr uint32_t x__tf16StateTazerAttack = 0x0015B140;   // size 0x98
constexpr uint32_t launchProjectile__16StateTazerAttackPv = 0x0015B1D8;   // size 0x1C
constexpr uint32_t x__tf17StateShieldAttack = 0x0015B1F8;   // size 0x98
constexpr uint32_t x__tf15StateBigTakeHit = 0x0015B290;   // size 0x6C
constexpr uint32_t x__tf12StateVictory = 0x0015B300;   // size 0x6C
constexpr uint32_t acceptHit__12StateVictoryR8HitEvent = 0x0015B370;   // size 0x8
constexpr uint32_t getVictoryAnim__12StateVictory = 0x0015B378;   // size 0x8
constexpr uint32_t fade__12StateVictoryPv = 0x0015B380;   // size 0x1C
constexpr uint32_t x__tf20PunchSwipeConfigBase = 0x0015B3A0;   // size 0x3C
constexpr uint32_t setWaterLevel__Ff = 0x0015B3E0;   // size 0x8
constexpr uint32_t getWaterLevel__Fv = 0x0015B3E8;   // size 0x8
constexpr uint32_t isUnderwater__Ff = 0x0015B3F0;   // size 0x20
constexpr uint32_t recomputeDynamics__7Monster = 0x0015B410;   // size 0x50
constexpr uint32_t playerUpdateInputs__7Monster = 0x0015B460;   // size 0x24
constexpr uint32_t update__7Monster = 0x0015B488;   // size 0x4C0
constexpr uint32_t startCinema__7Monster = 0x0015B948;   // size 0xC8
constexpr uint32_t updateCinema__7Monster = 0x0015BA10;   // size 0x134
constexpr uint32_t endCinema__7Monster = 0x0015BB48;   // size 0x1C
constexpr uint32_t updateBoundingSphere__7Monster = 0x0015BB68;   // size 0x3BC
constexpr uint32_t updateReticle__7Monster = 0x0015BF28;   // size 0x3EC
constexpr uint32_t setReticles__7Monsteri = 0x0015C318;   // size 0x178
constexpr uint32_t setTrans__7MonsterR8_fvector = 0x0015C490;   // size 0x80
constexpr uint32_t setRot__7Monsterfff = 0x0015C510;   // size 0x6C
constexpr uint32_t setMat__7MonsterRA3_A3_f = 0x0015C580;   // size 0x54
constexpr uint32_t setEnvMapping__7Monster = 0x0015C5D8;   // size 0x2C
constexpr uint32_t clearEnvMapping__7Monster = 0x0015C608;   // size 0x28
constexpr uint32_t updateShadow__7Monster = 0x0015C630;   // size 0x1A0
constexpr uint32_t calcGlowIntensity__Fiii = 0x0015C7D0;   // size 0x70
constexpr uint32_t updatePowerUpGlow__7Monster = 0x0015C840;   // size 0x81C
constexpr uint32_t updateBoostAndRage__7Monster = 0x0015D060;   // size 0x2C8
constexpr uint32_t updateLock__7Monster19MonsterReticleState = 0x0015D328;   // size 0xA48
constexpr uint32_t updateLookAt__7Monster = 0x0015DD70;   // size 0x3F0
constexpr uint32_t updateAirLegOverride__7Monster = 0x0015E160;   // size 0x1F4
constexpr uint32_t updateAnimContacts__7Monsterb = 0x0015E358;   // size 0x418
constexpr uint32_t getStaminaGain__7Monster = 0x0015E770;   // size 0x4C
constexpr uint32_t getTarget__7MonsterR8_fvectorb = 0x0015E7C0;   // size 0xB8
constexpr uint32_t throwPickup__7Monsterif = 0x0015E878;   // size 0x58
constexpr uint32_t dropPickup__7Monster = 0x0015E8D0;   // size 0xB4
constexpr uint32_t launchDefaultProjectile__7Monsteri = 0x0015E988;   // size 0x2AC
constexpr uint32_t updateDefaultProjectile__7Monster = 0x0015EC38;   // size 0x274
constexpr uint32_t updatePosition__7Monster = 0x0015EEB0;   // size 0x198
constexpr uint32_t takeHit__7MonsterP8_fvectorfi = 0x0015F048;   // size 0xDF8
constexpr uint32_t takeDamage__7MonsterfbP7Monster = 0x0015FE40;   // size 0x214
constexpr uint32_t takeAdditiveRecoil__7MonsterR8_fvectorf = 0x00160058;   // size 0x170
constexpr uint32_t knockBack__7MonsterR8_fvectorff = 0x001601C8;   // size 0x274
constexpr uint32_t blowUpMonster__7MonsterP7Monster = 0x00160440;   // size 0x9C
constexpr uint32_t updateDeathSequence__7Monster = 0x001604E0;   // size 0x58
constexpr uint32_t registerComboHit__7MonsterP7Monster = 0x00160538;   // size 0x68
constexpr uint32_t getClosestMonster__7Monsterf = 0x001605A0;   // size 0xBC
constexpr uint32_t getClosestMonster2D__7Monsterf = 0x00160660;   // size 0xBC
constexpr uint32_t getClosestMonster__7Monsteriif = 0x00160720;   // size 0xC0
constexpr uint32_t getClosestMonsterWithLos__7Monsterf = 0x001607E0;   // size 0xC8
constexpr uint32_t getClosestMonsterToOrientation__7Monsterff = 0x001608A8;   // size 0x114
constexpr uint32_t getClosestMonsterToOrientation__7Monsteriiff = 0x001609C0;   // size 0x194
constexpr uint32_t getClosestMonsterToPunch__7Monsterfff = 0x00160B58;   // size 0x3D0
constexpr uint32_t getClosestMonsterInFOV__7MonsterR8_fvectorfT1fRf = 0x00160F28;   // size 0x258
constexpr uint32_t getClosestTargetable__7MonsterUsbff = 0x00161180;   // size 0x24
constexpr uint32_t getClosestTargetable__7MonsterUsbfff = 0x001611A8;   // size 0x5FC
constexpr uint32_t getLookAtTarget__7Monster = 0x001617A8;   // size 0x1AC
constexpr uint32_t getDestructibleFromReticle__7Monsterf = 0x00161958;   // size 0xAC
constexpr uint32_t addAttachment__7MonsterP9_hierhead = 0x00161A08;   // size 0x280
constexpr uint32_t handleAction__7MonsterP15ActAiNavigation = 0x00161C88;   // size 0x8
constexpr uint32_t attachPickupImpaler__7MonsterGQ2t10LinkedList1ZP6Pickup8IteratorP8_fvector = 0x00161C90;   // size 0x1AC
constexpr uint32_t detachPickupImpaler__7Monsterb = 0x00161E40;   // size 0x68
constexpr uint32_t dropPickupImpaler__7Monster = 0x00161EA8;   // size 0x50
constexpr uint32_t handleLocator__7MonsterUiRA3_A3_fP8_fvector = 0x00161EF8;   // size 0x2CC
constexpr uint32_t enterNewState__7MonsterP12MonsterState = 0x001621C8;   // size 0x24C
constexpr uint32_t landingShake__7Monster = 0x00162418;   // size 0x8
constexpr uint32_t getCollisionDamage__7Monsterf = 0x00162420;   // size 0x14
constexpr uint32_t creditStamina__7Monsterfb = 0x00162438;   // size 0x138
constexpr uint32_t creditHealth__7Monsterf = 0x00162570;   // size 0x70
constexpr uint32_t breathFire__7Monster = 0x001625E0;   // size 0x4C
constexpr uint32_t lightOnFire__7Monsterffi = 0x00162630;   // size 0x7C
constexpr uint32_t updateOnFire__7Monster = 0x001626B0;   // size 0x104
constexpr uint32_t startShocking__7Monsterff = 0x001627B8;   // size 0xC
constexpr uint32_t updateBeingShocked__7Monster = 0x001627C8;   // size 0x260
constexpr uint32_t startBeingImpaled__7Monsterff = 0x00162A28;   // size 0x8
constexpr uint32_t isHolding__7Monster = 0x00162A30;   // size 0x24
constexpr uint32_t isHoldingLarge__7Monster = 0x00162A58;   // size 0x3C
constexpr uint32_t isBlocking__7Monster = 0x00162A98;   // size 0x40
constexpr uint32_t isIdle__7MonsterUi = 0x00162AD8;   // size 0x64
constexpr uint32_t inCameraFov__7MonsterR8_fvectorT1 = 0x00162B40;   // size 0x34
constexpr uint32_t hasPinTarget__7Monster = 0x00162B78;   // size 0x20
constexpr uint32_t updateClosestPath__7Monster = 0x00162B98;   // size 0x68
constexpr uint32_t okToDrawReticle__7Monster = 0x00162C00;   // size 0xCC
constexpr uint32_t setCloakOn__7Monster = 0x00162CD0;   // size 0x54
constexpr uint32_t setCloakOff__7Monster = 0x00162D28;   // size 0x24
constexpr uint32_t attachFxToHandle__FPiP8_fvectorUi = 0x00162D50;   // size 0x5C
constexpr uint32_t updateWaterWake__7Monsterfb = 0x00162DB0;   // size 0xE0
constexpr uint32_t cleanUpForMovie__7Monster = 0x00162E90;   // size 0x74
constexpr uint32_t setShadowOnOff__7Monsterb = 0x00162F08;   // size 0x50
constexpr uint32_t setSecondaryShadowBlocker__7MonsterP3_cs = 0x00162F58;   // size 0x24
constexpr uint32_t drainSpecial__7Monster = 0x00162F80;   // size 0x44
constexpr uint32_t isSpecialAvailable__C7Monster = 0x00162FC8;   // size 0x38
constexpr uint32_t x__tf7Monster = 0x00163028;   // size 0x50
constexpr uint32_t x__7Monster = 0x00163078;   // size 0x3BC
constexpr uint32_t updateTurn__7Monsterb = 0x00163438;   // size 0x1C
constexpr uint32_t updateMove__7Monsterb = 0x00163458;   // size 0x1C
constexpr uint32_t stopFireBreath__7Monster = 0x00163478;   // size 0x1C
constexpr uint32_t putOutFire__7Monster = 0x00163498;   // size 0x8
constexpr uint32_t getMotionRot__7Monster = 0x001634A0;   // size 0x8
constexpr uint32_t getPrevTrans__7Monster = 0x001634A8;   // size 0x8
constexpr uint32_t getPrevMat__7Monster = 0x001634B0;   // size 0x8
constexpr uint32_t getLocatorTrans__7Monsteri = 0x001634B8;   // size 0x10
constexpr uint32_t getLocatorMat__7Monsteri = 0x001634C8;   // size 0x1C
constexpr uint32_t getPinTrans__7Monster = 0x001634E8;   // size 0x8
constexpr uint32_t getLookAtTrans__7Monster = 0x001634F0;   // size 0x8
constexpr uint32_t getAnim__7Monster11MonsterAnim = 0x001634F8;   // size 0x10
constexpr uint32_t getDynamics__7Monster = 0x00163508;   // size 0x8
constexpr uint32_t getVel__7Monster = 0x00163510;   // size 0x8
constexpr uint32_t getPickup__7Monster = 0x00163518;   // size 0x8
constexpr uint32_t getImpaler__7Monster = 0x00163520;   // size 0x8
constexpr uint32_t getReverseImpaler__7Monster = 0x00163528;   // size 0x8
constexpr uint32_t getGrapplee__7Monster = 0x00163530;   // size 0x8
constexpr uint32_t getGrappler__7Monster = 0x00163538;   // size 0x8
constexpr uint32_t getGrappleAttempt__7Monster = 0x00163540;   // size 0x8
constexpr uint32_t getBeamVictim__7Monster = 0x00163548;   // size 0x8
constexpr uint32_t getKiller__7Monster = 0x00163550;   // size 0x10
constexpr uint32_t getPinTarget__7Monster = 0x00163560;   // size 0x8
constexpr uint32_t getPinTime__7Monster = 0x00163568;   // size 0x8
constexpr uint32_t getPlayerInfo__7Monster = 0x00163570;   // size 0x8
constexpr uint32_t getAi__7Monster = 0x00163578;   // size 0x8
constexpr uint32_t getReticleCS__7Monster = 0x00163580;   // size 0x8
constexpr uint32_t getStickyReticleCS__7Monster = 0x00163588;   // size 0x8
constexpr uint32_t getShadow__7Monster = 0x00163590;   // size 0x8
constexpr uint32_t getFireBreath__7Monster = 0x00163598;   // size 0x8
constexpr uint32_t getMonsterSound__7Monster = 0x001635A0;   // size 0x8
constexpr uint32_t getStaminaMeter__7Monster = 0x001635A8;   // size 0x8
constexpr uint32_t getHealthMeter__7Monster = 0x001635B0;   // size 0x8
constexpr uint32_t getLeadVec__7Monster = 0x001635B8;   // size 0x8
constexpr uint32_t getAutoLeadMovesReticle__7Monster = 0x001635C0;   // size 0x8
constexpr uint32_t getReticleLosResult__7Monster = 0x001635C8;   // size 0x8
constexpr uint32_t getShadowHDResult__7Monster = 0x001635D0;   // size 0x8
constexpr uint32_t getClosestPath__7Monster = 0x001635D8;   // size 0x8
constexpr uint32_t getReticleState__C7Monster = 0x001635E0;   // size 0x8
constexpr uint32_t getEnemyInfo__7MonsterR7Monster = 0x001635E8;   // size 0x1C
constexpr uint32_t getState__7Monster = 0x00163608;   // size 0x8
constexpr uint32_t getStateId__7Monster = 0x00163610;   // size 0xC
constexpr uint32_t getPrevStateId__7Monster = 0x00163620;   // size 0xC
constexpr uint32_t getCameraData__7MonsteriQ26Camera9CameraPOV = 0x00163630;   // size 0x20
constexpr uint32_t getType__C7Monster = 0x00163650;   // size 0x8
constexpr uint32_t getIndex__C7Monster = 0x00163658;   // size 0x8
constexpr uint32_t getName__C7Monster = 0x00163660;   // size 0x8
constexpr uint32_t getDupId__C7Monster = 0x00163668;   // size 0x8
constexpr uint32_t getNumInits__C7Monster = 0x00163670;   // size 0x8
constexpr uint32_t getMonsterNum__C7Monster = 0x00163678;   // size 0x8
constexpr uint32_t getSkinNum__C7Monster = 0x00163680;   // size 0x8
constexpr uint32_t getSpeed__7Monster = 0x00163688;   // size 0x8
constexpr uint32_t getHealth__7Monster = 0x00163690;   // size 0x8
constexpr uint32_t getMaxHealth__7Monster = 0x00163698;   // size 0x8
constexpr uint32_t getStamina__7Monster = 0x001636A0;   // size 0x8
constexpr uint32_t getHeight__C7Monster = 0x001636A8;   // size 0x10
constexpr uint32_t getWidth__C7Monster = 0x001636B8;   // size 0x8
constexpr uint32_t getCameraThatFollows__C7Monster = 0x001636C0;   // size 0x8
constexpr uint32_t getWinsThisGame__C7Monster = 0x001636C8;   // size 0x8
constexpr uint32_t getClimbSpeed__C7Monster = 0x001636D0;   // size 0x8
constexpr uint32_t getClimbStrafeSpeed__C7Monster = 0x001636D8;   // size 0x8
constexpr uint32_t getFallTime__C7Monster = 0x001636E0;   // size 0x8
constexpr uint32_t getGroundHeight__C7Monster = 0x001636E8;   // size 0xC
constexpr uint32_t getHeightAboveCOG__C7Monster = 0x001636F8;   // size 0x8
constexpr uint32_t getRunTime__C7Monster = 0x00163700;   // size 0x8
constexpr uint32_t getPlayerAiOrFodderNum__C7Monster = 0x00163708;   // size 0x8
constexpr uint32_t getIsCameraFollowingThisMonster__7Monster = 0x00163710;   // size 0x8
constexpr uint32_t puPunchDamageMod__C7Monster11ePickupType = 0x00163718;   // size 0x10
constexpr uint32_t puLaunchDamageMod__C7Monster11ePickupType = 0x00163728;   // size 0x10
constexpr uint32_t puDurationMod__C7Monster11ePickupType = 0x00163738;   // size 0x10
constexpr uint32_t puSpeedMod__C7Monster11ePickupType = 0x00163748;   // size 0x10
constexpr uint32_t getDpDamage__C7Monster = 0x00163758;   // size 0x8
constexpr uint32_t getDpDuration__C7Monster = 0x00163760;   // size 0x8
constexpr uint32_t getDpSpeed__C7Monster = 0x00163768;   // size 0x8
constexpr uint32_t getDpHomingFactor__C7Monster = 0x00163770;   // size 0x8
constexpr uint32_t getDpVertHomingFactor__C7Monster = 0x00163778;   // size 0x8
constexpr uint32_t getDpHeadingBreak__C7Monster = 0x00163780;   // size 0x8
constexpr uint32_t getDpPitchBreak__C7Monster = 0x00163788;   // size 0x8
constexpr uint32_t getCamIdleCircuitTime__C7Monster = 0x00163790;   // size 0x8
constexpr uint32_t getCamIdleFactor__C7Monster = 0x00163798;   // size 0x8
constexpr uint32_t getLandingShakeAmp__C7Monster = 0x001637A0;   // size 0x8
constexpr uint32_t getLandingShakeFreq__C7Monster = 0x001637A8;   // size 0x8
constexpr uint32_t getLandingShakeDur__C7Monster = 0x001637B0;   // size 0x8
constexpr uint32_t getLandingShakeFalloff__C7Monster = 0x001637B8;   // size 0x8
constexpr uint32_t getLandingShakeMag__C7Monster = 0x001637C0;   // size 0x8
constexpr uint32_t getPitchRate1__C7Monster = 0x001637C8;   // size 0x8
constexpr uint32_t getPitchRate2__C7Monster = 0x001637D0;   // size 0x8
constexpr uint32_t getMaxHeadingChange__C7Monster = 0x001637D8;   // size 0x8
constexpr uint32_t getAimPitch__C7Monster = 0x001637E0;   // size 0x8
constexpr uint32_t getAimHeading__C7Monster = 0x001637E8;   // size 0x8
constexpr uint32_t getRearOffset__C7Monster = 0x001637F0;   // size 0x8
constexpr uint32_t getTargetingMod__C7Monster = 0x001637F8;   // size 0x8
constexpr uint32_t getFallTimeBeforePitch__C7Monster = 0x00163800;   // size 0x8
constexpr uint32_t getPinMaxPitch__C7Monster = 0x00163808;   // size 0x8
constexpr uint32_t getPinMuckingDist__C7Monster = 0x00163810;   // size 0x8
constexpr uint32_t getOnFireCount__C7Monster = 0x00163818;   // size 0x8
constexpr uint32_t getOnFireDamage__C7Monster = 0x00163820;   // size 0x8
constexpr uint32_t getBeingShockedCount__C7Monster = 0x00163828;   // size 0x8
constexpr uint32_t getBeingShockedDamage__C7Monster = 0x00163830;   // size 0x8
constexpr uint32_t getLaunchCounter__C7Monster = 0x00163838;   // size 0x8
constexpr uint32_t getLaunchDelay__C7Monster = 0x00163840;   // size 0x8
constexpr uint32_t isVulnerable__C7Monster = 0x00163848;   // size 0xC
constexpr uint32_t isDead__C7Monster = 0x00163858;   // size 0xC
constexpr uint32_t isCloaked__C7Monster = 0x00163868;   // size 0xC
constexpr uint32_t isTurning__C7Monster = 0x00163878;   // size 0xC
constexpr uint32_t isFalling__C7Monster = 0x00163888;   // size 0xC
constexpr uint32_t attacksEnabled__C7Monster = 0x00163898;   // size 0xC
constexpr uint32_t ranDeathSequence__C7Monster = 0x001638A8;   // size 0xC
constexpr uint32_t ranVictorySequence__C7Monster = 0x001638B8;   // size 0xC
constexpr uint32_t isFullHealth__7Monster = 0x001638C8;   // size 0x24
constexpr uint32_t isFullStamina__7Monster = 0x001638F0;   // size 0x28
constexpr uint32_t isTargetPinning__7Monster = 0x00163918;   // size 0x3C
constexpr uint32_t isOnFire__7Monster = 0x00163958;   // size 0x28
constexpr uint32_t isBeingShocked__7Monster = 0x00163980;   // size 0x28
constexpr uint32_t inSpecialState__7Monster = 0x001639A8;   // size 0x14
constexpr uint32_t setCameraFollowsMonster__7Monsterib = 0x001639C0;   // size 0xC
constexpr uint32_t setCs__7MonsterP3_cs = 0x001639D0;   // size 0x8
constexpr uint32_t setGodMode__7Monsterb = 0x001639D8;   // size 0x8
constexpr uint32_t setWinsThisGame__7Monsteri = 0x001639E0;   // size 0x8
constexpr uint32_t setDamageModifier__7Monsterf = 0x001639E8;   // size 0x8
constexpr uint32_t setRanDeathSequence__7Monsterb = 0x001639F0;   // size 0x8
constexpr uint32_t setRanVictorySequence__7Monsterb = 0x001639F8;   // size 0x8
constexpr uint32_t setDead__7Monsterb = 0x00163A00;   // size 0x8
constexpr uint32_t setTurning__7Monsterb = 0x00163A08;   // size 0x8
constexpr uint32_t setPadEnabled__7Monsterb = 0x00163A10;   // size 0x8
constexpr uint32_t setPickup__7MonsterGQ2t10LinkedList1ZP6Pickup8Iterator = 0x00163A18;   // size 0x8
constexpr uint32_t setGrapplee__7MonsterP7Monster = 0x00163A20;   // size 0x8
constexpr uint32_t setGrappler__7MonsterP7Monster = 0x00163A28;   // size 0x8
constexpr uint32_t setGrappleAttempt__7MonsterP7Monster = 0x00163A30;   // size 0x8
constexpr uint32_t setBeamVictim__7MonsterP7Monster = 0x00163A38;   // size 0x8
constexpr uint32_t setFreeFalling__7Monsterb = 0x00163A40;   // size 0x8
constexpr uint32_t setFallTime__7Monsteri = 0x00163A48;   // size 0x8
constexpr uint32_t setDupId__7Monsteri = 0x00163A50;   // size 0x8
constexpr uint32_t setTypeOfMonster__7Monsteri = 0x00163A58;   // size 0x8
constexpr uint32_t setMonsterNum__7Monsteri = 0x00163A60;   // size 0x8
constexpr uint32_t setSkinNum__7Monsteri = 0x00163A68;   // size 0x8
constexpr uint32_t setPlayerAiOrFodderNum__7Monsteri = 0x00163A70;   // size 0x8
constexpr uint32_t setPlayerInfo__7MonsterP9PlayerDat = 0x00163A78;   // size 0x8
constexpr uint32_t setInteractiveIndex__7Monsteri = 0x00163A80;   // size 0x8
constexpr uint32_t getInteractiveIndex__7Monster = 0x00163A88;   // size 0x8
constexpr uint32_t setName__7Monsteri = 0x00163A90;   // size 0x8
constexpr uint32_t setPitchRate1__7Monsterf = 0x00163A98;   // size 0x8
constexpr uint32_t setPitchRate2__7Monsterf = 0x00163AA0;   // size 0x8
constexpr uint32_t setCamIdleCircuitTime__7Monsteri = 0x00163AA8;   // size 0x8
constexpr uint32_t setCamIdleFactor__7Monsterf = 0x00163AB0;   // size 0x8
constexpr uint32_t setClosestPath__7MonsterP6AiPath = 0x00163AB8;   // size 0x8
constexpr uint32_t setVulnerable__7Monsterb = 0x00163AC0;   // size 0x8
constexpr uint32_t setInvulnerabilityDuration__7Monsteri = 0x00163AC8;   // size 0x38
constexpr uint32_t setAttacksEnabled__7Monsterb = 0x00163B00;   // size 0x8
constexpr uint32_t setAttackDisableDuration__7Monsteri = 0x00163B08;   // size 0x38
constexpr uint32_t setAimHeadingEnabled__7Monsterb = 0x00163B40;   // size 0x8
constexpr uint32_t setLookAtOverride__7MonsterP8_fvector = 0x00163B48;   // size 0x8
constexpr uint32_t drainStamina__7Monsterfb = 0x00163B50;   // size 0x20
constexpr uint32_t enableStaminaRegen__7Monsterb = 0x00163B70;   // size 0x8
constexpr uint32_t enableSpecialWeapon__7Monsterb = 0x00163B78;   // size 0x8
constexpr uint32_t startHealthPowerUpGlow__7Monsterii = 0x00163B80;   // size 0x10
constexpr uint32_t startStaminaPowerUpGlow__7Monsterii = 0x00163B90;   // size 0x10
constexpr uint32_t startSpecialPowerUpGlow__7Monsteri = 0x00163BA0;   // size 0x14
constexpr uint32_t endSpecialPowerUpGlow__7Monster = 0x00163BB8;   // size 0x8
constexpr uint32_t incrementWinsThisGame__7Monsteri = 0x00163BC0;   // size 0x10
constexpr uint32_t getHudTexture__7Monster = 0x00163BD0;   // size 0x8
constexpr uint32_t okToGlow__7Monsterb = 0x00163BD8;   // size 0x8
constexpr uint32_t okToUnify__7Monsterb = 0x00163BE0;   // size 0x8
constexpr uint32_t updateClosestPath__7MonsterPv = 0x00163BE8;   // size 0x1C
constexpr uint32_t restoreVulnerability__7MonsterPv = 0x00163C08;   // size 0x1C
constexpr uint32_t restoreVulnerability__7Monster = 0x00163C28;   // size 0x10
constexpr uint32_t restoreAttacksEnabled__7MonsterPv = 0x00163C38;   // size 0x1C
constexpr uint32_t restoreAttacksEnabled__7Monster = 0x00163C58;   // size 0x10
constexpr uint32_t setShadow__7MonsterP3_cs = 0x00163C68;   // size 0x8
constexpr uint32_t setHudTexture__7Monsteri = 0x00163C70;   // size 0x8
constexpr uint32_t setReticleCS__7MonsterP3_cs = 0x00163C78;   // size 0x8
constexpr uint32_t setStickyReticleCS__7MonsterP3_cs = 0x00163C80;   // size 0x8
constexpr uint32_t lyingOnGround__7Monster = 0x00163C88;   // size 0x28
constexpr uint32_t getFootHDResult__7Monster = 0x00163CB0;   // size 0x8
constexpr uint32_t init__9AnimPappyR11_animHandle = 0x00163CF8;   // size 0xCC
constexpr uint32_t update__9AnimPappyR13DbInteractive = 0x00163DC8;   // size 0x11C
constexpr uint32_t x__9AnimBlend14AnimBlendTypesi = 0x00163EE8;   // size 0x2C
constexpr uint32_t init__9AnimBlendR11_animHandle = 0x00163F18;   // size 0x84
constexpr uint32_t rampIn__9AnimBlendf = 0x00163FA0;   // size 0x9C
constexpr uint32_t rampOut__9AnimBlendf = 0x00164040;   // size 0xCC
constexpr uint32_t rampOutAndDealloc__9AnimBlendf = 0x00164110;   // size 0x130
constexpr uint32_t runAndDealloc__9AnimBlendff = 0x00164240;   // size 0x184
constexpr uint32_t updateRampOut__9AnimBlend = 0x001643C8;   // size 0x84
constexpr uint32_t updateRampOutAndDealloc__9AnimBlend = 0x00164450;   // size 0xAC
constexpr uint32_t setPercent__9AnimBlendf = 0x00164500;   // size 0x8C
constexpr uint32_t setSpeed__9AnimBlendf = 0x00164590;   // size 0x2C
constexpr uint32_t getPercent__9AnimBlend = 0x001645C0;   // size 0x40
constexpr uint32_t getWeight__9AnimBlend = 0x00164600;   // size 0x6C
constexpr uint32_t x__static_initialization_and_destruction_0_00164670 = 0x00164670;   // size 0xA4
constexpr uint32_t updateRampOut__9AnimBlendPv = 0x00164718;   // size 0x1C
constexpr uint32_t updateRampOutAndDealloc__9AnimBlendPv = 0x00164738;   // size 0x1C
constexpr uint32_t x_GLOBAL__I__13AnimBlendPool_s_pool = 0x00164758;   // size 0x20
constexpr uint32_t x__15MonsterDynamicsR7Monster = 0x00164778;   // size 0x114
constexpr uint32_t init__15MonsterDynamics = 0x00164890;   // size 0xC8
constexpr uint32_t allStop__15MonsterDynamics = 0x00164958;   // size 0x40
constexpr uint32_t smoothToUpright__15MonsterDynamicsf = 0x00164998;   // size 0xC0
constexpr uint32_t setMotionVec__15MonsterDynamicsR8_fvector = 0x00164A58;   // size 0x68
constexpr uint32_t setMotionVecFromPad__15MonsterDynamics = 0x00164AC0;   // size 0xAC
constexpr uint32_t setMotionRotFromPad__15MonsterDynamics = 0x00164B70;   // size 0x30
constexpr uint32_t computeTargetSpeed__15MonsterDynamicsff = 0x00164BA0;   // size 0xC4
constexpr uint32_t updateMove__15MonsterDynamicsb = 0x00164C68;   // size 0x8C
constexpr uint32_t updateAirMotion__15MonsterDynamicsff = 0x00164CF8;   // size 0x114
constexpr uint32_t updateFootMotion__15MonsterDynamicsfff = 0x00164E10;   // size 0x158
constexpr uint32_t updateTurn__15MonsterDynamicsb = 0x00164F68;   // size 0x50
constexpr uint32_t updateTurnMotion__15MonsterDynamicsfff = 0x00164FB8;   // size 0x148
constexpr uint32_t updatePosition__15MonsterDynamics = 0x00165100;   // size 0x3E4
constexpr uint32_t handleHeadCollis__15MonsterDynamicsR9_hdResultf = 0x001654E8;   // size 0x30
constexpr uint32_t handleBodyCollis__15MonsterDynamicsR9_hdResultf = 0x00165518;   // size 0x104
constexpr uint32_t handleGroundCollis__15MonsterDynamicsR9_hdResultf = 0x00165620;   // size 0x280
constexpr uint32_t getContactVel__15MonsterDynamicsR8_fvectorR9_hdResult = 0x001658A0;   // size 0xCC
constexpr uint32_t computeAuxiliaries__15MonsterDynamics = 0x00165970;   // size 0xF4
constexpr uint32_t getSpeedModifier__15MonsterDynamics = 0x00165A68;   // size 0x3C
constexpr uint32_t getConfig__15MonsterDynamics = 0x00165AA8;   // size 0x14
constexpr uint32_t initBeforeDbLoad__7Monster = 0x00165AC0;   // size 0x11C
constexpr uint32_t init__7Monster = 0x00165BE0;   // size 0x330
constexpr uint32_t playerInit__7Monster = 0x00165F10;   // size 0x124
constexpr uint32_t aiInit__7Monster = 0x00166038;   // size 0xDC
constexpr uint32_t initAfterDbLoad__7Monster = 0x00166118;   // size 0x140
constexpr uint32_t initDynamics__7Monster = 0x00166258;   // size 0x115C
constexpr uint32_t initCameraData__7Monster = 0x001673B8;   // size 0x2EC
constexpr uint32_t update__10BoneSpringPv = 0x001676A8;   // size 0x1C
constexpr uint32_t x__9MonsterMc = 0x001676C8;   // size 0x38
constexpr uint32_t getSettingsData__9MonsterMcR13_SettingsData = 0x00167700;   // size 0x11C
constexpr uint32_t setSettingsData__9MonsterMcR13_SettingsData = 0x00167820;   // size 0x114
constexpr uint32_t getSaveData__9MonsterMcR13_SaveGameDatai = 0x00167938;   // size 0x6C
constexpr uint32_t setSaveData__9MonsterMcR13_SaveGameDatai = 0x001679A8;   // size 0xD0
constexpr uint32_t copySaveData__9MonsterMcR13_SaveGameDataT1i = 0x00167A78;   // size 0x5C
constexpr uint32_t stdInit__9MonsterMc = 0x00167AD8;   // size 0xB8
constexpr uint32_t stdExit__9MonsterMc = 0x00167B90;   // size 0x30
constexpr uint32_t updateBootLoad__9MonsterMc = 0x00167BC0;   // size 0x2B8
constexpr uint32_t updateSettingsLoad__9MonsterMc = 0x00167E78;   // size 0x1D4
constexpr uint32_t updateSettingsSave__9MonsterMc = 0x00168050;   // size 0x22C
constexpr uint32_t updateEndOfStorySave__9MonsterMc = 0x00168280;   // size 0x244
constexpr uint32_t updateUnlockSave__9MonsterMc = 0x001684C8;   // size 0x23C
constexpr uint32_t updateStoryModeSave__9MonsterMc = 0x00168708;   // size 0x24C
constexpr uint32_t updateStoryModeLoad__9MonsterMc = 0x00168958;   // size 0x220
constexpr uint32_t updateEndureSave__9MonsterMc = 0x00168B78;   // size 0x23C
constexpr uint32_t onEnter__10McBootPage = 0x00168DB8;   // size 0xC
constexpr uint32_t update__10McBootPageR7McState = 0x00168DC8;   // size 0x2B4
constexpr uint32_t stateIsOk__10McBootPageR7McState = 0x00169080;   // size 0x8
constexpr uint32_t onSelection__10McBootPagei = 0x00169088;   // size 0x24
constexpr uint32_t onBack__10McBootPage = 0x001690B0;   // size 0x14
constexpr uint32_t update__20McCheckForSweetToothR7McState = 0x001690C8;   // size 0x230
constexpr uint32_t reset__20McCheckForSweetTooth = 0x001692F8;   // size 0x8
constexpr uint32_t onSelection__20McCheckForSweetToothi = 0x00169300;   // size 0x20
constexpr uint32_t x__14McSaveSettings = 0x00169320;   // size 0x4C
constexpr uint32_t update__14McSaveSettingsR7McState = 0x00169370;   // size 0x3EC
constexpr uint32_t stateIsOk__14McSaveSettingsR7McState = 0x00169760;   // size 0x8
constexpr uint32_t update__18McCheckForSettingsR7McState = 0x00169768;   // size 0x230
constexpr uint32_t stateIsOk__18McCheckForSettingsR7McState = 0x00169998;   // size 0x60
constexpr uint32_t onEnter__18McCheckForSettings = 0x001699F8;   // size 0xC
constexpr uint32_t onSelection__18McCheckForSettingsi = 0x00169A08;   // size 0x14
constexpr uint32_t update__14McLoadSettingsR7McState = 0x00169A20;   // size 0x1AC
constexpr uint32_t update__12McChooseSlotR7McState = 0x00169BD0;   // size 0xE8
constexpr uint32_t stateIsOk__12McChooseSlotR7McState = 0x00169CB8;   // size 0x8
constexpr uint32_t onSelection__12McChooseSloti = 0x00169CC0;   // size 0x78
constexpr uint32_t shouldSkipOnReturn__12McChooseSlot = 0x00169D38;   // size 0x8
constexpr uint32_t update__12McCheckSpaceR7McState = 0x00169D40;   // size 0x250
constexpr uint32_t shouldMonitorCard__12McCheckSpace = 0x00169F90;   // size 0x48
constexpr uint32_t stateIsOk__12McCheckSpaceR7McState = 0x00169FD8;   // size 0x108
constexpr uint32_t onSelection__12McCheckSpacei = 0x0016A0E0;   // size 0x10
constexpr uint32_t onEnter__12McCheckSpace = 0x0016A0F0;   // size 0xC
constexpr uint32_t update__13McCheckFormatR7McState = 0x0016A100;   // size 0x1A8
constexpr uint32_t stateIsOk__13McCheckFormatR7McState = 0x0016A2A8;   // size 0x3C
constexpr uint32_t onSelection__13McCheckFormati = 0x0016A2E8;   // size 0x1D4
constexpr uint32_t onEnter__13McCheckFormat = 0x0016A4C0;   // size 0xC
constexpr uint32_t update__16McCheckOverwriteR7McState = 0x0016A4D0;   // size 0x188
constexpr uint32_t stateIsOk__16McCheckOverwriteR7McState = 0x0016A658;   // size 0xC
constexpr uint32_t onSelection__16McCheckOverwritei = 0x0016A668;   // size 0x6C
constexpr uint32_t onEnter__16McCheckOverwrite = 0x0016A6D8;   // size 0x168
constexpr uint32_t stateIsOk__15McPrepBoxScreenR7McState = 0x0016A840;   // size 0x8
constexpr uint32_t onEnter__15McPrepBoxScreen = 0x0016A848;   // size 0xB4
constexpr uint32_t shouldSkipOnReturn__15McPrepBoxScreen = 0x0016A900;   // size 0x8
constexpr uint32_t x__15McLoadGameBoxes = 0x0016A908;   // size 0x34
constexpr uint32_t update__15McLoadGameBoxesR7McState = 0x0016A940;   // size 0x28
constexpr uint32_t stateIsOk__15McLoadGameBoxesR7McState = 0x0016A968;   // size 0x8
constexpr uint32_t reset__15McLoadGameBoxes = 0x0016A970;   // size 0x1C
constexpr uint32_t onSelection__15McLoadGameBoxesi = 0x0016A990;   // size 0x78
constexpr uint32_t onEnter__15McLoadGameBoxes = 0x0016AA08;   // size 0xB8
constexpr uint32_t shouldSkipOnReturn__15McLoadGameBoxes = 0x0016AAC0;   // size 0x2C
constexpr uint32_t animateBox__15McLoadGameBoxesb = 0x0016AAF0;   // size 0x58
constexpr uint32_t onBack__15McLoadGameBoxes = 0x0016AB48;   // size 0x1C
constexpr uint32_t x__12McSelectReel = 0x0016AB68;   // size 0x44
constexpr uint32_t update__12McSelectReelR7McState = 0x0016ABB0;   // size 0x2D8
constexpr uint32_t shouldMonitorCard__12McSelectReel = 0x0016AE88;   // size 0x8
constexpr uint32_t stateIsOk__12McSelectReelR7McState = 0x0016AE90;   // size 0x10
constexpr uint32_t refreshSaveData__12McSelectReel = 0x0016AEA0;   // size 0x3AC
constexpr uint32_t shellBackgroundIsVisible__12McSelectReel = 0x0016B250;   // size 0x74
constexpr uint32_t onSelection__12McSelectReeli = 0x0016B2C8;   // size 0x74
constexpr uint32_t reset__12McSelectReel = 0x0016B340;   // size 0x20
constexpr uint32_t onEnter__12McSelectReel = 0x0016B360;   // size 0x74
constexpr uint32_t onBack__12McSelectReel = 0x0016B3D8;   // size 0x78
constexpr uint32_t stateIsOk__15McSaveAdventureR7McState = 0x0016B450;   // size 0x8
constexpr uint32_t onEnter__15McSaveAdventure = 0x0016B458;   // size 0x1EC
constexpr uint32_t onEnter__20McSaveAdventureBlank = 0x0016B648;   // size 0x190
constexpr uint32_t stateIsOk__16McSelectSaveSlotR7McState = 0x0016B7D8;   // size 0x10
constexpr uint32_t onSelection__16McSelectSaveSloti = 0x0016B7E8;   // size 0x24
constexpr uint32_t update__16McSelectSaveSlotR7McState = 0x0016B810;   // size 0x1BC
constexpr uint32_t onEnter__16McSelectSaveSlot = 0x0016B9D0;   // size 0x118
constexpr uint32_t onEnter__11McAskToSave = 0x0016BAE8;   // size 0x28
constexpr uint32_t reset__11McAskToSave = 0x0016BB10;   // size 0x2C
constexpr uint32_t update__11McAskToSaveR7McState = 0x0016BB40;   // size 0x528
constexpr uint32_t onSelection__11McAskToSavei = 0x0016C068;   // size 0xA4
constexpr uint32_t update__11McAskIfSureR7McState = 0x0016C110;   // size 0xD8
constexpr uint32_t onSelection__11McAskIfSurei = 0x0016C1E8;   // size 0x44
constexpr uint32_t onEnter__11McAskIfSure = 0x0016C230;   // size 0xC
constexpr uint32_t update__11McErrorPageR7McState = 0x0016C240;   // size 0x144
constexpr uint32_t onSelection__11McErrorPagei = 0x0016C388;   // size 0x64
constexpr uint32_t onBack__11McErrorPage = 0x0016C3F0;   // size 0x34
constexpr uint32_t update__13McCardRemovedR7McState = 0x0016C428;   // size 0x10C
constexpr uint32_t onSelection__13McCardRemovedi = 0x0016C538;   // size 0x40
constexpr uint32_t x__static_initialization_and_destruction_0_0016C578 = 0x0016C578;   // size 0x234
constexpr uint32_t x__tf10McBootPage = 0x0016C7B8;   // size 0x50
constexpr uint32_t x__10McBootPage = 0x0016C808;   // size 0x34
constexpr uint32_t x__tf20McCheckForSweetTooth = 0x0016C840;   // size 0x50
constexpr uint32_t x__20McCheckForSweetTooth = 0x0016C890;   // size 0x34
constexpr uint32_t x__tf18McCheckForSettings = 0x0016C8C8;   // size 0x50
constexpr uint32_t x__18McCheckForSettings = 0x0016C918;   // size 0x34
constexpr uint32_t x__tf14McLoadSettings = 0x0016C950;   // size 0x50
constexpr uint32_t x__14McLoadSettings = 0x0016C9A0;   // size 0x34
constexpr uint32_t stateIsOk__14McLoadSettingsR7McState = 0x0016C9D8;   // size 0x8
constexpr uint32_t x__tf14McSaveSettings = 0x0016C9E0;   // size 0x50
constexpr uint32_t x__tf12McChooseSlot = 0x0016CA30;   // size 0x50
constexpr uint32_t x__12McChooseSlot = 0x0016CA80;   // size 0x34
constexpr uint32_t setIsOk__12McChooseSlotb = 0x0016CAB8;   // size 0x8
constexpr uint32_t x__tf12McCheckSpace = 0x0016CAC0;   // size 0x50
constexpr uint32_t x__12McCheckSpace = 0x0016CB10;   // size 0x34
constexpr uint32_t x__tf13McCheckFormat = 0x0016CB48;   // size 0x50
constexpr uint32_t x__13McCheckFormat = 0x0016CB98;   // size 0x34
constexpr uint32_t shouldMonitorCard__13McCheckFormat = 0x0016CBD0;   // size 0x8
constexpr uint32_t x__tf16McCheckOverwrite = 0x0016CBD8;   // size 0x50
constexpr uint32_t x__16McCheckOverwrite = 0x0016CC28;   // size 0x34
constexpr uint32_t shouldMonitorCard__16McCheckOverwrite = 0x0016CC60;   // size 0x8
constexpr uint32_t x__tf15McPrepBoxScreen = 0x0016CC68;   // size 0x50
constexpr uint32_t x__15McPrepBoxScreen = 0x0016CCB8;   // size 0x34
constexpr uint32_t update__15McPrepBoxScreenR7McState = 0x0016CCF0;   // size 0x28
constexpr uint32_t getSave__15McPrepBoxScreeni = 0x0016CD18;   // size 0x14
constexpr uint32_t hasData__15McPrepBoxScreeni = 0x0016CD30;   // size 0x10
constexpr uint32_t x__tf15McLoadGameBoxes = 0x0016CD40;   // size 0x50
constexpr uint32_t x__tf12McSelectReel = 0x0016CD90;   // size 0x50
constexpr uint32_t setNeedsSaveDataRefresh__12McSelectReelb = 0x0016CDE0;   // size 0x8
constexpr uint32_t x__tf15McSaveAdventure = 0x0016CDE8;   // size 0x50
constexpr uint32_t x__15McSaveAdventure = 0x0016CE38;   // size 0x34
constexpr uint32_t shouldMonitorCard__15McSaveAdventure = 0x0016CE70;   // size 0x8
constexpr uint32_t x__tf20McSaveAdventureBlank = 0x0016CE78;   // size 0x50
constexpr uint32_t x__20McSaveAdventureBlank = 0x0016CEC8;   // size 0x34
constexpr uint32_t stateIsOk__20McSaveAdventureBlankR7McState = 0x0016CF00;   // size 0x8
constexpr uint32_t shouldMonitorCard__20McSaveAdventureBlank = 0x0016CF08;   // size 0x8
constexpr uint32_t x__tf16McSelectSaveSlot = 0x0016CF10;   // size 0x50
constexpr uint32_t x__16McSelectSaveSlot = 0x0016CF60;   // size 0x38
constexpr uint32_t shouldMonitorCard__16McSelectSaveSlot = 0x0016CF98;   // size 0x8
constexpr uint32_t x__tf11McAskToSave = 0x0016CFA0;   // size 0x50
constexpr uint32_t x__11McAskToSave = 0x0016CFF0;   // size 0x34
constexpr uint32_t stateIsOk__11McAskToSaveR7McState = 0x0016D028;   // size 0x8
constexpr uint32_t whichBackgroundPoly__11McAskToSave = 0x0016D030;   // size 0x8
constexpr uint32_t shouldFadeBackground__11McAskToSave = 0x0016D038;   // size 0x8
constexpr uint32_t x__tf11McAskIfSure = 0x0016D040;   // size 0x50
constexpr uint32_t x__11McAskIfSure = 0x0016D090;   // size 0x34
constexpr uint32_t stateIsOk__11McAskIfSureR7McState = 0x0016D0C8;   // size 0x8
constexpr uint32_t shouldFadeBackground__11McAskIfSure = 0x0016D0D0;   // size 0x8
constexpr uint32_t x__tf11McErrorPage = 0x0016D0D8;   // size 0x50
constexpr uint32_t x__11McErrorPage = 0x0016D128;   // size 0x34
constexpr uint32_t x__tf13McCardRemoved = 0x0016D160;   // size 0x50
constexpr uint32_t x__13McCardRemoved = 0x0016D1B0;   // size 0x34
constexpr uint32_t x_GLOBAL__I__9MonsterMc_global = 0x0016D1E8;   // size 0x20
constexpr uint32_t x__11HealthMeter = 0x0016D208;   // size 0x28
constexpr uint32_t init__11HealthMeter = 0x0016D230;   // size 0x10
constexpr uint32_t reset__11HealthMeter = 0x0016D240;   // size 0x34
constexpr uint32_t creditFull__11HealthMeter = 0x0016D278;   // size 0x20
constexpr uint32_t credit__11HealthMeterf = 0x0016D298;   // size 0x3C
constexpr uint32_t drain__11HealthMeterf = 0x0016D2D8;   // size 0x50
constexpr uint32_t x__12StaminaMeter = 0x0016D328;   // size 0x28
constexpr uint32_t init__12StaminaMeter = 0x0016D350;   // size 0x48
constexpr uint32_t reset__12StaminaMeter = 0x0016D398;   // size 0x14
constexpr uint32_t update__12StaminaMeteri = 0x0016D3B0;   // size 0x70
constexpr uint32_t creditFull__12StaminaMeter = 0x0016D420;   // size 0x1C
constexpr uint32_t creditBaseOnly__12StaminaMeterf = 0x0016D440;   // size 0xBC
constexpr uint32_t credit__12StaminaMeterf = 0x0016D500;   // size 0xC0
constexpr uint32_t drain__12StaminaMeterfbT2 = 0x0016D5C0;   // size 0x164
constexpr uint32_t hasEnough__12StaminaMeterf = 0x0016D728;   // size 0x94
constexpr uint32_t checkTriggerTree__Fv = 0x0016D7C0;   // size 0x1D8
constexpr uint32_t doDeathVolume__FP7MonsterP11TriggerHead = 0x0016D998;   // size 0x24
constexpr uint32_t doSoundVolume__FP7MonsterP11TriggerHead = 0x0016D9C0;   // size 0x50
constexpr uint32_t doDamageVolume__FP7MonsterP11TriggerHead = 0x0016DA10;   // size 0xDC
constexpr uint32_t doHealthVolume__FP7MonsterP11TriggerHead = 0x0016DAF0;   // size 0x50
constexpr uint32_t doStaminaVolume__FP7MonsterP11TriggerHead = 0x0016DB40;   // size 0xAC
constexpr uint32_t doFireVolume__FP7MonsterP11TriggerHead = 0x0016DBF0;   // size 0xC4
constexpr uint32_t doShockVolume__FP7MonsterP11TriggerHead = 0x0016DCB8;   // size 0x1B0
constexpr uint32_t doImpaleVolume__FP7MonsterP11TriggerHead = 0x0016DE68;   // size 0x118
constexpr uint32_t doCamUnifyVolume__FP7MonsterP11TriggerHead = 0x0016DF80;   // size 0x8
constexpr uint32_t doCamVolume__FP7MonsterP11TriggerHead = 0x0016DF88;   // size 0x100
constexpr uint32_t init__13MonsterTweaks = 0x0016E088;   // size 0x20
constexpr uint32_t setActiveMonster__13MonsterTweaksP7Monsterb = 0x0016E0A8;   // size 0x58
constexpr uint32_t resetTweaks__13MonsterTweaks = 0x0016E100;   // size 0x8
constexpr uint32_t x__static_initialization_and_destruction_0_0016E108 = 0x0016E108;   // size 0x30
constexpr uint32_t x_GLOBAL__I_init__13MonsterTweaks = 0x0016E138;   // size 0x20
constexpr uint32_t x__13StateButtSlam = 0x0016E158;   // size 0x1DC
constexpr uint32_t transitionOK__13StateButtSlam = 0x0016E338;   // size 0xBC
constexpr uint32_t transitionFeasible__13StateButtSlam = 0x0016E3F8;   // size 0x140
constexpr uint32_t transitionInto__13StateButtSlam = 0x0016E538;   // size 0x180
constexpr uint32_t update__13StateButtSlam = 0x0016E6B8;   // size 0x1A8
constexpr uint32_t handleCollis__13StateButtSlamR9_hdResult = 0x0016E860;   // size 0x594
constexpr uint32_t acceptHit__13StateButtSlamR8HitEvent = 0x0016EDF8;   // size 0xB4
constexpr uint32_t handlePreemption__13StateButtSlamP12MonsterState = 0x0016EEB0;   // size 0x78
constexpr uint32_t x__10StateClimb = 0x0016EF28;   // size 0xB8
constexpr uint32_t init__10StateClimbP7Monster = 0x0016EFE0;   // size 0x34
constexpr uint32_t transitionOK__10StateClimb = 0x0016F018;   // size 0x7C
constexpr uint32_t transitionFeasible__10StateClimb = 0x0016F098;   // size 0xFC
constexpr uint32_t transitionInto__10StateClimb = 0x0016F198;   // size 0x104
constexpr uint32_t update__10StateClimb = 0x0016F2A0;   // size 0x98C
constexpr uint32_t handlePreemption__10StateClimbP12MonsterState = 0x0016FC30;   // size 0x2C
constexpr uint32_t enterSubState__10StateClimbQ210StateClimb8SubState = 0x0016FC60;   // size 0x308
constexpr uint32_t checkWallContact__10StateClimb = 0x0016FF68;   // size 0x4A0
constexpr uint32_t getMinContact__10StateClimbP9_hdResultib = 0x00170408;   // size 0xE0
constexpr uint32_t x__9StateDash = 0x001704E8;   // size 0xA0
constexpr uint32_t transitionOK__9StateDash = 0x00170588;   // size 0xA4
constexpr uint32_t transitionInto__9StateDash = 0x00170630;   // size 0x7C
constexpr uint32_t update__9StateDash = 0x001706B0;   // size 0x348
constexpr uint32_t enterSubState__9StateDashQ29StateDash8SubState = 0x001709F8;   // size 0x84
constexpr uint32_t handleCollis__9StateDashR9_hdResult = 0x00170A80;   // size 0x194
constexpr uint32_t handlePreemption__9StateDashP12MonsterState = 0x00170C18;   // size 0x38
constexpr uint32_t transitionInto__11JumpFlyBase = 0x00170C50;   // size 0x1C
constexpr uint32_t update__11JumpFlyBase = 0x00170C70;   // size 0x3C
constexpr uint32_t handleStateTransitions__11JumpFlyBase = 0x00170CB0;   // size 0x424
constexpr uint32_t x__8StateFly = 0x001710D8;   // size 0x2D8
constexpr uint32_t init__8StateFlyP7Monster = 0x001713B0;   // size 0x54
constexpr uint32_t getJumpHeightGain__8StateFly = 0x00171408;   // size 0x44
constexpr uint32_t getFlapHeightGain__8StateFly = 0x00171450;   // size 0x44
constexpr uint32_t getFlapStaminaDrain__8StateFly = 0x00171498;   // size 0x20
constexpr uint32_t getRelevantConfig__8StateFly = 0x001714B8;   // size 0x48
constexpr uint32_t transitionOK__8StateFly = 0x00171500;   // size 0x98
constexpr uint32_t transitionFeasible__8StateFly = 0x00171598;   // size 0x2C
constexpr uint32_t transitionInto__8StateFly = 0x001715C8;   // size 0x260
constexpr uint32_t update__8StateFly = 0x00171828;   // size 0xFE4
constexpr uint32_t enterSubState__8StateFlyQ211JumpFlyBase8SubState = 0x00172810;   // size 0x800
constexpr uint32_t handleCollis__8StateFlyR9_hdResult = 0x00173010;   // size 0x1D4
constexpr uint32_t handlePreemption__8StateFlyP12MonsterState = 0x001731E8;   // size 0xBC
constexpr uint32_t x__9StateJump = 0x001732A8;   // size 0x1D0
constexpr uint32_t init__9StateJumpP7Monster = 0x00173478;   // size 0x54
constexpr uint32_t getJumpHeightGain__9StateJump = 0x001734D0;   // size 0x44
constexpr uint32_t getRelevantConfig__9StateJump = 0x00173518;   // size 0x48
constexpr uint32_t transitionOK__9StateJump = 0x00173560;   // size 0xA0
constexpr uint32_t transitionFeasible__9StateJump = 0x00173600;   // size 0x2C
constexpr uint32_t transitionInto__9StateJump = 0x00173630;   // size 0x138
constexpr uint32_t update__9StateJump = 0x00173768;   // size 0x750
constexpr uint32_t enterSubState__9StateJumpQ211JumpFlyBase8SubState = 0x00173EB8;   // size 0x5BC
constexpr uint32_t handleCollis__9StateJumpR9_hdResult = 0x00174478;   // size 0x11C
constexpr uint32_t handlePreemption__9StateJumpP12MonsterState = 0x00174598;   // size 0x84
constexpr uint32_t x__14StateRamAttack = 0x00174620;   // size 0x100
constexpr uint32_t setMaxSpeedMPH__14StateRamAttackf = 0x00174720;   // size 0x24
constexpr uint32_t transitionOK__14StateRamAttack = 0x00174748;   // size 0xB0
constexpr uint32_t transitionInto__14StateRamAttack = 0x001747F8;   // size 0x4C
constexpr uint32_t update__14StateRamAttack = 0x00174848;   // size 0x334
constexpr uint32_t handleCollis__14StateRamAttackR9_hdResult = 0x00174B80;   // size 0x460
constexpr uint32_t enterSubState__14StateRamAttackQ214StateRamAttack8SubState = 0x00174FE0;   // size 0x3B4
constexpr uint32_t handlePreemption__14StateRamAttackP12MonsterState = 0x00175398;   // size 0x44
constexpr uint32_t acceptHit__14StateRamAttackR8HitEvent = 0x001753E0;   // size 0xBC
constexpr uint32_t x__8StateRun = 0x001754A0;   // size 0x84
constexpr uint32_t init__8StateRunP7Monster = 0x00175528;   // size 0x32C
constexpr uint32_t transitionOK__8StateRun = 0x00175858;   // size 0xFC
constexpr uint32_t transitionInto__8StateRun = 0x00175958;   // size 0x234
constexpr uint32_t update__8StateRun = 0x00175B90;   // size 0x700
constexpr uint32_t enterSubState__8StateRunQ28StateRun8SubState = 0x00176290;   // size 0x608
constexpr uint32_t handlePreemption__8StateRunP12MonsterState = 0x00176898;   // size 0x5C
constexpr uint32_t getAnimSpeed__8StateRunf = 0x001768F8;   // size 0xC0
constexpr uint32_t x__tf13StateButtSlam = 0x001769E0;   // size 0x50
constexpr uint32_t x__tf10StateClimb = 0x00176A30;   // size 0x50
constexpr uint32_t getSubState__10StateClimb = 0x00176A80;   // size 0x8
constexpr uint32_t getClimbMotion__10StateClimb = 0x00176A88;   // size 0x8
constexpr uint32_t getHuckVelocity__10StateClimb = 0x00176A90;   // size 0x8
constexpr uint32_t getContactNormal__10StateClimb = 0x00176A98;   // size 0x8
constexpr uint32_t getContactInteractive__10StateClimb = 0x00176AA0;   // size 0x8
constexpr uint32_t getContactFlags__10StateClimb = 0x00176AA8;   // size 0x8
constexpr uint32_t x__tf9StateDash = 0x00176AB0;   // size 0x50
constexpr uint32_t getSubState__9StateDash = 0x00176B00;   // size 0x8
constexpr uint32_t x__tf11JumpFlyBase = 0x00176B08;   // size 0x50
constexpr uint32_t x__11JumpFlyBaseQ212MonsterState2IdQ212MonsterState4Caps = 0x00176B58;   // size 0x3C
constexpr uint32_t setFootPref__11JumpFlyBaseQ211JumpFlyBase8FootPref = 0x00176B98;   // size 0x8
constexpr uint32_t getSubState__11JumpFlyBase = 0x00176BA0;   // size 0x8
constexpr uint32_t getApexHeight__11JumpFlyBase = 0x00176BA8;   // size 0x8
constexpr uint32_t getHeightGain__11JumpFlyBase = 0x00176BB0;   // size 0x10
constexpr uint32_t x__tf8StateFly = 0x00176BC0;   // size 0x88
constexpr uint32_t getSubState__8StateFly = 0x00176C48;   // size 0x8
constexpr uint32_t x__tf9StateJump = 0x00176C50;   // size 0x88
constexpr uint32_t x__tf14StateRamAttack = 0x00176CD8;   // size 0x50
constexpr uint32_t setRamDuration__14StateRamAttackf = 0x00176D28;   // size 0x8
constexpr uint32_t setAirDeccel__14StateRamAttackf = 0x00176D30;   // size 0x8
constexpr uint32_t setAcceleration__14StateRamAttackf = 0x00176D38;   // size 0x8
constexpr uint32_t x__tf8StateRun = 0x00176D40;   // size 0x50
constexpr uint32_t setLandMomentum__8StateRunf = 0x00176D90;   // size 0x8
constexpr uint32_t getAnimIndex__8StateRun = 0x00176D98;   // size 0x8
constexpr uint32_t x__8OgreMaceP7Monster = 0x00176DF0;   // size 0x1B0
constexpr uint32_t setMode__8OgreMaceQ28OgreMace4Mode = 0x00176FA0;   // size 0x60
constexpr uint32_t update__8OgreMace = 0x00177000;   // size 0x678
constexpr uint32_t setEndPos__8OgreMaceP8_fvector = 0x00177678;   // size 0x8
constexpr uint32_t x__8PadFlags = 0x00177680;   // size 0x10C
constexpr uint32_t init__8PadFlagsP7Monster = 0x00177790;   // size 0x148
constexpr uint32_t setCurrentAction__8PadFlags13ButtonActions13MappedActions = 0x001778D8;   // size 0x30
constexpr uint32_t pushAction__8PadFlags13ButtonActions13MappedActions = 0x00177908;   // size 0x170
constexpr uint32_t nextButtonAction__8PadFlags = 0x00177A78;   // size 0xAC
constexpr uint32_t nextMappedAction__8PadFlags = 0x00177B28;   // size 0xAC
constexpr uint32_t popAction__8PadFlags = 0x00177BD8;   // size 0x60
constexpr uint32_t clear__8PadFlagsii = 0x00177C38;   // size 0x2C
constexpr uint32_t saveAndClear__8PadFlagsi = 0x00177C68;   // size 0x68
constexpr uint32_t clearModifiers__8PadFlags = 0x00177CD0;   // size 0x14
constexpr uint32_t x__vc__8PadFlagsi = 0x00177CE8;   // size 0x34
constexpr uint32_t interpretInputs__8PadFlagsR7GamePad = 0x00177D20;   // size 0x148
constexpr uint32_t computeMotionVec__8PadFlagsR8_fvector = 0x00177E68;   // size 0x98
constexpr uint32_t computeMotionRot__8PadFlags = 0x00177F00;   // size 0x128
constexpr uint32_t updateViewChanges__8PadFlagsR7GamePad = 0x00178028;   // size 0xF4
constexpr uint32_t okToChangeMap__8PadFlags = 0x00178120;   // size 0x1D4
constexpr uint32_t dashDoubleTap__8PadFlagsR7GamePad = 0x001782F8;   // size 0xF0
constexpr uint32_t checkCombos__8PadFlagsR7GamePad = 0x001783E8;   // size 0x8F0
constexpr uint32_t checkTaunt__8PadFlags = 0x00178CD8;   // size 0x8
constexpr uint32_t clearSecretCode__8PadFlags = 0x00178CE0;   // size 0x14
constexpr uint32_t clearCombo__8PadFlags = 0x00178CF8;   // size 0x60
constexpr uint32_t updateInput0ConfigCR__8PadFlagsR7GamePad = 0x00178D58;   // size 0x8E0
constexpr uint32_t updateInput0ConfigSR__8PadFlagsR7GamePad = 0x00179638;   // size 0xEA4
constexpr uint32_t updateActions0__8PadFlagsR7GamePad = 0x0017A4E0;   // size 0x774
constexpr uint32_t updateHistory0__8PadFlagsR7GamePad = 0x0017AC58;   // size 0x29C
constexpr uint32_t updateInput1ConfigCR__8PadFlagsR7GamePad = 0x0017AEF8;   // size 0x8C8
constexpr uint32_t updateInput1ConfigSR__8PadFlagsR7GamePad = 0x0017B7C0;   // size 0xE90
constexpr uint32_t updateActions1__8PadFlagsR7GamePad = 0x0017C650;   // size 0x6E0
constexpr uint32_t updateHistory1__8PadFlagsR7GamePad = 0x0017CD30;   // size 0x29C
constexpr uint32_t updateInput2ConfigCR__8PadFlagsR7GamePad = 0x0017CFD0;   // size 0x714
constexpr uint32_t updateInput2ConfigSR__8PadFlagsR7GamePad = 0x0017D6E8;   // size 0xCE0
constexpr uint32_t updateActions2__8PadFlagsR7GamePad = 0x0017E3C8;   // size 0x6CC
constexpr uint32_t updateHistory2__8PadFlagsR7GamePad = 0x0017EA98;   // size 0x22C
constexpr uint32_t removePath__8PathNodeUs = 0x0017ECC8;   // size 0x8C
constexpr uint32_t getRandNode__7PathNet = 0x0017ED58;   // size 0x40
constexpr uint32_t getClosestNode__7PathNetR8_fvectori = 0x0017ED98;   // size 0x8C
constexpr uint32_t getClosestPath__7PathNetR8_fvectorRii = 0x0017EE28;   // size 0x174
constexpr uint32_t getClosestPath__7PathNetR8_fvectorT1Rii = 0x0017EFA0;   // size 0x118
constexpr uint32_t loadPoints__8PathToolPcb = 0x0017F0B8;   // size 0x70
constexpr uint32_t init__8PathTooli = 0x0017F128;   // size 0x54
constexpr uint32_t exportPoints__8PathTool = 0x0017F180;   // size 0x16C
constexpr uint32_t x__static_initialization_and_destruction_0_0017F2F0 = 0x0017F2F0;   // size 0x50
constexpr uint32_t x__tf8PathTool = 0x0017F340;   // size 0x50
constexpr uint32_t x__8PathToolP7PathNetPcT2 = 0x0017F390;   // size 0x20
constexpr uint32_t x_GLOBAL__I__8PathTool_instance = 0x0017F3B0;   // size 0x20
constexpr uint32_t x__6Pickup = 0x0017F3D0;   // size 0x24
constexpr uint32_t init__6PickupP9_hierheadR8_fvectorPA3_fb11ePickupType = 0x0017F3F8;   // size 0x298
constexpr uint32_t initAfterDbLoad__6Pickup = 0x0017F690;   // size 0x5C
constexpr uint32_t update__6Pickup = 0x0017F6F0;   // size 0x60
constexpr uint32_t regenUpdate__6Pickup = 0x0017F750;   // size 0x2C
constexpr uint32_t regen__6Pickup = 0x0017F780;   // size 0x64
constexpr uint32_t takeHit__6PickupP8_fvectorfi = 0x0017F7E8;   // size 0x10
constexpr uint32_t takeUseDamage__6Pickupf = 0x0017F7F8;   // size 0x10
constexpr uint32_t isTargetClear__6PickupP13DbInteractive = 0x0017F808;   // size 0x198
constexpr uint32_t grab__6Pickupi = 0x0017F9A0;   // size 0x118
constexpr uint32_t drop__6Pickup = 0x0017FAB8;   // size 0x44
constexpr uint32_t kill__6Pickup = 0x0017FB00;   // size 0x284
constexpr uint32_t initConfig__6Pickup = 0x0017FD88;   // size 0x28
constexpr uint32_t updateCS__6PickupP8_fvectorRA3_A3_fb = 0x0017FDB0;   // size 0xCC
constexpr uint32_t getVel__6PickupR8_fvector = 0x0017FE80;   // size 0x68
constexpr uint32_t getVel__6Pickup = 0x0017FEE8;   // size 0x40
constexpr uint32_t setVisualState__6Pickupi = 0x0017FF28;   // size 0x40
constexpr uint32_t hatCheck__6Pickup = 0x0017FF68;   // size 0x298
constexpr uint32_t initLocators__6PickupP9_hierhead = 0x00180200;   // size 0x18C
constexpr uint32_t initHandle__6PickupP14_hiertranslate = 0x00180390;   // size 0x74
constexpr uint32_t initHandle__6PickupP11_hierrotate = 0x00180408;   // size 0xC8
constexpr uint32_t initCollisSpheres__6PickupP8_fvector = 0x001804D0;   // size 0x118
constexpr uint32_t swipeTrailEffect__6Pickup = 0x001805E8;   // size 0x98
constexpr uint32_t collisTestBatAttack__6PickupfUi11MonsterAnimR10HitHistoryfff = 0x00180680;   // size 0x3C8
constexpr uint32_t getCollisSphereTrans__6Pickupi = 0x00180A48;   // size 0x10
constexpr uint32_t getCollisSphereRadius__6Pickupi = 0x00180A58;   // size 0x10
constexpr uint32_t updateGenerator__6Pickup = 0x00180A68;   // size 0x20C
constexpr uint32_t init__12LevelPickups = 0x00180C78;   // size 0x2C8
constexpr uint32_t turnOffAllVehicles__12LevelPickups = 0x00180F40;   // size 0xC4
constexpr uint32_t turnOffMilitary__12LevelPickups = 0x00181008;   // size 0xE0
constexpr uint32_t turnOnMilitary__12LevelPickups = 0x001810E8;   // size 0xE4
constexpr uint32_t turnOff2Handeds__12LevelPickups = 0x001811D0;   // size 0xE0
constexpr uint32_t initMilitaryForCentral__12LevelPickups = 0x001812B0;   // size 0x134
constexpr uint32_t initMilitaryForVegas__12LevelPickups = 0x001813E8;   // size 0x368
constexpr uint32_t createPickup__12LevelPickupsP9_hierheadR8_fvectorPA3_fb11ePickupType = 0x00181750;   // size 0x4D0
constexpr uint32_t initAfterDbLoad__12LevelPickups = 0x00181C20;   // size 0x64
constexpr uint32_t createPickupHighlight__12LevelPickupsP9_hierhead = 0x00181C88;   // size 0x8C
constexpr uint32_t startCinema__12LevelPickups = 0x00181D18;   // size 0xF8
constexpr uint32_t endCinema__12LevelPickups = 0x00181E10;   // size 0xB0
constexpr uint32_t update__12LevelPickups = 0x00181EC0;   // size 0x1FC
constexpr uint32_t hatCheck__12LevelPickupsP8_fvectorf = 0x001820C0;   // size 0x12C
constexpr uint32_t turnOffForDelayedHat__12LevelPickupsP8_fvectorf = 0x001821F0;   // size 0xF8
constexpr uint32_t computeHighlight__12LevelPickupsR7Monster = 0x001822E8;   // size 0xD0
constexpr uint32_t enableHighlights__12LevelPickupsi = 0x001823B8;   // size 0x2E0
constexpr uint32_t impalePickup__12LevelPickupsGQ2t10LinkedList1ZP6Pickup8Iterator = 0x00182698;   // size 0x58
constexpr uint32_t grabPickup__12LevelPickupsGQ2t10LinkedList1ZP6Pickup8Iteratori = 0x001826F0;   // size 0x7C
constexpr uint32_t dropPickup__12LevelPickupsGQ2t10LinkedList1ZP6Pickup8Iterator = 0x00182770;   // size 0x7C
constexpr uint32_t throwPickup__12LevelPickupsGQ2t10LinkedList1ZP6Pickup8IteratorR8_fvectorP13DbInteractiveT3 = 0x001827F0;   // size 0x15C
constexpr uint32_t throwRBPickup__12LevelPickupsGQ2t10LinkedList1ZP6Pickup8IteratorR8_fvectorP13DbInteractive = 0x00182950;   // size 0xBC
constexpr uint32_t deflectThrownPickup__12LevelPickupsGQ2t10LinkedList1ZP6Pickup8IteratorR8_fvectorP13DbInteractiveT3 = 0x00182A10;   // size 0x168
constexpr uint32_t grabThrownPickup__12LevelPickupsGQ2t10LinkedList1ZP6Pickup8Iteratori = 0x00182B78;   // size 0x108
constexpr uint32_t prunePickup__12LevelPickupsGQ2t10LinkedList1ZP6Pickup8Iterator = 0x00182C80;   // size 0x9C
constexpr uint32_t killPickup__12LevelPickupsGQ2t10LinkedList1ZP6Pickup8Iteratori = 0x00182D20;   // size 0x130
constexpr uint32_t inFlight__12LevelPickupsGQ2t10LinkedList1ZP6Pickup8Iterator = 0x00182E50;   // size 0x3C
constexpr uint32_t getClosestPickup__12LevelPickupsR8_fvectorT1 = 0x00182E90;   // size 0x210
constexpr uint32_t getClosestThrownPickup__12LevelPickupsR8_fvectorf = 0x001830A0;   // size 0x88
constexpr uint32_t x__static_initialization_and_destruction_0_00183128 = 0x00183128;   // size 0xA8
constexpr uint32_t x__tf6Pickup = 0x00183208;   // size 0x50
constexpr uint32_t getRef__6Pickup = 0x00183258;   // size 0x8
constexpr uint32_t getHealth__6Pickup = 0x00183260;   // size 0x8
constexpr uint32_t getSpeed__6Pickup = 0x00183268;   // size 0x8
constexpr uint32_t getDbId__6Pickup = 0x00183270;   // size 0x14
constexpr uint32_t getPickupType__6Pickup = 0x00183288;   // size 0x8
constexpr uint32_t getWeapIdx__6Pickup = 0x00183290;   // size 0x8
constexpr uint32_t getGrabber__6Pickup = 0x00183298;   // size 0x8
constexpr uint32_t isRegenable__6Pickup = 0x001832A0;   // size 0xC
constexpr uint32_t isTwoHanded__6Pickup = 0x001832B0;   // size 0x14
constexpr uint32_t isRigidBody__6Pickup = 0x001832C8;   // size 0x14
constexpr uint32_t isADragonHead__6Pickup = 0x001832E0;   // size 0x14
constexpr uint32_t getSwipeRange__6Pickup = 0x001832F8;   // size 0x10
constexpr uint32_t getNumCollisSpheres__6Pickup = 0x00183308;   // size 0x8
constexpr uint32_t setRef__6PickupGQ2t10LinkedList1ZP6Pickup8Iterator = 0x00183310;   // size 0xC
constexpr uint32_t setGrabber__6Pickupi = 0x00183320;   // size 0x8
constexpr uint32_t setWeapIdx__6Pickupi = 0x00183328;   // size 0x8
constexpr uint32_t updateGenerator__6PickupPv = 0x00183330;   // size 0x1C
constexpr uint32_t resetHealth__6Pickup = 0x00183350;   // size 0xC
constexpr uint32_t getHandleRange__6Pickup = 0x00183360;   // size 0x8
constexpr uint32_t x_GLOBAL__I___6Pickup = 0x001833A8;   // size 0x20
constexpr uint32_t initBefore__9PlantBoss = 0x001833C8;   // size 0x98
constexpr uint32_t initAfter__9PlantBoss = 0x00183460;   // size 0x94
constexpr uint32_t wakeUp__9PlantBoss = 0x001834F8;   // size 0x64
constexpr uint32_t update__9PlantBoss = 0x00183560;   // size 0x8A8
constexpr uint32_t decideChompOrSpit__9PlantBoss = 0x00183E08;   // size 0x1AC
constexpr uint32_t initBefore__13PlantBossHead = 0x00183FB8;   // size 0x2C
constexpr uint32_t initAfter__13PlantBossHeadi = 0x00183FE8;   // size 0x1F4
constexpr uint32_t wakeUp__13PlantBossHeadi = 0x001841E0;   // size 0xC8
constexpr uint32_t enterState__13PlantBossHeadi = 0x001842A8;   // size 0x2B8
constexpr uint32_t update__13PlantBossHead = 0x00184560;   // size 0x10DC
constexpr uint32_t takeHit__13PlantBossHeadP8_fvectorfi = 0x00185640;   // size 0x2A4
constexpr uint32_t initBefore__13PlantBossRoot = 0x001858E8;   // size 0xC
constexpr uint32_t initAfter__13PlantBossRooti = 0x001858F8;   // size 0x13C
constexpr uint32_t wakeUp__13PlantBossRooti = 0x00185A38;   // size 0x2B4
constexpr uint32_t enterState__13PlantBossRooti = 0x00185CF0;   // size 0xEC
constexpr uint32_t update__13PlantBossRoot = 0x00185DE0;   // size 0x318
constexpr uint32_t takeHit__13PlantBossRootP8_fvectorfi = 0x001860F8;   // size 0x3C
constexpr uint32_t initBefore__13PlantBossBase = 0x00186138;   // size 0xC
constexpr uint32_t initAfter__13PlantBossBase = 0x00186148;   // size 0xC4
constexpr uint32_t wakeUp__13PlantBossBase = 0x00186210;   // size 0x64
constexpr uint32_t enterState__13PlantBossBasei = 0x00186278;   // size 0x154
constexpr uint32_t update__13PlantBossBase = 0x001863D0;   // size 0x504
constexpr uint32_t takeHit__13PlantBossBaseP8_fvectorfi = 0x001868D8;   // size 0x50
constexpr uint32_t x__tf13PlantBossHead = 0x00186970;   // size 0x50
constexpr uint32_t getPinTrans__13PlantBossHead = 0x001869C0;   // size 0x8
constexpr uint32_t x__tf13PlantBossRoot = 0x001869C8;   // size 0x50
constexpr uint32_t x__tf13PlantBossBase = 0x00186A18;   // size 0x50
constexpr uint32_t testHidden__7PowerUp = 0x00186AA8;   // size 0x164
constexpr uint32_t placeShadow__7PowerUp = 0x00186C10;   // size 0x11C
constexpr uint32_t initPowerUpsBefore__8PowerUps = 0x00186D30;   // size 0xE0
constexpr uint32_t initPowerUpsAfter__8PowerUps = 0x00186E10;   // size 0x8
constexpr uint32_t update__8PowerUps = 0x00186E18;   // size 0x28C
constexpr uint32_t CreatePowerUp__8PowerUpsiP8_fvector = 0x001870A8;   // size 0x20
constexpr uint32_t CreatePowerUp__8PowerUpsiUcP8_fvector = 0x001870C8;   // size 0x2A0
constexpr uint32_t addPowerUpEpNode__8PowerUpsP9_hierhead = 0x00187368;   // size 0x44
constexpr uint32_t KillPowerUps__8PowerUps = 0x001873B0;   // size 0xD4
constexpr uint32_t KillPowerUp__8PowerUpsi = 0x00187488;   // size 0xCC
constexpr uint32_t KillPowerUp__8PowerUpsP3_cs = 0x00187558;   // size 0x6C
constexpr uint32_t RemovePowerUpsByTypeEntry__8PowerUpsi = 0x001875C8;   // size 0xE8
constexpr uint32_t AddPowerUpsByTypeEntry__8PowerUpsi = 0x001876B0;   // size 0x40
constexpr uint32_t ChangePowerUpsByTypeEntry__8PowerUpsii = 0x001876F0;   // size 0x60
constexpr uint32_t ChangePowerUpType__8PowerUpsii = 0x00187750;   // size 0x8C
constexpr uint32_t setRegenTimeMultiplier__8PowerUpsf = 0x001877E0;   // size 0x8
constexpr uint32_t setRegenTime__8PowerUpsQ27PowerUp4Typeii = 0x001877E8;   // size 0x58
constexpr uint32_t setCredit__8PowerUpsQ27PowerUp4Typei = 0x00187840;   // size 0x18
constexpr uint32_t setCloakTime__8PowerUpsi = 0x00187858;   // size 0x8
constexpr uint32_t getRegenTimeMultiplier__8PowerUps = 0x00187860;   // size 0x8
constexpr uint32_t getMinRegenTime__8PowerUpsQ27PowerUp4Type = 0x00187868;   // size 0x18
constexpr uint32_t getMaxRegenTime__8PowerUpsQ27PowerUp4Type = 0x00187880;   // size 0x18
constexpr uint32_t getCredit__8PowerUpsQ27PowerUp4Type = 0x00187898;   // size 0x18
constexpr uint32_t getCloakTime__8PowerUps = 0x001878B0;   // size 0x8
constexpr uint32_t LoadRegenTimes__8PowerUps = 0x001878B8;   // size 0x64
constexpr uint32_t TakeHit__8PowerUpsP7MonsterP3_cs = 0x00187920;   // size 0x5D4
constexpr uint32_t GetClosestPowerUp__8PowerUpsP8_fvectori = 0x00187EF8;   // size 0x160
constexpr uint32_t startCinema__8PowerUps = 0x00188058;   // size 0x50
constexpr uint32_t endCinema__8PowerUps = 0x001880A8;   // size 0x54
constexpr uint32_t loadPoints__11PowerUpToolPcb = 0x00188100;   // size 0x74
constexpr uint32_t init__11PowerUpTooli = 0x00188178;   // size 0x54
constexpr uint32_t exportPoints__11PowerUpTool = 0x001881D0;   // size 0x80
constexpr uint32_t x__static_initialization_and_destruction_0_00188250 = 0x00188250;   // size 0x28
constexpr uint32_t x__tf11PowerUpTool = 0x00188278;   // size 0x50
constexpr uint32_t x__11PowerUpTool = 0x001882C8;   // size 0x14
constexpr uint32_t x_GLOBAL__I__11PowerUpTool_instance = 0x001882E0;   // size 0x20
constexpr uint32_t startFrame__10RtLoopViewv = 0x00188300;   // size 0x8
constexpr uint32_t start__10RtLoopViewi = 0x00188308;   // size 0x8
constexpr uint32_t end__10RtLoopViewUii = 0x00188310;   // size 0x8
constexpr uint32_t rtMain__Fb = 0x00188318;   // size 0x848
constexpr uint32_t rtPauseRT__FUi = 0x00188B60;   // size 0x278
constexpr uint32_t rtReturnToShell__Fii = 0x00188DD8;   // size 0x28
constexpr uint32_t rtTimeToReturnToShell__Fv = 0x00188E00;   // size 0xA0
constexpr uint32_t rtFadeScreen__FiUi = 0x00188EA0;   // size 0x138
constexpr uint32_t rtWaitForVu1__Fv = 0x00188FD8;   // size 0x54
constexpr uint32_t x__5reset11CREATOR_KEY = 0x00189030;   // size 0x3C
constexpr uint32_t writeKeys__5resetPc = 0x00189070;   // size 0x130
constexpr uint32_t readKey__5resetPc = 0x001891A0;   // size 0x13C
constexpr uint32_t oldStyleRead__5resetPc = 0x001892E0;   // size 0x23C
constexpr uint32_t x__tf5reset = 0x00189520;   // size 0x50
constexpr uint32_t init__11RigidDebrisP14ActDebrisChunkP3_csR8_fvectorf = 0x00189570;   // size 0x114
constexpr uint32_t setFriction__11RigidDebrisff = 0x00189688;   // size 0xC
constexpr uint32_t configBox__11RigidDebrisfffffff = 0x00189698;   // size 0x170
constexpr uint32_t configBox__11RigidDebrisffff = 0x00189808;   // size 0x38
constexpr uint32_t configTent__11RigidDebrisfffif = 0x00189840;   // size 0x154
constexpr uint32_t configPyramid__11RigidDebrisfff = 0x00189998;   // size 0x130
constexpr uint32_t configPole__11RigidDebrisfff = 0x00189AC8;   // size 0x2C
constexpr uint32_t configPole__11RigidDebrisffff = 0x00189AF8;   // size 0xF4
constexpr uint32_t configCylinder__11RigidDebrisfff = 0x00189BF0;   // size 0x168
constexpr uint32_t configPointCloud__11RigidDebrisifffffff = 0x00189D58;   // size 0xB4
constexpr uint32_t takeHit__11RigidDebrisfP8_fvectorP8HitEvent = 0x00189E10;   // size 0x158
constexpr uint32_t pointCollis__11RigidDebris = 0x00189F68;   // size 0x2C
constexpr uint32_t cylinderCollis__11RigidDebris = 0x00189F98;   // size 0x2C
constexpr uint32_t sphereCollis__11RigidDebris = 0x00189FC8;   // size 0x9C
constexpr uint32_t update__11RigidDebris = 0x0018A068;   // size 0x444
constexpr uint32_t init__11RigidPickupP9_hierheadR8_fvectorPA3_fb11ePickupType = 0x0018A4B0;   // size 0x460
constexpr uint32_t initAfterDbLoad__11RigidPickup = 0x0018A910;   // size 0x134
constexpr uint32_t drop__11RigidPickup = 0x0018AA48;   // size 0xB0
constexpr uint32_t grab__11RigidPickupi = 0x0018AAF8;   // size 0x3C
constexpr uint32_t kill__11RigidPickup = 0x0018AB38;   // size 0x104
constexpr uint32_t configSphereCloud__11RigidPickupf = 0x0018AC40;   // size 0x1B4
constexpr uint32_t update__11RigidPickup = 0x0018ADF8;   // size 0x3E0
constexpr uint32_t updateHeld__11RigidPickup = 0x0018B1D8;   // size 0x40
constexpr uint32_t updateActive__11RigidPickup = 0x0018B218;   // size 0x7E4
constexpr uint32_t throwToTarget__11RigidPickupR8_fvectorP13DbInteractive = 0x0018BA00;   // size 0x130
constexpr uint32_t throwInDir__11RigidPickupR8_fvectorP13DbInteractive = 0x0018BB30;   // size 0x5C
constexpr uint32_t huck__11RigidPickupR8_fvectorP13DbInteractive = 0x0018BB90;   // size 0x164
constexpr uint32_t takeHit__11RigidPickupP8_fvectorfi = 0x0018BCF8;   // size 0xB98
constexpr uint32_t doBoundaryFx__11RigidPickupR9_hdResult = 0x0018C890;   // size 0x48
constexpr uint32_t getNextCollis__11RigidPickupP19_hdSphereBundleTestiP9_hdResult = 0x0018C8D8;   // size 0x15C
constexpr uint32_t createCollisEvents__11RigidPickupR9_hdResult = 0x0018CA38;   // size 0x494
constexpr uint32_t resolvePuntCollis__11RigidPickupP19_hdSphereBundleTest = 0x0018CED0;   // size 0x174
constexpr uint32_t resolveCollis__11RigidPickupR9_hdResultRf = 0x0018D048;   // size 0x10C
constexpr uint32_t getVel__11RigidPickupR8_fvector = 0x0018D158;   // size 0x14
constexpr uint32_t getContactVel__11RigidPickupR8_fvectorR9_hdResult = 0x0018D170;   // size 0x20
constexpr uint32_t x__static_initialization_and_destruction_0_0018D190 = 0x0018D190;   // size 0x30
constexpr uint32_t numInteractivesHit__10HitHistory = 0x0018D1F8;   // size 0x8
constexpr uint32_t numNonInteractivesHit__10HitHistory = 0x0018D200;   // size 0x8
constexpr uint32_t x__tf11RigidPickup = 0x0018D208;   // size 0x50
constexpr uint32_t x__11RigidPickup = 0x0018D258;   // size 0x44
constexpr uint32_t reset__11RigidPickup = 0x0018D2A0;   // size 0x30
constexpr uint32_t activate__11RigidPickup = 0x0018D2D0;   // size 0x18
constexpr uint32_t deActivate__11RigidPickup = 0x0018D2E8;   // size 0x14
constexpr uint32_t getNumSpheres__11RigidPickup = 0x0018D300;   // size 0x8
constexpr uint32_t getSphere__11RigidPickupi = 0x0018D308;   // size 0x10
constexpr uint32_t getType__11RigidPickup = 0x0018D318;   // size 0x8
constexpr uint32_t getNumBounces__11RigidPickup = 0x0018D320;   // size 0x8
constexpr uint32_t getVel__11RigidPickup = 0x0018D328;   // size 0x8
constexpr uint32_t inFlight__11RigidPickup = 0x0018D330;   // size 0x14
constexpr uint32_t setCherryBombCount__11RigidPickupi = 0x0018D348;   // size 0x8
constexpr uint32_t getCherryBombCount__11RigidPickup = 0x0018D350;   // size 0x8
constexpr uint32_t x_GLOBAL__I_init__11RigidPickupP9_hierheadR8_fvectorPA3_fb11ePickupType = 0x0018D358;   // size 0x20
constexpr uint32_t sanFranInitBefore__Fv = 0x0018D378;   // size 0x64
constexpr uint32_t sanFranInitAfter__Fv = 0x0018D3E0;   // size 0x90
constexpr uint32_t sanFranUpdate__Fv = 0x0018D470;   // size 0x510
constexpr uint32_t sanFranStartSteam__Fv = 0x0018D980;   // size 0x58
constexpr uint32_t sanFranQuake__Fi = 0x0018D9D8;   // size 0xA4
constexpr uint32_t sanFranSaveInteractive__Fii = 0x0018DA80;   // size 0x1C
constexpr uint32_t x__static_initialization_and_destruction_0_0018DAA0 = 0x0018DAA0;   // size 0x28
constexpr uint32_t x_GLOBAL__I_sanFranInitBefore__Fv = 0x0018DAC8;   // size 0x20
constexpr uint32_t monsterIdToMonsterEnum__Fi = 0x0018DAE8;   // size 0x18
constexpr uint32_t screenPrepare__Fv = 0x0018DB00;   // size 0x5C
constexpr uint32_t setReelSwitches__FiiP13_SaveGameData = 0x0018DB60;   // size 0x9C
constexpr uint32_t changeReel__Fiii = 0x0018DC00;   // size 0x84
constexpr uint32_t screenMain__Fv = 0x0018DC88;   // size 0x418
constexpr uint32_t updateSelectSwitches__Fiiii = 0x0018E0A0;   // size 0x240
constexpr uint32_t updateSelectSwitches3P__Fiiiiii = 0x0018E2E0;   // size 0x284
constexpr uint32_t updateSelectSwitches4P__Fiiiiiiii = 0x0018E568;   // size 0x3A4
constexpr uint32_t updateSelectSwitchesMG__Fiiii = 0x0018E910;   // size 0x240
constexpr uint32_t updateSelectSwitchesAI__Fii = 0x0018EB50;   // size 0x204
constexpr uint32_t initSelectSwitches__Fv = 0x0018ED58;   // size 0x188
constexpr uint32_t initSelectSwitchesMG__Fv = 0x0018EEE0;   // size 0x160
constexpr uint32_t screenGameModes1P__Fv = 0x0018F040;   // size 0x5E8
constexpr uint32_t screenCharSelect1P__Fv = 0x0018F628;   // size 0x1090
constexpr uint32_t screenSelect1AI__Fv = 0x001906B8;   // size 0x13A8
constexpr uint32_t screenCtlrCfg__Fv = 0x00191A60;   // size 0x4A4
constexpr uint32_t screenGameModes2P__Fv = 0x00191F08;   // size 0x4B4
constexpr uint32_t screenCharSelect3P__Fv = 0x001923C0;   // size 0x4110
constexpr uint32_t screenCharSelect2P__Fv = 0x001964D0;   // size 0x31F0
constexpr uint32_t screenMinigames2P__Fv = 0x001996C0;   // size 0x1D50
constexpr uint32_t screenLevelSelect__Fv = 0x0019B410;   // size 0x8EC
constexpr uint32_t screenLoadSave__Fv = 0x0019BD00;   // size 0xC8
constexpr uint32_t setSlider__Fif = 0x0019BDC8;   // size 0x10C
constexpr uint32_t screenOptions__Fv = 0x0019BED8;   // size 0xF94
constexpr uint32_t screenFreeForAllOptions1P__Fv = 0x0019CE70;   // size 0x41C
constexpr uint32_t screenFreeForAllOptions2P__Fv = 0x0019D290;   // size 0x2F4
constexpr uint32_t screenElimOptions2P__Fv = 0x0019D588;   // size 0x364
constexpr uint32_t screenMGSelect__Fv = 0x0019D8F0;   // size 0x3FC
constexpr uint32_t screenBestOf__Fv = 0x0019DCF0;   // size 0x898
constexpr uint32_t screenMemoryCardInit__Fv = 0x0019E588;   // size 0x10C
constexpr uint32_t screenTransition__Fb = 0x0019E698;   // size 0x9B8
constexpr uint32_t screenUnlocker__Fv = 0x0019F050;   // size 0xD8C
constexpr uint32_t screenCharSelect4P__Fv = 0x0019FDE0;   // size 0x5788
constexpr uint32_t screenWaitForStart__Fv = 0x001A5568;   // size 0x1AC
constexpr uint32_t changeScreen__Fiiiib = 0x001A5718;   // size 0x188
constexpr uint32_t screenGetInput__Fi = 0x001A58A0;   // size 0x230
constexpr uint32_t screenPrepareDefault__Fii = 0x001A5AD0;   // size 0x12C
constexpr uint32_t screenWriteInstructions__Fif = 0x001A5C00;   // size 0x29C
constexpr uint32_t screenGetFontAlpha__Fv = 0x001A5EA0;   // size 0x8
constexpr uint32_t setButtonAlpha__Ff = 0x001A5EA8;   // size 0x20
constexpr uint32_t screenSetButtonAlpha__Fv = 0x001A5EC8;   // size 0xBC
constexpr uint32_t showButtons__Fi = 0x001A5F88;   // size 0x1B0
constexpr uint32_t killButtons__Fv = 0x001A6138;   // size 0x70
constexpr uint32_t saveUnlocks__Fv = 0x001A61A8;   // size 0x60
constexpr uint32_t saveSettings__Fv = 0x001A6208;   // size 0x58
constexpr uint32_t loadSettings__Fv = 0x001A6260;   // size 0x60
constexpr uint32_t isSlotOpen__Fii = 0x001A62C0;   // size 0x4C
constexpr uint32_t resetSelectables__Fv = 0x001A6310;   // size 0x5C
constexpr uint32_t resetGameMode__Fv = 0x001A6370;   // size 0x1C
constexpr uint32_t modelsLeft__Fi = 0x001A6390;   // size 0x60
constexpr uint32_t levelUnlocked__Fi = 0x001A63F0;   // size 0xE0
constexpr uint32_t findAvailableMonster__Fiii = 0x001A64D0;   // size 0x160
constexpr uint32_t resetModels__Fv = 0x001A6630;   // size 0x34
constexpr uint32_t x__static_initialization_and_destruction_0_001A6668 = 0x001A6668;   // size 0x30
constexpr uint32_t x_GLOBAL__I_NamecardDistance = 0x001A6698;   // size 0x20
constexpr uint32_t main = 0x001A66B8;   // size 0x540
constexpr uint32_t SelectAI__5Shell = 0x001A6BF8;   // size 0x8
constexpr uint32_t RandomlySelectAI__5Shell = 0x001A6C00;   // size 0x15C
constexpr uint32_t InitPlayers__5Shell = 0x001A6D60;   // size 0x9A4
constexpr uint32_t InitBeforeUiDbLoad__5Shell = 0x001A7708;   // size 0x60
constexpr uint32_t InitBeforeUserintDbLoad__5Shell = 0x001A7768;   // size 0x74
constexpr uint32_t x__5Shell = 0x001A77E0;   // size 0x108
constexpr uint32_t x___5Shell = 0x001A78E8;   // size 0x24
constexpr uint32_t InitRTState__5Shell = 0x001A7910;   // size 0x4C
constexpr uint32_t EvaluateGameStatus__5Shelli = 0x001A7960;   // size 0x2A4
constexpr uint32_t EvaluateOnePlayerStoryStatus__5Shelli = 0x001A7C08;   // size 0x3CC
constexpr uint32_t EvaluateOnePlayerChallengeStatus__5Shelli = 0x001A7FD8;   // size 0x258
constexpr uint32_t EvaluateTwoPlayerCoopStatus__5Shelli = 0x001A8230;   // size 0x8
constexpr uint32_t EvaluateMultiPlayerBattleStatusNoAI__5Shelli = 0x001A8238;   // size 0x54C
constexpr uint32_t EvaluateMultiPlayerBattleStatusAI__5Shelli = 0x001A8788;   // size 0x3E0
constexpr uint32_t EvaluateOnePlayerEnduranceStatus__5Shelli = 0x001A8B68;   // size 0x78
constexpr uint32_t EvaluateBigShotStatus__5Shelli = 0x001A8BE0;   // size 0x48
constexpr uint32_t EvaluateCrushStatus__5Shelli = 0x001A8C28;   // size 0x48
constexpr uint32_t EvaluateDodgeBallStatus__5Shelli = 0x001A8C70;   // size 0x48
constexpr uint32_t EvaluateOnlineBattleStatus__5Shelli = 0x001A8CB8;   // size 0x8
constexpr uint32_t InitPlayerLives__5Shell = 0x001A8CC0;   // size 0x38
constexpr uint32_t BootInitUi__5Shell = 0x001A8CF8;   // size 0x44
constexpr uint32_t BootInitUserint__5Shell = 0x001A8D40;   // size 0x7C
constexpr uint32_t BootInitUserint1__5Shell = 0x001A8DC0;   // size 0x6C
constexpr uint32_t BootInitGame__5Shell = 0x001A8E30;   // size 0x50
constexpr uint32_t ResolveCommandLineArguments__FiPPc = 0x001A8E80;   // size 0x174
constexpr uint32_t InitGS__5Shells = 0x001A8FF8;   // size 0x34
constexpr uint32_t LoadUserintTexture__5Shell = 0x001A9030;   // size 0xC0
constexpr uint32_t LoadUserintTexture1__5Shell = 0x001A90F0;   // size 0xC0
constexpr uint32_t LoadUserintDB__5Shell = 0x001A91B0;   // size 0x3C
constexpr uint32_t LoadUserintDB1__5Shell = 0x001A91F0;   // size 0x3C
constexpr uint32_t LoadResTexture__5Shell = 0x001A9230;   // size 0x45C
constexpr uint32_t LoadTexture__5Shell = 0x001A9690;   // size 0x41C
constexpr uint32_t LoadLevelDB__5Shell = 0x001A9AB0;   // size 0x140
constexpr uint32_t LoadMonstersDB__5Shell = 0x001A9BF0;   // size 0x49C
constexpr uint32_t AddEpNode__5ShelliP9_hierhead = 0x001AA090;   // size 0x8BC
constexpr uint32_t formatFilename__5ShellPcPCcT2Q25Shell11_shFileType = 0x001AA950;   // size 0x110
constexpr uint32_t formatFilename1__5ShellPcPCciT2Q25Shell11_shFileType = 0x001AAA60;   // size 0x98
constexpr uint32_t formatFilename__5ShellPcPCcN22 = 0x001AAAF8;   // size 0x8C
constexpr uint32_t getVramAddr__5Shell = 0x001AAB88;   // size 0x5C
constexpr uint32_t Use30HzMode__5Shell = 0x001AABE8;   // size 0x10
constexpr uint32_t GetLevelName__5Shell = 0x001AABF8;   // size 0xC
constexpr uint32_t SetMenuItemFlag__5Shelliii = 0x001AAC08;   // size 0x144
constexpr uint32_t EnableMonsterSelection__5Shelli = 0x001AAD50;   // size 0x50
constexpr uint32_t MonsterIsChosen__5Shelli = 0x001AADA0;   // size 0x28
constexpr uint32_t MonsterExists__5Shelli = 0x001AADC8;   // size 0x8
constexpr uint32_t DisplayLoadBackground__5Shellb = 0x001AADD0;   // size 0x31C
constexpr uint32_t MonsterIsLocked__5Shelli = 0x001AB0F0;   // size 0x28
constexpr uint32_t init__7TagList = 0x001AB118;   // size 0x8C
constexpr uint32_t LoadLevelFiles__5Shell = 0x001AB1A8;   // size 0x248
constexpr uint32_t FinishLoadBar__5Shell = 0x001AB3F0;   // size 0x6C
constexpr uint32_t ResetLevel__5Shell = 0x001AB460;   // size 0x3C
constexpr uint32_t GenesisMovie__5Shell = 0x001AB4A0;   // size 0xD8
constexpr uint32_t FadeScreen__5ShellibUcUcUcUcUcUc = 0x001AB578;   // size 0xE4
constexpr uint32_t FadeScreen__5ShellibRUiT3UcUcUcUcUcUc = 0x001AB660;   // size 0x464
constexpr uint32_t InitialMemCardScreen__5Shell = 0x001ABAC8;   // size 0x180
constexpr uint32_t x__3Hud = 0x001ABC48;   // size 0x80
constexpr uint32_t x__7TheGame = 0x001ABCC8;   // size 0x574
constexpr uint32_t x__static_initialization_and_destruction_0_001AC240 = 0x001AC240;   // size 0x88
constexpr uint32_t x__7TagList = 0x001AC2C8;   // size 0xD4
constexpr uint32_t x_GLOBAL__I_g_noScripts = 0x001AC3A0;   // size 0x20
constexpr uint32_t x_GLOBAL__D_g_noScripts = 0x001AC3C0;   // size 0x20
constexpr uint32_t displayDialog__Fi = 0x001AC3E0;   // size 0x848
constexpr uint32_t finishedMultPlayBattleLevel__FRi = 0x001ACC28;   // size 0x598
constexpr uint32_t finishedMultPlayElim__FRi = 0x001AD1C0;   // size 0x2CC
constexpr uint32_t allDeadMultPlayBattleLevel__FRi = 0x001AD490;   // size 0x2BC
constexpr uint32_t selectNextMultPlayer__FRi = 0x001AD750;   // size 0x710
constexpr uint32_t selectNextMultPlayBattleLevel__Fv = 0x001ADE60;   // size 0x5C
constexpr uint32_t wonStoryOverlay__Fv = 0x001ADEC0;   // size 0x7C
constexpr uint32_t lostLevelOverlay__FRi = 0x001ADF40;   // size 0x3CC
constexpr uint32_t pauseLevelOverlay__FRi = 0x001AE310;   // size 0x770
constexpr uint32_t areYouSure__FRi = 0x001AEA80;   // size 0x340
constexpr uint32_t wonLevelOverlay__Fv = 0x001AEDC0;   // size 0x94
constexpr uint32_t wonChallengeLevelOverlay__Fv = 0x001AEE58;   // size 0x22C
constexpr uint32_t lostEnduranceOverlay__FRi = 0x001AF088;   // size 0x370
constexpr uint32_t finishedOnlineBattle__Fv = 0x001AF3F8;   // size 0x36C
constexpr uint32_t finishedBigShot__FRi = 0x001AF768;   // size 0x264
constexpr uint32_t finishedCrush__FRi = 0x001AF9D0;   // size 0x234
constexpr uint32_t finishedDodgeBall__FRi = 0x001AFC08;   // size 0x234
constexpr uint32_t checkInputForExit__FiPb = 0x001AFE40;   // size 0x194
constexpr uint32_t setMenuFont__Fi = 0x001AFFD8;   // size 0x6C
constexpr uint32_t setTitleFont__Fv = 0x001B0048;   // size 0x28
constexpr uint32_t getCurrentMenuSelection__Fii = 0x001B0070;   // size 0x184
constexpr uint32_t monsterIdtomonsterSlot__Fi = 0x001B01F8;   // size 0xA4
constexpr uint32_t isSlotAvail__Fii = 0x001B02A0;   // size 0x4C
constexpr uint32_t resetSelecteds__Fv = 0x001B02F0;   // size 0x124
constexpr uint32_t startField__11ShellRtLoopb = 0x001B0418;   // size 0x40
constexpr uint32_t endField__11ShellRtLoopb = 0x001B0458;   // size 0x268
constexpr uint32_t onStart__11ShellRtLoop = 0x001B06C0;   // size 0xC
constexpr uint32_t onEnd__11ShellRtLoop = 0x001B06D0;   // size 0x8
constexpr uint32_t Init__9ShockWave = 0x001B06D8;   // size 0x34
constexpr uint32_t Activate__9ShockWaveP8_fvectorGt13ExpandingRing1Z15ShockWaveConfigQ28HitEvent11DamageCauseiP10SpecFxAnim = 0x001B0710;   // size 0x250
constexpr uint32_t Activate__9ShockWaveP8_fvectorGt13ExpandingRing1Z15ShockWaveConfigQ28HitEvent11DamageCauseiP11_animHandleP3_cs = 0x001B0960;   // size 0x278
constexpr uint32_t update__9ShockWave = 0x001B0BD8;   // size 0xBB0
constexpr uint32_t x__static_initialization_and_destruction_0_001B1788 = 0x001B1788;   // size 0x74
constexpr uint32_t update__9ShockWavePv = 0x001B1800;   // size 0x1C
constexpr uint32_t x_GLOBAL__I_Init__9ShockWave = 0x001B1820;   // size 0x20
constexpr uint32_t x__12SoundManager = 0x001B1840;   // size 0x68
constexpr uint32_t initPause__12SoundManager = 0x001B18A8;   // size 0x1C
constexpr uint32_t updatePauseAllSounds__12SoundManager = 0x001B18C8;   // size 0x100
constexpr uint32_t setSoundVolumeInfo__12SoundManagerP7MonsteriP8_fvector = 0x001B19C8;   // size 0x84
constexpr uint32_t unpauseAllSounds__12SoundManager = 0x001B1A50;   // size 0x54
constexpr uint32_t doOneTimeInit__12SoundManager = 0x001B1AA8;   // size 0x80
constexpr uint32_t manageReverb__12SoundManager = 0x001B1B28;   // size 0x80
constexpr uint32_t initSoundManager__12SoundManager = 0x001B1BA8;   // size 0x140
constexpr uint32_t disableSoundForCinema__12SoundManager = 0x001B1CE8;   // size 0x58
constexpr uint32_t updateSoundManager__12SoundManager = 0x001B1D40;   // size 0x240
constexpr uint32_t initVagStreaming__12SoundManager = 0x001B1F80;   // size 0x6C
constexpr uint32_t loadRTSoundBanks__12SoundManager = 0x001B1FF0;   // size 0x5B4
constexpr uint32_t swapMonsterSoundBankBlocking__12SoundManagerii = 0x001B25A8;   // size 0x1BC
constexpr uint32_t loadMechBossSoundBank__12SoundManager = 0x001B2768;   // size 0x30
constexpr uint32_t loadMonsterBank__12SoundManagerii = 0x001B2798;   // size 0x1E8
constexpr uint32_t loadLoResMonsterBank__12SoundManagerii = 0x001B2980;   // size 0x1E8
constexpr uint32_t loadShellSoundBanks__12SoundManager = 0x001B2B68;   // size 0xB8
constexpr uint32_t initLevelObjectSoundManager__23LevelObjectSoundManager = 0x001B2C20;   // size 0x1C4
constexpr uint32_t updateLevelObjectSoundManager__23LevelObjectSoundManager = 0x001B2DE8;   // size 0x384
constexpr uint32_t updateDestructLoops__23LevelObjectSoundManager = 0x001B3170;   // size 0x234
constexpr uint32_t triggerLevelDestructSound__23LevelObjectSoundManagerP8_fvectoriiffiP12ActPlaySound = 0x001B33A8;   // size 0x18C
constexpr uint32_t playDebrisImpactSound__23LevelObjectSoundManagerP8_fvectori = 0x001B3538;   // size 0xDC
constexpr uint32_t playTimedBuildingDestructibleSound__23LevelObjectSoundManagerP12ActPlaySoundP8_fvector = 0x001B3618;   // size 0xD4
constexpr uint32_t playDestructibleSound__23LevelObjectSoundManagerUiP8_fvector = 0x001B36F0;   // size 0x5D4
constexpr uint32_t findAvailableDestructHandle__23LevelObjectSoundManager = 0x001B3CC8;   // size 0x30
constexpr uint32_t initCanyonLevelSound__16CanyonLevelSound = 0x001B3CF8;   // size 0x34
constexpr uint32_t updateCanyonLevelSound__16CanyonLevelSound = 0x001B3D30;   // size 0x1C
constexpr uint32_t initVegasLevelSound__15VegasLevelSound = 0x001B3D50;   // size 0x50
constexpr uint32_t updateVegasLevelSound__15VegasLevelSound = 0x001B3DA0;   // size 0x88
constexpr uint32_t initFinalLevelSound__15FinalLevelSound = 0x001B3E28;   // size 0x34
constexpr uint32_t updateFinalLevelSound__15FinalLevelSound = 0x001B3E60;   // size 0x1C
constexpr uint32_t initAirportLevelSound__17AirportLevelSound = 0x001B3E80;   // size 0x40
constexpr uint32_t updateAirportLevelSound__17AirportLevelSound = 0x001B3EC0;   // size 0x2C
constexpr uint32_t initSanFranLevelSound__17SanFranLevelSound = 0x001B3EF0;   // size 0x21C
constexpr uint32_t updateSanFranLevelSound__17SanFranLevelSound = 0x001B4110;   // size 0xE8
constexpr uint32_t playPlaneLandingSound__17AirportLevelSoundP8_fvector = 0x001B41F8;   // size 0x38
constexpr uint32_t startQuakeSound__17SanFranLevelSound = 0x001B4230;   // size 0x30
constexpr uint32_t initTokyoLevelSound__15TokyoLevelSound = 0x001B4260;   // size 0xB4
constexpr uint32_t updateTokyoLevelSound__15TokyoLevelSound = 0x001B4318;   // size 0x144
constexpr uint32_t initBigTossLevelSound__17BigTossLevelSound = 0x001B4460;   // size 0x30
constexpr uint32_t updateBigTossLevelSound__17BigTossLevelSound = 0x001B4490;   // size 0x15C
constexpr uint32_t initThreeMileLevelSound__19ThreeMileLevelSound = 0x001B45F0;   // size 0x214
constexpr uint32_t setAllStacksDestroyed__19ThreeMileLevelSound = 0x001B4808;   // size 0x3C
constexpr uint32_t resetReactorSoundInfo__19ThreeMileLevelSoundi = 0x001B4848;   // size 0x6C
constexpr uint32_t updateThreeMileLevelSound__19ThreeMileLevelSound = 0x001B48B8;   // size 0x1FC
constexpr uint32_t playWickedBigExplosion__19ThreeMileLevelSound = 0x001B4AB8;   // size 0x4C
constexpr uint32_t playShockSound__19ThreeMileLevelSoundP8_fvector = 0x001B4B08;   // size 0x5C
constexpr uint32_t playThreeMileCountdown__19ThreeMileLevelSoundi = 0x001B4B68;   // size 0xF4
constexpr uint32_t initVolcanoLevelSound__17VolcanoLevelSound = 0x001B4C60;   // size 0x64
constexpr uint32_t updateVolcanoLevelSound__17VolcanoLevelSound = 0x001B4CC8;   // size 0x34
constexpr uint32_t initUFOLevelSound__13UFOLevelSound = 0x001B4D00;   // size 0x80
constexpr uint32_t updateUFOLevelSound__13UFOLevelSound = 0x001B4D80;   // size 0x168
constexpr uint32_t initMetroLevelSound__15MetroLevelSound = 0x001B4EE8;   // size 0x8
constexpr uint32_t updateMetroLevelSound__15MetroLevelSound = 0x001B4EF0;   // size 0x58
constexpr uint32_t initMilitaryLevelSound__18MilitaryLevelSound = 0x001B4F48;   // size 0x8
constexpr uint32_t updateMilitaryLevelSound__18MilitaryLevelSound = 0x001B4F50;   // size 0x8
constexpr uint32_t initIslandLevelSound__16IslandLevelSound = 0x001B4F58;   // size 0x8C
constexpr uint32_t updateIslandLevelSound__16IslandLevelSound = 0x001B4FE8;   // size 0x50
constexpr uint32_t initCanyon2LevelSound__17Canyon2LevelSound = 0x001B5038;   // size 0x10
constexpr uint32_t updateCanyon2LevelSound__17Canyon2LevelSound = 0x001B5048;   // size 0x8
constexpr uint32_t playDamDestroyedSound__17Canyon2LevelSound = 0x001B5050;   // size 0x84
constexpr uint32_t initHeightBasedSound__16HeightBasedSoundiii = 0x001B50D8;   // size 0x14
constexpr uint32_t updateHeightBasedSound__16HeightBasedSound = 0x001B50F0;   // size 0x144
constexpr uint32_t initLevelObjectSound__16LevelObjectSoundiiiiiii = 0x001B5238;   // size 0x48
constexpr uint32_t stopLevelObjectSound__16LevelObjectSound = 0x001B5280;   // size 0x24
constexpr uint32_t updateLevelObjectSound__16LevelObjectSound = 0x001B52A8;   // size 0xF4
constexpr uint32_t updateLevelObjectSound__21NoPanLevelObjectSound = 0x001B53A0;   // size 0xD0
constexpr uint32_t updateLevelObjectSound__24InvertedLevelObjectSound = 0x001B5470;   // size 0xE0
constexpr uint32_t initTankSound__9TankSound = 0x001B5550;   // size 0xC
constexpr uint32_t updateTankSound__9TankSoundP10TankPickupi = 0x001B5560;   // size 0x134
constexpr uint32_t terminateTankSound__9TankSound = 0x001B5698;   // size 0x24
constexpr uint32_t playTankFireSound__9TankSoundP8_fvector = 0x001B56C0;   // size 0x5C
constexpr uint32_t initHeliSound__9HeliSound = 0x001B5720;   // size 0x10
constexpr uint32_t updateHeliSound__9HeliSoundP10HeliPickup = 0x001B5730;   // size 0x320
constexpr uint32_t terminateHeliSound__9HeliSound = 0x001B5A50;   // size 0x54
constexpr uint32_t initCarSound__8CarSoundP9CarPickup = 0x001B5AA8;   // size 0xC
constexpr uint32_t updateCarSound__8CarSound = 0x001B5AB8;   // size 0x2BC
constexpr uint32_t playHornSound__8CarSound = 0x001B5D78;   // size 0x184
constexpr uint32_t terminateCarSound__8CarSound = 0x001B5F00;   // size 0x24
constexpr uint32_t initPickupSound__11PickupSoundP6Pickup = 0x001B5F28;   // size 0x10
constexpr uint32_t playBatImpactSound__11PickupSound = 0x001B5F38;   // size 0x64
constexpr uint32_t updatePickupSound__11PickupSound = 0x001B5FA0;   // size 0x21C
constexpr uint32_t updateTurretSound__11PickupSoundiff = 0x001B61C0;   // size 0x218
constexpr uint32_t terminateTurretSound__11PickupSound = 0x001B63D8;   // size 0x24
constexpr uint32_t terminatePickupSound__11PickupSound = 0x001B6400;   // size 0x24
constexpr uint32_t playTurretPickupFireSound__11PickupSoundi = 0x001B6428;   // size 0xBC
constexpr uint32_t x__10ShellSound = 0x001B64E8;   // size 0x18
constexpr uint32_t resetShellSoundFlags__10ShellSound = 0x001B6500;   // size 0x1C
constexpr uint32_t updateShellSound__10ShellSound = 0x001B6520;   // size 0xA4
constexpr uint32_t updateTokenSound__10ShellSound = 0x001B65C8;   // size 0x70
constexpr uint32_t playTokenCountFinishedSound__10ShellSound = 0x001B6638;   // size 0x34
constexpr uint32_t stopTokenSound__10ShellSound = 0x001B6670;   // size 0x40
constexpr uint32_t updateSFXSliderSound__10ShellSound = 0x001B66B0;   // size 0x58
constexpr uint32_t stopSFXSliderSound__10ShellSound = 0x001B6708;   // size 0x24
constexpr uint32_t terminateShellSound__10ShellSound = 0x001B6730;   // size 0x38
constexpr uint32_t playShellSFXSound__10ShellSoundi = 0x001B6768;   // size 0x60
constexpr uint32_t playSweetUnlockedSound__10ShellSound = 0x001B67C8;   // size 0x34
constexpr uint32_t playPauseMenuSound__10ShellSoundi = 0x001B6800;   // size 0x30
constexpr uint32_t initHudSound__8HudSound = 0x001B6830;   // size 0x8
constexpr uint32_t updateHudSound__8HudSound = 0x001B6838;   // size 0x8
constexpr uint32_t initGenericSound__12GenericSoundiiii = 0x001B6840;   // size 0x18
constexpr uint32_t terminateGenericSound__12GenericSound = 0x001B6858;   // size 0x24
constexpr uint32_t updateGenericSound__12GenericSoundP8_fvector = 0x001B6880;   // size 0xF0
constexpr uint32_t initPedSound__8PedSound = 0x001B6970;   // size 0x8
constexpr uint32_t terminatePedSound__8PedSound = 0x001B6978;   // size 0x24
constexpr uint32_t updatePedSound__8PedSoundP8_fvectori = 0x001B69A0;   // size 0x104
constexpr uint32_t initFireSound__9FireSound = 0x001B6AA8;   // size 0x8
constexpr uint32_t terminateFireSound__9FireSound = 0x001B6AB0;   // size 0x24
constexpr uint32_t updateFireSound__9FireSoundP8_fvector = 0x001B6AD8;   // size 0x108
constexpr uint32_t initPlaneSound__10PlaneSound = 0x001B6BE0;   // size 0xC
constexpr uint32_t terminatePlaneSound__10PlaneSound = 0x001B6BF0;   // size 0x24
constexpr uint32_t updatePlaneSound__10PlaneSoundP8_fvectorif = 0x001B6C18;   // size 0x270
constexpr uint32_t initWeaponSound__11WeaponSound = 0x001B6E88;   // size 0x8
constexpr uint32_t terminateWeaponSound__11WeaponSound = 0x001B6E90;   // size 0x24
constexpr uint32_t updateWeaponSound__11WeaponSoundP6Weapon = 0x001B6EB8;   // size 0x300
constexpr uint32_t playWeaponLaunchSound__11WeaponSoundP8_fvectori = 0x001B71B8;   // size 0x2DC
constexpr uint32_t playWeaponExplosion__11WeaponSoundP8_fvectorii = 0x001B7498;   // size 0x314
constexpr uint32_t playRockSpecialImpact__11WeaponSoundP8_fvector = 0x001B77B0;   // size 0x78
constexpr uint32_t x__12MonsterSound = 0x001B7828;   // size 0x1C
constexpr uint32_t initMonsterSound__12MonsterSoundP7Monster = 0x001B7848;   // size 0x1A0
constexpr uint32_t updateMonsterSound__12MonsterSound = 0x001B79E8;   // size 0xC8
constexpr uint32_t updateMonsterCinemaSound__12MonsterSound = 0x001B7AB0;   // size 0x1FC
constexpr uint32_t updateGeneralMonsterSounds__12MonsterSound = 0x001B7CB0;   // size 0xDEC
constexpr uint32_t playSoundFromAnimation__12MonsterSoundf = 0x001B8AA0;   // size 0x40
constexpr uint32_t playSignatureSound__12MonsterSound = 0x001B8AE0;   // size 0x2F0
constexpr uint32_t playTakeHitSound__12MonsterSound = 0x001B8DD0;   // size 0x460
constexpr uint32_t playCloakingSound__12MonsterSound = 0x001B9230;   // size 0x64
constexpr uint32_t playEnterCounterBlockSound__12MonsterSound = 0x001B9298;   // size 0x70
constexpr uint32_t playGeneratorTazerSound__12MonsterSound = 0x001B9308;   // size 0x64
constexpr uint32_t playJellyTazerOneShot__12MonsterSound = 0x001B9370;   // size 0x64
constexpr uint32_t playShockedSound__12MonsterSound = 0x001B93D8;   // size 0x64
constexpr uint32_t playPedSquishSound__12MonsterSound = 0x001B9440;   // size 0xA8
constexpr uint32_t playRockmanSpecialLaunch__12MonsterSound = 0x001B94E8;   // size 0x64
constexpr uint32_t playRockmanSpecialBounce__12MonsterSoundP8_fvector = 0x001B9550;   // size 0x8C
constexpr uint32_t playCounterAttackSound__12MonsterSound = 0x001B95E0;   // size 0x64
constexpr uint32_t playRockmanSpecialExplode__12MonsterSoundP8_fvector = 0x001B9648;   // size 0x5C
constexpr uint32_t updateClimbingSound__12MonsterSound11MonsterAnim = 0x001B96A8;   // size 0x8
constexpr uint32_t updateJellyTazer__12MonsterSound = 0x001B96B0;   // size 0xC0
constexpr uint32_t terminateJellyTazer__12MonsterSound = 0x001B9770;   // size 0x24
constexpr uint32_t terminateDashSound__12MonsterSound = 0x001B9798;   // size 0x48
constexpr uint32_t playRamSound__12MonsterSound = 0x001B97E0;   // size 0x64
constexpr uint32_t updateDashSound__12MonsterSound = 0x001B9848;   // size 0x19C
constexpr uint32_t updateOgreSound__12MonsterSound = 0x001B99E8;   // size 0x8
constexpr uint32_t updateRobotSound__12MonsterSound = 0x001B99F0;   // size 0x1BC
constexpr uint32_t updateMantisSound__12MonsterSound = 0x001B9BB0;   // size 0x10C
constexpr uint32_t updateShogunSound__12MonsterSound = 0x001B9CC0;   // size 0x124
constexpr uint32_t updateLavaSpecialInhaleChargeup__12MonsterSound = 0x001B9DE8;   // size 0x184
constexpr uint32_t terminateLavaSpecialInhaleChargeup__12MonsterSound = 0x001B9F70;   // size 0x24
constexpr uint32_t updateLavaSound__12MonsterSound = 0x001B9F98;   // size 0x8
constexpr uint32_t updateAlienSound__12MonsterSound = 0x001B9FA0;   // size 0x160
constexpr uint32_t updateFireBreath__12MonsterSound = 0x001BA100;   // size 0x144
constexpr uint32_t playPunchSound__12MonsterSound = 0x001BA248;   // size 0x68
constexpr uint32_t playVictorySound__12MonsterSoundi = 0x001BA2B0;   // size 0x4A4
constexpr uint32_t playTakeBatHitSound__12MonsterSound = 0x001BA758;   // size 0xE4
constexpr uint32_t playTakeHit__12MonsterSoundP7Monster = 0x001BA840;   // size 0x274
constexpr uint32_t playKnockBackSound__12MonsterSound = 0x001BAAB8;   // size 0x30
constexpr uint32_t terminateFlightGlideSound__12MonsterSound = 0x001BAAE8;   // size 0x24
constexpr uint32_t updateFlightGlideSound__12MonsterSoundi = 0x001BAB10;   // size 0x12C
constexpr uint32_t playButtSlamSound__12MonsterSound = 0x001BAC40;   // size 0x298
constexpr uint32_t playLandingSound__12MonsterSound = 0x001BAED8;   // size 0x260
constexpr uint32_t playFootstepSound__12MonsterSound = 0x001BB138;   // size 0x288
constexpr uint32_t playScrape__12MonsterSound = 0x001BB3C0;   // size 0xE4
constexpr uint32_t playRobotHeadSound__12MonsterSound = 0x001BB4A8;   // size 0x64
constexpr uint32_t playRoboJetOneShot__12MonsterSound = 0x001BB510;   // size 0x8
constexpr uint32_t playBodyImpactSound__12MonsterSound = 0x001BB518;   // size 0x118
constexpr uint32_t playGrappleAttackSound__12MonsterSound = 0x001BB630;   // size 0x220
constexpr uint32_t playStompAttackSound__12MonsterSound = 0x001BB850;   // size 0x240
constexpr uint32_t terminateGrappleSound__12MonsterSound = 0x001BBA90;   // size 0x24
constexpr uint32_t playGrappleThrowSound__12MonsterSound = 0x001BBAB8;   // size 0xB0
constexpr uint32_t playBatWhooshSound__12MonsterSound = 0x001BBB68;   // size 0x64
constexpr uint32_t playObjectThrowSound__12MonsterSound = 0x001BBBD0;   // size 0x240
constexpr uint32_t playCrowdControlSound__12MonsterSound = 0x001BBE10;   // size 0x2E4
constexpr uint32_t play206Sound__12MonsterSound = 0x001BC0F8;   // size 0x2E4
constexpr uint32_t playTauntSound__12MonsterSound = 0x001BC3E0;   // size 0x2F8
constexpr uint32_t playPickupSound__12MonsterSound = 0x001BC6D8;   // size 0xFC
constexpr uint32_t playGrapplePickupSound__12MonsterSound = 0x001BC7D8;   // size 0x138
constexpr uint32_t stopPickupSound__12MonsterSound = 0x001BC910;   // size 0x24
constexpr uint32_t playWingFlap__12MonsterSound = 0x001BC938;   // size 0xB8
constexpr uint32_t playImpalerRemoveSound__12MonsterSound = 0x001BC9F0;   // size 0x60
constexpr uint32_t playGrappleSwing__12MonsterSoundi = 0x001BCA50;   // size 0x60
constexpr uint32_t playSwing__12MonsterSoundii = 0x001BCAB0;   // size 0xEDC
constexpr uint32_t playJumpSound__12MonsterSound = 0x001BD990;   // size 0x204
constexpr uint32_t playDeathSound__12MonsterSoundi = 0x001BDB98;   // size 0x560
constexpr uint32_t playSonicRoarSound__12MonsterSound = 0x001BE0F8;   // size 0x64
constexpr uint32_t playWaterSplashSound__12MonsterSound = 0x001BE160;   // size 0x64
constexpr uint32_t playWaterFootstepSound__12MonsterSound = 0x001BE1C8;   // size 0x64
constexpr uint32_t updateAssBossRotorSound__12MonsterSound = 0x001BE230;   // size 0x178
constexpr uint32_t terminateAssBossRotorSound__12MonsterSound = 0x001BE3A8;   // size 0xB4
constexpr uint32_t updateAssBossSpinSound__12MonsterSound = 0x001BE460;   // size 0xC0
constexpr uint32_t terminateAssBossSpinSound__12MonsterSound = 0x001BE520;   // size 0x24
constexpr uint32_t playAssBossWeaponDrawSound__12MonsterSound = 0x001BE548;   // size 0x64
constexpr uint32_t updateRobotReticleSound__12MonsterSound = 0x001BE5B0;   // size 0xC0
constexpr uint32_t terminateRobotReticleSound__12MonsterSound = 0x001BE670;   // size 0x24
constexpr uint32_t updateElectricShockSound__12MonsterSound = 0x001BE698;   // size 0xC0
constexpr uint32_t terminateElectricShockSound__12MonsterSound = 0x001BE758;   // size 0x24
constexpr uint32_t updateAlienSpecial__12MonsterSoundi = 0x001BE780;   // size 0xFC
constexpr uint32_t terminateAlienSpecial__12MonsterSound = 0x001BE880;   // size 0x24
constexpr uint32_t updateZapAttackSound__12MonsterSoundiP7Monster = 0x001BE8A8;   // size 0x214
constexpr uint32_t terminateZapAttackSound__12MonsterSound = 0x001BEAC0;   // size 0x48
constexpr uint32_t playShogunSpecialImpactSound__12MonsterSound = 0x001BEB08;   // size 0x64
constexpr uint32_t updateShogunSpecialSound__12MonsterSound = 0x001BEB70;   // size 0xC0
constexpr uint32_t terminateShogunSpecialSound__12MonsterSound = 0x001BEC30;   // size 0x24
constexpr uint32_t terminateEvilEye__12MonsterSound = 0x001BEC58;   // size 0x24
constexpr uint32_t updateEvilEye__12MonsterSound = 0x001BEC80;   // size 0x84
constexpr uint32_t updateSpiderMagnoSpecial__12MonsterSound = 0x001BED08;   // size 0x84
constexpr uint32_t terminateSpiderMagnoSpecial__12MonsterSound = 0x001BED90;   // size 0x24
constexpr uint32_t playLavaSpecialInhale__12MonsterSound = 0x001BEDB8;   // size 0x94
constexpr uint32_t playSuckerBugLaunch__12MonsterSound = 0x001BEE50;   // size 0x64
constexpr uint32_t playShogunSpecialOneShot__12MonsterSound = 0x001BEEB8;   // size 0x64
constexpr uint32_t playAlienRaygunDrawSound__12MonsterSound = 0x001BEF20;   // size 0x40
constexpr uint32_t initSuckerBugSound__14SuckerBugSound = 0x001BEF60;   // size 0xC
constexpr uint32_t terminateSuckerBugSound__14SuckerBugSound = 0x001BEF70;   // size 0x48
constexpr uint32_t updateSuckerBugSound__14SuckerBugSoundP8_fvectori = 0x001BEFB8;   // size 0x198
constexpr uint32_t stopAllMonsterSounds__12MonsterSound = 0x001BF150;   // size 0x1BC
constexpr uint32_t initTriggerVolumeSound__18TriggerVolumeSoundiUi = 0x001BF310;   // size 0x10
constexpr uint32_t terminateTriggerVolumeSound__18TriggerVolumeSound = 0x001BF320;   // size 0x24
constexpr uint32_t updateTriggerVolumeSound__18TriggerVolumeSound = 0x001BF348;   // size 0xF4
constexpr uint32_t initTriggerVolumeSound__24RangedTriggerVolumeSoundiUiii = 0x001BF440;   // size 0x38
constexpr uint32_t terminateTriggerVolumeSound__24RangedTriggerVolumeSound = 0x001BF478;   // size 0x54
constexpr uint32_t updateTriggerVolumeSound__24RangedTriggerVolumeSound = 0x001BF4D0;   // size 0x340
constexpr uint32_t soundPlayPowerUpSound__FiiiP8_fvector = 0x001BF810;   // size 0x180
constexpr uint32_t playLowStaminaSound__Fi = 0x001BF990;   // size 0x3C
constexpr uint32_t initPlantBossRootSound__18PlantBossRootSound = 0x001BF9D0;   // size 0x8
constexpr uint32_t updatePlantBossRootSound__18PlantBossRootSoundP8_fvector = 0x001BF9D8;   // size 0xD0
constexpr uint32_t terminatePlantBossRootSound__18PlantBossRootSound = 0x001BFAA8;   // size 0x24
constexpr uint32_t initFinalBossSound__14FinalBossSoundP9FinalBoss = 0x001BFAD0;   // size 0x48
constexpr uint32_t updateFinalBossSound__14FinalBossSoundP8_fvectoriP11_animHandle = 0x001BFB18;   // size 0x144
constexpr uint32_t playGetReadyToFireSound__14FinalBossSoundP8_fvector = 0x001BFC60;   // size 0x5C
constexpr uint32_t playShieldPowerUpSound__14FinalBossSoundP8_fvector = 0x001BFCC0;   // size 0x84
constexpr uint32_t playShieldPowerDownSound__14FinalBossSoundP8_fvector = 0x001BFD48;   // size 0x5C
constexpr uint32_t playMechaPowerDownSound__14FinalBossSoundP8_fvector = 0x001BFDA8;   // size 0x5C
constexpr uint32_t playTakeHitSound__14FinalBossSoundP8_fvector = 0x001BFE08;   // size 0x88
constexpr uint32_t updateMechLaser__14FinalBossSoundP8_fvector = 0x001BFE90;   // size 0xB8
constexpr uint32_t terminateMechLaser__14FinalBossSound = 0x001BFF48;   // size 0x40
constexpr uint32_t stopMechaLaserImpactSound__12MonsterSound = 0x001BFF88;   // size 0x24
constexpr uint32_t initPlantBossSound__14PlantBossSound = 0x001BFFB0;   // size 0x10
constexpr uint32_t updatePlantBossSound__14PlantBossSound = 0x001BFFC0;   // size 0x8
constexpr uint32_t playPlantBossHideSound__14PlantBossSound = 0x001BFFC8;   // size 0x58
constexpr uint32_t playPlantBossEmergeSound__14PlantBossSound = 0x001C0020;   // size 0x58
constexpr uint32_t playPlantBossCabbageSmack__14PlantBossSound = 0x001C0078;   // size 0x9C
constexpr uint32_t playPlantBossDeathSound__18PlantBossHeadSoundP8_fvector = 0x001C0118;   // size 0x5C
constexpr uint32_t playPlantBossChompSound__18PlantBossHeadSoundP8_fvector = 0x001C0178;   // size 0x5C
constexpr uint32_t playPlantBossHeadDeadSound__18PlantBossHeadSoundP8_fvector = 0x001C01D8;   // size 0x84
constexpr uint32_t playPlantBossMissChompSound__18PlantBossHeadSoundP8_fvector = 0x001C0260;   // size 0x5C
constexpr uint32_t playPlantBossGrowlSound__18PlantBossHeadSoundP8_fvector = 0x001C02C0;   // size 0x5C
constexpr uint32_t playPlantBossHissSound__18PlantBossHeadSoundP8_fvector = 0x001C0320;   // size 0x5C
constexpr uint32_t playPlantBossSpitSound__18PlantBossHeadSoundP8_fvector = 0x001C0380;   // size 0x5C
constexpr uint32_t playPlantBossSporeLaunchSound__18PlantBossHeadSoundP8_fvector = 0x001C03E0;   // size 0x5C
constexpr uint32_t updateChompSound__18PlantBossHeadSoundP8_fvector = 0x001C0440;   // size 0xD0
constexpr uint32_t terminateChompSound__18PlantBossHeadSound = 0x001C0510;   // size 0x24
constexpr uint32_t initRigidPickupSound__16RigidPickupSound = 0x001C0538;   // size 0x38
constexpr uint32_t updateRigidPickupSound__16RigidPickupSoundP8_fvectorfi = 0x001C0570;   // size 0x1F4
constexpr uint32_t terminateRollyBallSound__16RigidPickupSound = 0x001C0768;   // size 0x24
constexpr uint32_t playRigidPickupSound__16RigidPickupSoundP8_fvectorfi = 0x001C0790;   // size 0x2B8
constexpr uint32_t initWindSound__9WindSoundiiffffiiiiiiii = 0x001C0A48;   // size 0x160
constexpr uint32_t terminateWindSound__9WindSound = 0x001C0BA8;   // size 0x24
constexpr uint32_t updateWindSound__9WindSound = 0x001C0BD0;   // size 0x2F0
constexpr uint32_t rampWindVolumeAndPitchUp__10LavaInhaleifi = 0x001C0EC0;   // size 0x8
constexpr uint32_t updateWindSound__10LavaInhale = 0x001C0EC8;   // size 0x8
constexpr uint32_t updateWindSound__11MantisGlide = 0x001C0ED0;   // size 0x1C
constexpr uint32_t rampWindVolumeDown__11MantisGlide = 0x001C0EF0;   // size 0xE4
constexpr uint32_t rampWindVolumeUp__11MantisGlide = 0x001C0FD8;   // size 0xE4
constexpr uint32_t calcDualMonoMixVolPan__FP8_fvectoriiRiT3Pi = 0x001C10C0;   // size 0x320
constexpr uint32_t calcCheapDualMonoMixPan__Fi = 0x001C13E0;   // size 0x3C
constexpr uint32_t findApproachingSpeed__FP8_fvectorPiPf = 0x001C1420;   // size 0x94
constexpr uint32_t calcDualMonoMixHeightBasedSoundVol__FffRiT2Pi = 0x001C14B8;   // size 0x2A0
constexpr uint32_t x__static_initialization_and_destruction_0_001C1758 = 0x001C1758;   // size 0x30
constexpr uint32_t x_GLOBAL__I_gIntroMoviesHavePlayed = 0x001C1788;   // size 0x20
constexpr uint32_t x__21StreamingSoundManager = 0x001C17A8;   // size 0x12C
constexpr uint32_t unpauseMusicStreams__21StreamingSoundManager = 0x001C18D8;   // size 0x34
constexpr uint32_t initStreamingSoundManager__21StreamingSoundManager = 0x001C1910;   // size 0x3D8
constexpr uint32_t reinitSoundManagerAfterDeath__21StreamingSoundManager = 0x001C1CE8;   // size 0x28
constexpr uint32_t fadeOutMusicBlocking__21StreamingSoundManager = 0x001C1D10;   // size 0x15C
constexpr uint32_t bufferIntroCinemaMusic__21StreamingSoundManager = 0x001C1E70;   // size 0x18C
constexpr uint32_t startCinemaMusic__21StreamingSoundManager = 0x001C2000;   // size 0x84
constexpr uint32_t stopCinemaMusic__21StreamingSoundManager = 0x001C2088;   // size 0xA4
constexpr uint32_t initSoundFadeOut__21StreamingSoundManageri = 0x001C2130;   // size 0x3C
constexpr uint32_t completeSoundFadeIn__21StreamingSoundManager = 0x001C2170;   // size 0x20
constexpr uint32_t rampDownSoundGlobalSoundVolume__21StreamingSoundManager = 0x001C2190;   // size 0x38
constexpr uint32_t rampUpSoundGlobalSoundVolume__21StreamingSoundManager = 0x001C21C8;   // size 0x44
constexpr uint32_t resetMasterSoundVolume__21StreamingSoundManager = 0x001C2210;   // size 0x28
constexpr uint32_t initMusicFadeOut__21StreamingSoundManager = 0x001C2238;   // size 0x44
constexpr uint32_t prepareForPlantBossIntroCinemaMusic__21StreamingSoundManager = 0x001C2280;   // size 0x1C0
constexpr uint32_t startCinemaMusicFromId__21StreamingSoundManageri = 0x001C2440;   // size 0x1DC
constexpr uint32_t updateStreamingSoundManager__21StreamingSoundManageri = 0x001C2620;   // size 0x810
constexpr uint32_t updateDeathEventMusic__21StreamingSoundManager = 0x001C2E30;   // size 0x8
constexpr uint32_t triggerDeathEventMusic__21StreamingSoundManagerP7Monster = 0x001C2E38;   // size 0x8
constexpr uint32_t updateLocationBasedStreamState__21StreamingSoundManagerP27LocationBasedStreamingSound = 0x001C2E40;   // size 0xCC
constexpr uint32_t findAvailableStream__21StreamingSoundManageri = 0x001C2F10;   // size 0x94
constexpr uint32_t updateGenericStreamState__21StreamingSoundManagerP21GenericStreamingSound = 0x001C2FA8;   // size 0x16C
constexpr uint32_t updateAmbientFadeoutState__21StreamingSoundManageri = 0x001C3118;   // size 0x1D8
constexpr uint32_t updateMusicFadeoutState__21StreamingSoundManager = 0x001C32F0;   // size 0xB8
constexpr uint32_t updateBattleFadeoutState__21StreamingSoundManageri = 0x001C33A8;   // size 0x1CC
constexpr uint32_t updateTauntLocation__21StreamingSoundManageriP8_fvector = 0x001C3578;   // size 0x138
constexpr uint32_t updateTakeDamageLocation__21StreamingSoundManageriP8_fvector = 0x001C36B0;   // size 0x138
constexpr uint32_t updateDeathStreamLocation__21StreamingSoundManageriP8_fvector = 0x001C37E8;   // size 0x138
constexpr uint32_t requestTaunt__21StreamingSoundManageriP8_fvector = 0x001C3920;   // size 0x1A8
constexpr uint32_t unpauseTaunt__21StreamingSoundManageri = 0x001C3AC8;   // size 0x13C
constexpr uint32_t requestGenericStream__21StreamingSoundManageriP8_fvector = 0x001C3C08;   // size 0xAC
constexpr uint32_t updateGenericStreamLocation__21StreamingSoundManageriP8_fvector = 0x001C3CB8;   // size 0x18
constexpr uint32_t requestDeathStream__21StreamingSoundManageriP8_fvector = 0x001C3CD0;   // size 0x1A8
constexpr uint32_t requestCrowdControlStream__21StreamingSoundManageriP8_fvector = 0x001C3E78;   // size 0x1A8
constexpr uint32_t requestTakeDamageStream__21StreamingSoundManageriP8_fvector = 0x001C4020;   // size 0x1A8
constexpr uint32_t initVagLocation__21StreamingSoundManagerPcPUi = 0x001C41C8;   // size 0xB0
constexpr uint32_t x__21GenericStreamingSound = 0x001C4278;   // size 0x20
constexpr uint32_t updateStreamingSound__21GenericStreamingSound = 0x001C4298;   // size 0x254
constexpr uint32_t initGenericStreamingSound__21GenericStreamingSoundPciiiif = 0x001C44F0;   // size 0xEC
constexpr uint32_t x__27LocationBasedStreamingSound = 0x001C45E0;   // size 0x34
constexpr uint32_t initLocationBasedStreamingSound__27LocationBasedStreamingSoundPcfffiiiii = 0x001C4618;   // size 0xE8
constexpr uint32_t setState__21GenericStreamingSound18_ambientSoundStatei = 0x001C4700;   // size 0x6C
constexpr uint32_t updateLocationBasedStreamingSound__27LocationBasedStreamingSound = 0x001C4770;   // size 0x294
constexpr uint32_t soundStopSound__FPUi = 0x001C4A08;   // size 0x2C
constexpr uint32_t x__14StateAirStrike = 0x001C4A38;   // size 0xC8
constexpr uint32_t init__14StateAirStrikeP7Monster = 0x001C4B00;   // size 0x8
constexpr uint32_t transitionOK__14StateAirStrike = 0x001C4B08;   // size 0xA4
constexpr uint32_t transitionInto__14StateAirStrike = 0x001C4BB0;   // size 0x8C
constexpr uint32_t update__14StateAirStrike = 0x001C4C40;   // size 0x5D4
constexpr uint32_t flyBeacon__14StateAirStrike = 0x001C5218;   // size 0x560
constexpr uint32_t handlePreemption__14StateAirStrikeP12MonsterState = 0x001C5778;   // size 0x14C
constexpr uint32_t acceptHit__14StateAirStrikeR8HitEvent = 0x001C58C8;   // size 0x48
constexpr uint32_t x__14StateBugAttack = 0x001C5910;   // size 0x8C
constexpr uint32_t transitionOK__14StateBugAttack = 0x001C59A0;   // size 0xA4
constexpr uint32_t transitionInto__14StateBugAttack = 0x001C5A48;   // size 0x9C
constexpr uint32_t update__14StateBugAttack = 0x001C5AE8;   // size 0x274
constexpr uint32_t handlePreemption__14StateBugAttackP12MonsterState = 0x001C5D60;   // size 0x38
constexpr uint32_t acceptHit__14StateBugAttackR8HitEvent = 0x001C5D98;   // size 0x48
constexpr uint32_t x__17StateCrowdControl = 0x001C5DE0;   // size 0x1AC
constexpr uint32_t init__17StateCrowdControlP7Monster = 0x001C5F90;   // size 0x30
constexpr uint32_t transitionOK__17StateCrowdControl = 0x001C5FC0;   // size 0xF8
constexpr uint32_t transitionInto__17StateCrowdControl = 0x001C60B8;   // size 0x618
constexpr uint32_t update__17StateCrowdControl = 0x001C66D0;   // size 0x334
constexpr uint32_t handlePreemption__17StateCrowdControlP12MonsterState = 0x001C6A08;   // size 0x44
constexpr uint32_t acceptHit__17StateCrowdControlR8HitEvent = 0x001C6A50;   // size 0x10
constexpr uint32_t x__15StateFireBreath = 0x001C6A60;   // size 0xC4
constexpr uint32_t init__15StateFireBreathP7Monster = 0x001C6B28;   // size 0xC
constexpr uint32_t transitionOK__15StateFireBreath = 0x001C6B38;   // size 0x88
constexpr uint32_t transitionInto__15StateFireBreath = 0x001C6BC0;   // size 0x94
constexpr uint32_t update__15StateFireBreath = 0x001C6C58;   // size 0x178
constexpr uint32_t acceptHit__15StateFireBreathR8HitEvent = 0x001C6DD0;   // size 0x48
constexpr uint32_t handleApplyMint__15StateFireBreath = 0x001C6E18;   // size 0x38
constexpr uint32_t x__16StateGrappleHook = 0x001C6E50;   // size 0x128
constexpr uint32_t init__16StateGrappleHookP7Monster = 0x001C6F78;   // size 0x90
constexpr uint32_t transitionOK__16StateGrappleHook = 0x001C7008;   // size 0x94
constexpr uint32_t transitionInto__16StateGrappleHook = 0x001C70A0;   // size 0x1D0
constexpr uint32_t update__16StateGrappleHook = 0x001C7270;   // size 0x624
constexpr uint32_t handlePreemption__16StateGrappleHookP12MonsterState = 0x001C7898;   // size 0x4C
constexpr uint32_t testCollis__16StateGrappleHook = 0x001C78E8;   // size 0x388
constexpr uint32_t getChainEnd__16StateGrappleHook = 0x001C7C70;   // size 0xC
constexpr uint32_t updateHoming__16StateGrappleHook = 0x001C7C80;   // size 0x158
constexpr uint32_t acceptHit__16StateGrappleHookR8HitEvent = 0x001C7DD8;   // size 0x48
constexpr uint32_t x__14StateLavaBlast = 0x001C7E20;   // size 0x124
constexpr uint32_t init__14StateLavaBlastP7Monster = 0x001C7F48;   // size 0x54
constexpr uint32_t transitionOK__14StateLavaBlast = 0x001C7FA0;   // size 0x94
constexpr uint32_t transitionInto__14StateLavaBlast = 0x001C8038;   // size 0xF8
constexpr uint32_t update__14StateLavaBlast = 0x001C8130;   // size 0x4C0
constexpr uint32_t enterSubState__14StateLavaBlastQ214StateLavaBlast8SubState = 0x001C85F0;   // size 0x23C
constexpr uint32_t handlePreemption__14StateLavaBlastP12MonsterState = 0x001C8830;   // size 0x68
constexpr uint32_t acceptHit__14StateLavaBlastR8HitEvent = 0x001C8898;   // size 0x48
constexpr uint32_t x__17StateRobotSpecial = 0x001C88E0;   // size 0x264
constexpr uint32_t init__17StateRobotSpecialP7Monster = 0x001C8B48;   // size 0x54
constexpr uint32_t transitionOK__17StateRobotSpecial = 0x001C8BA0;   // size 0xB0
constexpr uint32_t transitionInto__17StateRobotSpecial = 0x001C8C50;   // size 0x104
constexpr uint32_t update__17StateRobotSpecial = 0x001C8D58;   // size 0x5A0
constexpr uint32_t enterSubState__17StateRobotSpecialQ217StateRobotSpecial8SubState = 0x001C92F8;   // size 0x2F0
constexpr uint32_t acceptHit__17StateRobotSpecialR8HitEvent = 0x001C95E8;   // size 0x48
constexpr uint32_t handlePreemption__17StateRobotSpecialP12MonsterState = 0x001C9630;   // size 0x80
constexpr uint32_t x__16StateRockSpecial = 0x001C96B0;   // size 0x200
constexpr uint32_t init__16StateRockSpecialP7Monster = 0x001C98B0;   // size 0x8
constexpr uint32_t transitionOK__16StateRockSpecial = 0x001C98B8;   // size 0x2D8
constexpr uint32_t transitionInto__16StateRockSpecial = 0x001C9B90;   // size 0x128
constexpr uint32_t update__16StateRockSpecial = 0x001C9CB8;   // size 0x264
constexpr uint32_t handlePreemption__16StateRockSpecialP12MonsterState = 0x001C9F20;   // size 0x8
constexpr uint32_t acceptHit__16StateRockSpecialR8HitEvent = 0x001C9F28;   // size 0x48
constexpr uint32_t x__14StateSonicRoar = 0x001C9F70;   // size 0x110
constexpr uint32_t init__14StateSonicRoarP7Monster = 0x001CA080;   // size 0x64
constexpr uint32_t transitionOK__14StateSonicRoar = 0x001CA0E8;   // size 0x94
constexpr uint32_t transitionInto__14StateSonicRoar = 0x001CA180;   // size 0x1E4
constexpr uint32_t update__14StateSonicRoar = 0x001CA368;   // size 0x3F8
constexpr uint32_t testConeCollis__14StateSonicRoar = 0x001CA760;   // size 0x52C
constexpr uint32_t handlePreemption__14StateSonicRoarP12MonsterState = 0x001CAC90;   // size 0x84
constexpr uint32_t acceptHit__14StateSonicRoarR8HitEvent = 0x001CAD18;   // size 0x48
constexpr uint32_t x__14StateZapAttack = 0x001CAD60;   // size 0xBC
constexpr uint32_t init__14StateZapAttackP7Monster = 0x001CAE20;   // size 0x8
constexpr uint32_t transitionOK__14StateZapAttack = 0x001CAE28;   // size 0x94
constexpr uint32_t transitionInto__14StateZapAttack = 0x001CAEC0;   // size 0x160
constexpr uint32_t update__14StateZapAttack = 0x001CB020;   // size 0x11FC
constexpr uint32_t handlePreemption__14StateZapAttackP12MonsterState = 0x001CC220;   // size 0x20
constexpr uint32_t acceptHit__14StateZapAttackR8HitEvent = 0x001CC240;   // size 0x48
constexpr uint32_t x__12StateTopSpin = 0x001CC288;   // size 0xB0
constexpr uint32_t init__12StateTopSpinP7Monster = 0x001CC338;   // size 0x8
constexpr uint32_t transitionOK__12StateTopSpin = 0x001CC340;   // size 0x38
constexpr uint32_t transitionInto__12StateTopSpin = 0x001CC378;   // size 0x130
constexpr uint32_t update__12StateTopSpin = 0x001CC4A8;   // size 0x5D0
constexpr uint32_t handlePreemption__12StateTopSpinP12MonsterState = 0x001CCA78;   // size 0x38
constexpr uint32_t acceptHit__12StateTopSpinR8HitEvent = 0x001CCAB0;   // size 0x150
constexpr uint32_t x__16StateCannonHands = 0x001CCC00;   // size 0xD0
constexpr uint32_t init__16StateCannonHandsP7Monster = 0x001CCCD0;   // size 0x54
constexpr uint32_t transitionOK__16StateCannonHands = 0x001CCD28;   // size 0x90
constexpr uint32_t transitionInto__16StateCannonHands = 0x001CCDB8;   // size 0x11C
constexpr uint32_t update__16StateCannonHands = 0x001CCED8;   // size 0x47C
constexpr uint32_t enterSubState__16StateCannonHandsQ216StateCannonHands8SubState = 0x001CD358;   // size 0x220
constexpr uint32_t acceptHit__16StateCannonHandsR8HitEvent = 0x001CD578;   // size 0x8
constexpr uint32_t handlePreemption__16StateCannonHandsP12MonsterState = 0x001CD580;   // size 0x7C
constexpr uint32_t x__15StateUltraTazer = 0x001CD600;   // size 0x74
constexpr uint32_t init__15StateUltraTazerP7Monster = 0x001CD678;   // size 0x8
constexpr uint32_t transitionOK__15StateUltraTazer = 0x001CD680;   // size 0x5C
constexpr uint32_t transitionInto__15StateUltraTazer = 0x001CD6E0;   // size 0xBC
constexpr uint32_t update__15StateUltraTazer = 0x001CD7A0;   // size 0x5D0
constexpr uint32_t testCollis__15StateUltraTazer = 0x001CDD70;   // size 0x8
constexpr uint32_t handlePreemption__15StateUltraTazerP12MonsterState = 0x001CDD78;   // size 0x8
constexpr uint32_t acceptHit__15StateUltraTazerR8HitEvent = 0x001CDD80;   // size 0xC0
constexpr uint32_t x__tf14StateAirStrike = 0x001CDE50;   // size 0x50
constexpr uint32_t flyBeacon__14StateAirStrikePv = 0x001CDEA0;   // size 0x1C
constexpr uint32_t inWindUp__14StateAirStrike = 0x001CDEC0;   // size 0x10
constexpr uint32_t inWindDown__14StateAirStrike = 0x001CDED0;   // size 0x10
constexpr uint32_t x__tf14StateBugAttack = 0x001CDEE0;   // size 0x50
constexpr uint32_t inWindUp__14StateBugAttack = 0x001CDF30;   // size 0x10
constexpr uint32_t inWindDown__14StateBugAttack = 0x001CDF40;   // size 0x10
constexpr uint32_t x__tf17StateCrowdControl = 0x001CDF50;   // size 0x50
constexpr uint32_t setFxEp__17StateCrowdControlP9_hierhead = 0x001CDFA0;   // size 0x8
constexpr uint32_t getMaxRange__17StateCrowdControl = 0x001CDFA8;   // size 0x18
constexpr uint32_t x__tf15StateFireBreath = 0x001CDFC0;   // size 0x50
constexpr uint32_t inWindUp__15StateFireBreath = 0x001CE010;   // size 0x10
constexpr uint32_t inWindDown__15StateFireBreath = 0x001CE020;   // size 0x10
constexpr uint32_t x__tf16StateGrappleHook = 0x001CE030;   // size 0x50
constexpr uint32_t inWindUp__16StateGrappleHook = 0x001CE080;   // size 0x10
constexpr uint32_t inWindDown__16StateGrappleHook = 0x001CE090;   // size 0x10
constexpr uint32_t x__tf14StateLavaBlast = 0x001CE0A0;   // size 0x50
constexpr uint32_t inWindUp__14StateLavaBlast = 0x001CE0F0;   // size 0x10
constexpr uint32_t inWindDown__14StateLavaBlast = 0x001CE100;   // size 0x10
constexpr uint32_t getOnFireDamage__14StateLavaBlast = 0x001CE110;   // size 0x8
constexpr uint32_t getOnFireDuration__14StateLavaBlast = 0x001CE118;   // size 0x8
constexpr uint32_t getImpactDamage__14StateLavaBlast = 0x001CE120;   // size 0x8
constexpr uint32_t getSpeedMPH__14StateLavaBlast = 0x001CE128;   // size 0x8
constexpr uint32_t getHorizontalHoming__14StateLavaBlast = 0x001CE130;   // size 0x8
constexpr uint32_t getVerticalHoming__14StateLavaBlast = 0x001CE138;   // size 0x8
constexpr uint32_t x__tf17StateRobotSpecial = 0x001CE140;   // size 0x50
constexpr uint32_t inWindUp__17StateRobotSpecial = 0x001CE190;   // size 0xC
constexpr uint32_t inWindDown__17StateRobotSpecial = 0x001CE1A0;   // size 0x10
constexpr uint32_t getMissileDuration__17StateRobotSpecial = 0x001CE1B0;   // size 0x8
constexpr uint32_t getMissileMaxSpeedMPH__17StateRobotSpecial = 0x001CE1B8;   // size 0x8
constexpr uint32_t getMissileHomingFactor__17StateRobotSpecial = 0x001CE1C0;   // size 0x8
constexpr uint32_t getShockConfig__17StateRobotSpecial = 0x001CE1C8;   // size 0x8
constexpr uint32_t getFxAnimSpeed__17StateRobotSpecial = 0x001CE1D0;   // size 0x8
constexpr uint32_t x__tf16StateRockSpecial = 0x001CE1D8;   // size 0x50
constexpr uint32_t inWindUp__16StateRockSpecial = 0x001CE228;   // size 0x10
constexpr uint32_t inWindDown__16StateRockSpecial = 0x001CE238;   // size 0x10
constexpr uint32_t getHeadDuration__16StateRockSpecial = 0x001CE248;   // size 0x8
constexpr uint32_t getKnockBackUp__16StateRockSpecial = 0x001CE250;   // size 0x8
constexpr uint32_t getKnockBackOut__16StateRockSpecial = 0x001CE258;   // size 0x8
constexpr uint32_t x__tf14StateSonicRoar = 0x001CE260;   // size 0x50
constexpr uint32_t inWindUp__14StateSonicRoar = 0x001CE2B0;   // size 0x10
constexpr uint32_t inWindDown__14StateSonicRoar = 0x001CE2C0;   // size 0x10
constexpr uint32_t x__tf14StateZapAttack = 0x001CE2D0;   // size 0x50
constexpr uint32_t inWindUp__14StateZapAttack = 0x001CE320;   // size 0x10
constexpr uint32_t inWindDown__14StateZapAttack = 0x001CE330;   // size 0x10
constexpr uint32_t x__tf12StateTopSpin = 0x001CE340;   // size 0x50
constexpr uint32_t x__tf16StateCannonHands = 0x001CE390;   // size 0x50
constexpr uint32_t x__tf15StateUltraTazer = 0x001CE3E0;   // size 0x50
constexpr uint32_t update__8OgreMacePv = 0x001CE430;   // size 0x1C
constexpr uint32_t addInstance__10SpecFxAnimP9_hierheadi = 0x001CE4A0;   // size 0x40
constexpr uint32_t isInstanceAvailable__10SpecFxAnimP7Monsteri = 0x001CE4E0;   // size 0x50
constexpr uint32_t getInstance__10SpecFxAnimP7Monsteri = 0x001CE530;   // size 0x80
constexpr uint32_t x__10SpecFxAnim = 0x001CE5B0;   // size 0x3C
constexpr uint32_t registerModel__10SpecFxAnimP9_hierheadi = 0x001CE5F0;   // size 0x94
constexpr uint32_t initBeforeDbLoad__10SpecFxAnim = 0x001CE688;   // size 0x34
constexpr uint32_t initAfterDbLoad__10SpecFxAnim = 0x001CE6C0;   // size 0x8C
constexpr uint32_t launch__10SpecFxAnim = 0x001CE750;   // size 0x30
constexpr uint32_t end__10SpecFxAnim = 0x001CE780;   // size 0x34
constexpr uint32_t kill__10SpecFxAnim = 0x001CE7B8;   // size 0x10
constexpr uint32_t startAnim__10SpecFxAnimi = 0x001CE7C8;   // size 0x48
constexpr uint32_t updateOrient__10SpecFxAnim = 0x001CE810;   // size 0x198
constexpr uint32_t x__static_initialization_and_destruction_0_001CE9A8 = 0x001CE9A8;   // size 0x64
constexpr uint32_t x_GLOBAL__I__10SpecFxAnim_s_numFxs = 0x001CEA10;   // size 0x20
constexpr uint32_t addPoint__11StartPointsUcP8_fvectorfb = 0x001CEA30;   // size 0x54
constexpr uint32_t clear__11StartPoints = 0x001CEA88;   // size 0x34
constexpr uint32_t getNumPoints__11StartPointsUc = 0x001CEAC0;   // size 0x1C
constexpr uint32_t getPoint__11StartPointsUci = 0x001CEAE0;   // size 0x24
constexpr uint32_t isThisTypeFull__11StartPointsUc = 0x001CEB08;   // size 0x2C
constexpr uint32_t getNextPoint__11StartPointsUc = 0x001CEB38;   // size 0x3C
constexpr uint32_t loadPoints__14StartPointTool = 0x001CEB78;   // size 0x70
constexpr uint32_t init__14StartPointTooli = 0x001CEBE8;   // size 0x54
constexpr uint32_t exportPoints__14StartPointTool = 0x001CEC40;   // size 0x98
constexpr uint32_t degreesToData__14StartPointToolf = 0x001CECD8;   // size 0x40
constexpr uint32_t dataToDegrees__14StartPointToolUc = 0x001CED18;   // size 0x48
constexpr uint32_t x__static_initialization_and_destruction_0_001CED60 = 0x001CED60;   // size 0x28
constexpr uint32_t x__tf14StartPointTool = 0x001CED88;   // size 0x50
constexpr uint32_t x__14StartPointTool = 0x001CEDD8;   // size 0x14
constexpr uint32_t x_GLOBAL__I__14StartPointTool_instance = 0x001CEDF0;   // size 0x20
constexpr uint32_t DefaultSetup__11StickShakerRt13ExpandingRing1Z17StickShakerConfig = 0x001CEE10;   // size 0xF0
constexpr uint32_t Init__11StickShaker = 0x001CEF00;   // size 0x34
constexpr uint32_t Activate__11StickShakerP8_fvectorGt13ExpandingRing1Z17StickShakerConfigb = 0x001CEF38;   // size 0x244
constexpr uint32_t update__11StickShaker = 0x001CF180;   // size 0x218
constexpr uint32_t x__static_initialization_and_destruction_0_001CF398 = 0x001CF398;   // size 0x74
constexpr uint32_t update__11StickShakerPv = 0x001CF410;   // size 0x1C
constexpr uint32_t x_GLOBAL__I_DefaultSetup__11StickShakerRt13ExpandingRing1Z17StickShakerConfig = 0x001CF430;   // size 0x20
constexpr uint32_t x__12SubwayPickup = 0x001CF450;   // size 0x40
constexpr uint32_t initAfterDbLoad__12SubwayPickup = 0x001CF490;   // size 0x124
constexpr uint32_t takeHit__12SubwayPickupP8_fvectorfi = 0x001CF5B8;   // size 0x30
constexpr uint32_t kill__12SubwayPickup = 0x001CF5E8;   // size 0x3C
constexpr uint32_t drop__12SubwayPickup = 0x001CF628;   // size 0x40
constexpr uint32_t update__12SubwayPickup = 0x001CF668;   // size 0x204
constexpr uint32_t x__tf12SubwayPickup = 0x001CF8A8;   // size 0x50
constexpr uint32_t getVel__12SubwayPickup = 0x001CF8F8;   // size 0x8
constexpr uint32_t x__10TankPickup = 0x001CF900;   // size 0x5C
constexpr uint32_t initAfterDbLoad__10TankPickup = 0x001CF960;   // size 0xCC
constexpr uint32_t setTrans__10TankPickupR8_fvector = 0x001CFA30;   // size 0x48
constexpr uint32_t takeHit__10TankPickupP8_fvectorfi = 0x001CFA78;   // size 0x50
constexpr uint32_t drop__10TankPickup = 0x001CFAC8;   // size 0x3C
constexpr uint32_t kill__10TankPickup = 0x001CFB08;   // size 0x84
constexpr uint32_t fire__10TankPickupR7GamePad = 0x001CFB90;   // size 0x74
constexpr uint32_t update__10TankPickup = 0x001CFC08;   // size 0x164
constexpr uint32_t updateLeadBehavior__10TankPickupR7GamePad = 0x001CFD70;   // size 0xC4
constexpr uint32_t updateFollowBehavior__10TankPickupR7GamePad = 0x001CFE38;   // size 0x1C
constexpr uint32_t updateAttackBehavior__10TankPickupR7GamePad = 0x001CFE58;   // size 0x318
constexpr uint32_t enterState__10TankPickupQ214MilitaryPickup5State = 0x001D0170;   // size 0x164
constexpr uint32_t leadFormation__10TankPickup = 0x001D02D8;   // size 0x48
constexpr uint32_t followFormation__10TankPickup = 0x001D0320;   // size 0x4C
constexpr uint32_t resignFormation__10TankPickup = 0x001D0370;   // size 0x24
constexpr uint32_t x__tf10TankPickup = 0x001D03D0;   // size 0x50
constexpr uint32_t x__11TankVehicle = 0x001D0420;   // size 0x78
constexpr uint32_t updateControls__11TankVehicleR7GamePad = 0x001D0498;   // size 0x130
constexpr uint32_t x__12TurretPickup = 0x001D05C8;   // size 0x34
constexpr uint32_t initAfterDbLoad__12TurretPickup = 0x001D0600;   // size 0x100
constexpr uint32_t takeHit__12TurretPickupP8_fvectorfi = 0x001D0700;   // size 0x10
constexpr uint32_t drop__12TurretPickup = 0x001D0710;   // size 0x3C
constexpr uint32_t kill__12TurretPickup = 0x001D0750;   // size 0xA0
constexpr uint32_t update__12TurretPickup = 0x001D07F0;   // size 0x44
constexpr uint32_t fire__12TurretPickupR8_fvector = 0x001D0838;   // size 0x78
constexpr uint32_t grab__12TurretPickupi = 0x001D08B0;   // size 0x38
constexpr uint32_t updateBehavior__12TurretPickup = 0x001D08E8;   // size 0x640
constexpr uint32_t enterState__12TurretPickupQ212TurretPickup5State = 0x001D0F28;   // size 0xB8
constexpr uint32_t x__tf12TurretPickup = 0x001D1018;   // size 0x50
constexpr uint32_t getState__12TurretPickup = 0x001D1068;   // size 0x8
constexpr uint32_t threeMileIsTubActive__Fv = 0x001D1070;   // size 0x10
constexpr uint32_t threeMileGetTubInitiator__Fv = 0x001D1080;   // size 0x8
constexpr uint32_t threeMileInitBefore__Fv = 0x001D1088;   // size 0x154
constexpr uint32_t threeMileInitAfter__Fv = 0x001D11E0;   // size 0x184
constexpr uint32_t threeMileInitAi__Fv = 0x001D1368;   // size 0x78
constexpr uint32_t threeMileUpdate__Fv = 0x001D13E0;   // size 0x1B8
constexpr uint32_t threeMileUpdateStamina__Fv = 0x001D1598;   // size 0xF0
constexpr uint32_t threeMileUpdateTub__Fv = 0x001D1688;   // size 0xEC
constexpr uint32_t threeMileUpdateStacks__Fv = 0x001D1778;   // size 0x56C
constexpr uint32_t threeMileUpdateCountDown__Fv = 0x001D1CE8;   // size 0x820
constexpr uint32_t threeMileUpdateTransition__Fv = 0x001D2508;   // size 0x90
constexpr uint32_t threeMilePlayFx__FP8_fvector = 0x001D2598;   // size 0x9C
constexpr uint32_t threeMileDrawFlash__Fv = 0x001D2638;   // size 0x14C
constexpr uint32_t threeMileUpdateRespawn__Fv = 0x001D2788;   // size 0x354
constexpr uint32_t threeMileFillTub__FP7Monster = 0x001D2AE0;   // size 0xA8
constexpr uint32_t threeMileStartVent__Fv = 0x001D2B88;   // size 0x88
constexpr uint32_t threeMileAddEpNode__FP9_hierhead = 0x001D2C10;   // size 0x110
constexpr uint32_t threeMileSetLocator__FiRA3_A3_fP8_fvector = 0x001D2D20;   // size 0x80
constexpr uint32_t threeMileDecStamina__Ff = 0x001D2DA0;   // size 0x14
constexpr uint32_t threeMileActivateStack__Fi = 0x001D2DB8;   // size 0x4C
constexpr uint32_t threeMileStackDestroyed__Fi = 0x001D2E08;   // size 0x168
constexpr uint32_t threeMileRespawnAi__Fi = 0x001D2F70;   // size 0x60
constexpr uint32_t threeMileGetHeadHealth__Fi = 0x001D2FD0;   // size 0x1C
constexpr uint32_t threeMileGetHeadPos__Fi = 0x001D2FF0;   // size 0x18
constexpr uint32_t threeMileGetAttackState__Fv = 0x001D3008;   // size 0xC
constexpr uint32_t threeMileGetCountDown__Fv = 0x001D3018;   // size 0x8
constexpr uint32_t x__9PlantBoss = 0x001D3020;   // size 0x11C
constexpr uint32_t x__static_initialization_and_destruction_0_001D3140 = 0x001D3140;   // size 0x5C
constexpr uint32_t x_GLOBAL__I_plantBoss = 0x001D3228;   // size 0x20
constexpr uint32_t x__14StateThrowBackQ212MonsterState2IdUi = 0x001D3248;   // size 0x19C
constexpr uint32_t init__14StateThrowBackP7Monster = 0x001D33E8;   // size 0x120
constexpr uint32_t transitionOK__14StateThrowBack = 0x001D3508;   // size 0x28
constexpr uint32_t transitionInto__14StateThrowBack = 0x001D3530;   // size 0x1A0
constexpr uint32_t takeStomp__14StateThrowBack = 0x001D36D0;   // size 0x34
constexpr uint32_t update__14StateThrowBack = 0x001D3708;   // size 0x800
constexpr uint32_t enterSubState__14StateThrowBackQ214StateThrowBack8SubState = 0x001D3F08;   // size 0xBB4
constexpr uint32_t updateFlail__14StateThrowBack = 0x001D4AC0;   // size 0x1BC
constexpr uint32_t rampInAdds__14StateThrowBackf = 0x001D4C80;   // size 0x154
constexpr uint32_t rampOutAdds__14StateThrowBackf = 0x001D4DD8;   // size 0x70
constexpr uint32_t updateDynamics__14StateThrowBackf = 0x001D4E48;   // size 0xCC
constexpr uint32_t updateUpperAdds__14StateThrowBackR8_fvectorf = 0x001D4F18;   // size 0x164
constexpr uint32_t updateLowerAdds__14StateThrowBackR8_fvectorf = 0x001D5080;   // size 0x160
constexpr uint32_t rotAroundBaseSphere__14StateThrowBackR8_fvector = 0x001D51E0;   // size 0xC4
constexpr uint32_t handleCollis__14StateThrowBackR9_hdResult = 0x001D52A8;   // size 0x720
constexpr uint32_t handleDamage__14StateThrowBackR9_hdResult = 0x001D59C8;   // size 0x56C
constexpr uint32_t acceptHit__14StateThrowBackR8HitEvent = 0x001D5F38;   // size 0xD0
constexpr uint32_t handlePreemption__14StateThrowBackP12MonsterState = 0x001D6008;   // size 0xC0
constexpr uint32_t x__17StateBeingSlammed = 0x001D60C8;   // size 0x40
constexpr uint32_t transitionInto__17StateBeingSlammed = 0x001D6108;   // size 0x30
constexpr uint32_t x__16StateBeingThrown = 0x001D6138;   // size 0x40
constexpr uint32_t transitionInto__16StateBeingThrown = 0x001D6178;   // size 0x3C
constexpr uint32_t x__14StateKnockBack = 0x001D61B8;   // size 0x3C
constexpr uint32_t transitionInto__14StateKnockBack = 0x001D61F8;   // size 0x3C
constexpr uint32_t x__17StateGrappleBreak = 0x001D6238;   // size 0x40
constexpr uint32_t transitionInto__17StateGrappleBreak = 0x001D6278;   // size 0x30
constexpr uint32_t x__tf14StateThrowBack = 0x001D62A8;   // size 0x50
constexpr uint32_t setThrowSource__14StateThrowBackP7Monster = 0x001D62F8;   // size 0x8
constexpr uint32_t setPauseDuration__14StateThrowBackf = 0x001D6300;   // size 0x8
constexpr uint32_t setCounterable__14StateThrowBackb = 0x001D6308;   // size 0x8
constexpr uint32_t getSubState__14StateThrowBack = 0x001D6310;   // size 0x8
constexpr uint32_t getThrowSource__14StateThrowBack = 0x001D6318;   // size 0x8
constexpr uint32_t isCounterAvailable__14StateThrowBack = 0x001D6320;   // size 0x28
constexpr uint32_t x__tf17StateBeingSlammed = 0x001D6348;   // size 0x88
constexpr uint32_t setDownTime__17StateBeingSlammedf = 0x001D63D0;   // size 0x8
constexpr uint32_t x__tf16StateBeingThrown = 0x001D63D8;   // size 0x88
constexpr uint32_t x__tf17StateGrappleBreak = 0x001D6460;   // size 0x88
constexpr uint32_t x__tf14StateKnockBack = 0x001D64E8;   // size 0x88
constexpr uint32_t x__12TokenManager = 0x001D6598;   // size 0x28
constexpr uint32_t init__12TokenManager = 0x001D65C0;   // size 0x68
constexpr uint32_t setMax__12TokenManagerPci = 0x001D6628;   // size 0xCC
constexpr uint32_t credit__12TokenManagerPci = 0x001D66F8;   // size 0xEC
constexpr uint32_t grandTotal__12TokenManager = 0x001D67E8;   // size 0x64
constexpr uint32_t getTokenKey__12TokenManageri = 0x001D6850;   // size 0xC
constexpr uint32_t setMilestone__12TokenManagerPciiQ212TokenManager13MilestoneType = 0x001D6860;   // size 0x130
constexpr uint32_t tokenValue__12TokenManagerPc = 0x001D6990;   // size 0x98
constexpr uint32_t tokenValue__12TokenManageri = 0x001D6A28;   // size 0xB0
constexpr uint32_t tokyoGetTidalWaveSource__Fv = 0x001D6AD8;   // size 0x8
constexpr uint32_t tokyoInitBefore__Fv = 0x001D6AE0;   // size 0x28
constexpr uint32_t tokyoInitAfter__Fv = 0x001D6B08;   // size 0x80
constexpr uint32_t tokyoUpdate__Fv = 0x001D6B88;   // size 0x434
constexpr uint32_t tokyoStartTidalWave__FP7Monster = 0x001D6FC0;   // size 0x74
constexpr uint32_t tokyoAddEpNode__FP9_hierhead = 0x001D7038;   // size 0x8
constexpr uint32_t init__8UfoTokyo = 0x001D7040;   // size 0x100
constexpr uint32_t update__8UfoTokyo = 0x001D7140;   // size 0x190
constexpr uint32_t takeHit__8UfoTokyoP8_fvectorfi = 0x001D72D0;   // size 0xC0
constexpr uint32_t x__static_initialization_and_destruction_0_001D7390 = 0x001D7390;   // size 0x4C
constexpr uint32_t x__tf8UfoTokyo = 0x001D7428;   // size 0x50
constexpr uint32_t x_GLOBAL__I_tidalWaveAnim = 0x001D74B8;   // size 0x20
constexpr uint32_t ufoInitBefore__Fv = 0x001D74D8;   // size 0x8
constexpr uint32_t ufoInitAfter__Fv = 0x001D74E0;   // size 0x14
constexpr uint32_t ufoUpdate__Fv = 0x001D74F8;   // size 0x210
constexpr uint32_t x__6UfoNavR9UfoPickupP7PathNet = 0x001D7708;   // size 0x20
constexpr uint32_t init__6UfoNav = 0x001D7728;   // size 0x90
constexpr uint32_t update__6UfoNav = 0x001D77B8;   // size 0x68
constexpr uint32_t disable__6UfoNav = 0x001D7820;   // size 0x8
constexpr uint32_t wander__6UfoNav = 0x001D7828;   // size 0xC
constexpr uint32_t seek__6UfoNavR8_fvectorf = 0x001D7838;   // size 0x40
constexpr uint32_t target__6UfoNavR8_fvector = 0x001D7878;   // size 0x10
constexpr uint32_t updateWander__6UfoNav = 0x001D7888;   // size 0x50
constexpr uint32_t updateSeek__6UfoNav = 0x001D78D8;   // size 0x298
constexpr uint32_t flee__6UfoNavR8_fvectorff = 0x001D7B70;   // size 0x14
constexpr uint32_t updateFlee__6UfoNav = 0x001D7B88;   // size 0x104
constexpr uint32_t followPath__6UfoNav = 0x001D7C90;   // size 0x150
constexpr uint32_t nextTargetTrans__6UfoNav = 0x001D7DE0;   // size 0x318
constexpr uint32_t computePathInfo__6UfoNavP8PathNodeP4Path = 0x001D80F8;   // size 0xF0
constexpr uint32_t getRandPath__6UfoNav = 0x001D81E8;   // size 0xB8
constexpr uint32_t getSeekPath__6UfoNavR8_fvector = 0x001D82A0;   // size 0x160
constexpr uint32_t getFleePath__6UfoNavR8_fvector = 0x001D8400;   // size 0x160
constexpr uint32_t x__9UfoPickup = 0x001D8560;   // size 0x48
constexpr uint32_t initAfterDbLoad__9UfoPickup = 0x001D85A8;   // size 0x1EC
constexpr uint32_t takeHit__9UfoPickupP8_fvectorfi = 0x001D8798;   // size 0x10C
constexpr uint32_t grab__9UfoPickupi = 0x001D88A8;   // size 0x30
constexpr uint32_t drop__9UfoPickup = 0x001D88D8;   // size 0x3C
constexpr uint32_t kill__9UfoPickup = 0x001D8918;   // size 0xBC
constexpr uint32_t update__9UfoPickup = 0x001D89D8;   // size 0x278
constexpr uint32_t fire__9UfoPickupR8_fvector = 0x001D8C50;   // size 0xB8
constexpr uint32_t regen__9UfoPickup = 0x001D8D08;   // size 0x78
constexpr uint32_t updateAttackBehavior__9UfoPickup = 0x001D8D80;   // size 0x46C
constexpr uint32_t enterState__9UfoPickupQ29UfoPickup5State = 0x001D91F0;   // size 0x158
constexpr uint32_t x__tf9UfoPickup = 0x001D9380;   // size 0x50
constexpr uint32_t getVel__9UfoPickup = 0x001D93D0;   // size 0x8
constexpr uint32_t uiMain__Fi = 0x001D93D8;   // size 0x490
constexpr uint32_t userintMain__Fv = 0x001D9868;   // size 0x750
constexpr uint32_t userintReturnToShell__Fiii = 0x001D9FB8;   // size 0x30
constexpr uint32_t userintTimeToReturnToShell__Fv = 0x001D9FE8;   // size 0x30
constexpr uint32_t uiFadeScreen__FUi = 0x001DA018;   // size 0x164
constexpr uint32_t uiInitScreens__FP3_cs = 0x001DA180;   // size 0x244
constexpr uint32_t uiIntro__Fv = 0x001DA3C8;   // size 0x74
constexpr uint32_t uiOutro__Fv = 0x001DA440;   // size 0x74
constexpr uint32_t playMovie__Fv = 0x001DA4B8;   // size 0x3E8
constexpr uint32_t uiValidateMonster__Fi = 0x001DA8A0;   // size 0xD4
constexpr uint32_t uiAlignItemWithCamera__FP3_csfff = 0x001DA978;   // size 0xE8
constexpr uint32_t letterboxDmaData__Ff = 0x001DAA60;   // size 0x1D8
constexpr uint32_t blackenScreen__Fv = 0x001DAC38;   // size 0x130
constexpr uint32_t uiInit__Fv = 0x001DAD68;   // size 0xD8
constexpr uint32_t x__static_initialization_and_destruction_0_001DAE40 = 0x001DAE40;   // size 0xA8
constexpr uint32_t x_GLOBAL__I_myMovie = 0x001DAEE8;   // size 0x20
constexpr uint32_t uidevMain__Fv = 0x001DAF08;   // size 0x1C0
constexpr uint32_t x__________CDevUi____________Fv = 0x001DB0C8;   // size 0x8
constexpr uint32_t Init__6CDevUi = 0x001DB0D0;   // size 0xB0
constexpr uint32_t Draw__6CDevUi = 0x001DB180;   // size 0x2C
constexpr uint32_t Update__6CDevUi = 0x001DB1B0;   // size 0x2C
constexpr uint32_t AddScreen__6CDevUiP7CScreenb = 0x001DB1E0;   // size 0x68
constexpr uint32_t MoveOn__6CDevUi = 0x001DB248;   // size 0x43C
constexpr uint32_t MoveBack__6CDevUi = 0x001DB688;   // size 0x4C
constexpr uint32_t x__________CScreen____________Fv = 0x001DB6D8;   // size 0x8
constexpr uint32_t Init__7CScreen = 0x001DB6E0;   // size 0xC
constexpr uint32_t Setup__7CScreen = 0x001DB6F0;   // size 0xC
constexpr uint32_t Cleanup__7CScreen = 0x001DB700;   // size 0xC
constexpr uint32_t Update__7CScreen = 0x001DB710;   // size 0x8
constexpr uint32_t Draw__7CScreen = 0x001DB718;   // size 0x8
constexpr uint32_t x__________CDevScreen____________Fv = 0x001DB720;   // size 0x8
constexpr uint32_t Init__10CDevScreen = 0x001DB728;   // size 0x2E8
constexpr uint32_t Setup__10CDevScreen = 0x001DBA10;   // size 0x8
constexpr uint32_t Cleanup__10CDevScreen = 0x001DBA18;   // size 0x8
constexpr uint32_t Update__10CDevScreen = 0x001DBA20;   // size 0x160
constexpr uint32_t Draw__10CDevScreen = 0x001DBB80;   // size 0x60
constexpr uint32_t x__________CNetworkScreen____________Fv = 0x001DBBE0;   // size 0x8
constexpr uint32_t Init__14CNetworkScreen = 0x001DBBE8;   // size 0x324
constexpr uint32_t Setup__14CNetworkScreen = 0x001DBF10;   // size 0x8
constexpr uint32_t Cleanup__14CNetworkScreen = 0x001DBF18;   // size 0x8
constexpr uint32_t Update__14CNetworkScreen = 0x001DBF20;   // size 0x138
constexpr uint32_t Draw__14CNetworkScreen = 0x001DC058;   // size 0x60
constexpr uint32_t x__________CBoundBox____________Fv = 0x001DC0B8;   // size 0x8
constexpr uint32_t SetBox__9CBoundBoxiiii = 0x001DC0C0;   // size 0x14
constexpr uint32_t GetBox__9CBoundBoxPiN31 = 0x001DC0D8;   // size 0x24
constexpr uint32_t Draw__9CBoundBoxi = 0x001DC100;   // size 0x250
constexpr uint32_t x__________CScreenSection____________Fv = 0x001DC350;   // size 0x8
constexpr uint32_t SetTitle__14CScreenSectionPc = 0x001DC358;   // size 0x1C
constexpr uint32_t Init__14CScreenSection = 0x001DC378;   // size 0x8
constexpr uint32_t Draw__14CScreenSection = 0x001DC380;   // size 0x1C
constexpr uint32_t DrawTitle__14CScreenSectioni = 0x001DC3A0;   // size 0xA0
constexpr uint32_t Update__14CScreenSection = 0x001DC440;   // size 0x8
constexpr uint32_t x__________CFunctionalSection____________Fv = 0x001DC448;   // size 0x8
constexpr uint32_t SetActive__18CFunctionalSectioni = 0x001DC450;   // size 0x8
constexpr uint32_t Init__18CFunctionalSectionPc = 0x001DC458;   // size 0x8
constexpr uint32_t Draw__18CFunctionalSection = 0x001DC460;   // size 0x64
constexpr uint32_t DrawContents__18CFunctionalSection = 0x001DC4C8;   // size 0x8
constexpr uint32_t DrawFooter__18CFunctionalSectioni = 0x001DC4D0;   // size 0x8
constexpr uint32_t Update__18CFunctionalSection = 0x001DC4D8;   // size 0x8
constexpr uint32_t MoveForward__18CFunctionalSectioni = 0x001DC4E0;   // size 0x9C
constexpr uint32_t MoveBack__18CFunctionalSectioni = 0x001DC580;   // size 0x9C
constexpr uint32_t HandleInput__18CFunctionalSection = 0x001DC620;   // size 0xB0
constexpr uint32_t FinishSelection__18CFunctionalSection = 0x001DC6D0;   // size 0x34
constexpr uint32_t UndoSelection__18CFunctionalSection = 0x001DC708;   // size 0x34
constexpr uint32_t x__________CVerticalSection____________Fv = 0x001DC740;   // size 0x8
constexpr uint32_t Init__16CVerticalSectionPc = 0x001DC748;   // size 0xA4
constexpr uint32_t DrawContents__16CVerticalSection = 0x001DC7F0;   // size 0x210
constexpr uint32_t DrawFooter__16CVerticalSectioni = 0x001DCA00;   // size 0x8
constexpr uint32_t Update__16CVerticalSectioni = 0x001DCA08;   // size 0xD4
constexpr uint32_t HandleInput__16CVerticalSection = 0x001DCAE0;   // size 0x188
constexpr uint32_t x__________CHorizontalSection____________Fv = 0x001DCC68;   // size 0x8
constexpr uint32_t Init__18CHorizontalSectionPc = 0x001DCC70;   // size 0x6C
constexpr uint32_t DrawContents__18CHorizontalSection = 0x001DCCE0;   // size 0x14C
constexpr uint32_t DrawFooter__18CHorizontalSectioni = 0x001DCE30;   // size 0x8
constexpr uint32_t Update__18CHorizontalSection = 0x001DCE38;   // size 0x8
constexpr uint32_t HandleInput__18CHorizontalSection = 0x001DCE40;   // size 0xF8
constexpr uint32_t FinishSelection__18CHorizontalSection = 0x001DCF38;   // size 0x34
constexpr uint32_t UndoSelection__18CHorizontalSection = 0x001DCF70;   // size 0x34
constexpr uint32_t FinishSelection__16CVerticalSection = 0x001DCFA8;   // size 0x34
constexpr uint32_t UndoSelection__16CVerticalSection = 0x001DCFE0;   // size 0x34
constexpr uint32_t updateLevelStrings__Fv = 0x001DD018;   // size 0x160
constexpr uint32_t updateServerStrings__Fv = 0x001DD178;   // size 0x164
constexpr uint32_t x__static_initialization_and_destruction_0_001DD2E0 = 0x001DD2E0;   // size 0xA0
constexpr uint32_t x__tf14CScreenSection = 0x001DD380;   // size 0x3C
constexpr uint32_t GetTitle__14CScreenSection = 0x001DD3C0;   // size 0x8
constexpr uint32_t SetBox__14CScreenSectioniiii = 0x001DD3C8;   // size 0x1C
constexpr uint32_t GetBox__14CScreenSectionPiN31 = 0x001DD3E8;   // size 0x1C
constexpr uint32_t x__tf18CFunctionalSection = 0x001DD408;   // size 0x6C
constexpr uint32_t IsActive__18CFunctionalSection = 0x001DD478;   // size 0x8
constexpr uint32_t x__tf18CHorizontalSection = 0x001DD480;   // size 0x98
constexpr uint32_t x__tf16CVerticalSection = 0x001DD518;   // size 0x98
constexpr uint32_t x__tf7CScreen = 0x001DD5B0;   // size 0x3C
constexpr uint32_t x__tf10CDevScreen = 0x001DD5F0;   // size 0x6C
constexpr uint32_t x__10CDevScreen = 0x001DD660;   // size 0x50
constexpr uint32_t x__tf14CNetworkScreen = 0x001DD6B0;   // size 0x6C
constexpr uint32_t x__14CNetworkScreen = 0x001DD720;   // size 0x48
constexpr uint32_t x_GLOBAL__I_devScreen = 0x001DD768;   // size 0x20
constexpr uint32_t vegasInitBefore__Fv = 0x001DD788;   // size 0x8
constexpr uint32_t vegasInitAfter__Fv = 0x001DD790;   // size 0x20
constexpr uint32_t vegasUpdate__Fv = 0x001DD7B0;   // size 0x374
constexpr uint32_t getHatField__Fv = 0x001DDB28;   // size 0x60
constexpr uint32_t x__7Vehicle = 0x001DDB88;   // size 0x88
constexpr uint32_t setCs__7VehicleP3_cs = 0x001DDC10;   // size 0x4C
constexpr uint32_t setPos__7VehicleR8_fvector = 0x001DDC60;   // size 0x18
constexpr uint32_t updateControls__7VehicleR7GamePad = 0x001DDC78;   // size 0xA4
constexpr uint32_t updatePosition__7Vehicle = 0x001DDD20;   // size 0x74
constexpr uint32_t updateCollision__7Vehicle = 0x001DDD98;   // size 0x110
constexpr uint32_t updateSteering__7Vehiclef = 0x001DDEA8;   // size 0xC0
constexpr uint32_t updateThrottle__7Vehiclef = 0x001DDF68;   // size 0x68
constexpr uint32_t orientUpToNormal__7VehicleR8_fvectorf = 0x001DDFD0;   // size 0xE4
constexpr uint32_t x__16VehicleNavigatorR7VehicleP7PathNet = 0x001DE0B8;   // size 0x58
constexpr uint32_t init__16VehicleNavigator = 0x001DE110;   // size 0x8C
constexpr uint32_t update__16VehicleNavigatorR7GamePad = 0x001DE1A0;   // size 0x128
constexpr uint32_t updateSteering__16VehicleNavigatorR7GamePadff = 0x001DE2C8;   // size 0x12C
constexpr uint32_t disable__16VehicleNavigator = 0x001DE3F8;   // size 0x8
constexpr uint32_t wander__16VehicleNavigator = 0x001DE400;   // size 0x14
constexpr uint32_t seek__16VehicleNavigatorR8_fvectorf = 0x001DE418;   // size 0x1C
constexpr uint32_t target__16VehicleNavigatorR8_fvector = 0x001DE438;   // size 0x10
constexpr uint32_t arrive__16VehicleNavigatorR8_fvectorf = 0x001DE448;   // size 0x1C
constexpr uint32_t updateSeek__16VehicleNavigator = 0x001DE468;   // size 0x184
constexpr uint32_t targetPin__16VehicleNavigatorR8_fvector = 0x001DE5F0;   // size 0x84
constexpr uint32_t updateArrive__16VehicleNavigator = 0x001DE678;   // size 0x244
constexpr uint32_t flee__16VehicleNavigatorR8_fvectorff = 0x001DE8C0;   // size 0x1C
constexpr uint32_t updateFlee__16VehicleNavigator = 0x001DE8E0;   // size 0x100
constexpr uint32_t orientTo__16VehicleNavigatorR8_fvectorf = 0x001DE9E0;   // size 0xB0
constexpr uint32_t unalignedAvoid__16VehicleNavigator = 0x001DEA90;   // size 0x708
constexpr uint32_t pathContain__16VehicleNavigatorf = 0x001DF198;   // size 0x20C
constexpr uint32_t followPath__16VehicleNavigator = 0x001DF3A8;   // size 0x2D4
constexpr uint32_t nextPathPoint__16VehicleNavigator = 0x001DF680;   // size 0xB8
constexpr uint32_t initPathInfo__16VehicleNavigatorP8PathNodeP4Path = 0x001DF738;   // size 0xDC
constexpr uint32_t nextPathInfo__16VehicleNavigatorP4Path = 0x001DF818;   // size 0xDC
constexpr uint32_t inPath__16VehicleNavigatorR8_fvector = 0x001DF8F8;   // size 0xA0
constexpr uint32_t clampTargetToPath__16VehicleNavigatorR8_fvectorT1 = 0x001DF998;   // size 0x180
constexpr uint32_t flipPath__16VehicleNavigator = 0x001DFB18;   // size 0x7C
constexpr uint32_t getRandPath__16VehicleNavigator = 0x001DFB98;   // size 0xB8
constexpr uint32_t getSeekPath__16VehicleNavigatorR8_fvector = 0x001DFC50;   // size 0x160
constexpr uint32_t getFleePath__16VehicleNavigatorR8_fvector = 0x001DFDB0;   // size 0x160
constexpr uint32_t x__9WaterGridP9_hierhead = 0x001DFF10;   // size 0x170
constexpr uint32_t reset__9WaterGrid = 0x001E0080;   // size 0x3C
constexpr uint32_t update__9WaterGrid = 0x001E00C0;   // size 0x3E8
constexpr uint32_t disturb__9WaterGridR8_fvectorff = 0x001E04A8;   // size 0x1C0
constexpr uint32_t update__9WaterGridPv = 0x001E0668;   // size 0x1C
constexpr uint32_t x__Q26Weapon8Dynamics = 0x001E0688;   // size 0x40
constexpr uint32_t InitWeaponsBefore__7Weapons = 0x001E06C8;   // size 0xE4
constexpr uint32_t InitWeaponsAfter__7Weapons = 0x001E07B0;   // size 0x470
constexpr uint32_t UpdateWeapons__7Weapons = 0x001E0C20;   // size 0x190
constexpr uint32_t WeaponInRadius__7WeaponsP8_fvectorfPP6Weapon = 0x001E0DB0;   // size 0xB0
constexpr uint32_t UpdateRotation__6Weapon = 0x001E0E60;   // size 0x1BC
constexpr uint32_t UpdateLockOnTarget__6Weapon = 0x001E1020;   // size 0x2FC
constexpr uint32_t UpdateLockAheadOfMonster__6Weapon = 0x001E1320;   // size 0x35C
constexpr uint32_t UpdateLockVertical__6Weapon = 0x001E1680;   // size 0x2A8
constexpr uint32_t UpdateErraticity__6Weapon = 0x001E1928;   // size 0x278
constexpr uint32_t UpdateTranslation__6Weapon = 0x001E1BA0;   // size 0x328
constexpr uint32_t WeaponHD__6Weapon = 0x001E1EC8;   // size 0x660
constexpr uint32_t setCsTransform__6WeaponP8_fvectorRA3_A3_f = 0x001E2528;   // size 0x48
constexpr uint32_t ComputeReflection__6WeaponP8_fvectorT1b = 0x001E2570;   // size 0x1B8
constexpr uint32_t CreateStaticWeapon__7WeaponsQ26Weapon10WeaponTypeiiP8_fvectorT4P9_hierhead = 0x001E2728;   // size 0x1B0
constexpr uint32_t CreateStaticWeapon__7WeaponsGQ2t10LinkedList1ZP6Pickup8IteratoriiP8_fvector = 0x001E28D8;   // size 0x198
constexpr uint32_t CreateLavaBall__7WeaponsP8_fvectorN21ii = 0x001E2A70;   // size 0x304
constexpr uint32_t CalculateBallisticArc__6Weapon = 0x001E2D78;   // size 0x130
constexpr uint32_t InitWeapon__6Weapon = 0x001E2EA8;   // size 0x94C
constexpr uint32_t KillWeapon__6Weapon = 0x001E37F8;   // size 0x98
constexpr uint32_t ricochetParticlesOffSurface__FiP8_fvectorN31 = 0x001E3890;   // size 0x198
constexpr uint32_t DetonateWeapon__6WeaponR9_hdResult = 0x001E3A28;   // size 0x1220
constexpr uint32_t DispatchRangeBasedDamage__6Weapon = 0x001E4C48;   // size 0x138
constexpr uint32_t GetVel__6WeaponR8_fvector = 0x001E4D80;   // size 0x64
constexpr uint32_t DecrementWeaponCount__7WeaponsP7Monsteri = 0x001E4DE8;   // size 0x8
constexpr uint32_t AddWeaponEpNode__7WeaponsiP9_hierhead = 0x001E4DF0;   // size 0x170
constexpr uint32_t CreateFire__7WeaponsP8_fvectorfi = 0x001E4F60;   // size 0x17C
constexpr uint32_t UpdateFlames__7Weapons = 0x001E50E0;   // size 0x280
constexpr uint32_t x__do_global_dtors = 0x001E5360;   // size 0x54
constexpr uint32_t x__do_global_ctors = 0x001E53B8;   // size 0xB0
constexpr uint32_t x__main = 0x001E5468;   // size 0x20
constexpr uint32_t x__pure_virtual = 0x001E5488;   // size 0x8
constexpr uint32_t x__pack_f = 0x001E5490;   // size 0x10C
constexpr uint32_t x__unpack_f = 0x001E55A0;   // size 0x90
constexpr uint32_t x_fpadd_parts_001E5630 = 0x001E5630;   // size 0x234
constexpr uint32_t fpadd = 0x001E5868;   // size 0x58
constexpr uint32_t fpsub = 0x001E58C0;   // size 0x64
constexpr uint32_t fpmul = 0x001E5928;   // size 0x1F4
constexpr uint32_t fpdiv = 0x001E5B20;   // size 0x160
constexpr uint32_t x__fpcmp_parts_f = 0x001E5C80;   // size 0x114
constexpr uint32_t fpcmp = 0x001E5D98;   // size 0x4C
constexpr uint32_t sitofp = 0x001E5DE8;   // size 0xB8
constexpr uint32_t fptosi = 0x001E5EA0;   // size 0x8C
constexpr uint32_t fptoui = 0x001E5F30;   // size 0x98
constexpr uint32_t x__negsf2 = 0x001E5FC8;   // size 0x38
constexpr uint32_t x__make_fp = 0x001E6000;   // size 0x2C
constexpr uint32_t fptodp = 0x001E6030;   // size 0x40
constexpr uint32_t set_new_handler__FPFv_v = 0x001E6070;   // size 0x10
constexpr uint32_t x__vn__FUiPv = 0x001E6080;   // size 0x8
constexpr uint32_t x__nw__FUiPv = 0x001E6088;   // size 0x8
constexpr uint32_t what__C9bad_alloc = 0x001E6090;   // size 0xC
constexpr uint32_t x__tf9bad_alloc = 0x001E60A0;   // size 0x50
constexpr uint32_t x___9bad_alloc = 0x001E60F0;   // size 0x30
constexpr uint32_t x__builtin_delete = 0x001E6120;   // size 0x78
constexpr uint32_t x___9type_info = 0x001E6198;   // size 0x30
constexpr uint32_t x__eq__C9type_infoRC9type_info = 0x001E61C8;   // size 0x40
constexpr uint32_t x__rtti_class = 0x001E6208;   // size 0x24
constexpr uint32_t x__rtti_si = 0x001E6230;   // size 0x20
constexpr uint32_t x__rtti_user = 0x001E6250;   // size 0x1C
constexpr uint32_t dcast__C16__user_type_infoRC9type_infoiPvPC9type_infoT3 = 0x001E6270;   // size 0x2C
constexpr uint32_t dcast__C14__si_type_infoRC9type_infoiPvPC9type_infoT3 = 0x001E62A0;   // size 0x94
constexpr uint32_t dcast__C17__class_type_infoRC9type_infoiPvPC9type_infoT3 = 0x001E6338;   // size 0x1D8
constexpr uint32_t x__17__class_type_infoPCcPCQ217__class_type_info9base_infoUi = 0x001E6510;   // size 0x20
constexpr uint32_t x__tf17__class_type_info = 0x001E6530;   // size 0x50
constexpr uint32_t x___17__class_type_info = 0x001E6580;   // size 0x1C
constexpr uint32_t x__14__si_type_infoPCcRC16__user_type_info = 0x001E65A0;   // size 0x1C
constexpr uint32_t x__tf14__si_type_info = 0x001E65C0;   // size 0x50
constexpr uint32_t x___14__si_type_info = 0x001E6610;   // size 0x1C
constexpr uint32_t x__16__user_type_infoPCc = 0x001E6630;   // size 0x18
constexpr uint32_t x__tf16__user_type_info = 0x001E6648;   // size 0x50
constexpr uint32_t x___16__user_type_info = 0x001E6698;   // size 0x1C
constexpr uint32_t x___10bad_typeid = 0x001E66B8;   // size 0x30
constexpr uint32_t x__10bad_typeid = 0x001E66E8;   // size 0x14
constexpr uint32_t x__tf10bad_typeid = 0x001E6700;   // size 0x50
constexpr uint32_t x___8bad_cast = 0x001E6750;   // size 0x30
constexpr uint32_t x__8bad_cast = 0x001E6780;   // size 0x14
constexpr uint32_t x__tf8bad_cast = 0x001E6798;   // size 0x50
constexpr uint32_t x__ne__C9type_infoRC9type_info = 0x001E67E8;   // size 0x20
constexpr uint32_t name__C9type_info = 0x001E6808;   // size 0x8
constexpr uint32_t x__9type_infoPCc = 0x001E6810;   // size 0x18
constexpr uint32_t x__tf9type_info = 0x001E6828;   // size 0x40
constexpr uint32_t x__default_terminate = 0x001E6868;   // size 0x10
constexpr uint32_t x__terminate = 0x001E6878;   // size 0x24
constexpr uint32_t x__throw_type_match = 0x001E68A0;   // size 0x2C
constexpr uint32_t x__empty = 0x001E68D0;   // size 0x8
constexpr uint32_t new_eh_context = 0x001E68D8;   // size 0x54
constexpr uint32_t x__get_eh_context = 0x001E6930;   // size 0x24
constexpr uint32_t x__get_eh_info = 0x001E6958;   // size 0x28
constexpr uint32_t eh_context_initialize = 0x001E6980;   // size 0x28
constexpr uint32_t eh_context_static = 0x001E69A8;   // size 0x68
constexpr uint32_t x__get_dynamic_handler_chain = 0x001E6A10;   // size 0x28
constexpr uint32_t x__sjthrow = 0x001E6A38;   // size 0x178
constexpr uint32_t x__sjpopnthrow = 0x001E6BB0;   // size 0x118
constexpr uint32_t x__eh_rtime_match = 0x001E6CC8;   // size 0x48
constexpr uint32_t x__get_eh_table_version = 0x001E6D10;   // size 0x8
constexpr uint32_t x__get_eh_table_language = 0x001E6D18;   // size 0x8
constexpr uint32_t old_find_exception_handler = 0x001E6D20;   // size 0xD8
constexpr uint32_t find_exception_handler = 0x001E6DF8;   // size 0x144
constexpr uint32_t get_reg_addr = 0x001E6F40;   // size 0x88
constexpr uint32_t copy_reg = 0x001E6FC8;   // size 0x7C
constexpr uint32_t next_stack_level = 0x001E7048;   // size 0x8C
constexpr uint32_t x__unwinding_cleanup = 0x001E70D8;   // size 0x8
constexpr uint32_t throw_helper = 0x001E70E0;   // size 0x330
constexpr uint32_t x__throw = 0x001E7410;   // size 0x1DC
constexpr uint32_t x__rethrow = 0x001E75F0;   // size 0x1E4
constexpr uint32_t x__pack_d = 0x001E77D8;   // size 0x12C
constexpr uint32_t x__unpack_d = 0x001E7908;   // size 0x9C
constexpr uint32_t x_fpadd_parts_001E79A8 = 0x001E79A8;   // size 0x240
constexpr uint32_t dpadd = 0x001E7BE8;   // size 0x58
constexpr uint32_t dpsub = 0x001E7C40;   // size 0x64
constexpr uint32_t dpmul = 0x001E7CA8;   // size 0x2A8
constexpr uint32_t dpdiv = 0x001E7F50;   // size 0x168
constexpr uint32_t x__fpcmp_parts_d = 0x001E80B8;   // size 0x114
constexpr uint32_t dpcmp = 0x001E81D0;   // size 0x4C
constexpr uint32_t litodp = 0x001E8220;   // size 0xB8
constexpr uint32_t dptoli = 0x001E82D8;   // size 0x94
constexpr uint32_t dptoul = 0x001E8370;   // size 0xA0
constexpr uint32_t x__negdf2 = 0x001E8410;   // size 0x38
constexpr uint32_t x__make_dp = 0x001E8448;   // size 0x2C
constexpr uint32_t dptofp = 0x001E8478;   // size 0x54
constexpr uint32_t terminate__Fv = 0x001E84D0;   // size 0x18
constexpr uint32_t x__default_unexpected__Fv = 0x001E84E8;   // size 0x10
constexpr uint32_t set_terminate__FPFv_v = 0x001E84F8;   // size 0x10
constexpr uint32_t set_unexpected__FPFv_v = 0x001E8508;   // size 0x10
constexpr uint32_t unexpected__Fv = 0x001E8518;   // size 0x18
constexpr uint32_t x__cp_exception_info = 0x001E8530;   // size 0x24
constexpr uint32_t x__cp_eh_info = 0x001E8558;   // size 0x20
constexpr uint32_t x__start_cp_handler = 0x001E8578;   // size 0x38
constexpr uint32_t x__eh_alloc = 0x001E85B0;   // size 0x28
constexpr uint32_t x__eh_free = 0x001E85D8;   // size 0x1C
constexpr uint32_t x__cplus_type_matcher = 0x001E85F8;   // size 0x6C
constexpr uint32_t x__cp_push_exception = 0x001E8668;   // size 0x90
constexpr uint32_t x__cp_pop_exception = 0x001E86F8;   // size 0xC4
constexpr uint32_t x__uncatch_exception = 0x001E87C0;   // size 0x30
constexpr uint32_t x__check_eh_spec = 0x001E87F0;   // size 0x1B4
constexpr uint32_t x__throw_bad_cast = 0x001E89A8;   // size 0x80
constexpr uint32_t x__throw_bad_typeid = 0x001E8A28;   // size 0x80
constexpr uint32_t uncaught_exception__Fv = 0x001E8AA8;   // size 0x34
constexpr uint32_t what__C9exception = 0x001E8AE0;   // size 0x28
constexpr uint32_t x___13bad_exception = 0x001E8B08;   // size 0x30
constexpr uint32_t x__13bad_exception = 0x001E8B38;   // size 0x14
constexpr uint32_t x__tf13bad_exception = 0x001E8B50;   // size 0x50
constexpr uint32_t x___9exception = 0x001E8BA0;   // size 0x30
constexpr uint32_t x__9exception = 0x001E8BD0;   // size 0x14
constexpr uint32_t x__tf9exception = 0x001E8BE8;   // size 0x40
constexpr uint32_t decode_uleb128 = 0x001E8C28;   // size 0x44
constexpr uint32_t decode_sleb128 = 0x001E8C70;   // size 0x50
constexpr uint32_t fde_merge = 0x001E8CC0;   // size 0xFC
constexpr uint32_t end_fde_sort = 0x001E8DC0;   // size 0x264
constexpr uint32_t count_fdes = 0x001E9028;   // size 0x48
constexpr uint32_t add_fdes = 0x001E9070;   // size 0x88
constexpr uint32_t frame_init = 0x001E90F8;   // size 0x13C
constexpr uint32_t find_fde = 0x001E9238;   // size 0xE4
constexpr uint32_t extract_cie_info = 0x001E9320;   // size 0x104
constexpr uint32_t execute_cfa_insn = 0x001E9428;   // size 0x398
constexpr uint32_t x__register_frame_info = 0x001E97C0;   // size 0x28
constexpr uint32_t x__register_frame = 0x001E97E8;   // size 0x30
constexpr uint32_t x__register_frame_info_table = 0x001E9818;   // size 0x28
constexpr uint32_t x__register_frame_table = 0x001E9840;   // size 0x30
constexpr uint32_t x__deregister_frame_info = 0x001E9870;   // size 0x78
constexpr uint32_t x__deregister_frame = 0x001E98E8;   // size 0x20
constexpr uint32_t x__frame_state_for = 0x001E9908;   // size 0x1D8
constexpr uint32_t fde_split = 0x001E9AE0;   // size 0x174
constexpr uint32_t before__C9type_infoRC9type_info = 0x001E9C58;   // size 0x24
constexpr uint32_t x__throw_type_match_rtti = 0x001E9C80;   // size 0x4C8
constexpr uint32_t x__is_pointer__FPv = 0x001EA148;   // size 0x50
constexpr uint32_t x__rtti_ptr = 0x001EA198;   // size 0x20
constexpr uint32_t x__rtti_attr = 0x001EA1B8;   // size 0x24
constexpr uint32_t x__rtti_func = 0x001EA1E0;   // size 0x1C
constexpr uint32_t x__rtti_ptmf = 0x001EA200;   // size 0x1C
constexpr uint32_t x__rtti_ptmd = 0x001EA220;   // size 0x1C
constexpr uint32_t x__rtti_array = 0x001EA240;   // size 0x1C
constexpr uint32_t x__dynamic_cast = 0x001EA260;   // size 0xAC
constexpr uint32_t x__tfv = 0x001EA310;   // size 0x34
constexpr uint32_t x__tfx = 0x001EA348;   // size 0x34
constexpr uint32_t x__tfl = 0x001EA380;   // size 0x34
constexpr uint32_t x__tfi = 0x001EA3B8;   // size 0x34
constexpr uint32_t x__tfs = 0x001EA3F0;   // size 0x34
constexpr uint32_t x__tfb = 0x001EA428;   // size 0x34
constexpr uint32_t x__tfc = 0x001EA460;   // size 0x34
constexpr uint32_t x__tfw = 0x001EA498;   // size 0x34
constexpr uint32_t x__tfr = 0x001EA4D0;   // size 0x34
constexpr uint32_t x__tfd = 0x001EA508;   // size 0x34
constexpr uint32_t x__tff = 0x001EA540;   // size 0x34
constexpr uint32_t x__tfUi = 0x001EA578;   // size 0x34
constexpr uint32_t x__tfUl = 0x001EA5B0;   // size 0x34
constexpr uint32_t x__tfUx = 0x001EA5E8;   // size 0x34
constexpr uint32_t x__tfUs = 0x001EA620;   // size 0x34
constexpr uint32_t x__tfUc = 0x001EA658;   // size 0x34
constexpr uint32_t x__tfSc = 0x001EA690;   // size 0x34
constexpr uint32_t x__tf17__array_type_info = 0x001EA6C8;   // size 0x50
constexpr uint32_t x___17__array_type_info = 0x001EA718;   // size 0x1C
constexpr uint32_t x__tf16__ptmd_type_info = 0x001EA738;   // size 0x50
constexpr uint32_t x___16__ptmd_type_info = 0x001EA788;   // size 0x1C
constexpr uint32_t x__tf16__ptmf_type_info = 0x001EA7A8;   // size 0x50
constexpr uint32_t x___16__ptmf_type_info = 0x001EA7F8;   // size 0x1C
constexpr uint32_t x__tf16__func_type_info = 0x001EA818;   // size 0x50
constexpr uint32_t x___16__func_type_info = 0x001EA868;   // size 0x1C
constexpr uint32_t x__tf19__builtin_type_info = 0x001EA888;   // size 0x50
constexpr uint32_t x___19__builtin_type_info = 0x001EA8D8;   // size 0x1C
constexpr uint32_t x__tf16__attr_type_info = 0x001EA8F8;   // size 0x50
constexpr uint32_t x___16__attr_type_info = 0x001EA948;   // size 0x1C
constexpr uint32_t x__tf19__pointer_type_info = 0x001EA968;   // size 0x50
constexpr uint32_t x___19__pointer_type_info = 0x001EA9B8;   // size 0x1C
constexpr uint32_t x__muldi3 = 0x001EA9D8;   // size 0x60
constexpr uint32_t snd_StartSoundSystem = 0x001EAA38;   // size 0x240
constexpr uint32_t snd_FlushSoundCommands = 0x001EAC78;   // size 0x230
constexpr uint32_t snd_StopSoundSystem = 0x001EAEA8;   // size 0x2C
constexpr uint32_t snd_GetTick = 0x001EAED8;   // size 0x24
constexpr uint32_t snd_GetTick_CB = 0x001EAF00;   // size 0x2C
constexpr uint32_t snd_GotReturns = 0x001EAF30;   // size 0x64
constexpr uint32_t snd_PrepareReturnBuffer = 0x001EAF98;   // size 0x24
constexpr uint32_t snd_BankLoad = 0x001EAFC0;   // size 0x134
constexpr uint32_t snd_BankLoad_CB = 0x001EB0F8;   // size 0xDC
constexpr uint32_t snd_BankLoadByLoc = 0x001EB1D8;   // size 0x138
constexpr uint32_t snd_BankLoadByLoc_CB = 0x001EB310;   // size 0x134
constexpr uint32_t snd_MMDLoad = 0x001EB448;   // size 0x134
constexpr uint32_t snd_MMDLoad_CB = 0x001EB580;   // size 0xDC
constexpr uint32_t snd_MMDLoadByLoc = 0x001EB660;   // size 0x138
constexpr uint32_t snd_MMDLoadByLoc_CB = 0x001EB798;   // size 0x11C
constexpr uint32_t snd_ResolveBankXREFS = 0x001EB8B8;   // size 0x2C
constexpr uint32_t snd_UnloadBank = 0x001EB8E8;   // size 0x30
constexpr uint32_t snd_UnloadBank_CB = 0x001EB918;   // size 0x30
constexpr uint32_t snd_UnloadMMD = 0x001EB948;   // size 0x30
constexpr uint32_t snd_UnloadMMD_CB = 0x001EB978;   // size 0x30
constexpr uint32_t snd_SetMasterVolume = 0x001EB9A8;   // size 0x34
constexpr uint32_t snd_GetMasterVolume = 0x001EB9E0;   // size 0x28
constexpr uint32_t snd_GetMasterVolume_CB = 0x001EBA08;   // size 0x30
constexpr uint32_t snd_SetPlaybackMode = 0x001EBA38;   // size 0x30
constexpr uint32_t snd_GetPlaybackMode = 0x001EBA68;   // size 0x24
constexpr uint32_t snd_GetPlaybackMode_CB = 0x001EBA90;   // size 0x2C
constexpr uint32_t snd_SetMixerMode = 0x001EBAC0;   // size 0x34
constexpr uint32_t snd_SetPanMode = 0x001EBAF8;   // size 0x30
constexpr uint32_t snd_SetGroupVoiceRange = 0x001EBB28;   // size 0x3C
constexpr uint32_t snd_PlaySoundVolPanPMPB = 0x001EBB68;   // size 0x3C
constexpr uint32_t snd_PlaySoundByNameVolPanPMPB = 0x001EBBA8;   // size 0x40
constexpr uint32_t snd_PlaySoundVolPanPMPB_A = 0x001EBBE8;   // size 0x44
constexpr uint32_t snd_PlaySoundByNameVolPanPMPB_A = 0x001EBC30;   // size 0x4C
constexpr uint32_t snd_PlaySoundVolPanPMPB_CB = 0x001EBC80;   // size 0x48
constexpr uint32_t snd_PlaySoundByNameVolPanPMPB_CB = 0x001EBCC8;   // size 0x50
constexpr uint32_t snd_PlaySoundByStruct = 0x001EBD18;   // size 0x24
constexpr uint32_t snd_PlaySoundByStruct_A = 0x001EBD40;   // size 0x2C
constexpr uint32_t snd_PlaySoundByStruct_CB = 0x001EBD70;   // size 0x2C
constexpr uint32_t snd_StopSound = 0x001EBDA0;   // size 0x30
constexpr uint32_t snd_GetSoundUserData = 0x001EBDD0;   // size 0x3C
constexpr uint32_t snd_GetSoundUserData_CB = 0x001EBE10;   // size 0x44
constexpr uint32_t snd_PauseSound = 0x001EBE58;   // size 0x30
constexpr uint32_t snd_ContinueSound = 0x001EBE88;   // size 0x30
constexpr uint32_t snd_StopAllSounds = 0x001EBEB8;   // size 0x2C
constexpr uint32_t snd_PauseAllSoundsInGroup = 0x001EBEE8;   // size 0x30
constexpr uint32_t snd_ContinueAllSoundsInGroup = 0x001EBF18;   // size 0x30
constexpr uint32_t snd_SoundIsStillPlaying = 0x001EBF48;   // size 0x28
constexpr uint32_t snd_SoundIsStillPlaying_CB = 0x001EBF70;   // size 0x30
constexpr uint32_t snd_IsSoundALooper = 0x001EBFA0;   // size 0x2C
constexpr uint32_t snd_IsSoundALooper_CB = 0x001EBFD0;   // size 0x34
constexpr uint32_t snd_SetSoundVolPan = 0x001EC008;   // size 0x3C
constexpr uint32_t snd_GetSoundOriginalPitch = 0x001EC048;   // size 0x2C
constexpr uint32_t snd_GetSoundOriginalPitch_CB = 0x001EC078;   // size 0x34
constexpr uint32_t snd_GetSoundCurrentPitch = 0x001EC0B0;   // size 0x28
constexpr uint32_t snd_GetSoundCurrentPitch_CB = 0x001EC0D8;   // size 0x30
constexpr uint32_t snd_SetSoundPitch = 0x001EC108;   // size 0x34
constexpr uint32_t snd_SetSoundPitchBend = 0x001EC140;   // size 0x34
constexpr uint32_t snd_SetSoundPitchModifier = 0x001EC178;   // size 0x34
constexpr uint32_t snd_SetSoundParams = 0x001EC1B0;   // size 0x3C
constexpr uint32_t snd_SetSoundParams_A = 0x001EC1F0;   // size 0x20
constexpr uint32_t snd_SetSoundParams_CB = 0x001EC210;   // size 0x48
constexpr uint32_t snd_AutoVol = 0x001EC258;   // size 0x3C
constexpr uint32_t snd_AutoPan = 0x001EC298;   // size 0x40
constexpr uint32_t snd_AutoPitch = 0x001EC2D8;   // size 0x3C
constexpr uint32_t snd_AutoPitchBend = 0x001EC318;   // size 0x3C
constexpr uint32_t snd_SetMIDIRegister = 0x001EC358;   // size 0x3C
constexpr uint32_t snd_GetMIDIRegister = 0x001EC398;   // size 0x2C
constexpr uint32_t snd_GetMIDIRegister_CB = 0x001EC3C8;   // size 0x34
constexpr uint32_t snd_SetGlobalExcite = 0x001EC400;   // size 0x30
constexpr uint32_t snd_GetGlobalExcite = 0x001EC430;   // size 0x24
constexpr uint32_t snd_GetGlobalExcite_CB = 0x001EC458;   // size 0x2C
constexpr uint32_t snd_GetVoiceAllocation = 0x001EC488;   // size 0x28
constexpr uint32_t snd_GetVoiceAllocation_CB = 0x001EC4B0;   // size 0x30
constexpr uint32_t snd_SendIOPCommandAndWait = 0x001EC4E0;   // size 0x1A0
constexpr uint32_t snd_SendIOPCommandNoWait = 0x001EC680;   // size 0x2D4
constexpr uint32_t snd_PostMessage = 0x001EC958;   // size 0x40
constexpr uint32_t snd_SendCurrentBatch = 0x001EC998;   // size 0x13C
constexpr uint32_t snd_StartCaching = 0x001ECAD8;   // size 0x10
constexpr uint32_t snd_SendCache = 0x001ECAE8;   // size 0x20
constexpr uint32_t snd_InitVAGStreamingEx = 0x001ECB08;   // size 0xC0
constexpr uint32_t snd_StopVAGStream = 0x001ECBC8;   // size 0x30
constexpr uint32_t snd_StopAllStreams = 0x001ECBF8;   // size 0x2C
constexpr uint32_t snd_CloseVAGStreaming = 0x001ECC28;   // size 0x78
constexpr uint32_t snd_PlayVAGStreamByLoc = 0x001ECCA0;   // size 0x60
constexpr uint32_t snd_PlayVAGStreamByLoc_A = 0x001ECD00;   // size 0x28
constexpr uint32_t snd_PlayVAGStreamByLoc_CB = 0x001ECD28;   // size 0x64
constexpr uint32_t snd_PauseVAGStream = 0x001ECD90;   // size 0x30
constexpr uint32_t snd_ContinueVAGStream = 0x001ECDC0;   // size 0x30
constexpr uint32_t snd_GetVAGStreamQueueCount = 0x001ECDF0;   // size 0x28
constexpr uint32_t snd_GetVAGStreamQueueCount_CB = 0x001ECE18;   // size 0x30
constexpr uint32_t snd_GetVAGStreamLoc = 0x001ECE48;   // size 0x28
constexpr uint32_t snd_GetVAGStreamLoc_CB = 0x001ECE70;   // size 0x30
constexpr uint32_t snd_GetVAGStreamTimeRemaining = 0x001ECEA0;   // size 0x28
constexpr uint32_t snd_GetVAGStreamTimeRemaining_CB = 0x001ECEC8;   // size 0x30
constexpr uint32_t snd_GetVAGStreamTotalTime = 0x001ECEF8;   // size 0x28
constexpr uint32_t snd_GetVAGStreamTotalTime_CB = 0x001ECF20;   // size 0x30
constexpr uint32_t snd_IsVAGStreamBuffered = 0x001ECF50;   // size 0x28
constexpr uint32_t snd_IsVAGStreamBuffered_CB = 0x001ECF78;   // size 0x30
constexpr uint32_t snd_StreamSafeCheckCDIdle = 0x001ECFA8;   // size 0x28
constexpr uint32_t snd_StreamSafeCheckCDIdle_CB = 0x001ECFD0;   // size 0x30
constexpr uint32_t snd_StreamSafeCdRead = 0x001ED000;   // size 0xBC
constexpr uint32_t snd_StreamSafeCdSync = 0x001ED0C0;   // size 0xAC
constexpr uint32_t snd_StreamSafeCdBreak = 0x001ED170;   // size 0x48
constexpr uint32_t snd_StreamSafeCdGetError = 0x001ED1B8;   // size 0x34
constexpr uint32_t snd_StreamSafeCdCallback = 0x001ED1F0;   // size 0x38
constexpr uint32_t snd_SetReverbType = 0x001ED228;   // size 0x34
constexpr uint32_t snd_SetReverbDepth = 0x001ED260;   // size 0x3C
constexpr uint32_t snd_SetReverbEx = 0x001ED2A0;   // size 0x40
constexpr uint32_t snd_PreAllocReverbWorkArea = 0x001ED2E0;   // size 0x34
constexpr uint32_t snd_AutoReverb = 0x001ED318;   // size 0x3C
constexpr uint32_t snd_LockVoiceAllocator = 0x001ED358;   // size 0x24
constexpr uint32_t snd_ExternVoiceAlloc = 0x001ED380;   // size 0x2C
constexpr uint32_t snd_ExternVoiceFree = 0x001ED3B0;   // size 0x30
constexpr uint32_t snd_UnlockVoiceAllocator = 0x001ED3E0;   // size 0x2C
constexpr uint32_t snd_SRAMMalloc = 0x001ED410;   // size 0x28
constexpr uint32_t snd_SRAMMarkUsed = 0x001ED438;   // size 0x2C
constexpr uint32_t snd_SRAMFree = 0x001ED468;   // size 0x34
constexpr uint32_t snd_SRAMGetFreeMem = 0x001ED4A0;   // size 0x24
constexpr uint32_t snd_SRAMGetFreeMem_CB = 0x001ED4C8;   // size 0x2C
constexpr uint32_t snd_SRAMMaxFreeBlockSize = 0x001ED4F8;   // size 0x24
constexpr uint32_t snd_SRAMMaxFreeBlockSize_CB = 0x001ED520;   // size 0x2C
constexpr uint32_t snd_InitMovieSound = 0x001ED550;   // size 0x34
constexpr uint32_t snd_ResetMovieSound = 0x001ED588;   // size 0x24
constexpr uint32_t snd_PauseMovieSound = 0x001ED5B0;   // size 0x24
constexpr uint32_t snd_CloseMovieSound = 0x001ED5D8;   // size 0x24
constexpr uint32_t snd_StartMovieSound = 0x001ED600;   // size 0x34
constexpr uint32_t snd_GetTransStatus = 0x001ED638;   // size 0x24
constexpr uint32_t snd_GetLastLoadError = 0x001ED660;   // size 0x24
constexpr uint32_t snd_DoExternCall = 0x001ED688;   // size 0x40
constexpr uint32_t snd_DoExternCall_A = 0x001ED6C8;   // size 0x48
constexpr uint32_t snd_DoExternCall_CB = 0x001ED710;   // size 0x50
constexpr uint32_t snd_GetDopplerPitchMod = 0x001ED760;   // size 0x24
constexpr uint32_t init__12MotionBlurAAiff = 0x001ED788;   // size 0x474
constexpr uint32_t setAlpha__12MotionBlurAAfi = 0x001EDC00;   // size 0x90
constexpr uint32_t setEnable__12MotionBlurAAb = 0x001EDC90;   // size 0x8
constexpr uint32_t getEnable__12MotionBlurAA = 0x001EDC98;   // size 0x8
constexpr uint32_t setEnlarge__12MotionBlurAAfi = 0x001EDCA0;   // size 0x35C
constexpr uint32_t getEnlarge__12MotionBlurAA = 0x001EE000;   // size 0x10
constexpr uint32_t setIntensity__12MotionBlurAAi = 0x001EE010;   // size 0x8
constexpr uint32_t getIntensity__12MotionBlurAA = 0x001EE018;   // size 0x8
constexpr uint32_t drawOverlay__12MotionBlurAAiii = 0x001EE020;   // size 0xA8
constexpr uint32_t animationInitModifierBlends__FP17_animCharInstance = 0x001EE0C8;   // size 0x3C
constexpr uint32_t animationTransitionInto__FR11_animHandlefii = 0x001EE108;   // size 0x39C
constexpr uint32_t animationIsTransitioning__FR11_animHandle = 0x001EE4A8;   // size 0x44
constexpr uint32_t animationIsTargetAnim__FR11_animHandle = 0x001EE4F0;   // size 0x58
constexpr uint32_t animationAddBlend__FG11_animHandleP14_animBlendNodeiffsi = 0x001EE548;   // size 0x1BC
constexpr uint32_t animationRemoveBlend__FG11_animHandleP14_animBlendNodef = 0x001EE708;   // size 0x78
constexpr uint32_t animationAddProcBlend__FG11_animHandleP14_animBlendNodeifs = 0x001EE780;   // size 0x17C
constexpr uint32_t animationGetHandle__FP11_animHandleUiUiUi = 0x001EE900;   // size 0x13C
constexpr uint32_t animationGetNullHandle__FP11_animHandleUiUi = 0x001EEA40;   // size 0xD0
constexpr uint32_t animationGetCurrentTime__FG11_animHandle = 0x001EEB10;   // size 0xA8
constexpr uint32_t animationCalculatePlayTime__FG11_animHandle = 0x001EEBB8;   // size 0x6C
constexpr uint32_t animationResetCharacter__FR11_animHandle = 0x001EEC28;   // size 0x98
constexpr uint32_t animationStart__FG11_animHandleb = 0x001EECC0;   // size 0xCC
constexpr uint32_t animationStartReverse__FG11_animHandleb = 0x001EED90;   // size 0xCC
constexpr uint32_t animationSetToFrame__FG11_animHandlefi = 0x001EEE60;   // size 0xE8
constexpr uint32_t animationGetCurrentFrame__FG11_animHandle = 0x001EEF48;   // size 0x50
constexpr uint32_t animationSetToPercent__FG11_animHandlefi = 0x001EEF98;   // size 0xF0
constexpr uint32_t animationGetCurrentPercent__FG11_animHandle = 0x001EF088;   // size 0x8C
constexpr uint32_t animationPause__FG11_animHandle = 0x001EF118;   // size 0x74
constexpr uint32_t animationResume__FG11_animHandle = 0x001EF190;   // size 0xB0
constexpr uint32_t animationLoop__FG11_animHandleb = 0x001EF240;   // size 0x44
constexpr uint32_t animationSetSpeed__FG11_animHandlef = 0x001EF288;   // size 0xE8
constexpr uint32_t animationResetSpeed__FG11_animHandle = 0x001EF370;   // size 0x50
constexpr uint32_t animationSetToBeginning__FG11_animHandleb = 0x001EF3C0;   // size 0xB4
constexpr uint32_t animationSetToEnd__FG11_animHandleb = 0x001EF478;   // size 0xB4
constexpr uint32_t animationSetDirection__FG11_animHandleb = 0x001EF530;   // size 0x118
constexpr uint32_t animationGetDirection__FG11_animHandle = 0x001EF648;   // size 0x60
constexpr uint32_t animationGetLastFrame__FG11_animHandle = 0x001EF6A8;   // size 0x5C
constexpr uint32_t animationIsRunning__FG11_animHandle = 0x001EF708;   // size 0x50
constexpr uint32_t animationSetTotalRunFrames__FG11_animHandlef = 0x001EF758;   // size 0x68
constexpr uint32_t animationGetTotalRunFrames__FG11_animHandle = 0x001EF7C0;   // size 0x6C
constexpr uint32_t animationGetFrameAtPercent__FG11_animHandlef = 0x001EF830;   // size 0x64
constexpr uint32_t animationSetIterations__FG11_animHandleUs = 0x001EF898;   // size 0x44
constexpr uint32_t animationGetIterations__FG11_animHandle = 0x001EF8E0;   // size 0x4C
constexpr uint32_t animationCheckDirty__FP17_animCharInstanceP14_animtransform = 0x001EF930;   // size 0x19C
constexpr uint32_t animationClearDirty = 0x001EFAD0;   // size 0x1A8
constexpr uint32_t animationGetChannelIdx__FiP16AnimChannelIdMap = 0x001EFC78;   // size 0x74
constexpr uint32_t animationGetChannelVal__FiR11_animHandle = 0x001EFCF0;   // size 0x4C
constexpr uint32_t animationUpdateHandle__FR11_animHandle = 0x001EFD40;   // size 0x278
constexpr uint32_t animationUpdateFromControlNode__FP16_animControlNodeP17_animCharInstance = 0x001EFFB8;   // size 0x100
constexpr uint32_t animationUpdateActiveTree__FP9_hierheadP17_animCharInstance = 0x001F00B8;   // size 0x9C
constexpr uint32_t animationProcessActiveTree__FP9_hierheadP17_animCharInstance = 0x001F0158;   // size 0x168
constexpr uint32_t animationProcessTransitionBlend__FP14_animBlendNodeP17_animCharInstance = 0x001F02C0;   // size 0x4B0
constexpr uint32_t animationProcessStaticBlend__FP14_animBlendNodeP17_animCharInstance = 0x001F0770;   // size 0x2E4
constexpr uint32_t animationProcessOverrideBlend__FP14_animBlendNodeP17_animCharInstance = 0x001F0A58;   // size 0x218
constexpr uint32_t animationProcessAdditiveBlend__FP14_animBlendNodeP17_animCharInstance = 0x001F0C70;   // size 0x248
constexpr uint32_t animationProcessProceeduralBlend__FP14_animBlendNodeP17_animCharInstance = 0x001F0EB8;   // size 0x8
constexpr uint32_t animationEvaluateFromNode__FP14_animBlendNodeP17_animCharInstancePP21_animationOutputBlock = 0x001F0EC0;   // size 0x2B8
constexpr uint32_t animationEvaluateToNode__FP14_animBlendNodeP17_animCharInstancePP21_animationOutputBlock = 0x001F1178;   // size 0x334
constexpr uint32_t animationCollapseBlend__FP14_animBlendNodeP17_animCharInstance = 0x001F14B0;   // size 0x68
constexpr uint32_t animationCleanUpTree__FP9_hierheadP17_animCharInstance = 0x001F1518;   // size 0x110
constexpr uint32_t animationAddToActiveTree__FG11_animHandle = 0x001F1628;   // size 0xD0
constexpr uint32_t animationUpdate__FP10_animationP17_animCharInstance = 0x001F16F8;   // size 0x4C
constexpr uint32_t animationManager__FP8_animmgr = 0x001F1748;   // size 0x7C
constexpr uint32_t animationSetInstance__FP8_animmgri = 0x001F17C8;   // size 0x58
constexpr uint32_t animationRunGlobal__Fv = 0x001F1820;   // size 0x60
constexpr uint32_t animationInitialize__FP10_animationP17_animCharInstancei = 0x001F1880;   // size 0xF0
constexpr uint32_t animationInitGlobal__Fv = 0x001F1970;   // size 0x80
constexpr uint32_t animationSetGlobal__FP9_hierheadi = 0x001F19F0;   // size 0x128
constexpr uint32_t animationInitTweakers__Fv = 0x001F1B18;   // size 0x8
constexpr uint32_t animationUpdateTweakers__FiPv = 0x001F1B20;   // size 0x8
constexpr uint32_t x__10AnimPlayerP17_animCharInstance = 0x001F1B28;   // size 0x30
constexpr uint32_t UpdateAnimations__10AnimPlayer = 0x001F1B58;   // size 0x140
constexpr uint32_t AddBlendToActiveList__10AnimPlayerP14_animBlendNode = 0x001F1C98;   // size 0x1A8
constexpr uint32_t RemoveBlendFromActiveList__10AnimPlayerP14_animBlendNode = 0x001F1E40;   // size 0xD4
constexpr uint32_t IsBlendInActiveList__10AnimPlayerP14_animBlendNode = 0x001F1F18;   // size 0x94
constexpr uint32_t GetMainTreeNode__10AnimPlayer = 0x001F1FB0;   // size 0x8
constexpr uint32_t GetActiveList__10AnimPlayer = 0x001F1FB8;   // size 0x8
constexpr uint32_t GetNumNodesInActiveList__10AnimPlayer = 0x001F1FC0;   // size 0x8
constexpr uint32_t x__static_initialization_and_destruction_0_001F1FC8 = 0x001F1FC8;   // size 0x64
constexpr uint32_t x_GLOBAL__I_animationInitModifierBlends__FP17_animCharInstance = 0x001F2030;   // size 0x20
constexpr uint32_t x_GLOBAL__D_animationInitModifierBlends__FP17_animCharInstance = 0x001F2050;   // size 0x20
constexpr uint32_t evalStatic__FP15AnimCurveHeaderfPUs = 0x001F2070;   // size 0x8
constexpr uint32_t animCurveEvaluate__FP15AnimCurveHeaderfPUs = 0x001F2078;   // size 0x48
constexpr uint32_t binarySearch__H2Zt14AnimKeyStepped1ZfZf_Pt13AnimCurveData1ZX01fPUs_i = 0x001F20C0;   // size 0x138
constexpr uint32_t evaluateBetweenKeyframes__H1ZUc_Pt13AnimCurveData1Zt13AnimKeyLinear1ZX01fi_f = 0x001F21F8;   // size 0x7C
constexpr uint32_t animCurveEval__H1Zt13AnimKeyLinear1ZUc_Pt13AnimCurveData1ZX01fPUs_f = 0x001F2278;   // size 0x340
constexpr uint32_t binarySearch__H2Zt13AnimKeySmooth2ZUcZsZi_Pt13AnimCurveData1ZX01fPUs_i = 0x001F25B8;   // size 0x150
constexpr uint32_t evaluateBetweenKeyframes__H2ZUcZs_Pt13AnimCurveData1Zt13AnimKeySmooth2ZX01ZX11fi_f = 0x001F2708;   // size 0x1E0
constexpr uint32_t animCurveEval__H1Zt13AnimKeySmooth2ZUcZs_Pt13AnimCurveData1ZX01fPUs_f = 0x001F28E8;   // size 0x35C
constexpr uint32_t binarySearch__H2Zt14AnimKeyHermite2ZfZfZf_Pt13AnimCurveData1ZX01fPUs_i = 0x001F2C48;   // size 0x148
constexpr uint32_t evaluateBetweenKeyframes__H2ZfZf_Pt13AnimCurveData1Zt14AnimKeyHermite2ZX01ZX11fi_f = 0x001F2D90;   // size 0x4C
constexpr uint32_t animCurveEval__H1Zt14AnimKeyHermite2ZfZf_Pt13AnimCurveData1ZX01fPUs_f = 0x001F2DE0;   // size 0x290
constexpr uint32_t binarySearch__H2Zt13AnimKeyBroken2ZfZfZf_Pt13AnimCurveData1ZX01fPUs_i = 0x001F3070;   // size 0x138
constexpr uint32_t evaluateBetweenKeyframes__H2ZfZf_Pt13AnimCurveData1Zt13AnimKeyBroken2ZX01ZX11fi_f = 0x001F31A8;   // size 0x8C
constexpr uint32_t animCurveEval__H1Zt13AnimKeyBroken2ZfZf_Pt13AnimCurveData1ZX01fPUs_f = 0x001F3238;   // size 0x278
constexpr uint32_t binarySearch__H2Zt13AnimKeyBroken2ZUsZsZi_Pt13AnimCurveData1ZX01fPUs_i = 0x001F34B0;   // size 0x160
constexpr uint32_t evaluateBetweenKeyframes__H2ZUsZs_Pt13AnimCurveData1Zt13AnimKeyBroken2ZX01ZX11fi_f = 0x001F3610;   // size 0x1C8
constexpr uint32_t animCurveEval__H1Zt13AnimKeyBroken2ZUsZs_Pt13AnimCurveData1ZX01fPUs_f = 0x001F37D8;   // size 0x35C
constexpr uint32_t binarySearch__H2Zt13AnimKeyBroken2ZUcZsZi_Pt13AnimCurveData1ZX01fPUs_i = 0x001F3B38;   // size 0x160
constexpr uint32_t evaluateBetweenKeyframes__H2ZUcZs_Pt13AnimCurveData1Zt13AnimKeyBroken2ZX01ZX11fi_f = 0x001F3C98;   // size 0x1C4
constexpr uint32_t animCurveEval__H1Zt13AnimKeyBroken2ZUcZs_Pt13AnimCurveData1ZX01fPUs_f = 0x001F3E60;   // size 0x370
constexpr uint32_t evaluateBetweenKeyframes__H1Zf_Pt13AnimCurveData1Zt13AnimKeyLinear1ZX01fi_f = 0x001F41D0;   // size 0x58
constexpr uint32_t animCurveEval__H1Zt13AnimKeyLinear1Zf_Pt13AnimCurveData1ZX01fPUs_f = 0x001F4228;   // size 0x294
constexpr uint32_t evaluateBetweenKeyframes__H1ZUs_Pt13AnimCurveData1Zt13AnimKeyLinear1ZX01fi_f = 0x001F44C0;   // size 0x88
constexpr uint32_t animCurveEval__H1Zt13AnimKeyLinear1ZUs_Pt13AnimCurveData1ZX01fPUs_f = 0x001F4548;   // size 0x34C
constexpr uint32_t binarySearch__H2Zt13AnimKeySmooth2ZfZfZf_Pt13AnimCurveData1ZX01fPUs_i = 0x001F4898;   // size 0x148
constexpr uint32_t evaluateBetweenKeyframes__H2ZfZf_Pt13AnimCurveData1Zt13AnimKeySmooth2ZX01ZX11fi_f = 0x001F49E0;   // size 0xB4
constexpr uint32_t animCurveEval__H1Zt13AnimKeySmooth2ZfZf_Pt13AnimCurveData1ZX01fPUs_f = 0x001F4A98;   // size 0x290
constexpr uint32_t binarySearch__H2Zt13AnimKeySmooth2ZUsZsZi_Pt13AnimCurveData1ZX01fPUs_i = 0x001F4D28;   // size 0x168
constexpr uint32_t evaluateBetweenKeyframes__H2ZUsZs_Pt13AnimCurveData1Zt13AnimKeySmooth2ZX01ZX11fi_f = 0x001F4E90;   // size 0x1F8
constexpr uint32_t animCurveEval__H1Zt13AnimKeySmooth2ZUsZs_Pt13AnimCurveData1ZX01fPUs_f = 0x001F5088;   // size 0x370
constexpr uint32_t evaluateBetweenKeyframes__H1Zf_Pt13AnimCurveData1Zt14AnimKeyStepped1ZX01fi_f = 0x001F53F8;   // size 0x10
constexpr uint32_t animCurveEval__H1Zt14AnimKeyStepped1Zf_Pt13AnimCurveData1ZX01fPUs_f = 0x001F5408;   // size 0x278
constexpr uint32_t binarySearch__H2Zt14AnimKeyStepped1ZUsZi_Pt13AnimCurveData1ZX01fPUs_i = 0x001F5680;   // size 0x160
constexpr uint32_t evaluateBetweenKeyframes__H1ZUs_Pt13AnimCurveData1Zt14AnimKeyStepped1ZX01fi_f = 0x001F57E0;   // size 0x34
constexpr uint32_t animCurveEval__H1Zt14AnimKeyStepped1ZUs_Pt13AnimCurveData1ZX01fPUs_f = 0x001F5818;   // size 0x35C
constexpr uint32_t binarySearch__H2Zt14AnimKeyStepped1ZUcZi_Pt13AnimCurveData1ZX01fPUs_i = 0x001F5B78;   // size 0x160
constexpr uint32_t evaluateBetweenKeyframes__H1ZUc_Pt13AnimCurveData1Zt14AnimKeyStepped1ZX01fi_f = 0x001F5CD8;   // size 0x34
constexpr uint32_t animCurveEval__H1Zt14AnimKeyStepped1ZUc_Pt13AnimCurveData1ZX01fPUs_f = 0x001F5D10;   // size 0x35C
constexpr uint32_t x__static_initialization_and_destruction_0_001F6070 = 0x001F6070;   // size 0xEC
constexpr uint32_t x_GLOBAL__I_linearChar = 0x001F6160;   // size 0x20
constexpr uint32_t x__12AnimQueuable = 0x001F6180;   // size 0x18
constexpr uint32_t x__9AnimQueue = 0x001F6198;   // size 0x68
constexpr uint32_t Reset__9AnimQueue = 0x001F6200;   // size 0xC8
constexpr uint32_t Push__9AnimQueueP11_animHandleiii = 0x001F62C8;   // size 0x130
constexpr uint32_t Pop__9AnimQueue = 0x001F63F8;   // size 0x64
constexpr uint32_t taskUpdate__9AnimQueue = 0x001F6460;   // size 0x1B8
constexpr uint32_t RequestInterruption__9AnimQueue = 0x001F6618;   // size 0x3C
constexpr uint32_t taskUpdate__9AnimQueuePv = 0x001F6658;   // size 0x1C
constexpr uint32_t boundEulerAngles__FP8_fvector = 0x001F6678;   // size 0x34
constexpr uint32_t boundEulerAngle__FPf = 0x001F66B0;   // size 0xC8
constexpr uint32_t boundBetweenMinusMaxAndMax__FPff = 0x001F6778;   // size 0x44
constexpr uint32_t chainCreate__FffiUi11FxTextureIdf = 0x001F67C0;   // size 0x11C
constexpr uint32_t chainAddSegment__FP13_ParticleTypei = 0x001F68E0;   // size 0x50
constexpr uint32_t chainKill__FPP13_ParticleType = 0x001F6930;   // size 0x2C
constexpr uint32_t x__16SoundBankManager = 0x001F6960;   // size 0x4C
constexpr uint32_t loadSoundBank__16SoundBankManagerPPvPci = 0x001F69B0;   // size 0x188
constexpr uint32_t findFirstAvailableBankSlot__16SoundBankManager = 0x001F6B38;   // size 0x44
constexpr uint32_t loadRTSoundBank__16SoundBankManagerPPvPci = 0x001F6B80;   // size 0x15C
constexpr uint32_t unloadSoundBankBlocking__16SoundBankManagerPv = 0x001F6CE0;   // size 0xA8
constexpr uint32_t updateSoundBankManager__16SoundBankManager = 0x001F6D88;   // size 0x284
constexpr uint32_t unloadAllSoundBanks__16SoundBankManager = 0x001F7010;   // size 0xB8
constexpr uint32_t x__11LoopManager = 0x001F70C8;   // size 0x30
constexpr uint32_t addErrantLoop__11LoopManagerPUi = 0x001F70F8;   // size 0x80
constexpr uint32_t updateErrantLoops__11LoopManager = 0x001F7178;   // size 0xB4
constexpr uint32_t terminateLoop__11LoopManagerPUi = 0x001F7230;   // size 0x54
constexpr uint32_t soundPlayCB__FUiUl = 0x001F7288;   // size 0x10
constexpr uint32_t soundUnloadBankCB__FUiUl = 0x001F7298;   // size 0x10
constexpr uint32_t calculatePanAngle__FfP8_fvectorT1 = 0x001F72A8;   // size 0x188
constexpr uint32_t calculateRangedVolume__FP8_fvectorT0ii = 0x001F7430;   // size 0xA0
constexpr uint32_t init__6CsPool = 0x001F74D0;   // size 0xE4
constexpr uint32_t csActivate__6CsPool = 0x001F75B8;   // size 0x94
constexpr uint32_t csDeactivate__6CsPoolP3_cs = 0x001F7650;   // size 0xB8
constexpr uint32_t csHPActivate__6CsPool = 0x001F7708;   // size 0x94
constexpr uint32_t csHPDeactivate__6CsPoolP3_cs = 0x001F77A0;   // size 0xB8
constexpr uint32_t x__static_initialization_and_destruction_0_001F7858 = 0x001F7858;   // size 0x48
constexpr uint32_t x_GLOBAL__I__6CsPool_m_activeList = 0x001F78A0;   // size 0x20
constexpr uint32_t dbsPush__FPP9_hierheadP8_fvectorUiPA3_fP17_animCharInstance = 0x001F78C0;   // size 0x7C
constexpr uint32_t dbsPop__FP9_dbsstackPP9_hierhead = 0x001F7940;   // size 0xB8
constexpr uint32_t dbsTraverse__FPP9_hierheadPFP9_hierheadP8_fvectorPA3_f_vP8_fvector = 0x001F79F8;   // size 0xA14
constexpr uint32_t dbsRelocateViaPtrListFile__Fi10_vramAddrs = 0x001F8410;   // size 0x338
constexpr uint32_t dbsRelocateFileZero__F10_vramAddrsb = 0x001F8748;   // size 0x310
constexpr uint32_t dispCreate__FP8_Displayii = 0x001F8A58;   // size 0xAC
constexpr uint32_t dispSetTags__FP8_DisplayPUiPviiiiiii = 0x001F8B08;   // size 0x220
constexpr uint32_t setDMAscTag__FPUiUiPvUiUiUiUi = 0x001F8D28;   // size 0x54
constexpr uint32_t setGIFtag__FPUiUlUiUiUiUiUiUi = 0x001F8D80;   // size 0x54
constexpr uint32_t setGIFad__FPUiUiUl = 0x001F8DD8;   // size 0x30
constexpr uint32_t setBITBLTBUF__FPUiUiUiUi = 0x001F8E08;   // size 0x30
constexpr uint32_t setTRXPOS__FPUiUiUiUi = 0x001F8E38;   // size 0x30
constexpr uint32_t setTRXREG__FPUiUiUi = 0x001F8E68;   // size 0x2C
constexpr uint32_t setTRXDIR__FPUiUi = 0x001F8E98;   // size 0x24
constexpr uint32_t setPMODE__F12PMODESetting = 0x001F8EC0;   // size 0x130
constexpr uint32_t vblankHandler__Fi = 0x001F8FF0;   // size 0x144
constexpr uint32_t handler_endimage__Fi = 0x001F9138;   // size 0x38
constexpr uint32_t startDisplay__Fi = 0x001F9170;   // size 0x40
constexpr uint32_t endDisplay__Fv = 0x001F91B0;   // size 0xC
constexpr uint32_t blitLastMovieFrame__Fv = 0x001F91C0;   // size 0xD0
constexpr uint32_t electricArc__FP8_fvectorT0UiUiffffiUiUi = 0x001F9290;   // size 0x4B4
constexpr uint32_t electricArcOnSphere__FP8_fvectorfffPUiUiiiiT0 = 0x001F9748;   // size 0x364
constexpr uint32_t energyCrowdControl__FP8_fvector = 0x001F9AB0;   // size 0x494
constexpr uint32_t electricArcChainInit__FfffffPUiUiii = 0x001F9F48;   // size 0x1A4
constexpr uint32_t electricArcChainAddLink__FP13_ParticleTypeP8_fvectorbiT1T1 = 0x001FA0F0;   // size 0x134
constexpr uint32_t fileInitializeCd__Fv = 0x001FA228;   // size 0xD8
constexpr uint32_t fileReadf__FPcPv = 0x001FA300;   // size 0x164
constexpr uint32_t fileReada__FPcPv = 0x001FA468;   // size 0x160
constexpr uint32_t fileReads__FPcPvUi = 0x001FA5C8;   // size 0x1A4
constexpr uint32_t fileWritef__FPcPvi = 0x001FA770;   // size 0x80
constexpr uint32_t getTexFilesLoaded__Fv = 0x001FA7F0;   // size 0xC
constexpr uint32_t getNgpFilesLoaded__Fv = 0x001FA800;   // size 0xC
constexpr uint32_t getResFilesLoaded__Fv = 0x001FA810;   // size 0xC
constexpr uint32_t getNextNgpLoadAddr__Fv = 0x001FA820;   // size 0x1C
constexpr uint32_t getNextTexLoadAddr__Fv = 0x001FA840;   // size 0x1C
constexpr uint32_t getNextResLoadAddr__Fv = 0x001FA860;   // size 0x1C
constexpr uint32_t getNgpAddr__Fi = 0x001FA880;   // size 0x2C
constexpr uint32_t getTexAddr__Fi = 0x001FA8B0;   // size 0x2C
constexpr uint32_t getResAddr__Fi = 0x001FA8E0;   // size 0x2C
constexpr uint32_t getGenericName__Fi = 0x001FA910;   // size 0x30
constexpr uint32_t getMaxTexId__Fi = 0x001FA940;   // size 0x18
constexpr uint32_t getMaxTexAddr__Fi = 0x001FA958;   // size 0x18
constexpr uint32_t getMaxResAddr__Fi = 0x001FA970;   // size 0x18
constexpr uint32_t setMaxTexId__FiUs = 0x001FA988;   // size 0x18
constexpr uint32_t setMaxTexAddr__FiUs = 0x001FA9A0;   // size 0x18
constexpr uint32_t setMaxResAddr__FiUs = 0x001FA9B8;   // size 0x18
constexpr uint32_t getIdxOfAddr__FPc = 0x001FA9D0;   // size 0x4C
constexpr uint32_t getIdxOfResAddr__FPc = 0x001FAA20;   // size 0x4C
constexpr uint32_t getIdxOfName__FPc = 0x001FAA70;   // size 0x88
constexpr uint32_t fileAddNgpFile__FPci = 0x001FAAF8;   // size 0x9C
constexpr uint32_t fileAddTexFile__FPci = 0x001FAB98;   // size 0x98
constexpr uint32_t fileAddResFile__FPci = 0x001FAC30;   // size 0xEC
constexpr uint32_t fileAddName__FPc = 0x001FAD20;   // size 0x98
constexpr uint32_t fileOnlyNgpFile__FPci = 0x001FADB8;   // size 0x48
constexpr uint32_t fileOnlyTexFile__FPci = 0x001FAE00;   // size 0x48
constexpr uint32_t fileOnlyResFile__FPci = 0x001FAE48;   // size 0x48
constexpr uint32_t fileInitBeforeDbLoad__Fv = 0x001FAE90;   // size 0x34
constexpr uint32_t filePrintFileStatus__Fv = 0x001FAEC8;   // size 0x8
constexpr uint32_t fileMakeDirTree__Fv = 0x001FAED0;   // size 0x448
constexpr uint32_t fourCharsToInt__Fi = 0x001FB318;   // size 0x44
constexpr uint32_t twoCharsToShort__Fi = 0x001FB360;   // size 0x2C
constexpr uint32_t fileCdRead__FllPc = 0x001FB390;   // size 0x4C
constexpr uint32_t fileCdSearchFile__FP10sceCdlFILEPc = 0x001FB3E0;   // size 0x2D4
constexpr uint32_t fileHierAddrOfSect__FUi = 0x001FB6B8;   // size 0x4C
constexpr uint32_t filePrintCdFiles__FP13_cdFileSystemi = 0x001FB708;   // size 0x14C
constexpr uint32_t fileStringCompare__FPcT0 = 0x001FB858;   // size 0x50
constexpr uint32_t fileTrimPath__FPc = 0x001FB8A8;   // size 0x124
constexpr uint32_t fileGetTimeString__Fv = 0x001FB9D0;   // size 0x64
constexpr uint32_t fileAdjustFileName__FPcT0 = 0x001FBA38;   // size 0x1A0
constexpr uint32_t fogInit__Fv = 0x001FBBD8;   // size 0x160
constexpr uint32_t fogInitStats__Fv = 0x001FBD38;   // size 0x8
constexpr uint32_t fogSetChanged__Fv = 0x001FBD40;   // size 0x50
constexpr uint32_t fogUpdate__Fi = 0x001FBD90;   // size 0x2D8
constexpr uint32_t fogCalcFogValues__FfffPfT3 = 0x001FC068;   // size 0x30
constexpr uint32_t fogGetFogValues__FP6QwData = 0x001FC098;   // size 0x34
constexpr uint32_t fogGetFogParms__FPfN30PiN24 = 0x001FC0D0;   // size 0x58
constexpr uint32_t fogGetFogMinRange__Fv = 0x001FC128;   // size 0x8
constexpr uint32_t fogSetFogParms__Fffffiii = 0x001FC130;   // size 0x7C
constexpr uint32_t fogSetVolumeParms__Fiffffiii = 0x001FC1B0;   // size 0x8C
constexpr uint32_t fogGetFarClipRange__Fv = 0x001FC240;   // size 0x2C
constexpr uint32_t fogGetIsSlavedToBg__Fv = 0x001FC270;   // size 0x8
constexpr uint32_t fogGetTODFogValues__FP6QwData = 0x001FC278;   // size 0x34
constexpr uint32_t fogGetTODFogParms__FPfN30PiN24 = 0x001FC2B0;   // size 0x58
constexpr uint32_t fogSetTODFogParms__Fffffiii = 0x001FC308;   // size 0x7C
constexpr uint32_t fogAdjust__Ff = 0x001FC388;   // size 0x48
constexpr uint32_t fontInit__F10_vramAddrsi = 0x001FC3D0;   // size 0x1E0
constexpr uint32_t fontDmaFontData__Fv = 0x001FC5B0;   // size 0x18C
constexpr uint32_t fontSetColorGifTag__Fi = 0x001FC740;   // size 0x104
constexpr uint32_t fontDimColor__Fi = 0x001FC848;   // size 0x58
constexpr uint32_t fontSetHilightColor__Fi = 0x001FC8A0;   // size 0x48
constexpr uint32_t fontSetColor__Fiiiii = 0x001FC8E8;   // size 0x3C
constexpr uint32_t fontClearCutOut__Fv = 0x001FC928;   // size 0x48
constexpr uint32_t fontSetCutOut__Fi = 0x001FC970;   // size 0x54
constexpr uint32_t fontSetDefaultColor__Fi = 0x001FC9C8;   // size 0x28
constexpr uint32_t fontSetSize__Fii = 0x001FC9F0;   // size 0x50
constexpr uint32_t fontSetCharWidth__Fii = 0x001FCA40;   // size 0x1C
constexpr uint32_t fontSetDefaultSize__Fi = 0x001FCA60;   // size 0x64
constexpr uint32_t fontSetAccentCharacters__FiPcf = 0x001FCAC8;   // size 0x40
constexpr uint32_t fontSetSpacing__Fii = 0x001FCB08;   // size 0x28
constexpr uint32_t fontStringWidth__FiPc = 0x001FCB30;   // size 0x124
constexpr uint32_t fontSpritePrintXY__FiiiPc = 0x001FCC58;   // size 0x48
constexpr uint32_t fontSpritePrintRightXY__FiiiPc = 0x001FCCA0;   // size 0x90
constexpr uint32_t fontSpritePrintCenteredXY__FiiiPc = 0x001FCD30;   // size 0x98
constexpr uint32_t fontSpritePrintCentered__FiPc = 0x001FCDC8;   // size 0x70
constexpr uint32_t fontSetCharSizesToFitScreen__Fiiiff = 0x001FCE38;   // size 0xF4
constexpr uint32_t fontSetCharSizesInPixels__Fiiiii = 0x001FCF30;   // size 0x40
constexpr uint32_t fontSetCharSizesInSubPixels__Fiiiii = 0x001FCF70;   // size 0x30
constexpr uint32_t fontSetCursorAtColumnRow__Fiii = 0x001FCFA0;   // size 0x34
constexpr uint32_t fontSetCursorAtRowColumn__Fiii = 0x001FCFD8;   // size 0x34
constexpr uint32_t fontSetCursorAtPixel__Fiii = 0x001FD010;   // size 0x2C
constexpr uint32_t fontSetCursorAtSubPixel__Fiii = 0x001FD040;   // size 0x24
constexpr uint32_t fontSpritePrint__FiPc = 0x001FD068;   // size 0x1C4
constexpr uint32_t fontBuildPrim__FicP6QwData = 0x001FD230;   // size 0x228
constexpr uint32_t fontInitPacket__FP6QwData10_vramAddrs = 0x001FD458;   // size 0x16C
constexpr uint32_t fontSetTexId__FP9_hierhead = 0x001FD5C8;   // size 0x88
constexpr uint32_t frameInit__5GsPktii = 0x001FD650;   // size 0xB8
constexpr uint32_t AddCallToQueue__5GsPktP6QwData = 0x001FD708;   // size 0x40
constexpr uint32_t AddCallToLastQueue__5GsPktP6QwData = 0x001FD748;   // size 0x40
constexpr uint32_t DmaGsPktQueue__5GsPkt = 0x001FD788;   // size 0x70
constexpr uint32_t hdInit__Fv = 0x001FD7F8;   // size 0x70
constexpr uint32_t hdTestNode__FP9_hierheadP8_fvectorT1fPA3_A3_fP9_hdResulti = 0x001FD868;   // size 0x384
constexpr uint32_t hdCsTest__FP12_hdCsTestPtsP8_fvectorPA3_A3_fP9_hdResult = 0x001FDBF0;   // size 0x454
constexpr uint32_t hdSaveColGrid__FP18_HierCollisionGrid = 0x001FE048;   // size 0x20
constexpr uint32_t hdGetSphereGridBox__FfffPiN33 = 0x001FE068;   // size 0xC4
constexpr uint32_t hdReparentCsGrid__FP3_csP8_fvectorf = 0x001FE130;   // size 0x1E8
constexpr uint32_t hdReparentCsGrid__FP3_cs = 0x001FE318;   // size 0x4C
constexpr uint32_t hdRemoveCsFromGrid__FP3_cs = 0x001FE368;   // size 0xC4
constexpr uint32_t hdCsCollect__FP8_fvectorfUiUiPP3_csi = 0x001FE430;   // size 0x1B4
constexpr uint32_t hdCsCollect2D__FP8_fvectorfUiUiPP3_csi = 0x001FE5E8;   // size 0x1AC
constexpr uint32_t hdGetNextCollis__FRiP19_hdSphereBundleTestiP9_hdResult = 0x001FE798;   // size 0xAC
constexpr uint32_t hdGetNextCollis__FP9_hdResultiT0 = 0x001FE848;   // size 0x78
constexpr uint32_t hdIgnore__FP3_cs = 0x001FE8C0;   // size 0x38
constexpr uint32_t hdClearIgnore__Fv = 0x001FE8F8;   // size 0x44
constexpr uint32_t x__static_initialization_and_destruction_0_001FE940 = 0x001FE940;   // size 0x20
constexpr uint32_t x_GLOBAL__I_colGrid = 0x001FE960;   // size 0x20
constexpr uint32_t hdInitHatTimer__Fv = 0x001FE980;   // size 0x8
constexpr uint32_t hdSetDefaultHeight__Ff = 0x001FE988;   // size 0x8
constexpr uint32_t hdHatTest__FP8_fvectorT0fbf = 0x001FE990;   // size 0x1A0
constexpr uint32_t x__8HdItCell = 0x001FEB30;   // size 0x2C
constexpr uint32_t reset__8HdItCellii = 0x001FEB60;   // size 0x8C
constexpr uint32_t next__8HdItCell = 0x001FEBF0;   // size 0xF0
constexpr uint32_t x__12HdItGridLine = 0x001FECE0;   // size 0x3C
constexpr uint32_t reset__12HdItGridLineR8_fvectorT1f = 0x001FED20;   // size 0x29C
constexpr uint32_t incrementPath__12HdItGridLine = 0x001FEFC0;   // size 0x180
constexpr uint32_t next__12HdItGridLine = 0x001FF140;   // size 0xE8
constexpr uint32_t isDone__C12HdItGridLine = 0x001FF228;   // size 0x24
constexpr uint32_t drawGrid__12HdItGridLine = 0x001FF250;   // size 0x8
constexpr uint32_t x__11HdItGridBox = 0x001FF258;   // size 0x44
constexpr uint32_t reset__11HdItGridBoxR8_fvectorT1f = 0x001FF2A0;   // size 0x174
constexpr uint32_t incrementPath__11HdItGridBox = 0x001FF418;   // size 0x164
constexpr uint32_t next__11HdItGridBox = 0x001FF580;   // size 0x78
constexpr uint32_t hdLosCSTest__FP8_fvectorT0P9_hdResulti = 0x001FF5F8;   // size 0x34
constexpr uint32_t hdLosTest__FP8_fvectorT0P9_hdResultb = 0x001FF630;   // size 0x1C0
constexpr uint32_t hdPolyCheckBackface__Fb = 0x001FF7F0;   // size 0x8
constexpr uint32_t edgePunt__FR8_fvectorN20ffT0fP11ContactInfo = 0x001FF7F8;   // size 0x1B0
constexpr uint32_t hdSphereVsEdge__FP8_fvectorT0fT0T0fT0T0P11ContactInfo = 0x001FF9A8;   // size 0x3F4
constexpr uint32_t hdSphereVsPoly__FP8_fvectorN20fT0N20P11ContactInfof = 0x001FFDA0;   // size 0x62C
constexpr uint32_t hdSphereVsPolyCollis__FP8_fvectorT0fT0T0P24_hierpolycollisionobjectP9_hdResulti = 0x002003D0;   // size 0x5A0
constexpr uint32_t hdAABBVsAABB__FPfN30 = 0x00200970;   // size 0x9C
constexpr uint32_t hdSphereVsOBB__FP8_fvectorfT0PA3_fT0P11ContactInfo = 0x00200A10;   // size 0xF0
constexpr uint32_t hdSphereVsAABB__FP8_fvectorfT0T0P11ContactInfo = 0x00200B00;   // size 0x58
constexpr uint32_t hdSphereVsAABB__FPffT0T0 = 0x00200B58;   // size 0x70
constexpr uint32_t hdSphereVsAABB__FPffT0T0P11ContactInfo = 0x00200BC8;   // size 0x144
constexpr uint32_t hdSphereVsAABB__FPfT0fRfT3T0T0 = 0x00200D10;   // size 0x15C
constexpr uint32_t hdSphereVsSphere__FP8_fvectorfT0f = 0x00200E70;   // size 0x50
constexpr uint32_t hdSphereVsSphere__FP8_fvectorfT0fP11ContactInfo = 0x00200EC0;   // size 0xB0
constexpr uint32_t hdSphereVsHull__FP8_fvectorfPA3_fP6_planeiP11ContactInfo = 0x00200F70;   // size 0xA4
constexpr uint32_t hdSphereVsHull__FP8_fvectorfP6_planei = 0x00201018;   // size 0x60
constexpr uint32_t hdSphereVsHull__FP8_fvectorfP6_planeiP11ContactInfo = 0x00201078;   // size 0xB8
constexpr uint32_t hdSphereVsHull__FP8_fvectorT0fP6_planeiP11ContactInfoPfT6 = 0x00201130;   // size 0x2AC
constexpr uint32_t hdCapsuleVsCapsule__FR8_fvectorT0ffT0T0ffP11ContactInfo = 0x002013E0;   // size 0x308
constexpr uint32_t hdLineVsLine2D__FR8_fvectorT0RfT0T0T2 = 0x002016E8;   // size 0xC4
constexpr uint32_t hdSphereTest__FP8_fvectorfP9_hdResultUii = 0x002017B0;   // size 0x2C
constexpr uint32_t hdSphereTest__FP8_fvectorT0fP9_hdResultUii = 0x002017E0;   // size 0x1C8
constexpr uint32_t hdBundledSphereTest__FP19_hdSphereBundleTestii = 0x002019A8;   // size 0x3B4
constexpr uint32_t updateAnimTransMatrix__FP14_animtransformP17_animCharInstancePPA3_A3_f = 0x00201D60;   // size 0x234
constexpr uint32_t hdTraversePath__FP18_HierCollisionPathP15hdTraverseStateUs = 0x00201F98;   // size 0x32C
constexpr uint32_t hdTraverseHier__FP15hdTraverseStateUs = 0x002022C8;   // size 0x5C0
constexpr uint32_t hdInitTraverseHier__FPA3_A3_fPP9_hierhead = 0x00202888;   // size 0x84
constexpr uint32_t hierGetLastGSCtx__Fv = 0x00202910;   // size 0x8
constexpr uint32_t addSkeleton__FP13_hierSkelBonei = 0x00202918;   // size 0x18
constexpr uint32_t addSkelMat__FPA3_A3_fi = 0x00202930;   // size 0x38
constexpr uint32_t skelGetRoot__Fi = 0x00202968;   // size 0x24
constexpr uint32_t skelGetSkelMat__Fi = 0x00202990;   // size 0x20
constexpr uint32_t hier__Fii = 0x002029B0;   // size 0x52C
constexpr uint32_t hierTraceSky__Fi = 0x00202EE0;   // size 0x60
constexpr uint32_t hierTraceWorld__Fi = 0x00202F40;   // size 0x94
constexpr uint32_t hierTraceCsList__Fi = 0x00202FD8;   // size 0xE4
constexpr uint32_t hierTraceHPCsList__Fi = 0x002030C0;   // size 0xE4
constexpr uint32_t hierLoadVu1Ucode__Fv = 0x002031A8;   // size 0x74
constexpr uint32_t hierLoadVu0Ucode__Fv = 0x00203220;   // size 0x6C
constexpr uint32_t hierCsUpdate__FP3_csP9_worldctxP9_lightenvP8_fvector = 0x00203290;   // size 0xC4
constexpr uint32_t vu0MulMatrix3x3_1__FPA3_fN20 = 0x00203358;   // size 0x54
constexpr uint32_t hierPush__FPP9_hierheadiUiUiPUifP17_animCharInstanceii = 0x002033B0;   // size 0xEC
constexpr uint32_t hierTranslateSkel = 0x002034A0;   // size 0xDC
constexpr uint32_t hierRotateSkel = 0x00203580;   // size 0xBC
constexpr uint32_t hierAnimTransNode = 0x00203640;   // size 0x2C8
constexpr uint32_t hierStartRotMatPRH = 0x00203908;   // size 0x10
constexpr uint32_t hierGetRotMatPRH = 0x00203918;   // size 0x1C
constexpr uint32_t hierAttachPtNode = 0x00203938;   // size 0xC0
constexpr uint32_t hierScaleNode = 0x002039F8;   // size 0x240
constexpr uint32_t hierAnimScaleNode = 0x00203C38;   // size 0x3C4
constexpr uint32_t hierSkelBoneNode = 0x00204000;   // size 0x70
constexpr uint32_t hierLightNode = 0x00204070;   // size 0x234
constexpr uint32_t hierParticleNode = 0x002042A8;   // size 0x184
constexpr uint32_t hierShadow__Fii = 0x00204430;   // size 0x178
constexpr uint32_t hierCheckVifComplete__Fv = 0x002045A8;   // size 0x1C
constexpr uint32_t hierWaitForVif1__Fv = 0x002045C8;   // size 0x24
constexpr uint32_t vu0HierRotateAsm__FPA3_A3_fi = 0x002045F0;   // size 0x94
constexpr uint32_t hierGetRotatesAsm__Fi = 0x00204688;   // size 0x8C
constexpr uint32_t vu0FovTestResAsm__Fv = 0x00204718;   // size 0x24
constexpr uint32_t vu0MulEoForObj__FPA3_f = 0x00204740;   // size 0x28
constexpr uint32_t hierLod__FP8_hierlodfPP9_hierheadP10_hierstack = 0x00204768;   // size 0x188
constexpr uint32_t hierCtrlNode = 0x002048F0;   // size 0x29C
constexpr uint32_t hierInit__Fv = 0x00204B90;   // size 0x1D4
constexpr uint32_t hierDisableFov__Fv = 0x00204D68;   // size 0x8
constexpr uint32_t hierSetCsEpNode__FP3_csP9_hierhead = 0x00204D70;   // size 0x8
constexpr uint32_t hierSetCsDrawMe__FP3_csUc = 0x00204D78;   // size 0x14
constexpr uint32_t hierSetTraversalCallback__FPFP3_csUiUiRA3_A3_fP8_fvector_v = 0x00204D90;   // size 0x8
constexpr uint32_t hierInitCs__FP3_cs = 0x00204D98;   // size 0xCC
constexpr uint32_t hierSetSwitch__FP11_hierswitchi = 0x00204E68;   // size 0x30
constexpr uint32_t hierSetCameraLosPoint__FP8_fvectorii = 0x00204E98;   // size 0xA0
constexpr uint32_t hierSetLosPointsPerView__Fii = 0x00204F38;   // size 0x18
constexpr uint32_t hierSetCamera = 0x00204F50;   // size 0xE4
constexpr uint32_t hierKillLocator = 0x00205038;   // size 0x6C
constexpr uint32_t hierRegisterLocator__FUii = 0x002050A8;   // size 0x30
constexpr uint32_t hierClearLocatorListForReplay__Fv = 0x002050D8;   // size 0x68
constexpr uint32_t hierTraverseCSForLocators__FP3_csb = 0x00205140;   // size 0x140
constexpr uint32_t vu0LoadEoAsm__FP6QwData = 0x00205280;   // size 0xC
constexpr uint32_t vu0GetEoAsm__FP6QwData = 0x00205290;   // size 0xC
constexpr uint32_t texmInit__F10_vramAddrs = 0x002052A0;   // size 0xB8
constexpr uint32_t texmResInit__F10_vramAddrsP6QwData = 0x00205358;   // size 0xB0
constexpr uint32_t texmLoadResTexture__FP6QwData = 0x00205408;   // size 0x318
constexpr uint32_t texmLoadTexture__FP6QwData = 0x00205720;   // size 0x2B0
constexpr uint32_t texmSetupTexture__FiiiiiP6QwDataii = 0x002059D0;   // size 0x12C
constexpr uint32_t texmGetTextureDMATagAddress__FiPPUi = 0x00205B00;   // size 0x20
constexpr uint32_t texmPrepTexInfoZero__FP11tGS_DISPFB2 = 0x00205B20;   // size 0x1C
constexpr uint32_t texmParticleTexture__FiP6QwDataT1 = 0x00205B40;   // size 0x18C
constexpr uint32_t texmActivateTexture__FiP6QwDataP9Vu1DmaPkt = 0x00205CD0;   // size 0x110
constexpr uint32_t texmActivateBitBlitTexture__FiP6QwData = 0x00205DE0;   // size 0x194
constexpr uint32_t texmFindVramSpace__Fi = 0x00205F78;   // size 0x128
constexpr uint32_t texmGetMipLevels__Fi = 0x002060A0;   // size 0x20
constexpr uint32_t texmAllocateBinSpace__FP8_texbins = 0x002060C0;   // size 0x434
constexpr uint32_t texmAlreadyGotBin__FP8_texbinsUiPUs = 0x002064F8;   // size 0x34
constexpr uint32_t texmFindNextLargestTexSize__FP8_texbinsUiPUs = 0x00206530;   // size 0x48
constexpr uint32_t texmMatchingBlkSizeInFirstFile__FUiP8_texfile = 0x00206578;   // size 0x38
constexpr uint32_t texmRelocate__FPc = 0x002065B0;   // size 0x228
constexpr uint32_t texmResCheckForData__FPc = 0x002067D8;   // size 0x28
constexpr uint32_t texmResRelocate__FPc10_vramAddrs = 0x00206800;   // size 0x468
constexpr uint32_t texmCacheForAsm__FP6QwData = 0x00206C68;   // size 0x34
constexpr uint32_t texmRestoreCache__FP6QwData = 0x00206CA0;   // size 0xC
constexpr uint32_t texmSetMipMapTbl__FiP6QwDataP9Vu1DmaPkt = 0x00206CB0;   // size 0x230
constexpr uint32_t texmUpdateLastUsed__Fi = 0x00206EE0;   // size 0x50
constexpr uint32_t texmGetTextureAddrs = 0x00206F30;   // size 0x134
constexpr uint32_t hierCsUpdateAsm = 0x00207068;   // size 0x130
constexpr uint32_t hierTraverseAsm = 0x00207198;   // size 0x11E8
constexpr uint32_t hierCacheForAsm = 0x00208380;   // size 0x38
constexpr uint32_t hierCacheWorldMatsAsm = 0x002083B8;   // size 0x58
constexpr uint32_t pktAddVu1ObjAsm = 0x00208410;   // size 0x54C
constexpr uint32_t pktLinkAlphaObjAsm = 0x0020895C;   // size 0xC0
constexpr uint32_t psObjectCull = 0x00208A20;   // size 0x300
constexpr uint32_t psGetMatrix__FP6QwData = 0x00208D20;   // size 0x1C
constexpr uint32_t psFovTestResAsm__Fv = 0x00208D40;   // size 0xC
constexpr uint32_t psBldBlkRObjs = 0x00208D50;   // size 0xD8
constexpr uint32_t psBlockerAdd__FP3_csf = 0x00208E28;   // size 0xE0
constexpr uint32_t psBlockerSetShadowCs__FP19_pshadowblockerlistP3_cs = 0x00208F08;   // size 0x8
constexpr uint32_t psBlockerSet2ndCs__FP19_pshadowblockerlistP3_cs = 0x00208F10;   // size 0x8
constexpr uint32_t psBlockerSetLodRange__FP19_pshadowblockerlistf = 0x00208F18;   // size 0xC
constexpr uint32_t psBlockerSetWsOrigin__FP19_pshadowblockerlistP8_fvector = 0x00208F28;   // size 0x8
constexpr uint32_t psBlockerSetRadius__FP19_pshadowblockerlistPf = 0x00208F30;   // size 0x8
constexpr uint32_t psBlockerInit__Fv = 0x00208F38;   // size 0x128
constexpr uint32_t psBlockerRemove__FP3_cs = 0x00209060;   // size 0x3C
constexpr uint32_t psFindActiveBlockers__FP8_fvectori = 0x002090A0;   // size 0x29C
constexpr uint32_t psInitBlockerDMA__Fv = 0x00209340;   // size 0x20
constexpr uint32_t psBldBlockerPkt__FP8_fvectorii = 0x00209360;   // size 0x180
constexpr uint32_t psEndBlkObjs__Fi = 0x002094E0;   // size 0x94
constexpr uint32_t psEndBlkDma__Fv = 0x00209578;   // size 0x28
constexpr uint32_t psGetBlkPktStart__Fv = 0x002095A0;   // size 0xC
constexpr uint32_t psAttachDmaToAlpha__FP9Vu1DmaPkt = 0x002095B0;   // size 0x40
constexpr uint32_t psAttachDmaToMain__FP9Vu1DmaPkt = 0x002095F0;   // size 0x40
constexpr uint32_t psSetVU1ShadowMat__FPA3_fP6QwData = 0x00209630;   // size 0x90
constexpr uint32_t psSetLightDir__FP8_fvector = 0x002096C0;   // size 0xA4
constexpr uint32_t psGetLightDir__Fv = 0x00209768;   // size 0xC
constexpr uint32_t psIsBlockerCsActive__Fi = 0x00209778;   // size 0x3C
constexpr uint32_t psBlockerHas2ndCS__Fi = 0x002097B8;   // size 0x1C
constexpr uint32_t psGetBlockerLodRangeSq__Fi = 0x002097D8;   // size 0x1C
constexpr uint32_t psSetShadowOnOff__Fi = 0x002097F8;   // size 0x8
constexpr uint32_t psSetBlkLitPos__FP8_fvectori = 0x00209800;   // size 0x8C
constexpr uint32_t psSetCurrentBuffer__Fi = 0x00209890;   // size 0x8
constexpr uint32_t psGetCurrentBuffer__Fv = 0x00209898;   // size 0x8
constexpr uint32_t psInitFilterTexPkt__Fv = 0x002098A0;   // size 0x224
constexpr uint32_t psDrawFilterSprite__FP6QwDataiiiiiiiii = 0x00209AC8;   // size 0xF4
constexpr uint32_t psInitBorderPkt__Fv = 0x00209BC0;   // size 0xE0
constexpr uint32_t psDrawBorderSprite__FP6QwDataiiii = 0x00209CA0;   // size 0x88
constexpr uint32_t psVarInit__Fv = 0x00209D28;   // size 0x38
constexpr uint32_t psStoreQword__FP6QwDataT0 = 0x00209D60;   // size 0x10
constexpr uint32_t psFovTestAsm__FP8_fvectorP11_hierobject = 0x00209D70;   // size 0x18
constexpr uint32_t hierFlushObjQ__Fv = 0x00209D88;   // size 0xA0
constexpr uint32_t hierSetDmaIntHandler__Fv = 0x00209E28;   // size 0x40
constexpr uint32_t hierDmaHandler__Fi = 0x00209E68;   // size 0x50
constexpr uint32_t hoserInfoInit__FP9HoserInfo = 0x00209EB8;   // size 0x178
constexpr uint32_t hoserFindFreeBookmark__FP9HoserInfo = 0x0020A030;   // size 0x40
constexpr uint32_t hoserAddBookmark__FP9HoserInfoPA3_fP8_fvector = 0x0020A070;   // size 0xEC
constexpr uint32_t hoserDeleteBookmark__FP9HoserInfo = 0x0020A160;   // size 0x11C
constexpr uint32_t hoserDeleteAllBookmarks__FP9HoserInfo = 0x0020A280;   // size 0xDC
constexpr uint32_t hoserGotoBookmark__FP9HoserInfo = 0x0020A360;   // size 0x8
constexpr uint32_t hoserNextBookmark__FP9HoserInfo = 0x0020A368;   // size 0x30
constexpr uint32_t hoserPrevBookmark__FP9HoserInfo = 0x0020A398;   // size 0x30
constexpr uint32_t hoserClosestBookmark__FP9HoserInfoP8_fvector = 0x0020A3C8;   // size 0xA8
constexpr uint32_t hoserGetHoserInfo__Fv = 0x0020A470;   // size 0xC
constexpr uint32_t calcFrameBufferPositions__24IncognitoEntertainmentGSPQ224IncognitoEntertainmentGS11ieGsDBuffDcsssRsN25 = 0x0020A480;   // size 0x3C
constexpr uint32_t ieGsSetDefDBuffDc__24IncognitoEntertainmentGSPQ224IncognitoEntertainmentGS11ieGsDBuffDcssssss = 0x0020A4C0;   // size 0x4D0
constexpr uint32_t ieGsPutDispEnv__24IncognitoEntertainmentGSPQ224IncognitoEntertainmentGS11ieGsDBuffDcii = 0x0020A990;   // size 0x9C
constexpr uint32_t ieGsPutDrawEnv__24IncognitoEntertainmentGSPQ224IncognitoEntertainmentGS11ieGsDBuffDcii = 0x0020AA30;   // size 0x114
constexpr uint32_t ieGsSwapDBuffDc__24IncognitoEntertainmentGSPQ224IncognitoEntertainmentGS11ieGsDBuffDciii = 0x0020AB48;   // size 0x58
constexpr uint32_t ieGsSetHalfOffsetDc__24IncognitoEntertainmentGSiPQ224IncognitoEntertainmentGS11ieGsDBuffDcUiii = 0x0020ABA0;   // size 0x17C
constexpr uint32_t ieGsInitAA__24IncognitoEntertainmentGSb = 0x0020AD20;   // size 0x20
constexpr uint32_t ieGsTweakInit__24IncognitoEntertainmentGSv = 0x0020AD40;   // size 0x8
constexpr uint32_t tweakSetGsStuff__24IncognitoEntertainmentGSv = 0x0020AD48;   // size 0x8
constexpr uint32_t ieGs2CircuitOnOff__24IncognitoEntertainmentGSv = 0x0020AD50;   // size 0x8
constexpr uint32_t ieGsHalfTexelOffsetOnOff__24IncognitoEntertainmentGSv = 0x0020AD58;   // size 0x8
constexpr uint32_t inputInit__Fv = 0x0020AD60;   // size 0x348
constexpr uint32_t inputClearInputs__Fi = 0x0020B0A8;   // size 0x5C
constexpr uint32_t inputUpdate__Fv = 0x0020B108;   // size 0x260
constexpr uint32_t inputUpdateState__Fii = 0x0020B368;   // size 0x540
constexpr uint32_t inputSetActuator__FiP12ActuatorData = 0x0020B8A8;   // size 0x17C
constexpr uint32_t inputStopActuator__FiUc = 0x0020BA28;   // size 0xA4
constexpr uint32_t inputUsingActuator__Fi = 0x0020BAD0;   // size 0x18
constexpr uint32_t inputUseActuator__Fib = 0x0020BAE8;   // size 0x18
constexpr uint32_t inputAnyKey__Fi = 0x0020BB00;   // size 0x6C
constexpr uint32_t inputIsCtlAvailable__Fi = 0x0020BB70;   // size 0x18
constexpr uint32_t inputGetPadButtons__Fi = 0x0020BB88;   // size 0x3C
constexpr uint32_t inputGetPadType__Fi = 0x0020BBC8;   // size 0x18
constexpr uint32_t inputGetCtlPadState__Fi = 0x0020BBE0;   // size 0x18
constexpr uint32_t inputGetPadAnalog__Fi = 0x0020BBF8;   // size 0xA8
constexpr uint32_t inputSetInputMode__Fi = 0x0020BCA0;   // size 0x50
constexpr uint32_t inputGetInputMode__Fv = 0x0020BCF0;   // size 0x8
constexpr uint32_t inputSetButtonMap__Fii = 0x0020BCF8;   // size 0x18
constexpr uint32_t inputSetCButtonMap__Fiii = 0x0020BD10;   // size 0x24
constexpr uint32_t inputGetConfigButtons__Fi = 0x0020BD38;   // size 0x18
constexpr uint32_t inputGetPlayerPad__Fi = 0x0020BD50;   // size 0x18
constexpr uint32_t inputAreButtonsMapped__Fi = 0x0020BD68;   // size 0x18
constexpr uint32_t inputGetCButtonMap__Fii = 0x0020BD80;   // size 0x24
constexpr uint32_t inputGetAnyInput__Fi = 0x0020BDA8;   // size 0x5C
constexpr uint32_t inputGetInput__Fii = 0x0020BE08;   // size 0x230
constexpr uint32_t inputGetAnalogButton__Fii = 0x0020C038;   // size 0x180
constexpr uint32_t inputGetCtlPadAnalogAxis__Fii = 0x0020C1B8;   // size 0xC0
constexpr uint32_t inputGetShellAnalogInput__Fii = 0x0020C278;   // size 0x114
constexpr uint32_t inputFixAnalogValue__Fii = 0x0020C390;   // size 0x1D8
constexpr uint32_t inputScaleAnalogButton__Fiii = 0x0020C568;   // size 0x70
constexpr uint32_t lightInit__Fv = 0x0020C5D8;   // size 0xA8
constexpr uint32_t lightSetAmbient__Fifff = 0x0020C680;   // size 0x24
constexpr uint32_t lightGetEnv__Fi = 0x0020C6A8;   // size 0x18
constexpr uint32_t mathfMulVec__FPA3_fP8_fvectorT1 = 0x0020C6C0;   // size 0x44
constexpr uint32_t mathfMulVec4x4__FPA3_fP8_fvectorT1 = 0x0020C708;   // size 0x60
constexpr uint32_t mathfMulTransVecOld__FPA3_fP8_fvectorT1 = 0x0020C768;   // size 0x28
constexpr uint32_t mathfMulTransVec4x4__FPA3_fP8_fvectorT1 = 0x0020C790;   // size 0x30
constexpr uint32_t mathfMulMatrix__FPA3_fN20 = 0x0020C7C0;   // size 0x78
constexpr uint32_t mathfMulMatrixTP__FPA3_fN20 = 0x0020C838;   // size 0x88
constexpr uint32_t mathfMulMatrix3x3__FPA3_fN20 = 0x0020C8C0;   // size 0x58
constexpr uint32_t mathfMulMatrixTP3x3__FPA3_fN20 = 0x0020C918;   // size 0xB8
constexpr uint32_t mathfAddMatrix3x3__FPA3_fN20 = 0x0020C9D0;   // size 0x38
constexpr uint32_t mathfAddMatrixTP3x3__FPA3_fN20 = 0x0020CA08;   // size 0x5C
constexpr uint32_t mathfSubMatrix3x3__FPA3_fN20 = 0x0020CA68;   // size 0x38
constexpr uint32_t mathfSubMatrixTP3x3__FPA3_fN20 = 0x0020CAA0;   // size 0x5C
constexpr uint32_t mathfScaleMatrix3x3__FPA3_fT0f = 0x0020CB00;   // size 0x34
constexpr uint32_t mathfVectorCrossUp__FP8_fvectorT0 = 0x0020CB38;   // size 0x1C
constexpr uint32_t mathfVectorCrossRight__FP8_fvectorT0 = 0x0020CB58;   // size 0x1C
constexpr uint32_t mathfPlaneTest__FP6_planeP8_fvector = 0x0020CB78;   // size 0x38
constexpr uint32_t mathfNormalizeColumns__FPA3_f = 0x0020CBB0;   // size 0x94
constexpr uint32_t mathfTransposeMatrix__FPA3_fT0 = 0x0020CC48;   // size 0x54
constexpr uint32_t mathfInverseMatrix__FPA3_fT0 = 0x0020CCA0;   // size 0xB8
constexpr uint32_t mathfRotAxisToMatrix__FPA3_fP8_fvector = 0x0020CD58;   // size 0x5C
constexpr uint32_t mathfRotAxisToMatrix__FPA3_fP8_fvectorff = 0x0020CDB8;   // size 0xFC
constexpr uint32_t mathfMatricesToRotAxis__FP8_fvectorPA3_fT1 = 0x0020CEB8;   // size 0x364
constexpr uint32_t mathfMatrixToRotAxis__FP8_fvectorPA3_f = 0x0020D220;   // size 0x44
constexpr uint32_t mathfCopyVectorNotAligned__FP8_fvectorT0 = 0x0020D268;   // size 0x1C
constexpr uint32_t mathfScaleVectorNotAligned__FP8_fvectorT0f = 0x0020D288;   // size 0x28
constexpr uint32_t mathfCopyMatrixNotAligned__FPA3_fT0 = 0x0020D2B0;   // size 0x4C
constexpr uint32_t mathfCopyMatrixNoTrans__FPA3_fT0 = 0x0020D300;   // size 0x4C
constexpr uint32_t mathfUnitMatrix__FPA3_f = 0x0020D350;   // size 0x30
constexpr uint32_t mathfRotMatrixRPH__FPA3_fP8_fvector = 0x0020D380;   // size 0x3C
constexpr uint32_t mathfRotMatrixPRH__FPA3_fP8_fvector = 0x0020D3C0;   // size 0x12C
constexpr uint32_t mathfRotMatrixRPHNoTrans__FPA3_fP8_fvector = 0x0020D4F0;   // size 0x24
constexpr uint32_t mathfHPtoVector__FP8_fvectorff = 0x0020D518;   // size 0x80
constexpr uint32_t mathfViewScreenMatrix__FPA3_fN20fffffffffff = 0x0020D598;   // size 0x1FC
constexpr uint32_t mathfDumpMatrix__FPA3_f = 0x0020D798;   // size 0x158
constexpr uint32_t mathfManhatDist__FP8_fvectorT0 = 0x0020D8F0;   // size 0xC0
constexpr uint32_t mathfManhatDist2D__FP8_fvectorT0 = 0x0020D9B0;   // size 0x6C
constexpr uint32_t mathfRPHFromMatrix__FPA3_fP8_fvector = 0x0020DA20;   // size 0x80
constexpr uint32_t mathfPRHFromMatrix__FPA3_fP8_fvector = 0x0020DAA0;   // size 0x94
constexpr uint32_t mathfRotationFromPointToPoint__FP8_fvectorN20 = 0x0020DB38;   // size 0x2C
constexpr uint32_t mathfMatrixFromPointToPoint__FPA3_fP8_fvectorT1 = 0x0020DB68;   // size 0x78
constexpr uint32_t mathfRotationFromVector__FP8_fvectorT0 = 0x0020DBE0;   // size 0x64
constexpr uint32_t mathfHeadingFromPointToPoint__FP8_fvectorT0 = 0x0020DC48;   // size 0x2C
constexpr uint32_t mathfHeadingFromVector__FP8_fvector = 0x0020DC78;   // size 0x20
constexpr uint32_t mathfPitchFromPointToPoint__FP8_fvectorT0 = 0x0020DC98;   // size 0x2C
constexpr uint32_t mathfPitchFromVector__FP8_fvector = 0x0020DCC8;   // size 0x34
constexpr uint32_t mathfMatrixFromNormal__FPA3_fP8_fvector = 0x0020DD00;   // size 0xD4
constexpr uint32_t mathfDecomposeVector__FP8_fvectorN30 = 0x0020DDD8;   // size 0x50
constexpr uint32_t mathfClosestApproach__FR8_fvectorN30 = 0x0020DE28;   // size 0x64
constexpr uint32_t mathfClosestApproach__FR8_fvectorN30fRf = 0x0020DE90;   // size 0xB0
constexpr uint32_t mathfOrthonormalize__FPA3_f = 0x0020DF40;   // size 0xF0
constexpr uint32_t mathfOrthonormalizeOverTime__FPA3_fPi = 0x0020E030;   // size 0x178
constexpr uint32_t mathfRandfND__Fff = 0x0020E1A8;   // size 0x9C
constexpr uint32_t mathfRandND__Fii = 0x0020E248;   // size 0x60
constexpr uint32_t mathfRandf__Fff = 0x0020E2A8;   // size 0x7C
constexpr uint32_t mathfRand__Fii = 0x0020E328;   // size 0x5C
constexpr uint32_t mathfRandVector__FP8_fvectorf = 0x0020E388;   // size 0x148
constexpr uint32_t mathfTransform3dTo2d__FiP8_fvectorT1 = 0x0020E4D0;   // size 0xD4
constexpr uint32_t mathfRandInit__Fii = 0x0020E5A8;   // size 0x5C
constexpr uint32_t mathfRotAxisToQuaternion__FP8_fvectorT0f = 0x0020E608;   // size 0x78
constexpr uint32_t mathfRotAxisToQuaternion__FP8_fvectorT0 = 0x0020E680;   // size 0x1C
constexpr uint32_t mathfAxisAngleToQuaternion__FP8_fvectorT0 = 0x0020E6A0;   // size 0x80
constexpr uint32_t mathfRPHToQuaternion__FP8_fvectorT0 = 0x0020E720;   // size 0x118
constexpr uint32_t mathfRotationArcQuaternion__FP8_fvectorN20 = 0x0020E838;   // size 0x80
constexpr uint32_t mathfNormalizeQuaternion__FP8_fvectorT0 = 0x0020E8B8;   // size 0xB0
constexpr uint32_t mathfNormalizeQuaternionFromW__FP8_fvectorT0 = 0x0020E968;   // size 0xA8
constexpr uint32_t mathfConcatQuaternions__FP8_fvectorN20 = 0x0020EA10;   // size 0x34
constexpr uint32_t mathfDeConcatQuaternions__FP8_fvectorN20 = 0x0020EA48;   // size 0x6C
constexpr uint32_t mathfQuaternionToUpVector__FP8_fvectorT0 = 0x0020EAB8;   // size 0x74
constexpr uint32_t mathfQuaternionToForwardVector__FP8_fvectorT0 = 0x0020EB30;   // size 0x74
constexpr uint32_t mathfQuaternionToMatrix4x4__FPA3_fP8_fvector = 0x0020EBA8;   // size 0x80
constexpr uint32_t mathfMatrixToQuaternion__FP8_fvectorPA3_f = 0x0020EC28;   // size 0x34
constexpr uint32_t mathfUnitizeQuaternion__FP8_fvectorT0f = 0x0020EC60;   // size 0x34
constexpr uint32_t mathfQuaternionMulVec__FP8_fvectorN20 = 0x0020EC98;   // size 0x60
constexpr uint32_t mathfQuaternionMulTransVec__FP8_fvectorN20 = 0x0020ECF8;   // size 0x70
constexpr uint32_t mathfSlerpQuaternion__FP8_fvectorN20f = 0x0020ED68;   // size 0xBC
constexpr uint32_t mathfVectorMinMaxGetMac__FP8_fvectorT0 = 0x0020EE28;   // size 0x10
constexpr uint32_t mathfApproxSin__Ff = 0x0020EE38;   // size 0xEC
constexpr uint32_t mathfApproxSin2__Ff = 0x0020EF28;   // size 0x4C
constexpr uint32_t mathfApproxASin2__Ff = 0x0020EF78;   // size 0xBC
constexpr uint32_t mathfApproxCos__Ff = 0x0020F038;   // size 0x4C
constexpr uint32_t mathfApproxCos2__Ff = 0x0020F088;   // size 0x4C
constexpr uint32_t mathfApproxACos2__Ff = 0x0020F0D8;   // size 0x54
constexpr uint32_t x__static_initialization_and_destruction_0_0020F130 = 0x0020F130;   // size 0x30
constexpr uint32_t x_GLOBAL__I_gRand = 0x0020F160;   // size 0x20
constexpr uint32_t vu0MulVecAsm__FPA3_fP8_fvectorT1 = 0x0020F180;   // size 0x50
constexpr uint32_t vu0MulTransVecAsm__FPA3_fP9_fvector4T1 = 0x0020F1D0;   // size 0x28
constexpr uint32_t vu0MulMatrix3x3__FPA3_fN20 = 0x0020F1F8;   // size 0x60
constexpr uint32_t vu0MulMatrixTP3x3__FPA3_fN20 = 0x0020F258;   // size 0x88
constexpr uint32_t vu0CopyMatrix__FPA3_fT0 = 0x0020F2E0;   // size 0x30
constexpr uint32_t vu0UnitMatrix__FPA3_f = 0x0020F310;   // size 0x30
constexpr uint32_t x__11MemoryStackPvUi = 0x0020F340;   // size 0x34
constexpr uint32_t clear__11MemoryStack = 0x0020F378;   // size 0x4C
constexpr uint32_t x__static_initialization_and_destruction_0_0020F3C8 = 0x0020F3C8;   // size 0x38
constexpr uint32_t x_GLOBAL__I__11MemoryStack_global = 0x0020F400;   // size 0x20
constexpr uint32_t x_GLOBAL__D__11MemoryStack_global = 0x0020F420;   // size 0x20
constexpr uint32_t playMpegMovie__FPc = 0x0020F440;   // size 0x70
constexpr uint32_t playMpegMovieWithDB__FPcb = 0x0020F4B0;   // size 0x90
constexpr uint32_t updateMpegMovieDecoding__Fb = 0x0020F540;   // size 0x344
constexpr uint32_t readMpeg__FP8VideoDecP7ReadBufP7StrFile = 0x0020F888;   // size 0x32C
constexpr uint32_t switchThread__Fv = 0x0020FBB8;   // size 0x1C
constexpr uint32_t isAudioOK__Fv = 0x0020FBD8;   // size 0x30
constexpr uint32_t initAll__FPc = 0x0020FC08;   // size 0x270
constexpr uint32_t termAll__Fv = 0x0020FE78;   // size 0xC4
constexpr uint32_t defMain__Fv_0020FF40 = 0x0020FF40;   // size 0x24
constexpr uint32_t ErrMessage__FPc = 0x0020FF68;   // size 0x24
constexpr uint32_t proceedAudio__Fv = 0x0020FF90;   // size 0x24
constexpr uint32_t loadImage__FPUi = 0x0020FFB8;   // size 0x60
constexpr uint32_t getTimeStampAddr__Fv = 0x00210018;   // size 0xC
constexpr uint32_t getAudioBuffAddr__Fv = 0x00210028;   // size 0xC
constexpr uint32_t getViBufTagAddr__Fv = 0x00210038;   // size 0xC
constexpr uint32_t x__static_initialization_and_destruction_0_00210048 = 0x00210048;   // size 0x28
constexpr uint32_t x_GLOBAL__I_movieRequested = 0x00210070;   // size 0x20
constexpr uint32_t Play__6CMoviebPc = 0x00210090;   // size 0x2D0
constexpr uint32_t End__6CMovie = 0x00210360;   // size 0xA8
constexpr uint32_t Update__6CMovieb = 0x00210408;   // size 0x3C0
constexpr uint32_t moviesReadyScreenObject__FP11_hierobjectii = 0x002107C8;   // size 0x3C
constexpr uint32_t moviesSetScreenObject__Fv = 0x00210808;   // size 0x74
constexpr uint32_t moviesReleaseScreenObject__Fv = 0x00210880;   // size 0x50
constexpr uint32_t isAudioOK__6CMovie = 0x002108D0;   // size 0x30
constexpr uint32_t IsStarted__6CMovie = 0x00210900;   // size 0x8
constexpr uint32_t IsIdle__6CMovie = 0x00210908;   // size 0x8
constexpr uint32_t defMain__Fv_00210910 = 0x00210910;   // size 0x24
constexpr uint32_t realignToQuadword__FPUi = 0x00210938;   // size 0x3C
constexpr uint32_t GIF_SET_TAG__FPUiUiUiUiUiUiUiUii = 0x00210978;   // size 0x40
constexpr uint32_t setOpeningContext__FPUiUii = 0x002109B8;   // size 0x10C
constexpr uint32_t getClosingContextSize__FUib = 0x00210AC8;   // size 0x24
constexpr uint32_t setClosingContext__FPUiUiT0T0b = 0x00210AF0;   // size 0x15C
constexpr uint32_t setTextureContext__FPUiP13_ParticleType = 0x00210C50;   // size 0x78
constexpr uint32_t defaultContextInit__Fv = 0x00210CC8;   // size 0x15C
constexpr uint32_t loadColors__FPUiP13_ParticleTypePsii = 0x00210E28;   // size 0x12C
constexpr uint32_t loadBytes__FPUiPUciiUi = 0x00210F58;   // size 0x78
constexpr uint32_t linePacketInit__FP13_ParticleType = 0x00210FD0;   // size 0x214
constexpr uint32_t linePacketBuild__FP13_ParticleType = 0x002111E8;   // size 0x100
constexpr uint32_t spritePacketInit__FP13_ParticleType = 0x002112E8;   // size 0x438
constexpr uint32_t spritePacketBuild__FP13_ParticleType = 0x00211720;   // size 0x114
constexpr uint32_t stritePacketInit__FP13_ParticleType = 0x00211838;   // size 0x1C0
constexpr uint32_t stritePacketBuild__FP13_ParticleType = 0x002119F8;   // size 0xD0
constexpr uint32_t texSpritePacketInit__FP13_ParticleType = 0x00211AC8;   // size 0x38
constexpr uint32_t texSpritePacketBuild__FP13_ParticleType = 0x00211B00;   // size 0x608
constexpr uint32_t fragPacketInit__FP13_ParticleType = 0x00212108;   // size 0x284
constexpr uint32_t fragPacketBuild__FP13_ParticleType = 0x00212390;   // size 0xE0
constexpr uint32_t kitePacketInit__FP13_ParticleType = 0x00212470;   // size 0x214
constexpr uint32_t kitePacketBuild__FP13_ParticleType = 0x00212688;   // size 0xCC
constexpr uint32_t stripePacketInit__FP13_ParticleType = 0x00212758;   // size 0x32C
constexpr uint32_t stripePacketBuild__FP13_ParticleType = 0x00212A88;   // size 0x1B0
constexpr uint32_t sprinePacketInit__FP13_ParticleType = 0x00212C38;   // size 0x450
constexpr uint32_t sprinePacketBuild__FP13_ParticleType = 0x00213088;   // size 0x108
constexpr uint32_t chainPacketInit__FP13_ParticleTypeff = 0x00213190;   // size 0x170
constexpr uint32_t chainPacketBuild__FP13_ParticleTypei = 0x00213300;   // size 0x9C
constexpr uint32_t lensFlarePacketInit__FP13_ParticleTypei = 0x002133A0;   // size 0x6F4
constexpr uint32_t lensFlarePacketBuild__FP13_ParticleTypei = 0x00213A98;   // size 0x12C
constexpr uint32_t sweepTrailKillStaleHandle__Fi = 0x00213BC8;   // size 0x58
constexpr uint32_t sweepTrailPacketInit__FP13_ParticleType = 0x00213C20;   // size 0x1A0
constexpr uint32_t sweepTrailPacketBuild__FP13_ParticleType = 0x00213DC0;   // size 0x1D8
constexpr uint32_t dustTrailPacketInit__FP13_ParticleType = 0x00213F98;   // size 0x24C
constexpr uint32_t dustTrailPacketBuild__FP13_ParticleType = 0x002141E8;   // size 0x108
constexpr uint32_t electricArcPacketInit__FP13_ParticleType = 0x002142F0;   // size 0x1D0
constexpr uint32_t electricArcPacketBuild__FP13_ParticleType = 0x002144C0;   // size 0x8C
constexpr uint32_t electricArcPacketUpdate__FP13_ParticleTypefffffPUiUi = 0x00214550;   // size 0xD0
constexpr uint32_t wakePacketInit__FP13_ParticleType = 0x00214620;   // size 0x410
constexpr uint32_t wakePacketBuild__FP13_ParticleType = 0x00214A30;   // size 0x104
constexpr uint32_t particleActiveFires__FPP8_fvector = 0x00214B38;   // size 0x14
constexpr uint32_t verifyParticleBelongsToHandleAndViceVersa__FPi = 0x00214B50;   // size 0x38
constexpr uint32_t disconnectBackPointer__FP13_ParticleType = 0x00214B88;   // size 0x50
constexpr uint32_t particleParseTexture__FiP9_hierhead = 0x00214BD8;   // size 0xEC
constexpr uint32_t particleFOVcalcMaxRadius__FP16ParticleInfoTypeUi = 0x00214CC8;   // size 0x168
constexpr uint32_t particleLensFlareAddToDeBounceList__FP9HierLight = 0x00214E30;   // size 0x70
constexpr uint32_t particleGetLensFlareInfo__FPP17HierLensFlareListPii = 0x00214EA0;   // size 0x4C
constexpr uint32_t particleSetLensFlareNumEntries__Fii = 0x00214EF0;   // size 0x1C
constexpr uint32_t particleAddLensFlareToList__FP13_ParticleTypei = 0x00214F10;   // size 0x198
constexpr uint32_t particleLensFlareTestZbuffer__Fi = 0x002150A8;   // size 0x3BC
constexpr uint32_t particleLensFlareRender__FP6QwDataiPPUi = 0x00215468;   // size 0x4C4
constexpr uint32_t particleInit__Fv = 0x00215930;   // size 0x32C
constexpr uint32_t particleInitAfter__Fv = 0x00215C60;   // size 0x5C4
constexpr uint32_t particleMonsterArcSetNodePos__FP3_csUiUiP8_fvector = 0x00216228;   // size 0x318
constexpr uint32_t sweepTrailGetHandleFromNewId__FUi = 0x00216540;   // size 0xF4
constexpr uint32_t sweepTrailGetHandleFromId__FUi = 0x00216638;   // size 0x6C
constexpr uint32_t interpolateCurve__FP8_fvectorN30ii = 0x002166A8;   // size 0x144
constexpr uint32_t particleSweepTrail__FP8_fvectorT0Ui = 0x002167F0;   // size 0x3D4
constexpr uint32_t utilUpdateEmitterMatrices__FP11EmitterTypeP8_fvector = 0x00216BC8;   // size 0x180
constexpr uint32_t pedSplat__FP13_ParticleTypeP8_fvector = 0x00216D48;   // size 0xC4
constexpr uint32_t pedSplatEmit__FP13_ParticleType = 0x00216E10;   // size 0x84
constexpr uint32_t particleEmitBoundaryFx__FP13_ParticleType = 0x00216E98;   // size 0x1D0
constexpr uint32_t wakeEmit__FP13_ParticleTypeP8_fvectorT1bT3fi = 0x00217068;   // size 0x1B0
constexpr uint32_t particleEmit__FP13_ParticleTypeP8_fvectorT1 = 0x00217218;   // size 0xE44
constexpr uint32_t utilScaleFlame__FP13_ParticleTypef = 0x00218060;   // size 0xC4
constexpr uint32_t emitterMove__FP13_ParticleType = 0x00218128;   // size 0x37C
constexpr uint32_t arcMove__FP13_ParticleType = 0x002184A8;   // size 0x134
constexpr uint32_t dustMove__FP13_ParticleTypeP8_fvectorN21f = 0x002185E0;   // size 0x19C
constexpr uint32_t wakeAndKiteMove__FP13_ParticleTypeP8_fvectorN21fUi = 0x00218780;   // size 0x194
constexpr uint32_t particleMove__Fv = 0x00218918;   // size 0xCF0
constexpr uint32_t particleInitFx__Fif = 0x00219608;   // size 0x6A8
constexpr uint32_t particlePostInitFx__FP8_fvectorP13_ParticleTypef = 0x00219CB0;   // size 0x100
constexpr uint32_t initSelfAnimated__FP13_ParticleTypefPA3_ff = 0x00219DB0;   // size 0x80
constexpr uint32_t initRandomDirection__FP13_ParticleTypefPA3_f = 0x00219E30;   // size 0x68
constexpr uint32_t particleCreateFx__FP8_fvectorifPA3_ff = 0x00219E98;   // size 0x180
constexpr uint32_t particleCreateFx__FP8_fvectorPA3_fifT0T1bf = 0x0021A018;   // size 0x1A8
constexpr uint32_t particleCreateModeledFx__FP8_fvectorP19HierParticleEmitter = 0x0021A1C0;   // size 0x5A8
constexpr uint32_t particleScrapeSparks__FP8_fvectorN20 = 0x0021A768;   // size 0x318
constexpr uint32_t particleGetParticle__Fi = 0x0021AA80;   // size 0x24
constexpr uint32_t particleKillFx__FRi = 0x0021AAA8;   // size 0x48
constexpr uint32_t particleKillAllFx__Fv = 0x0021AAF0;   // size 0xC0
constexpr uint32_t particleDestroyEffect__FP13_ParticleType = 0x0021ABB0;   // size 0x128
constexpr uint32_t particleInitForReplay__Fv = 0x0021ACD8;   // size 0x178
constexpr uint32_t particleUpdatePickupFX__FP3_csi = 0x0021AE50;   // size 0x8
constexpr uint32_t particleCreateFlyingFrags__FP8_fvectorfii = 0x0021AE58;   // size 0xF4
constexpr uint32_t particleCreateFadingFlame__FP8_fvectoriT0f = 0x0021AF50;   // size 0x128
constexpr uint32_t particlePrepDraw__Fv = 0x0021B078;   // size 0x19C
constexpr uint32_t particleClearBuffers__Fv = 0x0021B218;   // size 0x160
constexpr uint32_t particleDraw__Fi = 0x0021B378;   // size 0x674
constexpr uint32_t pedsTurnOff__FP8_fvectorb = 0x0021B9F0;   // size 0x54
constexpr uint32_t stampPacketInit__FP13_ParticleType = 0x0021BA48;   // size 0x33C
constexpr uint32_t pedsCreate__FiffUiPP8_fvectorii = 0x0021BD88;   // size 0x204
constexpr uint32_t pedsUpdateFOVbox__FP13_ParticleType = 0x0021BF90;   // size 0xFC
constexpr uint32_t pedsKill__FPP13_ParticleType = 0x0021C090;   // size 0x48
constexpr uint32_t pktBldEnvMapDma = 0x0021C0D8;   // size 0x610
constexpr uint32_t pktAddVu1Tex__FP9Vu1DmaPktPUi = 0x0021C6E8;   // size 0x40
constexpr uint32_t pktAddPlaceHolderTex__FP9Vu1DmaPkt = 0x0021C728;   // size 0x20
constexpr uint32_t pktFlushVu1Objs__FP9Vu1DmaPkt = 0x0021C748;   // size 0x40
constexpr uint32_t pktLinkAlphaObj__FP9Vu1DmaPktP6QwDataf = 0x0021C788;   // size 0xB4
constexpr uint32_t pktInitVu1Dma__FP9Vu1DmaPktP6QwDataT1 = 0x0021C840;   // size 0x14
constexpr uint32_t pktInitReverseAlphaOT__FP9Vu1DmaPktP6QwDataPP6QwData = 0x0021C858;   // size 0x58
constexpr uint32_t pktAttachAlphaOT__FP9Vu1DmaPktT0 = 0x0021C8B0;   // size 0x14
constexpr uint32_t pktCacheForAsm__FP6QwData = 0x0021C8C8;   // size 0x29C
constexpr uint32_t pktAdjustLightRender__FiP6QwData = 0x0021CB68;   // size 0x48
constexpr uint32_t plightInit__Fv = 0x0021CBB0;   // size 0x30
constexpr uint32_t plightParaActivate__Fv = 0x0021CBE0;   // size 0xC
constexpr uint32_t plightParaActive__Fv = 0x0021CBF0;   // size 0x8
constexpr uint32_t plightParaDeactivate__Fv = 0x0021CBF8;   // size 0x8
constexpr uint32_t plightGetParaLight__Fv = 0x0021CC00;   // size 0xC
constexpr uint32_t plightUpdateParaLights__Fv = 0x0021CC10;   // size 0x278
constexpr uint32_t pLightCompSpecular__Fi = 0x0021CE88;   // size 0x130
constexpr uint32_t plightGetParaLight__FiPfN41 = 0x0021CFB8;   // size 0x6C
constexpr uint32_t plightSetParaLight__Fifffff = 0x0021D028;   // size 0x58
constexpr uint32_t plightSetTODParaLight__Fifffff = 0x0021D080;   // size 0x68
constexpr uint32_t plightUpdateLights__Fv = 0x0021D0E8;   // size 0x2C4
constexpr uint32_t pLightSetSpecularIntens__Ff = 0x0021D3B0;   // size 0x8
constexpr uint32_t Init__10PointLight = 0x0021D3B8;   // size 0x5C
constexpr uint32_t SetPos__10PointLightP8_fvector = 0x0021D418;   // size 0x1C
constexpr uint32_t SetPos__10PointLightfff = 0x0021D438;   // size 0x10
constexpr uint32_t CompIntensity__10PointLightf = 0x0021D448;   // size 0x24
constexpr uint32_t SetRange__10PointLightf = 0x0021D470;   // size 0x24
constexpr uint32_t CompIntensity__10PointLight = 0x0021D498;   // size 0x24
constexpr uint32_t SetColor__10PointLightfff = 0x0021D4C0;   // size 0x10
constexpr uint32_t Init__11PointLights = 0x0021D4D0;   // size 0x50
constexpr uint32_t Create__11PointLights = 0x0021D520;   // size 0x34
constexpr uint32_t GetPktPtr__11PointLights = 0x0021D558;   // size 0x8
constexpr uint32_t RenderObject__11PointLights = 0x0021D560;   // size 0x24
constexpr uint32_t IntersectObject__11PointLightsPA3_fP8_fvectorT2fT2P3_cs = 0x0021D588;   // size 0x25C
constexpr uint32_t GetClosestPts__11PointLightsi = 0x0021D7E8;   // size 0x23C
constexpr uint32_t init__9PointMassfP8_fvector = 0x0021DA28;   // size 0x38
constexpr uint32_t applyImpulse__9PointMassR8_fvector = 0x0021DA60;   // size 0x38
constexpr uint32_t applyDampeningForce__9PointMassf = 0x0021DA98;   // size 0x3C
constexpr uint32_t clampForce__9PointMassf = 0x0021DAD8;   // size 0x98
constexpr uint32_t resolveForcesEuler__9PointMassf = 0x0021DB70;   // size 0x68
constexpr uint32_t init__6Springfff = 0x0021DBD8;   // size 0x18
constexpr uint32_t applyForces__6Spring = 0x0021DBF0;   // size 0xF4
constexpr uint32_t init__12PointToolKiti = 0x0021DCE8;   // size 0x94
constexpr uint32_t loadPoints__12PointToolKitPcb = 0x0021DD80;   // size 0x160
constexpr uint32_t resetReticle__12PointToolKit = 0x0021DEE0;   // size 0x1C
constexpr uint32_t getPoint__12PointToolKiti = 0x0021DF00;   // size 0xC
constexpr uint32_t getPathType__12PointToolKitPQ212PointToolKit5PointT1 = 0x0021DF10;   // size 0x60
constexpr uint32_t hasPath__12PointToolKitPQ212PointToolKit5PointT1 = 0x0021DF70;   // size 0x64
constexpr uint32_t getPointIndex__12PointToolKitPQ212PointToolKit5Point = 0x0021DFD8;   // size 0xC
constexpr uint32_t x__tf12PointToolKit = 0x0021DFE8;   // size 0x3C
constexpr uint32_t x__12PointToolKit = 0x0021E028;   // size 0x14
constexpr uint32_t setPointHeight__12PointToolKitf = 0x0021E040;   // size 0x18
constexpr uint32_t setPointWidth__12PointToolKitf = 0x0021E058;   // size 0x18
constexpr uint32_t setPointMoveStep__12PointToolKitf = 0x0021E070;   // size 0x8
constexpr uint32_t setReticleMoveStep__12PointToolKitf = 0x0021E078;   // size 0x8
constexpr uint32_t exportPoints__12PointToolKit = 0x0021E080;   // size 0x8
constexpr uint32_t getNumPoints__12PointToolKit = 0x0021E088;   // size 0x8
constexpr uint32_t videoCallback__FP7sceMpegP16sceMpegCbDataStrPv = 0x0021E090;   // size 0x128
constexpr uint32_t pcmCallback__FP7sceMpegP16sceMpegCbDataStrPv = 0x0021E1B8;   // size 0xD4
constexpr uint32_t copy2area__FPUciT0iT0iT0i = 0x0021E290;   // size 0x130
constexpr uint32_t readBufCreate__FP7ReadBufi = 0x0021E3C0;   // size 0x1C
constexpr uint32_t readBufDelete__FP7ReadBuf = 0x0021E3E0;   // size 0x8
constexpr uint32_t readBufBeginPut__FP7ReadBufPPUc = 0x0021E3E8;   // size 0x30
constexpr uint32_t readBufEndPut__FP7ReadBufi = 0x0021E418;   // size 0x48
constexpr uint32_t readBufBeginGet__FP7ReadBufPPUc = 0x0021E460;   // size 0x48
constexpr uint32_t readBufEndGet__FP7ReadBufi = 0x0021E4A8;   // size 0x24
constexpr uint32_t x__8resetcom = 0x0021E4D0;   // size 0x24
constexpr uint32_t x__8resetcom11CREATOR_KEY = 0x0021E4F8;   // size 0x20
constexpr uint32_t writeKeys__8resetcomPc = 0x0021E518;   // size 0x198
constexpr uint32_t write__8resetcomPc = 0x0021E6B0;   // size 0xD0
constexpr uint32_t readKey__8resetcomPc = 0x0021E780;   // size 0x160
constexpr uint32_t read__8resetcomPc = 0x0021E8E0;   // size 0x1D8
constexpr uint32_t oldStyleRead__8resetcomPc = 0x0021EAB8;   // size 0x8
constexpr uint32_t init__8resetcom = 0x0021EAC0;   // size 0x8
constexpr uint32_t update__8resetcom = 0x0021EAC8;   // size 0xF0
constexpr uint32_t getFileAndPathname__8resetcomi = 0x0021EBB8;   // size 0x170
constexpr uint32_t validFileAndExtension__8resetcomi = 0x0021ED28;   // size 0x74
constexpr uint32_t forceRead__8resetcom = 0x0021EDA0;   // size 0x1C
constexpr uint32_t forceRead__8resetcomi = 0x0021EDC0;   // size 0x28
constexpr uint32_t forceWrite__8resetcom = 0x0021EDE8;   // size 0x1C
constexpr uint32_t forceWrite__8resetcomi = 0x0021EE08;   // size 0x28
constexpr uint32_t getFileName__8resetcomPPc = 0x0021EE30;   // size 0x10
constexpr uint32_t setFileName__8resetcomPcb = 0x0021EE40;   // size 0x88
constexpr uint32_t x__tf8resetcom = 0x0021EEC8;   // size 0x3C
constexpr uint32_t x__9RigidBody = 0x0021EF08;   // size 0x28
constexpr uint32_t setCenterOfMass__9RigidBodyR8_fvector = 0x0021EF30;   // size 0x24
constexpr uint32_t setCenterOfMass__9RigidBodyfff = 0x0021EF58;   // size 0x28
constexpr uint32_t setTensor__9RigidBodyfff = 0x0021EF80;   // size 0x28
constexpr uint32_t updateFromCS__9RigidBody = 0x0021EFA8;   // size 0x1C
constexpr uint32_t setTrans__9RigidBodyR8_fvector = 0x0021EFC8;   // size 0x48
constexpr uint32_t deltaTrans__9RigidBodyR8_fvector = 0x0021F010;   // size 0x34
constexpr uint32_t setMatrix__9RigidBodyRA3_A3_f = 0x0021F048;   // size 0x70
constexpr uint32_t initBlock__9RigidBodyffff = 0x0021F0B8;   // size 0x70
constexpr uint32_t initSphere__9RigidBodyff = 0x0021F128;   // size 0x64
constexpr uint32_t initCylindrical__9RigidBodyffif = 0x0021F190;   // size 0xE4
constexpr uint32_t initEllipsoid__9RigidBodyffff = 0x0021F278;   // size 0x84
constexpr uint32_t reset__9RigidBody = 0x0021F300;   // size 0x40
constexpr uint32_t clearForces__9RigidBody = 0x0021F340;   // size 0x10
constexpr uint32_t applyGravForce__9RigidBodyf = 0x0021F350;   // size 0x18
constexpr uint32_t applyWorldForce__9RigidBodyR8_fvector = 0x0021F368;   // size 0x1C
constexpr uint32_t applyWorldForce__9RigidBodyR8_fvectorT1 = 0x0021F388;   // size 0x60
constexpr uint32_t applyLocalForce__9RigidBodyR8_fvector = 0x0021F3E8;   // size 0x44
constexpr uint32_t applyLocalForce__9RigidBodyR8_fvectorT1 = 0x0021F430;   // size 0x7C
constexpr uint32_t applyLocalTorque__9RigidBodyR8_fvectorT1 = 0x0021F4B0;   // size 0x7C
constexpr uint32_t applyWorldTorque__9RigidBodyR8_fvectorT1 = 0x0021F530;   // size 0x4C
constexpr uint32_t applyWorldImpulse__9RigidBodyR8_fvector = 0x0021F580;   // size 0x34
constexpr uint32_t applyWorldImpulse__9RigidBodyR8_fvectorT1 = 0x0021F5B8;   // size 0x88
constexpr uint32_t applyLocalImpulse__9RigidBodyR8_fvector = 0x0021F640;   // size 0x44
constexpr uint32_t applyLocalImpulse__9RigidBodyR8_fvectorT1 = 0x0021F688;   // size 0x7C
constexpr uint32_t resolveCollision__9RigidBodyR9RigidBodyR8_fvectorT2fff = 0x0021F708;   // size 0x2B0
constexpr uint32_t resolveCollision__9RigidBodyR8_fvectorT1fff = 0x0021F9B8;   // size 0x1D0
constexpr uint32_t resolveCollision__9RigidBodyR8_fvectorN21fff = 0x0021FB88;   // size 0x1C0
constexpr uint32_t resolveFriction__9RigidBodyR8_fvectorT1ff = 0x0021FD48;   // size 0x260
constexpr uint32_t resolveForces__9RigidBodyf = 0x0021FFA8;   // size 0x190
constexpr uint32_t computeAuxiliarys__9RigidBody = 0x00220138;   // size 0xCC
constexpr uint32_t computeX__9RigidBody = 0x00220208;   // size 0x50
constexpr uint32_t getPointVelocity__9RigidBodyR8_fvectorT1 = 0x00220258;   // size 0x4C
constexpr uint32_t setLinearVelocity__9RigidBodyR8_fvector = 0x002202A8;   // size 0x38
constexpr uint32_t createSkewSymmetric__9RigidBodyPA3_fR8_fvector = 0x002202E0;   // size 0x54
constexpr uint32_t checkpoint__9RigidBody = 0x00220338;   // size 0xE4
constexpr uint32_t rollback__9RigidBody = 0x00220420;   // size 0xF0
constexpr uint32_t clearSources__11ScreenPolysv = 0x00220510;   // size 0xC
constexpr uint32_t initSourceObject__11ScreenPolysP11_hierobjecti = 0x00220520;   // size 0xA0
constexpr uint32_t dmaData__11ScreenPolysv = 0x002205C0;   // size 0x13C
constexpr uint32_t setColor__11ScreenPolysiiiii = 0x00220700;   // size 0x24
constexpr uint32_t getColor__11ScreenPolysiRiN32 = 0x00220728;   // size 0x34
constexpr uint32_t setDefaultColor__11ScreenPolysi = 0x00220760;   // size 0x28
constexpr uint32_t dimColor__11ScreenPolysi = 0x00220788;   // size 0x38
constexpr uint32_t setHilightColor__11ScreenPolysi = 0x002207C0;   // size 0x2C
constexpr uint32_t setSourceFX__11ScreenPolysiUs = 0x002207F0;   // size 0x18
constexpr uint32_t addPoly__11ScreenPolysiiiRQ211ScreenPolys8PolyData = 0x00220808;   // size 0x3C
constexpr uint32_t addFractionalPoly__11ScreenPolysffiiiRQ211ScreenPolys8PolyData = 0x00220848;   // size 0x108
constexpr uint32_t polyBuildPrim__FiiiRQ211ScreenPolys8PolyDataP6QwData = 0x00220950;   // size 0x1D0
constexpr uint32_t resolveFX__FiiiRQ211ScreenPolys8PolyDataT3 = 0x00220B20;   // size 0x1D0
constexpr uint32_t polyInitPacket__FP11_hierobjectP9SourceObjP6QwData = 0x00220CF0;   // size 0xBC
constexpr uint32_t x__15SpriteAnimation = 0x00220DB0;   // size 0x28
constexpr uint32_t clear__15SpriteAnimation = 0x00220DD8;   // size 0x1C
constexpr uint32_t addFrame__15SpriteAnimationRQ211ScreenPolys8PolyData = 0x00220DF8;   // size 0x68
constexpr uint32_t draw__15SpriteAnimationiib = 0x00220E60;   // size 0x60
constexpr uint32_t crossfadeGraphics__15SpriteAnimationbii = 0x00220EC0;   // size 0x1D0
constexpr uint32_t Sjis2Ascii__8ShiftJISPUc = 0x00221090;   // size 0xC8
constexpr uint32_t Sjis2AsciiString__8ShiftJISPUcPc = 0x00221158;   // size 0xA4
constexpr uint32_t IsSjis__8ShiftJISPUc = 0x00221200;   // size 0x3C
constexpr uint32_t IsAscii__8ShiftJISPc = 0x00221240;   // size 0x10
constexpr uint32_t Ascii2Sjis__8ShiftJISUc = 0x00221250;   // size 0x10C
constexpr uint32_t AsciiString2Sjis__8ShiftJISPUcPUs = 0x00221360;   // size 0x98
constexpr uint32_t SwapShort__8ShiftJISUs = 0x002213F8;   // size 0x1C
constexpr uint32_t smoothInit__FP10_smoothDatPfffffT1b = 0x00221418;   // size 0x20
constexpr uint32_t smoothEasyIn__Fffff = 0x00221438;   // size 0x8C
constexpr uint32_t smoothEasyInAngle__Fffff = 0x002214C8;   // size 0x14C
constexpr uint32_t smoothCanIncreaseToMax__Ffff = 0x00221618;   // size 0x68
constexpr uint32_t smoothCanDecreaseToMinusMax__Ffff = 0x00221680;   // size 0x68
constexpr uint32_t smoothEasyOutEasyIn__FP10_smoothDat = 0x002216E8;   // size 0x368
constexpr uint32_t smoothEasyInTC__Fffff = 0x00221A50;   // size 0xB0
constexpr uint32_t smoothEasyInAngleTC__Fffff = 0x00221B00;   // size 0xB0
constexpr uint32_t strFileOpen__FP7StrFilePc = 0x00221BB0;   // size 0x20C
constexpr uint32_t strFileClose__FP7StrFile = 0x00221DC0;   // size 0x3C
constexpr uint32_t strFileRead__FP7StrFilePvi = 0x00221E00;   // size 0xD4
constexpr uint32_t strFileWait__Fi = 0x00221ED8;   // size 0x1C
constexpr uint32_t x__11TaskManager = 0x00221EF8;   // size 0x4C
constexpr uint32_t init__11TaskManagerUi = 0x00221F48;   // size 0xF8
constexpr uint32_t add__11TaskManagerPFPv_UiPvi = 0x00222040;   // size 0x6C
constexpr uint32_t add__11TaskManagerPFv_Uii = 0x002220B0;   // size 0x60
constexpr uint32_t remove__11TaskManagerRPv = 0x00222110;   // size 0x38
constexpr uint32_t update__11TaskManager = 0x00222148;   // size 0xBC
constexpr uint32_t executeAndDeleteAll__11TaskManager = 0x00222208;   // size 0x80
constexpr uint32_t createTask__11TaskManager = 0x00222288;   // size 0x60
constexpr uint32_t deleteTask__11TaskManagerPQ211TaskManager10TaskConfig = 0x002222E8;   // size 0x50
constexpr uint32_t x__static_initialization_and_destruction_0_00222338 = 0x00222338;   // size 0x48
constexpr uint32_t x_GLOBAL__I__11TaskManager_global = 0x00222390;   // size 0x20
constexpr uint32_t tdColTriggerNode__FP20HierCollisionTriggerP8_fvectorPA3_A3_fP9_tdResult = 0x002223B0;   // size 0x7C
constexpr uint32_t tdTest__FP8_fvectorP9_tdResulti = 0x00222430;   // size 0xCC
constexpr uint32_t START_PACKET__FP6QwData = 0x00222500;   // size 0xC
constexpr uint32_t START_DMA__FUi = 0x00222510;   // size 0x30
constexpr uint32_t START_DIRECT__Fv = 0x00222540;   // size 0x38
constexpr uint32_t START_GIF__FUiUiUiUiUiUi = 0x00222578;   // size 0x80
constexpr uint32_t END_GIF__Fv = 0x002225F8;   // size 0x3C
constexpr uint32_t END_DIRECT__Fv = 0x00222638;   // size 0x2C
constexpr uint32_t END_DMA__Fv = 0x00222668;   // size 0x28
constexpr uint32_t SET_PACKET__FP6QwData = 0x00222690;   // size 0x8
constexpr uint32_t GET_PACKET__Fv = 0x00222698;   // size 0x8
constexpr uint32_t setTextureAsDrawEnv__FUiUiUiUi = 0x002226A0;   // size 0xCC
constexpr uint32_t setDrawEnvAsTexture__FUiUiUiUiUib = 0x00222770;   // size 0xE8
constexpr uint32_t setContextForParticleDraw__FUi = 0x00222858;   // size 0x48
constexpr uint32_t copyGreenChannelToAlpha__FUiUiUi = 0x002228A0;   // size 0x20C
constexpr uint32_t restoreScreenDrawEnv__Fv = 0x00222AB0;   // size 0x90
constexpr uint32_t drawTestSprite__Fiiiiii = 0x00222B40;   // size 0x164
constexpr uint32_t texDrawPreamble__FP6QwDataiiUib = 0x00222CA8;   // size 0xB0
constexpr uint32_t texDrawCopyGreenToAlpha__FP6QwDataiib = 0x00222D58;   // size 0xE4
constexpr uint32_t texDrawTestPostlude__FP6QwDataii = 0x00222E40;   // size 0x68
constexpr uint32_t texDrawDustPacketBuild__FP6QwDataiiPPUiT3PPfN25PP6QwData = 0x00222EA8;   // size 0x398
constexpr uint32_t texDrawInitDustTextureInfo__FPiii = 0x00223240;   // size 0xE8
constexpr uint32_t timerInit__Fi = 0x00223328;   // size 0x94
constexpr uint32_t timerEndOfRealTimeProcessing__Fv = 0x002233C0;   // size 0x9C
constexpr uint32_t timerSetFieldCount__Fi = 0x00223460;   // size 0x10
constexpr uint32_t timerSetFrameStart__Fv = 0x00223470;   // size 0x34
constexpr uint32_t timer1Handler__FPUi = 0x002234A8;   // size 0xC0
constexpr uint32_t timerWaitForMinUpdateRate__FPUi = 0x00223568;   // size 0x150
constexpr uint32_t timerGetCurTics__Fv = 0x002236B8;   // size 0x1C
constexpr uint32_t timerGetUpdateRate__Fv = 0x002236D8;   // size 0x8
constexpr uint32_t timerGetFieldCount__Fv = 0x002236E0;   // size 0x8
constexpr uint32_t timerGetFieldsLastFrame__Fv = 0x002236E8;   // size 0x8
constexpr uint32_t timerSetMinUpdateRate__Fi = 0x002236F0;   // size 0xC0
constexpr uint32_t timerGetMinUpdateRate__Fv = 0x002237B0;   // size 0x8
constexpr uint32_t timerGetFrameStart__Fv = 0x002237B8;   // size 0x8
constexpr uint32_t timerGetFrameTime__Fv = 0x002237C0;   // size 0x1C
constexpr uint32_t timerGetAverageFrameTime__Fv = 0x002237E0;   // size 0x70
constexpr uint32_t timerGetOverloadScale__Fv = 0x00223850;   // size 0x8
constexpr uint32_t todBoundAngle__FPf = 0x00223858;   // size 0x98
constexpr uint32_t todCalculateValue__Fiff = 0x002238F0;   // size 0x70
constexpr uint32_t todInit__Fv = 0x00223960;   // size 0x42C
constexpr uint32_t todInitStats__Fv = 0x00223D90;   // size 0x8
constexpr uint32_t todUpdate__Fi = 0x00223D98;   // size 0xC8C
constexpr uint32_t todActive__Fv = 0x00224A28;   // size 0x8
constexpr uint32_t todGetTOD__FPiPfP10_todInfo14 = 0x00224A30;   // size 0x22C
constexpr uint32_t todSetTOD__FifPvf = 0x00224C60;   // size 0x628
constexpr uint32_t todGetCurrentTOD__Fv = 0x00225288;   // size 0x2C
constexpr uint32_t todGetAmbient__FPfN20 = 0x002252B8;   // size 0x28
constexpr uint32_t todSetSkyEntry__FP9_hierhead = 0x002252E0;   // size 0x12C
constexpr uint32_t todSetSkyObjects__Fiffffff = 0x00225410;   // size 0xBC
constexpr uint32_t todSetPlightsActive__Fii = 0x002254D0;   // size 0x18
constexpr uint32_t x__12CTODLinkList = 0x002254E8;   // size 0x90
constexpr uint32_t x___12CTODLinkList = 0x00225578;   // size 0x24
constexpr uint32_t AddEntry__12CTODLinkList = 0x002255A0;   // size 0x6C
constexpr uint32_t DeleteEntry__12CTODLinkListi = 0x00225610;   // size 0x8
constexpr uint32_t GetNumEntries__12CTODLinkList = 0x00225618;   // size 0x8
constexpr uint32_t FindNodeByTime__12CTODLinkListf = 0x00225620;   // size 0x8
constexpr uint32_t FindFreeNode__12CTODLinkList = 0x00225628;   // size 0x38
constexpr uint32_t SortList__12CTODLinkList = 0x00225660;   // size 0x8
constexpr uint32_t ClearNode__12CTODLinkListP9_LinkNode = 0x00225668;   // size 0x1C
constexpr uint32_t x__static_initialization_and_destruction_0_00225688 = 0x00225688;   // size 0x40
constexpr uint32_t x_GLOBAL__I_todLink = 0x002256C8;   // size 0x20
constexpr uint32_t x_GLOBAL__D_todLink = 0x002256E8;   // size 0x20
constexpr uint32_t loadShellTextureFile__10BlitMasterPCc = 0x00225708;   // size 0x20
constexpr uint32_t preloadShellTextureFile__10BlitMasterPCc = 0x00225728;   // size 0x8
constexpr uint32_t blitToZBuffer__10BlitMasteriiibi = 0x00225730;   // size 0x2B4
constexpr uint32_t blitToFrameBuffer__10BlitMasteriii = 0x002259E8;   // size 0x1B4
constexpr uint32_t DMAToScratchPad__FP6QwDatai = 0x00225BA0;   // size 0x88
constexpr uint32_t DMARestoreFromScratchPad__FP6QwDataT0i = 0x00225C28;   // size 0x80
constexpr uint32_t WaitDMA__Fv = 0x00225CA8;   // size 0x2C
constexpr uint32_t buildProgressBar__FUi = 0x00225CD8;   // size 0x1F4
constexpr uint32_t getLoadbarPacket__Fv = 0x00225ED0;   // size 0x18
constexpr uint32_t initProgressBar__Fiii = 0x00225EE8;   // size 0x364
constexpr uint32_t informProgressBar__Ff = 0x00226250;   // size 0x120
constexpr uint32_t getFIFOindex__FP5ViBufPv = 0x00226370;   // size 0x44
constexpr uint32_t setD3_CHCR__FUi = 0x002263B8;   // size 0x6C
constexpr uint32_t setD4_CHCR__FUi = 0x00226428;   // size 0x6C
constexpr uint32_t scTag2__FP5QWORDPvUiUi = 0x00226498;   // size 0x24
constexpr uint32_t viBufCreate__FP5ViBufPUxT1iP9TimeStampi = 0x002264C0;   // size 0x78
constexpr uint32_t viBufReset__FP5ViBuf = 0x00226538;   // size 0x160
constexpr uint32_t viBufBeginPut__FP5ViBufPPUcPiT1T2 = 0x00226698;   // size 0xF4
constexpr uint32_t viBufEndPut__FP5ViBufi = 0x00226790;   // size 0x54
constexpr uint32_t viBufAddDMA__FP5ViBuf = 0x002267E8;   // size 0x1FC
constexpr uint32_t viBufStopDMA__FP5ViBuf = 0x002269E8;   // size 0x110
constexpr uint32_t viBufRestartDMA__FP5ViBuf = 0x00226AF8;   // size 0x338
constexpr uint32_t viBufDelete__FP5ViBuf = 0x00226E30;   // size 0x58
constexpr uint32_t viBufIsActive__FP5ViBuf = 0x00226E88;   // size 0x40
constexpr uint32_t viBufCount__FP5ViBuf = 0x00226EC8;   // size 0x4C
constexpr uint32_t viBufFlush__FP5ViBuf = 0x00226F18;   // size 0x54
constexpr uint32_t viBufModifyPts__FP5ViBufP9TimeStamp = 0x00226F70;   // size 0x158
constexpr uint32_t viBufPutTs__FP5ViBufP9TimeStamp = 0x002270C8;   // size 0x110
constexpr uint32_t viBufGetTs__FP5ViBufP9TimeStamp = 0x002271D8;   // size 0x1A4
constexpr uint32_t videoDecCreate__FP8VideoDecPUciPUxT3iP9TimeStampi = 0x00227380;   // size 0x100
constexpr uint32_t videoDecSetDecodeMode__FP8VideoDeciii = 0x00227480;   // size 0x1C
constexpr uint32_t videoDecSetStream__FP8VideoDeciiPFP7sceMpegP13sceMpegCbDataPv_iPv = 0x002274A0;   // size 0x20
constexpr uint32_t videoDecBeginPut__FP8VideoDecPPUcPiT1T2 = 0x002274C0;   // size 0x1C
constexpr uint32_t videoDecEndPut__FP8VideoDeci = 0x002274E0;   // size 0x1C
constexpr uint32_t videoDecReset__FP8VideoDec = 0x00227500;   // size 0xC
constexpr uint32_t videoDecDelete__FP8VideoDec = 0x00227510;   // size 0x34
constexpr uint32_t videoDecAbort__FP8VideoDec = 0x00227548;   // size 0x10
constexpr uint32_t videoDecGetState__FP8VideoDec = 0x00227558;   // size 0x8
constexpr uint32_t videoDecSetState__FP8VideoDecUi = 0x00227560;   // size 0xC
constexpr uint32_t videoDecPutTs__FP8VideoDecllPUci = 0x00227570;   // size 0x3C
constexpr uint32_t videoDecInputCount__FP8VideoDec = 0x002275B0;   // size 0x1C
constexpr uint32_t videoDecInputSpaceCount__FP8VideoDec = 0x002275D0;   // size 0x34
constexpr uint32_t videoDecFlush__FP8VideoDec = 0x00227608;   // size 0xCC
constexpr uint32_t videoDecIsFlushed__FP8VideoDec = 0x002276D8;   // size 0x48
constexpr uint32_t videoDecMain__FP8VideoDec = 0x00227720;   // size 0x78
constexpr uint32_t decBs0__FP8VideoDec = 0x00227798;   // size 0x21C
constexpr uint32_t mpegError__FP7sceMpegP18sceMpegCbDataErrorPv = 0x002279B8;   // size 0x28
constexpr uint32_t mpegNodata__FP7sceMpegP13sceMpegCbDataPv = 0x002279E0;   // size 0x2C
constexpr uint32_t mpegStopDMA__FP7sceMpegP13sceMpegCbDataPv = 0x00227A10;   // size 0x24
constexpr uint32_t mpegRestartDMA__FP7sceMpegP13sceMpegCbDataPv = 0x00227A38;   // size 0x24
constexpr uint32_t mpegTS__FP7sceMpegP22sceMpegCbDataTimeStampPv = 0x00227A60;   // size 0x44
constexpr uint32_t cpy2area__FPUciT0iT0iT0i = 0x00227AA8;   // size 0x130
constexpr uint32_t viewCreate__F10_viewportsi = 0x00227BD8;   // size 0x234
constexpr uint32_t viewUpdate__Fi = 0x00227E10;   // size 0x580
constexpr uint32_t viewInFOV__FiP8_fvectorT1 = 0x00228390;   // size 0xC0
constexpr uint32_t viewInFOV__FiP8_fvectorT1f = 0x00228450;   // size 0x94
constexpr uint32_t viewShadowUpdate__Fi = 0x002284E8;   // size 0xF0
constexpr uint32_t viewStoreNorms1InVu0__Fv = 0x002285D8;   // size 0x1C
constexpr uint32_t viewStoreNorms2InVu0__Fv = 0x002285F8;   // size 0x1C
constexpr uint32_t viewGetWorld2ScreenMat__Fi = 0x00228618;   // size 0x14
constexpr uint32_t viewGetShadWorldScreenMat__Fv = 0x00228630;   // size 0xC
constexpr uint32_t viewGetTOD__FPiPfP10_todInfo14 = 0x00228640;   // size 0x1C
constexpr uint32_t viewSetTOD__FifPvf = 0x00228660;   // size 0x1C
constexpr uint32_t viewGetRot__Fi = 0x00228680;   // size 0x20
constexpr uint32_t viewGetMat__Fi = 0x002286A0;   // size 0x20
constexpr uint32_t viewGetTrans__Fi = 0x002286C0;   // size 0x20
constexpr uint32_t viewSetRot__FP8_fvectori = 0x002286E0;   // size 0x54
constexpr uint32_t viewSetRot__FPA3_A3_fi = 0x00228738;   // size 0x40
constexpr uint32_t viewSetTrans__FP8_fvectori = 0x00228778;   // size 0x3C
constexpr uint32_t viewGetCurView__Fv = 0x002287B8;   // size 0x8
constexpr uint32_t viewGetViewCs__Fi = 0x002287C0;   // size 0x1C
constexpr uint32_t viewGetWorldCtx__Fi = 0x002287E0;   // size 0x18
constexpr uint32_t viewSetWorldEpNode__FP9_hierhead = 0x002287F8;   // size 0x34
constexpr uint32_t viewSetAmbientVol__Fiffff = 0x00228830;   // size 0x48
constexpr uint32_t viewGetWeTrans__Fi = 0x00228878;   // size 0x18
constexpr uint32_t viewGetWeMat__Fi = 0x00228890;   // size 0x18
constexpr uint32_t viewSetSkyEntry__FP9_hierheadi = 0x002288A8;   // size 0xE0
constexpr uint32_t viewGetSky__Fi = 0x00228988;   // size 0x1C
constexpr uint32_t viewSetSkyTrans__FP8_fvectori = 0x002289A8;   // size 0xA0
constexpr uint32_t viewSetSkyNode__FP9_hierheadi = 0x00228A48;   // size 0xB4
constexpr uint32_t viewSetVUPacketMat__FPA3_fN30i = 0x00228B00;   // size 0x2AC
constexpr uint32_t viewApplySwap__FPA3_fT0 = 0x00228DB0;   // size 0x94
constexpr uint32_t viewSetBgColor__FUlUlUlUl = 0x00228E48;   // size 0xE0
constexpr uint32_t viewSetTODBgColor__FUlUlUlUl = 0x00228F28;   // size 0xE0
constexpr uint32_t viewGetBgColor__FRiN30 = 0x00229008;   // size 0x2C
constexpr uint32_t viewGetDb__Fi = 0x00229038;   // size 0x18
constexpr uint32_t viewGetNumViews__Fv = 0x00229050;   // size 0x18
constexpr uint32_t viewSetNumViews__Fi = 0x00229068;   // size 0x8
constexpr uint32_t viewGetCenter__FiPiT1 = 0x00229070;   // size 0x34
constexpr uint32_t viewGetWH__FiPiT1 = 0x002290A8;   // size 0x34
constexpr uint32_t viewGetWorldToScreenMat__Fi = 0x002290E0;   // size 0x14
constexpr uint32_t viewGetZBuffParams__FiPfT1 = 0x002290F8;   // size 0x24
constexpr uint32_t viewInit__Fv = 0x00229120;   // size 0x64
constexpr uint32_t viewTweakInit__Fv = 0x00229188;   // size 0x8
constexpr uint32_t viewTweakSetFov__Fv = 0x00229190;   // size 0x64
constexpr uint32_t viewSetAmbient__Ffff = 0x002291F8;   // size 0x98
constexpr uint32_t viewGetAmbient__FPfN20 = 0x00229290;   // size 0x28
constexpr uint32_t viewGetAnimAmbient__FPfN20 = 0x002292B8;   // size 0x2C
constexpr uint32_t viewGetFov__FiPfT1 = 0x002292E8;   // size 0x40
constexpr uint32_t viewSetFov__Fiff = 0x00229328;   // size 0x3D4
constexpr uint32_t viewComputeNormal__FP8_fvectorN30 = 0x00229700;   // size 0x88
constexpr uint32_t viewToggleSplitScreen__FbT0 = 0x00229788;   // size 0x64
constexpr uint32_t viewGetScreenDisplay__Fv = 0x002297F0;   // size 0x44
constexpr uint32_t viewGrappleConfig__Fv = 0x00229838;   // size 0x19C
constexpr uint32_t viewGetDef__Fi = 0x002299D8;   // size 0x1C
constexpr uint32_t viewGetDef__F10_viewports = 0x002299F8;   // size 0x18
constexpr uint32_t viewSetOddEven__Fi = 0x00229A10;   // size 0xC
constexpr uint32_t viewIsOddScan__Fv = 0x00229A20;   // size 0x8
constexpr uint32_t voBufCreate__FP5VoBufP6VoDataP5VoTagi = 0x00229A28;   // size 0x4C
constexpr uint32_t voBufDelete__FP5VoBuf = 0x00229A78;   // size 0x8
constexpr uint32_t voBufReset__FP5VoBuf = 0x00229A80;   // size 0x14
constexpr uint32_t voBufIsFull__FP5VoBuf = 0x00229A98;   // size 0x14
constexpr uint32_t voBufIncCount__FP5VoBuf = 0x00229AB0;   // size 0x78
constexpr uint32_t voBufGetData__FP5VoBuf = 0x00229B28;   // size 0x44
constexpr uint32_t voBufIsEmpty__FP5VoBuf = 0x00229B70;   // size 0xC
constexpr uint32_t voBufGetTag__FP5VoBuf = 0x00229B80;   // size 0x64
constexpr uint32_t voBufDecCount__FP5VoBuf = 0x00229BE8;   // size 0x20
constexpr uint32_t zipCrc32__FUlPCUcl = 0x00229C08;   // size 0x1EC
constexpr uint32_t zipFlush__FUl = 0x00229DF8;   // size 0x6C
constexpr uint32_t zipCheckHeader__Fv = 0x00229E68;   // size 0x90
constexpr uint32_t zipInflateCodes__FP4huftT0ii = 0x00229EF8;   // size 0x494
constexpr uint32_t zipFreeHuffmanTable__FP4huft = 0x0022A390;   // size 0x48
constexpr uint32_t zipBuildHuffmanTable__FPCUiUiUiPCUsT3PP4huftPi = 0x0022A3D8;   // size 0x774
constexpr uint32_t zipInflateBlockStored__Fv = 0x0022AB50;   // size 0x1C0
constexpr uint32_t zipInflateBlockFixed__Fv = 0x0022AD10;   // size 0x1BC
constexpr uint32_t zipInflateBlockDynamic__Fv = 0x0022AED0;   // size 0x62C
constexpr uint32_t zipInflateBlock__FPi = 0x0022B500;   // size 0x120
constexpr uint32_t zipInflateAll__FPcPv = 0x0022B620;   // size 0x108
constexpr uint32_t zipGetChar__Fv = 0x0022B728;   // size 0x74
constexpr uint32_t x__static_initialization_and_destruction_0_0022B7A0 = 0x0022B7A0;   // size 0x20
constexpr uint32_t x_GLOBAL__I_whichHalfMeg = 0x0022B7C0;   // size 0x20
constexpr uint32_t audioDecCreate__FP8AudioDecPUcii = 0x0022B7E0;   // size 0x70
constexpr uint32_t audioDecReset__FP8AudioDec = 0x0022B850;   // size 0x48
constexpr uint32_t audioDecDelete__FP8AudioDec = 0x0022B898;   // size 0x28
constexpr uint32_t audioDecBeginPut__FP8AudioDecPPUcPiT1T2 = 0x0022B8C0;   // size 0xB0
constexpr uint32_t audioDecEndPut__FP8AudioDeci = 0x0022B970;   // size 0x7C
constexpr uint32_t audioDecIsPreset__FP8AudioDec = 0x0022B9F0;   // size 0x14
constexpr uint32_t audioDecStart__FP8AudioDec = 0x0022BA08;   // size 0x58
constexpr uint32_t audioDecSendToIOP__FP8AudioDec = 0x0022BA60;   // size 0x1B0
constexpr uint32_t iopGetArea__FPiN30P8AudioDeci = 0x0022BC10;   // size 0xA8
constexpr uint32_t sendToIOP2area__FiiiiPUciT4i = 0x0022BCB8;   // size 0x148
constexpr uint32_t sendToIOP__FiPUci = 0x0022BE00;   // size 0x80
constexpr uint32_t audioDecPause__FP8AudioDec = 0x0022BE80;   // size 0x3C
constexpr uint32_t audioDecResume__FP8AudioDec = 0x0022BEC0;   // size 0x1C
constexpr uint32_t bspVecTest__FP8_hierbspP8_fvectorT1P10_bspresult = 0x0022BEE0;   // size 0x24
constexpr uint32_t bspVecTestIn__FP8_hierbspP8_fvectorT1 = 0x0022BF08;   // size 0x2D8
constexpr uint32_t bspPointTest__FP8_hierbspP8_fvector = 0x0022C1E0;   // size 0x84
constexpr uint32_t bspVecTest__FP8_hierbspP12_linesegmentP10_bspresult = 0x0022C268;   // size 0x20
constexpr uint32_t bspPlaneTest2Vec__FP6_planeP8_fvectorN21 = 0x0022C288;   // size 0x44
constexpr uint32_t bspInterp__FP8_fvector = 0x0022C2D0;   // size 0x30
constexpr uint32_t bspTestChild__FP8_hierbspsP8_fvectorT2P10_bspresult = 0x0022C300;   // size 0x48
constexpr uint32_t x__Q27CsColor4Data = 0x0022C348;   // size 0x20
constexpr uint32_t isEnabled__7CsColorv = 0x0022C368;   // size 0xC
constexpr uint32_t contrast__7CsColorv = 0x0022C378;   // size 0xC
constexpr uint32_t brightness__7CsColorv = 0x0022C388;   // size 0xC
constexpr uint32_t load__7CsColorPCQ27CsColor4Data = 0x0022C398;   // size 0x50
constexpr uint32_t save__7CsColorPQ27CsColor4Data = 0x0022C3E8;   // size 0x4C
constexpr uint32_t x__static_initialization_and_destruction_0_0022C438 = 0x0022C438;   // size 0x30
constexpr uint32_t x_GLOBAL__I___Q27CsColor4Data = 0x0022C468;   // size 0x20
constexpr uint32_t InitColGridCsListArray__Fv = 0x0022C488;   // size 0x40
constexpr uint32_t allocateColGridCsListNode__Fv = 0x0022C4C8;   // size 0x48
constexpr uint32_t deallocColGridCsListNode__FP17ColGridCsListNode = 0x0022C510;   // size 0x14
constexpr uint32_t pushCsFront__13ColGridCsListP3_cs = 0x0022C528;   // size 0x48
constexpr uint32_t removeCs__13ColGridCsListP3_cs = 0x0022C570;   // size 0x74
constexpr uint32_t ctxSetUpGSCtx__Fv = 0x0022C5E8;   // size 0x8C
constexpr uint32_t ctxGetDefaultGSCtx__Fv = 0x0022C678;   // size 0xC
constexpr uint32_t init__9DebugDraw = 0x0022C688;   // size 0x34
constexpr uint32_t registerVisual__9DebugDrawP9_hierhead = 0x0022C6C0;   // size 0x30
constexpr uint32_t drawSphere__9DebugDraw = 0x0022C6F0;   // size 0x64
constexpr uint32_t drawSphere__9DebugDrawR8_fvectorf = 0x0022C758;   // size 0x5C
constexpr uint32_t drawCS__9DebugDrawR8_fvectorRA3_A3_ff = 0x0022C7B8;   // size 0x80
constexpr uint32_t drawCollisionLines__9DebugDrawR8_fvectorUiUi = 0x0022C838;   // size 0xA8
constexpr uint32_t drawCollisionLines__9DebugDrawR8_fvectorfiUiUi = 0x0022C8E0;   // size 0x9C
constexpr uint32_t drawNormal__9DebugDrawR8_fvectorT1fUi = 0x0022C980;   // size 0x3C
constexpr uint32_t drawBox__9DebugDrawR8_fvectorT1Ui = 0x0022C9C0;   // size 0x64
constexpr uint32_t drawBox__9DebugDrawRA3_A3_fR8_fvectorN22Ui = 0x0022CA28;   // size 0xAC
constexpr uint32_t drawCone__9DebugDrawR8_fvectorT1ffUi = 0x0022CAD8;   // size 0x1D8
constexpr uint32_t destroyCS__9DebugDrawPv = 0x0022CCB0;   // size 0x20
constexpr uint32_t x__7GenRand = 0x0022CCD0;   // size 0x8
constexpr uint32_t x__7GenRandUi = 0x0022CCD8;   // size 0x28
constexpr uint32_t seed__7GenRandUi = 0x0022CD00;   // size 0x1DC
constexpr uint32_t rand__7GenRand = 0x0022CEE0;   // size 0x114
constexpr uint32_t ilnkListInit__FP9_ilnklistiPi = 0x0022CFF8;   // size 0x30
constexpr uint32_t ilnkListFill__FP9_ilnklistiPib = 0x0022D028;   // size 0x50
constexpr uint32_t ilnkListAdd__FP9_ilnklistiPi = 0x0022D078;   // size 0x50
constexpr uint32_t ilnkListRem__FP9_ilnklistiPi = 0x0022D0C8;   // size 0x78
constexpr uint32_t atanf = 0x0022D140;   // size 0x2A8
constexpr uint32_t ceilf = 0x0022D3E8;   // size 0xD4
constexpr uint32_t cosf = 0x0022D4C0;   // size 0xE8
constexpr uint32_t fabsf = 0x0022D5A8;   // size 0x1C
constexpr uint32_t sinf = 0x0022D5C8;   // size 0xF0
constexpr uint32_t tanf = 0x0022D6B8;   // size 0x88
constexpr uint32_t acosf = 0x0022D740;   // size 0xFC
constexpr uint32_t asinf = 0x0022D840;   // size 0xFC
constexpr uint32_t atan2f = 0x0022D940;   // size 0x128
constexpr uint32_t powf = 0x0022DA68;   // size 0x508
constexpr uint32_t sqrtf = 0x0022DF70;   // size 0x114
constexpr uint32_t x__ieee754_acosf = 0x0022E088;   // size 0x430
constexpr uint32_t x__ieee754_asinf = 0x0022E4B8;   // size 0x39C
constexpr uint32_t x__ieee754_atan2f = 0x0022E858;   // size 0x2E8
constexpr uint32_t x__ieee754_powf = 0x0022EB40;   // size 0x88C
constexpr uint32_t x__ieee754_rem_pio2f = 0x0022F3D0;   // size 0x3E0
constexpr uint32_t x__ieee754_sqrtf = 0x0022F7B0;   // size 0x138
constexpr uint32_t x__kernel_cosf = 0x0022F8E8;   // size 0x158
constexpr uint32_t x__kernel_rem_pio2f = 0x0022FA40;   // size 0x94C
constexpr uint32_t x__kernel_sinf = 0x00230390;   // size 0x104
constexpr uint32_t x__kernel_tanf = 0x00230498;   // size 0x294
constexpr uint32_t matherr = 0x00230730;   // size 0x24
constexpr uint32_t rint = 0x00230758;   // size 0x1FC
constexpr uint32_t finitef = 0x00230958;   // size 0x24
constexpr uint32_t floorf = 0x00230980;   // size 0xE4
constexpr uint32_t isnanf = 0x00230A68;   // size 0x24
constexpr uint32_t scalbnf = 0x00230A90;   // size 0x160
constexpr uint32_t copysignf = 0x00230BF0;   // size 0x30
constexpr uint32_t sceGsResetPath = 0x00230C20;   // size 0xA0
constexpr uint32_t sceGsSyncPath = 0x00230CC0;   // size 0x8C
constexpr uint32_t sceGsResetGraph = 0x00230D50;   // size 0x190
constexpr uint32_t sceGsGetGParam = 0x00230EE0;   // size 0xC
constexpr uint32_t sceGsSetDefDispEnv = 0x00230EF0;   // size 0x270
constexpr uint32_t sceGsSetDefDrawEnv = 0x00231160;   // size 0x1E4
constexpr uint32_t sceGsSetDefClear = 0x00231348;   // size 0x104
constexpr uint32_t sceGsSyncV = 0x00231450;   // size 0x94
constexpr uint32_t sceGsSetDefLoadImage = 0x002314E8;   // size 0x1E4
constexpr uint32_t sceGsSetDefStoreImage = 0x002316D0;   // size 0x140
constexpr uint32_t sceGsExecLoadImage = 0x00231810;   // size 0x17C
constexpr uint32_t sceGsExecStoreImage = 0x00231990;   // size 0x68C
constexpr uint32_t sceGsSyncVCallback = 0x00232020;   // size 0xA0
constexpr uint32_t sceGsSetHalfOffset = 0x002320C0;   // size 0x88
constexpr uint32_t sceGsSetDefDrawEnv2 = 0x00232148;   // size 0x1E0
constexpr uint32_t sceGsSetHalfOffset2 = 0x00232328;   // size 0x88
constexpr uint32_t sceGszbufaddr = 0x002323B0;   // size 0xC8
constexpr uint32_t x_send_to_iop = 0x00232478;   // size 0x130
constexpr uint32_t scePadInit = 0x002325A8;   // size 0x140
constexpr uint32_t scePadInit2 = 0x002326E8;   // size 0x9C
constexpr uint32_t scePadEnd = 0x00232788;   // size 0x80
constexpr uint32_t scePadPortOpen = 0x00232808;   // size 0x1E4
constexpr uint32_t scePadPortClose = 0x002329F0;   // size 0xB8
constexpr uint32_t scePadGetDmaStr = 0x00232AA8;   // size 0x60
constexpr uint32_t scePadGetFrameCount = 0x00232B08;   // size 0x50
constexpr uint32_t scePadRead = 0x00232B58;   // size 0x7C
constexpr uint32_t scePadGetState = 0x00232BD8;   // size 0x78
constexpr uint32_t scePadStateIntToStr = 0x00232C50;   // size 0x34
constexpr uint32_t scePadSetReqState = 0x00232C88;   // size 0x64
constexpr uint32_t scePadGetReqState = 0x00232CF0;   // size 0x50
constexpr uint32_t scePadReqIntToStr = 0x00232D40;   // size 0x34
constexpr uint32_t scePadInfoAct = 0x00232D78;   // size 0x120
constexpr uint32_t scePadInfoComb = 0x00232E98;   // size 0x11C
constexpr uint32_t scePadInfoMode = 0x00232FB8;   // size 0x138
constexpr uint32_t scePadSetMainMode = 0x002330F0;   // size 0xB4
constexpr uint32_t scePadSetActDirect = 0x002331A8;   // size 0xB8
constexpr uint32_t scePadSetActAlign = 0x00233260;   // size 0xD8
constexpr uint32_t scePadGetButtonMask = 0x00233338;   // size 0xB4
constexpr uint32_t scePadSetButtonInfo = 0x002333F0;   // size 0xB0
constexpr uint32_t scePadInfoPressMode = 0x002334A0;   // size 0x5C
constexpr uint32_t scePadEnterPressMode = 0x00233500;   // size 0x54
constexpr uint32_t scePadExitPressMode = 0x00233558;   // size 0x54
constexpr uint32_t scePadSetVrefParam = 0x002335B0;   // size 0xCC
constexpr uint32_t scePadGetPortMax = 0x00233680;   // size 0x64
constexpr uint32_t scePadGetSlotMax = 0x002336E8;   // size 0x68
constexpr uint32_t scePadGetModVersion = 0x00233750;   // size 0x64
constexpr uint32_t scePadSetWarningLevel = 0x002337B8;   // size 0x68
constexpr uint32_t sceMcInit = 0x00233820;   // size 0x1B4
constexpr uint32_t x_lmcGetClientPtr = 0x002339D8;   // size 0x30
constexpr uint32_t sceMcChangeThreadPriority = 0x00233A08;   // size 0xB4
constexpr uint32_t sceMcGetSlotMax = 0x00233AC0;   // size 0xC0
constexpr uint32_t sceMcOpen = 0x00233B80;   // size 0x134
constexpr uint32_t sceMcMkdir = 0x00233CB8;   // size 0x34
constexpr uint32_t sceMcClose = 0x00233CF0;   // size 0xB4
constexpr uint32_t sceMcSeek = 0x00233DA8;   // size 0xD4
constexpr uint32_t mceIntrReadFixAlign = 0x00233E80;   // size 0x90
constexpr uint32_t sceMcRead = 0x00233F10;   // size 0x11C
constexpr uint32_t sceMcWrite = 0x00234030;   // size 0x17C
constexpr uint32_t mcHearAlarm = 0x002341B0;   // size 0x24
constexpr uint32_t mcDelayThread = 0x002341D8;   // size 0x44
constexpr uint32_t sceMcSync = 0x00234220;   // size 0xE0
constexpr uint32_t mceGetInfoApdx = 0x00234300;   // size 0x54
constexpr uint32_t sceMcGetInfo = 0x00234358;   // size 0x184
constexpr uint32_t sceMcGetDir = 0x002344E0;   // size 0x158
constexpr uint32_t mceStorePwd = 0x00234638;   // size 0x84
constexpr uint32_t sceMcChdir = 0x002346C0;   // size 0x150
constexpr uint32_t sceMcFormat = 0x00234810;   // size 0xC8
constexpr uint32_t sceMcDelete = 0x002348D8;   // size 0x128
constexpr uint32_t sceMcFlush = 0x00234A00;   // size 0xB4
constexpr uint32_t sceMcSetFileInfo = 0x00234AB8;   // size 0x1D8
constexpr uint32_t sceMcRename = 0x00234C90;   // size 0x168
constexpr uint32_t sceMcUnformat = 0x00234DF8;   // size 0xC8
constexpr uint32_t sceMcGetEntSpace = 0x00234EC0;   // size 0x118
constexpr uint32_t x_type2id = 0x00234FD8;   // size 0x88
constexpr uint32_t x_id2type = 0x00235060;   // size 0x1FC
constexpr uint32_t sceMpegDemuxPssRing = 0x00235260;   // size 0x2F8
constexpr uint32_t sceMpegDemuxPss = 0x00235558;   // size 0x20
constexpr uint32_t sceMpegAddStrCallback = 0x00235578;   // size 0xF4
constexpr uint32_t x_pack_header = 0x00235670;   // size 0x150
constexpr uint32_t x_system_header = 0x002357C0;   // size 0x6C
constexpr uint32_t x_PES_packet = 0x00235830;   // size 0x5B0
constexpr uint32_t sceMpegInit = 0x00235DE0;   // size 0x9C
constexpr uint32_t sceMpegCreate = 0x00235E80;   // size 0x1F4
constexpr uint32_t sceMpegDelete = 0x00236078;   // size 0x8
constexpr uint32_t sceMpegAddBs = 0x00236080;   // size 0x24
constexpr uint32_t sceMpegGetPicture = 0x002360A8;   // size 0x48
constexpr uint32_t sceMpegGetPictureRAW8 = 0x002360F0;   // size 0x44
constexpr uint32_t sceMpegGetPictureRAW8xy = 0x00236138;   // size 0x50
constexpr uint32_t sceMpegSetDecodeMode = 0x00236188;   // size 0x14
constexpr uint32_t sceMpegGetDecodeMode = 0x002361A0;   // size 0x20
constexpr uint32_t sceMpegIsEnd = 0x002361C0;   // size 0xC
constexpr uint32_t sceMpegIsRefBuffEmpty = 0x002361D0;   // size 0x10
constexpr uint32_t sceMpegReset = 0x002361E0;   // size 0x50
constexpr uint32_t sceMpegClearRefBuff = 0x00236230;   // size 0x54
constexpr uint32_t sceMpegAddCallback = 0x00236288;   // size 0x24
constexpr uint32_t x_dispatchMpegCallback = 0x002362B0;   // size 0x50
constexpr uint32_t x_dispatchMpegCbNodata = 0x00236300;   // size 0x24
constexpr uint32_t sceMpegSetDefaultPtsGap = 0x00236328;   // size 0x14
constexpr uint32_t sceMpegResetDefaultPtsGap = 0x00236340;   // size 0x10
constexpr uint32_t sceMpegSetImageBuff = 0x00236350;   // size 0xC
constexpr uint32_t sceMpegDispWidth = 0x00236360;   // size 0xC
constexpr uint32_t sceMpegDispHeight = 0x00236370;   // size 0xC
constexpr uint32_t sceMpegDispCenterOffX = 0x00236380;   // size 0xC
constexpr uint32_t sceMpegDispCenterOffY = 0x00236390;   // size 0xC
constexpr uint32_t sceSetBrokenLink = 0x002363A0;   // size 0x10
constexpr uint32_t sceSetPtm = 0x002363B0;   // size 0x14
constexpr uint32_t x_alalcInit = 0x002363C8;   // size 0x14
constexpr uint32_t x_alalcSetDynamic = 0x002363E0;   // size 0xC
constexpr uint32_t x_alalcFree = 0x002363F0;   // size 0xC
constexpr uint32_t x_alalcAlloc = 0x00236400;   // size 0x6C
constexpr uint32_t x_alalcRest = 0x00236470;   // size 0x18
constexpr uint32_t x_getpic = 0x00236488;   // size 0x170
constexpr uint32_t x_decodeOrSkipFrame = 0x002365F8;   // size 0x114
constexpr uint32_t x_decodeOrSkip = 0x00236710;   // size 0x44
constexpr uint32_t x_decodeOrSkipField = 0x00236758;   // size 0x15C
constexpr uint32_t x_sceMpegFlush = 0x002368B8;   // size 0x68
constexpr uint32_t x_initSeqAgain = 0x00236920;   // size 0xC
constexpr uint32_t x_lastFrame = 0x00236930;   // size 0x7C
constexpr uint32_t x_clearOnce = 0x002369B0;   // size 0x60
constexpr uint32_t x_clearEach = 0x00236A10;   // size 0xBC
constexpr uint32_t x_ErrMessage = 0x00236AD0;   // size 0x10
constexpr uint32_t x_Error1 = 0x00236AE0;   // size 0x34
constexpr uint32_t x_Error = 0x00236B18;   // size 0x54
constexpr uint32_t x_sendDataToIPU = 0x00236B70;   // size 0xF0
constexpr uint32_t x_RefImageInit = 0x00236C60;   // size 0x20
constexpr uint32_t x_sequenceHeader = 0x00236C80;   // size 0x124
constexpr uint32_t x_initSeq = 0x00236DA8;   // size 0x2A8
constexpr uint32_t x_initRefImages = 0x00237050;   // size 0xE0
constexpr uint32_t x_setDefaultQM = 0x00237130;   // size 0xD4
constexpr uint32_t x_sequenceExtension = 0x00237208;   // size 0x128
constexpr uint32_t x_sequenceDisplayExtension = 0x00237330;   // size 0x8C
constexpr uint32_t x_sequenceScalableExtension = 0x002373C0;   // size 0xC
constexpr uint32_t x_unknown_extension = 0x002373D0;   // size 0xC
constexpr uint32_t x_pictureSpatialScalableExtension = 0x002373E0;   // size 0xC
constexpr uint32_t x_pictureTemporalScalableExtension = 0x002373F0;   // size 0xC
constexpr uint32_t x_defStopDMA = 0x00237400;   // size 0xC
constexpr uint32_t x_defRestartDMA = 0x00237410;   // size 0xC
constexpr uint32_t x_motionComp0 = 0x00237420;   // size 0x368
constexpr uint32_t x_getAllRefs = 0x00237788;   // size 0x704
constexpr uint32_t x_getRef0 = 0x00237E90;   // size 0x41C
constexpr uint32_t x_doMC = 0x002382B0;   // size 0x21C
constexpr uint32_t x_rix_000 = 0x002384D0;   // size 0x74
constexpr uint32_t x_ri0_000 = 0x00238548;   // size 0x94
constexpr uint32_t x_rix_001 = 0x002385E0;   // size 0xB4
constexpr uint32_t x_ri0_001 = 0x00238698;   // size 0xCC
constexpr uint32_t x_rix_010 = 0x00238768;   // size 0xAC
constexpr uint32_t x_ri0_010 = 0x00238818;   // size 0xB8
constexpr uint32_t x_rix_011 = 0x002388D0;   // size 0xF8
constexpr uint32_t x_ri0_011 = 0x002389C8;   // size 0xFC
constexpr uint32_t x_rix_100 = 0x00238AC8;   // size 0x9C
constexpr uint32_t x_ri0_100 = 0x00238B68;   // size 0xAC
constexpr uint32_t x_rix_101 = 0x00238C18;   // size 0xDC
constexpr uint32_t x_ri0_101 = 0x00238CF8;   // size 0xE4
constexpr uint32_t x_rix_110 = 0x00238DE0;   // size 0xD4
constexpr uint32_t x_ri0_110 = 0x00238EB8;   // size 0xD0
constexpr uint32_t x_rix_111 = 0x00238F88;   // size 0x120
constexpr uint32_t x_ri0_111 = 0x002390A8;   // size 0x114
constexpr uint32_t x_copyAddRefImage = 0x002391C0;   // size 0x5C
constexpr uint32_t x_copyRefImage = 0x00239220;   // size 0x58
constexpr uint32_t x_ipuSetMPEG1 = 0x00239278;   // size 0x28
constexpr uint32_t x_waitBdecOut = 0x002392A0;   // size 0x1F0
constexpr uint32_t x_dmVector = 0x00239490;   // size 0x1C
constexpr uint32_t x_dualPrimeVector = 0x002394B0;   // size 0x184
constexpr uint32_t x_mbAddressIncrement = 0x00239638;   // size 0x110
constexpr uint32_t x_pictureData0 = 0x00239748;   // size 0x108
constexpr uint32_t x_sliceA0 = 0x00239850;   // size 0x12C
constexpr uint32_t x_slice0 = 0x00239980;   // size 0x1F0
constexpr uint32_t x_skipMB0 = 0x00239B70;   // size 0xBC
constexpr uint32_t x_decMB0 = 0x00239C30;   // size 0x4A8
constexpr uint32_t x_decode_motion_vector = 0x0023A0D8;   // size 0x88
constexpr uint32_t x_motionVectors = 0x0023A160;   // size 0x19C
constexpr uint32_t x_motionVector = 0x0023A300;   // size 0x14C
constexpr uint32_t x_sendIpuCommand = 0x0023A450;   // size 0x2C
constexpr uint32_t x_waitIpuIdle = 0x0023A480;   // size 0xA4
constexpr uint32_t x_waitIpuIdle64 = 0x0023A528;   // size 0xB0
constexpr uint32_t x_ipuVdec = 0x0023A5D8;   // size 0x178
constexpr uint32_t x_peepBit = 0x0023A750;   // size 0x108
constexpr uint32_t x_flushBuf = 0x0023A858;   // size 0xF0
constexpr uint32_t x_nextBit = 0x0023A948;   // size 0x150
constexpr uint32_t x_nextStartCode = 0x0023AA98;   // size 0x7C
constexpr uint32_t x_sliceB = 0x0023AB18;   // size 0x64
constexpr uint32_t x_nextHeader = 0x0023AB80;   // size 0x10C
constexpr uint32_t x_pictureHeader = 0x0023AC90;   // size 0xC4
constexpr uint32_t x_extensionAndUserData = 0x0023AD58;   // size 0x128
constexpr uint32_t x_pictureCodingExtension = 0x0023AE80;   // size 0x1F0
constexpr uint32_t x_extrainfo = 0x0023B070;   // size 0x48
constexpr uint32_t x_updateTempTackData = 0x0023B0B8;   // size 0x74
constexpr uint32_t x_groupOfPicturesHeader = 0x0023B130;   // size 0xA0
constexpr uint32_t x_quantMatrixExtension = 0x0023B1D0;   // size 0xC4
constexpr uint32_t x_pictureDisplayExtension = 0x0023B298;   // size 0xF4
constexpr uint32_t x_copyrightExtension = 0x0023B390;   // size 0x90
constexpr uint32_t x_decPicture = 0x0023B420;   // size 0xCC
constexpr uint32_t x_outputFrame = 0x0023B4F0;   // size 0x90
constexpr uint32_t x_updateRefImage = 0x0023B580;   // size 0x2B4
constexpr uint32_t x_isOutSizeOK = 0x0023B838;   // size 0xA0
constexpr uint32_t x_cpr8 = 0x0023B8D8;   // size 0x270
constexpr uint32_t x_markOutput = 0x0023BB48;   // size 0x28
constexpr uint32_t x_getPtsDtsFlags = 0x0023BB70;   // size 0x18C
constexpr uint32_t x_dispRefImage = 0x0023BD00;   // size 0x110
constexpr uint32_t x_dispRefImageField = 0x0023BE10;   // size 0x1B8
constexpr uint32_t receiveDataFromIPU = 0x0023BFC8;   // size 0x70
constexpr uint32_t x_doCSC = 0x0023C038;   // size 0x118
constexpr uint32_t x_ch3dmaCSC = 0x0023C150;   // size 0x174
constexpr uint32_t x_doCSC2 = 0x0023C2C8;   // size 0x1B0
constexpr uint32_t x_ch4dma = 0x0023C478;   // size 0x104
constexpr uint32_t x_csc_storeRefImage = 0x0023C580;   // size 0x274
constexpr uint32_t x_sysbitInit = 0x0023C7F8;   // size 0x34
constexpr uint32_t x_sysbitNext = 0x0023C830;   // size 0x1C
constexpr uint32_t x_sysbitFlush = 0x0023C850;   // size 0x98
constexpr uint32_t x_sysbitGet = 0x0023C8E8;   // size 0x4C
constexpr uint32_t x_sysbitMarker = 0x0023C938;   // size 0x44
constexpr uint32_t x_sysbitJump = 0x0023C980;   // size 0x54
constexpr uint32_t x_sysbitPtr = 0x0023C9D8;   // size 0x2C
constexpr uint32_t setD3_CHCR = 0x0023CA08;   // size 0x64
constexpr uint32_t setD4_CHCR_0023CA70 = 0x0023CA70;   // size 0x64
constexpr uint32_t sceIpuStopDMA = 0x0023CAD8;   // size 0xE4
constexpr uint32_t sceIpuRestartDMA = 0x0023CBC0;   // size 0x150
constexpr uint32_t sceIpuSync = 0x0023CD10;   // size 0x68
constexpr uint32_t setD4_CHCR_0023CD78 = 0x0023CD78;   // size 0x64
constexpr uint32_t sceIpuInit = 0x0023CDE0;   // size 0x238
constexpr uint32_t CB_DelayTh = 0x0023D018;   // size 0x24
constexpr uint32_t sceCdDelayThread = 0x0023D040;   // size 0x68
constexpr uint32_t sceCdCallback = 0x0023D0A8;   // size 0x54
constexpr uint32_t x_sceCd_cd_callback = 0x0023D100;   // size 0xA0
constexpr uint32_t x_Cdvd_cbLoop = 0x0023D1A0;   // size 0xBC
constexpr uint32_t sceCdInitEeCB = 0x0023D260;   // size 0xD4
constexpr uint32_t x_sceCd_cd_read_intr = 0x0023D338;   // size 0xA0
constexpr uint32_t cmd_sem_init = 0x0023D3D8;   // size 0x94
constexpr uint32_t cdvd_exit = 0x0023D470;   // size 0x80
constexpr uint32_t sceCdPOffCallback = 0x0023D4F0;   // size 0x6C
constexpr uint32_t x_sceCd_Poff_Intr = 0x0023D560;   // size 0x3C
constexpr uint32_t PowerOffCB = 0x0023D5A0;   // size 0x64
constexpr uint32_t sceCdSearchFile = 0x0023D608;   // size 0x2FC
constexpr uint32_t x_sceCd_ncmd_prechk = 0x0023D908;   // size 0x170
constexpr uint32_t sceCdNcmdDiskReady = 0x0023DA78;   // size 0x98
constexpr uint32_t sceCdSync = 0x0023DB10;   // size 0xA0
constexpr uint32_t sceCdSyncS = 0x0023DBB0;   // size 0x6C
constexpr uint32_t x_sceCd_scmd_prechk = 0x0023DC20;   // size 0x170
constexpr uint32_t sceCdInit = 0x0023DD90;   // size 0x2E0
constexpr uint32_t sceCdDiskReady = 0x0023E070;   // size 0x1F8
constexpr uint32_t sceCdMmode = 0x0023E268;   // size 0xC4
constexpr uint32_t sceCdGetReadPos = 0x0023E330;   // size 0x30
constexpr uint32_t sceCdRead = 0x0023E360;   // size 0x1E0
constexpr uint32_t sceCdGetError = 0x0023E540;   // size 0x98
constexpr uint32_t sceCdBreak = 0x0023E5D8;   // size 0xB8
constexpr uint32_t sceCdReadClock = 0x0023E690;   // size 0xF8
constexpr uint32_t sceCdStInit = 0x0023E788;   // size 0x2C
constexpr uint32_t sceCdStStart = 0x0023E7B8;   // size 0x34
constexpr uint32_t sceCdStSeekF = 0x0023E7F0;   // size 0x2C
constexpr uint32_t sceCdStSeek = 0x0023E820;   // size 0x2C
constexpr uint32_t sceCdStStop = 0x0023E850;   // size 0x38
constexpr uint32_t sceCdStRead = 0x0023E888;   // size 0x188
constexpr uint32_t sceCdStPause = 0x0023EA10;   // size 0x50
constexpr uint32_t sceCdStResume = 0x0023EA60;   // size 0x54
constexpr uint32_t sceCdStStat = 0x0023EAB8;   // size 0x48
constexpr uint32_t sceCdStream = 0x0023EB00;   // size 0x168
constexpr uint32_t abort = 0x0023EC68;   // size 0x20
constexpr uint32_t atoi = 0x0023EC88;   // size 0x28
constexpr uint32_t x__errno = 0x0023ECB0;   // size 0xC
constexpr uint32_t exit = 0x0023ECC0;   // size 0xB0
constexpr uint32_t malloc = 0x0023ED70;   // size 0x28
constexpr uint32_t free = 0x0023ED98;   // size 0x10
constexpr uint32_t malloc_extend_top = 0x0023EDA8;   // size 0x254
constexpr uint32_t x_malloc_r = 0x0023F000;   // size 0x778
constexpr uint32_t memcpy = 0x0023F778;   // size 0xB0
constexpr uint32_t memset = 0x0023F828;   // size 0xC0
constexpr uint32_t x__malloc_lock = 0x0023F8E8;   // size 0x8
constexpr uint32_t x__malloc_unlock = 0x0023F8F0;   // size 0x8
constexpr uint32_t x_printf_r = 0x0023F8F8;   // size 0x3C
constexpr uint32_t printf = 0x0023F938;   // size 0x50
constexpr uint32_t srand = 0x0023F988;   // size 0x10
constexpr uint32_t rand = 0x0023F998;   // size 0x30
constexpr uint32_t x_sbrk_r = 0x0023F9C8;   // size 0x5C
constexpr uint32_t x_init_signal_r = 0x0023FA28;   // size 0x70
constexpr uint32_t x_signal_r = 0x0023FA98;   // size 0x8C
constexpr uint32_t x_raise_r = 0x0023FB28;   // size 0xF0
constexpr uint32_t x__sigtramp_r = 0x0023FC18;   // size 0xB8
constexpr uint32_t raise = 0x0023FCD0;   // size 0x28
constexpr uint32_t signal = 0x0023FCF8;   // size 0x2C
constexpr uint32_t x_init_signal = 0x0023FD28;   // size 0x24
constexpr uint32_t x__sigtramp = 0x0023FD50;   // size 0x28
constexpr uint32_t x_kill_r = 0x0023FD78;   // size 0x5C
constexpr uint32_t x_getpid_r = 0x0023FDD8;   // size 0x1C
constexpr uint32_t x_sprintf_r = 0x0023FDF8;   // size 0x68
constexpr uint32_t sprintf = 0x0023FE60;   // size 0x70
constexpr uint32_t strcat = 0x0023FED0;   // size 0x12C
constexpr uint32_t strchr = 0x0023FFFC;   // size 0x190
constexpr uint32_t strcmp = 0x0024018C;   // size 0x144
constexpr uint32_t strcpy = 0x002402D0;   // size 0x114
constexpr uint32_t strlen = 0x002403E4;   // size 0x138
constexpr uint32_t strncat = 0x00240520;   // size 0x1AC
constexpr uint32_t strncmp = 0x002406D0;   // size 0x1B8
constexpr uint32_t strncpy = 0x00240888;   // size 0x1BC
constexpr uint32_t strpbrk = 0x00240A48;   // size 0x68
constexpr uint32_t x_strtol_r = 0x00240AB0;   // size 0x238
constexpr uint32_t strtol = 0x00240CE8;   // size 0x34
constexpr uint32_t x__sprint = 0x00240D20;   // size 0x44
constexpr uint32_t x__sbprintf = 0x00240D68;   // size 0xB4
constexpr uint32_t vfprintf = 0x00240E20;   // size 0x78
constexpr uint32_t x_vfprintf_r = 0x00240E98;   // size 0x1518
constexpr uint32_t cvt = 0x002423B0;   // size 0x1B0
constexpr uint32_t exponent = 0x00242560;   // size 0xE0
constexpr uint32_t x__swsetup = 0x00242640;   // size 0x10C
constexpr uint32_t quorem = 0x00242750;   // size 0x214
constexpr uint32_t x_dtoa_r = 0x00242968;   // size 0x11CC
constexpr uint32_t fflush = 0x00243B38;   // size 0x104
constexpr uint32_t std = 0x00243C40;   // size 0x58
constexpr uint32_t x__sfmoreglue = 0x00243C98;   // size 0x70
constexpr uint32_t x__sfp = 0x00243D08;   // size 0xE0
constexpr uint32_t x_cleanup_r = 0x00243DE8;   // size 0xC
constexpr uint32_t x_cleanup = 0x00243DF8;   // size 0xC
constexpr uint32_t x__sinit = 0x00243E08;   // size 0x8C
constexpr uint32_t x_free_r = 0x00243E98;   // size 0x31C
constexpr uint32_t x_malloc_trim_r = 0x002441B8;   // size 0x16C
constexpr uint32_t x__sfvwrite = 0x00244328;   // size 0x3D8
constexpr uint32_t x_fwalk = 0x00244700;   // size 0x94
constexpr uint32_t x_setlocale_r = 0x00244798;   // size 0x84
constexpr uint32_t x_localeconv_r = 0x00244820;   // size 0xC
constexpr uint32_t setlocale = 0x00244830;   // size 0x2C
constexpr uint32_t localeconv = 0x00244860;   // size 0x24
constexpr uint32_t x__smakebuf = 0x00244888;   // size 0x150
constexpr uint32_t x_mbtowc_r = 0x002449D8;   // size 0x3C
constexpr uint32_t memchr = 0x00244A14;   // size 0xE0
constexpr uint32_t memmove = 0x00244AF4;   // size 0x104
constexpr uint32_t x_Balloc = 0x00244BF8;   // size 0xA8
constexpr uint32_t x_Bfree = 0x00244CA0;   // size 0x2C
constexpr uint32_t x_multadd = 0x00244CD0;   // size 0x114
constexpr uint32_t x_s2b = 0x00244DE8;   // size 0x148
constexpr uint32_t x_hi0bits = 0x00244F30;   // size 0x84
constexpr uint32_t x_lo0bits = 0x00244FB8;   // size 0xC0
constexpr uint32_t x_i2b = 0x00245078;   // size 0x38
constexpr uint32_t x_multiply = 0x002450B0;   // size 0x22C
constexpr uint32_t x_pow5mult = 0x002452E0;   // size 0x100
constexpr uint32_t x_lshift = 0x002453E0;   // size 0x178
constexpr uint32_t x__mcmp = 0x00245558;   // size 0x68
constexpr uint32_t x__mdiff = 0x002455C0;   // size 0x190
constexpr uint32_t x_ulp = 0x00245750;   // size 0x98
constexpr uint32_t x_b2d = 0x002457E8;   // size 0x17C
constexpr uint32_t x_d2b = 0x00245968;   // size 0x17C
constexpr uint32_t x_ratio = 0x00245AE8;   // size 0xC0
constexpr uint32_t x_mprec_log10 = 0x00245BA8;   // size 0x6C
constexpr uint32_t isinf = 0x00245C18;   // size 0x48
constexpr uint32_t isnan = 0x00245C60;   // size 0x38
constexpr uint32_t x__sread = 0x00245C98;   // size 0x64
constexpr uint32_t x__swrite = 0x00245D00;   // size 0x80
constexpr uint32_t x__sseek = 0x00245D80;   // size 0x68
constexpr uint32_t x__sclose = 0x00245DE8;   // size 0x20
constexpr uint32_t x_write_r = 0x00245E08;   // size 0x60
constexpr uint32_t x_calloc_r = 0x00245E68;   // size 0xBC
constexpr uint32_t x_close_r = 0x00245F28;   // size 0x58
constexpr uint32_t x_fstat_r = 0x00245F80;   // size 0x5C
constexpr uint32_t x_lseek_r = 0x00245FE0;   // size 0x60
constexpr uint32_t x_read_r = 0x00246040;   // size 0x60
constexpr uint32_t RFU000_FullReset = 0x002460C0;   // size 0x10
constexpr uint32_t ResetEE = 0x002460D0;   // size 0x10
constexpr uint32_t SetGsCrt = 0x002460E0;   // size 0x10
constexpr uint32_t RFU003 = 0x002460F0;   // size 0x10
constexpr uint32_t x_Exit = 0x00246100;   // size 0x10
constexpr uint32_t RFU005 = 0x00246110;   // size 0x10
constexpr uint32_t x_LoadExecPS2 = 0x00246120;   // size 0x10
constexpr uint32_t x_ExecPS2 = 0x00246130;   // size 0x10
constexpr uint32_t RFU008 = 0x00246140;   // size 0x10
constexpr uint32_t RFU009 = 0x00246150;   // size 0x10
constexpr uint32_t AddSbusIntcHandler = 0x00246160;   // size 0x10
constexpr uint32_t RemoveSbusIntcHandler = 0x00246170;   // size 0x10
constexpr uint32_t Interrupt2Iop = 0x00246180;   // size 0x10
constexpr uint32_t SetVTLBRefillHandler = 0x00246190;   // size 0x10
constexpr uint32_t SetVCommonHandler = 0x002461A0;   // size 0x10
constexpr uint32_t SetVInterruptHandler = 0x002461B0;   // size 0x10
constexpr uint32_t AddIntcHandler = 0x002461C0;   // size 0x10
constexpr uint32_t AddIntcHandler2 = 0x002461D0;   // size 0x10
constexpr uint32_t RemoveIntcHandler = 0x002461E0;   // size 0x10
constexpr uint32_t AddDmacHandler = 0x002461F0;   // size 0x10
constexpr uint32_t AddDmacHandler2 = 0x00246200;   // size 0x10
constexpr uint32_t RemoveDmacHandler = 0x00246210;   // size 0x10
constexpr uint32_t x_EnableIntc = 0x00246220;   // size 0x10
constexpr uint32_t x_DisableIntc = 0x00246230;   // size 0x10
constexpr uint32_t x_EnableDmac = 0x00246240;   // size 0x10
constexpr uint32_t x_DisableDmac = 0x00246250;   // size 0x10
constexpr uint32_t SetAlarm = 0x00246260;   // size 0x10
constexpr uint32_t ReleaseAlarm = 0x00246270;   // size 0x10
constexpr uint32_t x_iEnableIntc = 0x00246280;   // size 0x10
constexpr uint32_t x_iDisableIntc = 0x00246290;   // size 0x10
constexpr uint32_t x_iEnableDmac = 0x002462A0;   // size 0x10
constexpr uint32_t x_iDisableDmac = 0x002462B0;   // size 0x10
constexpr uint32_t iSetAlarm = 0x002462C0;   // size 0x10
constexpr uint32_t iReleaseAlarm = 0x002462D0;   // size 0x10
constexpr uint32_t CreateThread = 0x002462E0;   // size 0x10
constexpr uint32_t DeleteThread = 0x002462F0;   // size 0x10
constexpr uint32_t StartThread = 0x00246300;   // size 0x10
constexpr uint32_t ExitThread = 0x00246310;   // size 0x10
constexpr uint32_t ExitDeleteThread = 0x00246320;   // size 0x10
constexpr uint32_t TerminateThread = 0x00246330;   // size 0x10
constexpr uint32_t iTerminateThread = 0x00246340;   // size 0x10
constexpr uint32_t DisableDispatchThread = 0x00246350;   // size 0x10
constexpr uint32_t EnableDispatchThread = 0x00246360;   // size 0x10
constexpr uint32_t ChangeThreadPriority = 0x00246370;   // size 0x10
constexpr uint32_t iChangeThreadPriority = 0x00246380;   // size 0x10
constexpr uint32_t RotateThreadReadyQueue = 0x00246390;   // size 0x10
constexpr uint32_t x_iRotateThreadReadyQueue = 0x002463A0;   // size 0x10
constexpr uint32_t ReleaseWaitThread = 0x002463B0;   // size 0x10
constexpr uint32_t iReleaseWaitThread = 0x002463C0;   // size 0x10
constexpr uint32_t GetThreadId = 0x002463D0;   // size 0x10
constexpr uint32_t ReferThreadStatus = 0x002463E0;   // size 0x10
constexpr uint32_t iReferThreadStatus = 0x002463F0;   // size 0x10
constexpr uint32_t SleepThread = 0x00246400;   // size 0x10
constexpr uint32_t WakeupThread = 0x00246410;   // size 0x10
constexpr uint32_t x_iWakeupThread = 0x00246420;   // size 0x10
constexpr uint32_t CancelWakeupThread = 0x00246430;   // size 0x10
constexpr uint32_t iCancelWakeupThread = 0x00246440;   // size 0x10
constexpr uint32_t SuspendThread = 0x00246450;   // size 0x10
constexpr uint32_t x_iSuspendThread = 0x00246460;   // size 0x10
constexpr uint32_t ResumeThread = 0x00246470;   // size 0x10
constexpr uint32_t iResumeThread = 0x00246480;   // size 0x10
constexpr uint32_t JoinThread = 0x00246490;   // size 0x10
constexpr uint32_t RFU060 = 0x002464A0;   // size 0x10
constexpr uint32_t RFU061 = 0x002464B0;   // size 0x10
constexpr uint32_t EndOfHeap = 0x002464C0;   // size 0x10
constexpr uint32_t RFU063 = 0x002464D0;   // size 0x10
constexpr uint32_t CreateSema = 0x002464E0;   // size 0x10
constexpr uint32_t DeleteSema = 0x002464F0;   // size 0x10
constexpr uint32_t SignalSema = 0x00246500;   // size 0x10
constexpr uint32_t iSignalSema = 0x00246510;   // size 0x10
constexpr uint32_t WaitSema = 0x00246520;   // size 0x10
constexpr uint32_t PollSema = 0x00246530;   // size 0x10
constexpr uint32_t iPollSema = 0x00246540;   // size 0x10
constexpr uint32_t ReferSemaStatus = 0x00246550;   // size 0x10
constexpr uint32_t iReferSemaStatus = 0x00246560;   // size 0x10
constexpr uint32_t RFU073 = 0x00246570;   // size 0x10
constexpr uint32_t SetOsdConfigParam = 0x00246580;   // size 0x10
constexpr uint32_t GetOsdConfigParam = 0x00246590;   // size 0x10
constexpr uint32_t GetGsHParam = 0x002465A0;   // size 0x10
constexpr uint32_t GetGsVParam = 0x002465B0;   // size 0x10
constexpr uint32_t SetGsHParam = 0x002465C0;   // size 0x10
constexpr uint32_t SetGsVParam = 0x002465D0;   // size 0x10
constexpr uint32_t RFU080_CreateEventFlag = 0x002465E0;   // size 0x10
constexpr uint32_t RFU081_DeleteEventFlag = 0x002465F0;   // size 0x10
constexpr uint32_t RFU082_SetEventFlag = 0x00246600;   // size 0x10
constexpr uint32_t RFU083_iSetEventFlag = 0x00246610;   // size 0x10
constexpr uint32_t RFU084_ClearEventFlag = 0x00246620;   // size 0x10
constexpr uint32_t RFU085_iClearEventFlag = 0x00246630;   // size 0x10
constexpr uint32_t RFU086_WaitEvnetFlag = 0x00246640;   // size 0x10
constexpr uint32_t RFU087_PollEvnetFlag = 0x00246650;   // size 0x10
constexpr uint32_t RFU088_iPollEvnetFlag = 0x00246660;   // size 0x10
constexpr uint32_t RFU089_ReferEventFlagStatus = 0x00246670;   // size 0x10
constexpr uint32_t RFU090_iReferEventFlagStatus = 0x00246680;   // size 0x10
constexpr uint32_t RFU091 = 0x00246690;   // size 0x10
constexpr uint32_t EnableIntcHandler = 0x002466A0;   // size 0x10
constexpr uint32_t iEnableIntcHandler = 0x002466B0;   // size 0x10
constexpr uint32_t DisableIntcHandler = 0x002466C0;   // size 0x10
constexpr uint32_t iDisableIntcHandler = 0x002466D0;   // size 0x10
constexpr uint32_t EnableDmacHandler = 0x002466E0;   // size 0x10
constexpr uint32_t iEnableDmacHandler = 0x002466F0;   // size 0x10
constexpr uint32_t DisableDmacHandler = 0x00246700;   // size 0x10
constexpr uint32_t iDisableDmacHandler = 0x00246710;   // size 0x10
constexpr uint32_t KSeg0 = 0x00246720;   // size 0x10
constexpr uint32_t EnableCache = 0x00246730;   // size 0x10
constexpr uint32_t DisableCache = 0x00246740;   // size 0x10
constexpr uint32_t GetCop0 = 0x00246750;   // size 0x10
constexpr uint32_t FlushCache = 0x00246760;   // size 0x10
constexpr uint32_t CpuConfig = 0x00246770;   // size 0x10
constexpr uint32_t iGetCop0 = 0x00246780;   // size 0x10
constexpr uint32_t iFlushCache = 0x00246790;   // size 0x10
constexpr uint32_t iCpuConfig = 0x002467A0;   // size 0x10
constexpr uint32_t sceSifStopDma = 0x002467B0;   // size 0x10
constexpr uint32_t SetCPUTimerHandler = 0x002467C0;   // size 0x10
constexpr uint32_t SetCPUTimer = 0x002467D0;   // size 0x10
constexpr uint32_t SetOsdConfigParam2 = 0x002467E0;   // size 0x10
constexpr uint32_t GetOsdConfigParam2 = 0x002467F0;   // size 0x10
constexpr uint32_t GsGetIMR = 0x00246800;   // size 0x10
constexpr uint32_t iGsGetIMR = 0x00246810;   // size 0x10
constexpr uint32_t GsPutIMR = 0x00246820;   // size 0x10
constexpr uint32_t iGsPutIMR = 0x00246830;   // size 0x10
constexpr uint32_t SetPgifHandler = 0x00246840;   // size 0x10
constexpr uint32_t SetVSyncFlag = 0x00246850;   // size 0x10
constexpr uint32_t RFU116 = 0x00246860;   // size 0x10
constexpr uint32_t x_print = 0x00246870;   // size 0x10
constexpr uint32_t sceSifDmaStat = 0x00246880;   // size 0x10
constexpr uint32_t isceSifDmaStat = 0x00246890;   // size 0x10
constexpr uint32_t sceSifSetDma = 0x002468A0;   // size 0x10
constexpr uint32_t isceSifSetDma = 0x002468B0;   // size 0x10
constexpr uint32_t sceSifSetDChain = 0x002468C0;   // size 0x10
constexpr uint32_t isceSifSetDChain = 0x002468D0;   // size 0x10
constexpr uint32_t sceSifSetReg = 0x002468E0;   // size 0x10
constexpr uint32_t sceSifGetReg = 0x002468F0;   // size 0x10
constexpr uint32_t x_ExecOSD = 0x00246900;   // size 0x10
constexpr uint32_t Deci2Call = 0x00246910;   // size 0x10
constexpr uint32_t PSMode = 0x00246920;   // size 0x10
constexpr uint32_t MachineType = 0x00246930;   // size 0x10
constexpr uint32_t GetMemorySize = 0x00246940;   // size 0x10
constexpr uint32_t x_InitTLB = 0x00246950;   // size 0x10
constexpr uint32_t sceResetttyinit = 0x00246960;   // size 0xC
constexpr uint32_t VSync = 0x00246970;   // size 0x40
constexpr uint32_t VSync2 = 0x002469B0;   // size 0x60
constexpr uint32_t write = 0x00246A10;   // size 0x7C
constexpr uint32_t read = 0x00246A90;   // size 0x74
constexpr uint32_t open = 0x00246B08;   // size 0x28
constexpr uint32_t close = 0x00246B30;   // size 0x8
constexpr uint32_t ioctl = 0x00246B38;   // size 0x8
constexpr uint32_t lseek = 0x00246B40;   // size 0x8
constexpr uint32_t sbrk = 0x00246B48;   // size 0xAC
constexpr uint32_t x_isatty = 0x00246BF8;   // size 0x8
constexpr uint32_t fstat = 0x00246C00;   // size 0x14
constexpr uint32_t x_getpid = 0x00246C18;   // size 0x8
constexpr uint32_t kill = 0x00246C20;   // size 0x28
constexpr uint32_t stat = 0x00246C48;   // size 0x28
constexpr uint32_t x_unlink = 0x00246C70;   // size 0x28
constexpr uint32_t x_sceSDC = 0x00246C98;   // size 0xA4
constexpr uint32_t SyncDCache = 0x00246D40;   // size 0x7C
constexpr uint32_t iSyncDCache = 0x00246DC0;   // size 0x14
constexpr uint32_t x_sceIDC = 0x00246DD8;   // size 0xA4
constexpr uint32_t InvalidDCache = 0x00246E80;   // size 0x7C
constexpr uint32_t iInvalidDCache = 0x00246F00;   // size 0x14
constexpr uint32_t DisableIntc = 0x00246F18;   // size 0x68
constexpr uint32_t EnableIntc = 0x00246F80;   // size 0x68
constexpr uint32_t DisableDmac = 0x00246FE8;   // size 0x68
constexpr uint32_t EnableDmac = 0x00247050;   // size 0x68
constexpr uint32_t iEnableIntc = 0x002470B8;   // size 0x20
constexpr uint32_t iDisableIntc = 0x002470D8;   // size 0x20
constexpr uint32_t iEnableDmac = 0x002470F8;   // size 0x20
constexpr uint32_t iDisableDmac = 0x00247118;   // size 0x20
constexpr uint32_t setup_00247138 = 0x00247138;   // size 0x10
constexpr uint32_t Copy_00247148 = 0x00247148;   // size 0x10
constexpr uint32_t kCopy_00247158 = 0x00247158;   // size 0x38
constexpr uint32_t GetEntryAddress_00247190 = 0x00247190;   // size 0x10
constexpr uint32_t InitAlarm = 0x002471A0;   // size 0xD4
constexpr uint32_t topThread = 0x00247278;   // size 0xD8
constexpr uint32_t InitThread = 0x00247350;   // size 0xD4
constexpr uint32_t iWakeupThread = 0x00247428;   // size 0x94
constexpr uint32_t iRotateThreadReadyQueue = 0x002474C0;   // size 0x7C
constexpr uint32_t iSuspendThread = 0x00247540;   // size 0x98
constexpr uint32_t QueueInit = 0x002475D8;   // size 0x24
constexpr uint32_t QueuePeekWriteDone = 0x00247600;   // size 0x3C
constexpr uint32_t QueuePeekReadDone = 0x00247640;   // size 0x3C
constexpr uint32_t sceTtyHandler = 0x00247680;   // size 0x194
constexpr uint32_t sceTtyWrite = 0x00247818;   // size 0x150
constexpr uint32_t sceTtyRead = 0x00247968;   // size 0xD0
constexpr uint32_t sceTtyInit = 0x00247A38;   // size 0xBC
constexpr uint32_t kputchar = 0x00247AF8;   // size 0x38
constexpr uint32_t deci2Putchar = 0x00247B30;   // size 0xB0
constexpr uint32_t serialPutchar = 0x00247BE0;   // size 0x34
constexpr uint32_t ftoi = 0x00247C18;   // size 0x90
constexpr uint32_t printfloat = 0x00247CA8;   // size 0x168
constexpr uint32_t x_printf = 0x00247E10;   // size 0x5C8
constexpr uint32_t kprintf = 0x002483D8;   // size 0x38
constexpr uint32_t scePrintf = 0x00248410;   // size 0x60
constexpr uint32_t x_set_sreg = 0x00248470;   // size 0x1C
constexpr uint32_t x_change_addr = 0x00248490;   // size 0xC
constexpr uint32_t sceSifGetSreg = 0x002484A0;   // size 0x18
constexpr uint32_t sceSifSetSreg = 0x002484B8;   // size 0x1C
constexpr uint32_t sceSifGetDataTable = 0x002484D8;   // size 0xC
constexpr uint32_t sceSifInitCmd = 0x002484E8;   // size 0x280
constexpr uint32_t sceSifExitCmd = 0x00248768;   // size 0x34
constexpr uint32_t sceSifSetCmdBuffer = 0x002487A0;   // size 0x18
constexpr uint32_t sceSifSetSysCmdBuffer = 0x002487B8;   // size 0x18
constexpr uint32_t sceSifAddCmdHandler = 0x002487D0;   // size 0x2C
constexpr uint32_t sceSifRemoveCmdHandler = 0x00248800;   // size 0x28
constexpr uint32_t x_sceSifSendCmd = 0x00248828;   // size 0x134
constexpr uint32_t sceSifSendCmd = 0x00248960;   // size 0x3C
constexpr uint32_t isceSifSendCmd = 0x002489A0;   // size 0x3C
constexpr uint32_t x_sceSifCmdIntrHdlr = 0x002489E0;   // size 0x128
constexpr uint32_t sceSifWriteBackDCache = 0x00248B08;   // size 0xB0
constexpr uint32_t sceSifInitRpc = 0x00248BB8;   // size 0x19C
constexpr uint32_t sceSifExitRpc = 0x00248D58;   // size 0x24
constexpr uint32_t x_sceRpcGetPacket = 0x00248D80;   // size 0xA8
constexpr uint32_t x_sceRpcFreePacket = 0x00248E28;   // size 0x1C
constexpr uint32_t x_sceRpcGetFPacket = 0x00248E48;   // size 0x30
constexpr uint32_t x_sceRpcGetFPacket2 = 0x00248E78;   // size 0x40
constexpr uint32_t x_request_end = 0x00248EB8;   // size 0xB4
constexpr uint32_t x_request_rdata = 0x00248F70;   // size 0x60
constexpr uint32_t sceSifGetOtherData = 0x00248FD0;   // size 0x158
constexpr uint32_t x_search_svdata = 0x00249128;   // size 0x4C
constexpr uint32_t x_request_bind = 0x00249178;   // size 0xB0
constexpr uint32_t sceSifBindRpc = 0x00249228;   // size 0x140
constexpr uint32_t x_request_call = 0x00249368;   // size 0x8C
constexpr uint32_t sceSifCallRpc = 0x002493F8;   // size 0x1EC
constexpr uint32_t sceSifCheckStatRpc = 0x002495E8;   // size 0x3C
constexpr uint32_t sceSifSetRpcQueue = 0x00249628;   // size 0x94
constexpr uint32_t sceSifRegisterRpc = 0x002496C0;   // size 0xD0
constexpr uint32_t sceSifRemoveRpc = 0x00249790;   // size 0x94
constexpr uint32_t sceSifRemoveRpcQueue = 0x00249828;   // size 0x90
constexpr uint32_t sceSifGetNextRequest = 0x002498B8;   // size 0x58
constexpr uint32_t sceSifExecRequest = 0x00249910;   // size 0x1C4
constexpr uint32_t sceSifRpcLoop = 0x00249AD8;   // size 0x40
constexpr uint32_t x_sceFsIobSemaMK = 0x00249B18;   // size 0x5C
constexpr uint32_t new_iob = 0x00249B78;   // size 0x88
constexpr uint32_t get_iob = 0x00249C00;   // size 0x6C
constexpr uint32_t x_sceFs_Rcv_Intr = 0x00249C70;   // size 0x3C0
constexpr uint32_t x_sceFsSemInit = 0x0024A030;   // size 0x4C
constexpr uint32_t x_sceFsWaitS = 0x0024A080;   // size 0x2C
constexpr uint32_t x_sceFsSigSema = 0x0024A0B0;   // size 0xC
constexpr uint32_t scePowerOffHandler = 0x0024A0C0;   // size 0x90
constexpr uint32_t x_sceFs_Poff_Intr = 0x0024A150;   // size 0x2C
constexpr uint32_t sceFsInit = 0x0024A180;   // size 0x1C8
constexpr uint32_t x_fs_version = 0x0024A348;   // size 0x8C
constexpr uint32_t sceFsReset = 0x0024A3D8;   // size 0x34
constexpr uint32_t sceOpen = 0x0024A410;   // size 0x284
constexpr uint32_t sceClose = 0x0024A698;   // size 0x17C
constexpr uint32_t sceLseek = 0x0024A818;   // size 0x238
constexpr uint32_t sceRead = 0x0024AA50;   // size 0x270
constexpr uint32_t sceWrite = 0x0024ACC0;   // size 0x2C0
constexpr uint32_t sceIoctl = 0x0024AF80;   // size 0x34C
constexpr uint32_t sceIoctl2 = 0x0024B2D0;   // size 0x1DC
constexpr uint32_t x_sceCallCode = 0x0024B4B0;   // size 0x1A4
constexpr uint32_t sceRemove = 0x0024B658;   // size 0x1C
constexpr uint32_t sceMkdir = 0x0024B678;   // size 0x1AC
constexpr uint32_t sceRmdir = 0x0024B828;   // size 0x1C
constexpr uint32_t sceFormat = 0x0024B848;   // size 0x26C
constexpr uint32_t sceAddDrv = 0x0024BAB8;   // size 0x120
constexpr uint32_t sceDelDrv = 0x0024BBD8;   // size 0x1C
constexpr uint32_t sceDopen = 0x0024BBF8;   // size 0xC8
constexpr uint32_t sceDclose = 0x0024BCC0;   // size 0x168
constexpr uint32_t sceDread = 0x0024BE28;   // size 0x158
constexpr uint32_t sceGetstat = 0x0024BF80;   // size 0x1A0
constexpr uint32_t sceChstat = 0x0024C120;   // size 0x23C
constexpr uint32_t sceRename = 0x0024C360;   // size 0x1F0
constexpr uint32_t sceChdir = 0x0024C550;   // size 0x1C
constexpr uint32_t sceSync = 0x0024C570;   // size 0x198
constexpr uint32_t sceMount = 0x0024C708;   // size 0x26C
constexpr uint32_t sceUmount = 0x0024C978;   // size 0x1C
constexpr uint32_t sceLseek64 = 0x0024C998;   // size 0x238
constexpr uint32_t sceDevctl = 0x0024CBD0;   // size 0x238
constexpr uint32_t sceSymlink = 0x0024CE08;   // size 0x1E0
constexpr uint32_t sceReadlink = 0x0024CFE8;   // size 0x1C8
constexpr uint32_t sceSifInitIopHeap = 0x0024D1B0;   // size 0x88
constexpr uint32_t sceSifAllocIopHeap = 0x0024D238;   // size 0x70
constexpr uint32_t sceSifAllocSysMemory = 0x0024D2A8;   // size 0x80
constexpr uint32_t sceSifFreeSysMemory = 0x0024D328;   // size 0x78
constexpr uint32_t sceSifFreeIopHeap = 0x0024D3A0;   // size 0x1C
constexpr uint32_t sceSifLoadIopHeap = 0x0024D3C0;   // size 0xF0
constexpr uint32_t x_lf_bind = 0x0024D4B0;   // size 0x100
constexpr uint32_t x_lf_version = 0x0024D5B0;   // size 0x8C
constexpr uint32_t sceSifLoadFileReset = 0x0024D640;   // size 0x38
constexpr uint32_t x_sceSifLoadModuleBuffer = 0x0024D678;   // size 0x208
constexpr uint32_t sceSifStopModule = 0x0024D880;   // size 0x208
constexpr uint32_t sceSifUnloadModule = 0x0024DA88;   // size 0x90
constexpr uint32_t sceSifSearchModuleByName = 0x0024DB18;   // size 0xA0
constexpr uint32_t sceSifSearchModuleByAddress = 0x0024DBB8;   // size 0x90
constexpr uint32_t sceSifLoadModuleBuffer = 0x0024DC48;   // size 0x1C
constexpr uint32_t sceSifLoadStartModuleBuffer = 0x0024DC68;   // size 0x1C
constexpr uint32_t x_sceSifLoadModule = 0x0024DC88;   // size 0x224
constexpr uint32_t sceSifLoadModule = 0x0024DEB0;   // size 0x20
constexpr uint32_t sceSifLoadStartModule = 0x0024DED0;   // size 0x1C
constexpr uint32_t x_sceSifLoadElfPart = 0x0024DEF0;   // size 0x104
constexpr uint32_t sceSifLoadElfPart = 0x0024DFF8;   // size 0x1C
constexpr uint32_t sceSifLoadElf = 0x0024E018;   // size 0x28
constexpr uint32_t sceSifGetIopAddr = 0x0024E040;   // size 0xEC
constexpr uint32_t sceSifSetIopAddr = 0x0024E130;   // size 0xDC
constexpr uint32_t sceSifResetIop = 0x0024E210;   // size 0x13C
constexpr uint32_t sceSifIsAliveIop = 0x0024E350;   // size 0x28
constexpr uint32_t sceSifSyncIop = 0x0024E378;   // size 0x38
constexpr uint32_t sceSifRebootIop = 0x0024E3B0;   // size 0x110
constexpr uint32_t DIntr = 0x0024E4C0;   // size 0x48
constexpr uint32_t EIntr = 0x0024E508;   // size 0x18
constexpr uint32_t supplement_crt0 = 0x0024E520;   // size 0x48
constexpr uint32_t kFindAddress = 0x0024E568;   // size 0x34
constexpr uint32_t FindAddress = 0x0024E5A0;   // size 0x10
constexpr uint32_t GetSystemCallTableEntry = 0x0024E5B0;   // size 0x3C
constexpr uint32_t setup_0024E5F0 = 0x0024E5F0;   // size 0x10
constexpr uint32_t x_setup = 0x0024E600;   // size 0x1C
constexpr uint32_t x_InitSys = 0x0024E620;   // size 0x3C
constexpr uint32_t setup_0024E660 = 0x0024E660;   // size 0x10
constexpr uint32_t Copy_0024E670 = 0x0024E670;   // size 0x10
constexpr uint32_t kCopy_0024E680 = 0x0024E680;   // size 0x38
constexpr uint32_t GetEntryAddress_0024E6B8 = 0x0024E6B8;   // size 0x10
constexpr uint32_t PatchIsNeeded = 0x0024E6C8;   // size 0x68
constexpr uint32_t InitExecPS2 = 0x0024E730;   // size 0xAC
constexpr uint32_t TerminateLibrary = 0x0024E7E0;   // size 0x8
constexpr uint32_t ExecPS2 = 0x0024E7E8;   // size 0x5C
constexpr uint32_t LoadExecPS2 = 0x0024E848;   // size 0x48
constexpr uint32_t Exit = 0x0024E890;   // size 0x28
constexpr uint32_t ExecOSD = 0x0024E8B8;   // size 0x38
constexpr uint32_t sceDeci2Open = 0x0024E8F0;   // size 0x44
constexpr uint32_t sceDeci2Close = 0x0024E938;   // size 0x24
constexpr uint32_t sceDeci2ReqSend = 0x0024E960;   // size 0x30
constexpr uint32_t sceDeci2Poll = 0x0024E990;   // size 0x24
constexpr uint32_t sceDeci2ExRecv = 0x0024E9B8;   // size 0x34
constexpr uint32_t sceDeci2ExSend = 0x0024E9F0;   // size 0x34
constexpr uint32_t sceDeci2ExReqSend = 0x0024EA28;   // size 0x30
constexpr uint32_t sceDeci2ExLock = 0x0024EA58;   // size 0x24
constexpr uint32_t sceDeci2ExUnLock = 0x0024EA80;   // size 0x24
constexpr uint32_t kputs = 0x0024EAA8;   // size 0x24
constexpr uint32_t SetTLBHandler = 0x0024EAD0;   // size 0x60
constexpr uint32_t SetDebugHandler = 0x0024EB30;   // size 0x84
constexpr uint32_t Copy_0024EBB8 = 0x0024EBB8;   // size 0x10
constexpr uint32_t kCopy_0024EBC8 = 0x0024EBC8;   // size 0x38
constexpr uint32_t GetEntryAddress_0024EC00 = 0x0024EC00;   // size 0x10
constexpr uint32_t setup_0024EC10 = 0x0024EC10;   // size 0x10
constexpr uint32_t InitTLBFunctions = 0x0024EC20;   // size 0xB4
constexpr uint32_t PutTLBEntry = 0x0024ECD8;   // size 0x10
constexpr uint32_t iPutTLBEntry = 0x0024ECE8;   // size 0x10
constexpr uint32_t x_SetTLBEntry = 0x0024ECF8;   // size 0x10
constexpr uint32_t SetTLBEntry = 0x0024ED08;   // size 0x30
constexpr uint32_t iSetTLBEntry = 0x0024ED38;   // size 0x10
constexpr uint32_t GetTLBEntry = 0x0024ED48;   // size 0x10
constexpr uint32_t iGetTLBEntry = 0x0024ED58;   // size 0x10
constexpr uint32_t ProbeTLBEntry = 0x0024ED68;   // size 0x10
constexpr uint32_t iProbeTLBEntry = 0x0024ED78;   // size 0x10
constexpr uint32_t ExpandScratchPad = 0x0024ED88;   // size 0x10
constexpr uint32_t InitTLB = 0x0024ED98;   // size 0x40
constexpr uint32_t InitTLB32MB = 0x0024EDD8;   // size 0x1F4
constexpr uint32_t x_kTLBException = 0x0024F000;   // size 0x11C
constexpr uint32_t x_kExitTLBHandler = 0x0024F140;   // size 0xF4
constexpr uint32_t x_kDebugException = 0x0024F240;   // size 0x11C
constexpr uint32_t x__divdi3 = 0x0024F360;   // size 0x6EC
constexpr uint32_t x__fixsfdi = 0x0024FA50;   // size 0x3C
constexpr uint32_t x__fixunsdfdi = 0x0024FA90;   // size 0xEC
constexpr uint32_t x__fixunssfdi = 0x0024FB80;   // size 0xF4
constexpr uint32_t x__floatdidf = 0x0024FC78;   // size 0x98
constexpr uint32_t x__floatdisf = 0x0024FD10;   // size 0xE0
constexpr uint32_t x__moddi3 = 0x0024FDF0;   // size 0x668
constexpr uint32_t x__udivdi3 = 0x00250458;   // size 0x5D0
constexpr uint32_t x__umoddi3 = 0x00250A28;   // size 0x540
constexpr uint32_t memcmp = 0x00250F68;   // size 0x94
constexpr uint32_t sn_reg_frame = 0x00250FFC;   // size 0x14
constexpr uint32_t sn_dereg_frame = 0x00251014;   // size 0xC
} // namespace fn

// Dados por endereco (nome do ELF, sem troca de caracteres), para dar nome a um endereco: symtab::find(a).
namespace symtab {
struct Entry { uint32_t addr, size; const char *name; };
inline const Entry kData[] = {
    {0x0024F100, 0x0, "_xlaunch"},
    {0x0025C480, 0x3C, "attackMessages"},
    {0x0025C4C0, 0x30, "retreatPts"},
    {0x0025C4F0, 0x4060, "_10AiPathTool$instance"},
    {0x00260550, 0x10, "ELEVATOR_LOWER_A.2511"},
    {0x00260560, 0x10, "ELEVATOR_LOWER_B.2512"},
    {0x00260570, 0x10, "DROP_ZONE_A.2516"},
    {0x00260580, 0x10, "DROP_ZONE_B.2517"},
    {0x00260590, 0x80, "airportPts"},
    {0x00260610, 0x90, "debrisOffset.2526"},
    {0x002606A0, 0x18, "_11AnimContact$s_locatorId"},
    {0x002606C0, 0x10, "antHill"},
    {0x002606D0, 0x44, "_14DodgeBallLevel$instance"},
    {0x00260720, 0x28800, "_6Debris$m_debrisPool"},
    {0x00288F20, 0x490, "finalBoss"},
    {0x002893B0, 0x20, "attractPts"},
    {0x00289400, 0xC0C0, "resetObj"},
    {0x002954C0, 0xC0C0, "globalResetObj"},
    {0x002A1580, 0x4B0, "monsterStack"},
    {0x002A1A40, 0x80, "monsterNametomonsterId"},
    {0x002A1AC0, 0x34, "PO_1PFreeForAll_Levels"},
    {0x002A1AF8, 0x34, "PO_MPFreeForAll_Levels"},
    {0x002A1B30, 0x18, "PO_MPElimination_Levels"},
    {0x002A1B48, 0x24, "PO_1PEndurance_Levels"},
    {0x002A1B70, 0xC, "PO_1PFreeForAll"},
    {0x002A1B80, 0xC, "PO_MPFreeForAll"},
    {0x002A1B90, 0xC, "PO_1PEndurance"},
    {0x002A1BA0, 0xC, "PO_MPElimination"},
    {0x002A1BB0, 0x240, "levelMonsters"},
    {0x002A1DF0, 0xC0, "levelMonsterModels"},
    {0x002A1EB0, 0x840, "monsterSelectMode"},
    {0x002A26F0, 0x10, "s_gunOffset_002A26F0"},
    {0x002A2700, 0x900, "_9HomingBug$s_bugPool"},
    {0x002A3000, 0x30, "_3Hud$m_letterbox"},
    {0x002A3030, 0x120, "s_hudGraphicData"},
    {0x002A3150, 0x9C, "gMessages"},
    {0x002A31F0, 0xF0, "centralParkMessagesA"},
    {0x002A32E0, 0xF0, "centralParkMessagesB"},
    {0x002A33D0, 0xF0, "centralParkMessagesC"},
    {0x002A34C0, 0x78, "canyon2Messages"},
    {0x002A3538, 0x60, "threeMileMessages"},
    {0x002A3598, 0x18, "capitolMessages"},
    {0x002A35B0, 0x14, "feelsLikeTheFirstTime"},
    {0x002A35D0, 0x10, "textBoxOffset.2724"},
    {0x002A35E0, 0x10, "gHudPos"},
    {0x002A35F0, 0xB0, "volcanoTargets"},
    {0x002A36A0, 0x40, "s_bgcolor"},
    {0x002A36E0, 0x30, "s_lightdir"},
    {0x002A3710, 0x30, "s_lightcol"},
    {0x002A3740, 0x10, "s_ambient"},
    {0x002A3750, 0x38, "_6McSync$global"},
    {0x002A3790, 0x11B0, "_9McPageMgr$global"},
    {0x002A4940, 0x30, "stNull"},
    {0x002A4970, 0xD, "stProductDirectory"},
    {0x002A4980, 0x14, "stGameName"},
    {0x002A4998, 0x30, "stAvailSuffix"},
    {0x002A49C8, 0x30, "stAvailUnformatted"},
    {0x002A49F8, 0x60, "stAvail"},
    {0x002A4A58, 0x60, "stPorts"},
    {0x002A4AB8, 0xC0, "stMedia"},
    {0x002A4B78, 0x120, "stPrompt"},
    {0x002A4C98, 0x120, "stWhichUnlock"},
    {0x002A4DB8, 0x270, "stInfo"},
    {0x002A5028, 0x60, "stToCard"},
    {0x002A5088, 0x60, "stFromCard"},
    {0x002A50E8, 0x60, "stWhatsOnTheCard"},
    {0x002A5148, 0x60, "stActivityWarning"},
    {0x002A51A8, 0x30, "stSure"},
    {0x002A51D8, 0x120, "stX"},
    {0x002A52F8, 0x120, "stT"},
    {0x002A5418, 0x120, "stC"},
    {0x002A5538, 0x120, "stS"},
    {0x002A5658, 0x60, "stTitles"},
    {0x002A56B8, 0x180, "stMCUtilStatus"},
    {0x002A5840, 0x10, "arcCLUT"},
    {0x002A5850, 0x40, "UpMatrix"},
    {0x002A5890, 0x13B4, "_13AnimBlendPool$s_pool"},
    {0x002A6C80, 0x284, "_9MonsterMc$global"},
    {0x002A6F08, 0x278, "_10McBootPage$global"},
    {0x002A7180, 0x278, "_18McCheckForSettings$global"},
    {0x002A73F8, 0x274, "_14McLoadSettings$global"},
    {0x002A7670, 0x278, "_12McChooseSlot$global"},
    {0x002A78E8, 0x27C, "_12McCheckSpace$global"},
    {0x002A7B68, 0x274, "_13McCheckFormat$global"},
    {0x002A7DE0, 0x278, "_16McCheckOverwrite$global"},
    {0x002A8058, 0x28C, "_14McSaveSettings$global"},
    {0x002A82E8, 0x274, "_15McSaveAdventure$global"},
    {0x002A8560, 0x274, "_20McSaveAdventureBlank$global"},
    {0x002A8800, 0x540, "_16McSelectSaveSlot$global"},
    {0x002A8D40, 0x284, "_11McAskToSave$global"},
    {0x002A8FC8, 0x274, "_11McAskIfSure$global"},
    {0x002A9240, 0x274, "_11McErrorPage$global"},
    {0x002A94B8, 0x278, "_20McCheckForSweetTooth$global"},
    {0x002A9730, 0x274, "_13McCardRemoved$global"},
    {0x002A99C0, 0x7C0, "_15McPrepBoxScreen$global"},
    {0x002AA180, 0x274, "_15McLoadGameBoxes$global"},
    {0x002AA400, 0x540, "_12McSelectReel$global"},
    {0x002AA940, 0x124, "s_animQueue"},
    {0x002AAA68, 0xD, "s_stStoryGameName"},
    {0x002AAA78, 0x14, "s_gameTitle"},
    {0x002AAA90, 0xD, "s_productDirectory"},
    {0x002AAAA0, 0x9, "s_iconFilename"},
    {0x002AAAB0, 0x110, "ENGLISH_MEMCARD_STRINGS"},
    {0x002AABC0, 0x40, "g_triggerFunctions"},
    {0x002AAC00, 0x4060, "_8PathTool$instance"},
    {0x002AEC60, 0x40, "_12LevelPickups$s_highlightPickup"},
    {0x002AECA0, 0xC, "_12LevelPickups$s_activeList"},
    {0x002AECB0, 0xC, "_12LevelPickups$s_inUseList"},
    {0x002AECC0, 0xC, "_12LevelPickups$s_inFlightList"},
    {0x002AECD0, 0xC, "_12LevelPickups$s_regenList"},
    {0x002AECE0, 0xC, "_12LevelPickups$s_deadList"},
    {0x002AECF0, 0x2A4, "_12LevelPickups$tankerShock"},
    {0x002AEF98, 0x38, "headAttacks"},
    {0x002AEFD0, 0x50, "s_powerUpCount"},
    {0x002AF020, 0x50, "s_tweakMinRegenCount"},
    {0x002AF070, 0x50, "s_tweakMaxRegenCount"},
    {0x002AF0C0, 0x4060, "_11PowerUpTool$instance"},
    {0x002B3120, 0xC, "gRtReturn"},
    {0x002B3130, 0x14, "MainMovie"},
    {0x002B3148, 0x58, "screenTitles"},
    {0x002B31A0, 0x58, "screenInstructions"},
    {0x002B31F8, 0x5C, "movieFileNames"},
    {0x002B3258, 0x124, "animQueue"},
    {0x002B3380, 0x10, "monsterIsSelected.2829"},
    {0x002B3390, 0x40, "promptString.2840"},
    {0x002B33D0, 0x40, "promptString.2960"},
    {0x002B3410, 0x160, "currentSelection"},
    {0x002B3570, 0x30, "NUM_LIVES"},
    {0x002B35A0, 0x122800, "shellGame"},
    {0x003D5DA0, 0x3EFE0, "shellDestructibles"},
    {0x00414D80, 0x31C0, "shellInfo"},
    {0x00417F40, 0x40, "uiResetFilename"},
    {0x00417F80, 0xF, "whichLevel"},
    {0x00417F90, 0x1FE0, "_9ShockWave$m_wavePool"},
    {0x00419F70, 0xCC, "gLoopManager"},
    {0x0041A040, 0x10, "gStackDestroyed"},
    {0x0041A050, 0x64, "gSoundBanks"},
    {0x0041A0B8, 0x440, "_10SpecFxAnim$s_fxPool"},
    {0x0041A4F8, 0x40, "_11StartPoints$s_pointTypes"},
    {0x0041A540, 0x4060, "_14StartPointTool$instance"},
    {0x0041E5A0, 0x4380, "_11StickShaker$m_shakePool"},
    {0x00422920, 0x10, "s_gunOffset_00422920"},
    {0x00422930, 0xA8, "_12TurretPickup$s_config"},
    {0x004229E0, 0x40, "stackStart"},
    {0x00422A20, 0x50, "stack1End"},
    {0x00422A70, 0x80, "stack2End"},
    {0x00422AF0, 0x70, "stack3End"},
    {0x00422B60, 0x90, "stack4End"},
    {0x00422BF0, 0x10, "centerTower"},
    {0x00422C00, 0x10B0, "plantBoss"},
    {0x00423CB0, 0x10, "tidalWaveAnim"},
    {0x00423CC0, 0x20, "ufoPts"},
    {0x00423CE0, 0x50, "alienSaucers"},
    {0x00423D40, 0x98, "myMovie"},
    {0x00423DD8, 0x40, "oldScreens"},
    {0x00423E18, 0x28, "chosenAI"},
    {0x00423E40, 0xC0C0, "userintResetObj"},
    {0x0042FF00, 0x10, "ticString"},
    {0x0042FF10, 0xC, "gUserintReturn"},
    {0x0042FF20, 0xD0, "bb"},
    {0x00430000, 0x1990, "devScreen"},
    {0x00431990, 0x1CF0, "netScreen"},
    {0x00433680, 0x18, "whichRoleStrings"},
    {0x00433698, 0xA, "numAiStrings"},
    {0x004336A8, 0x2C, "whichPlayerValues"},
    {0x004336D8, 0x84, "whichPlayerStrings"},
    {0x00433760, 0x84, "whichAiStrings"},
    {0x004337E8, 0x30, "whichModeStrings"},
    {0x00433818, 0x34, "devUi"},
    {0x00433850, 0x300, "whichLevelStrings"},
    {0x00433B50, 0x900, "whichServerStrings"},
    {0x00434450, 0x4, "p.3"},
    {0x00434454, 0x4, "__new_handler"},
    {0x00434458, 0x0, "_GLOBAL_$F$nothrow"},
    {0x004344F8, 0x0, "_GLOBAL_$F$__builtin_delete.._.._src_gcc_cp_new2.cc1GeU73"},
    {0x00434540, 0x0, "_GLOBAL_$F$_$_9type_info"},
    {0x004348C4, 0x4, "__terminate_func"},
    {0x004348C8, 0x4, "get_eh_context"},
    {0x004348CC, 0x0, "_GLOBAL_$F$__default_terminate"},
    {0x00434EBC, 0x4, "__unexpected_func"},
    {0x00434EC0, 0x0, "_GLOBAL_$F$terminate__Fv"},
    {0x00435250, 0x4, "object_mutex"},
    {0x00435258, 0x8, "__tiv"},
    {0x00435260, 0x8, "__tix"},
    {0x00435268, 0x8, "__til"},
    {0x00435270, 0x8, "__tii"},
    {0x00435278, 0x8, "__tis"},
    {0x00435280, 0x8, "__tib"},
    {0x00435288, 0x8, "__tic"},
    {0x00435290, 0x8, "__tiw"},
    {0x00435298, 0x8, "__tir"},
    {0x004352A0, 0x8, "__tid"},
    {0x004352A8, 0x8, "__tif"},
    {0x004352B0, 0x8, "__tiUi"},
    {0x004352B8, 0x8, "__tiUl"},
    {0x004352C0, 0x8, "__tiUx"},
    {0x004352C8, 0x8, "__tiUs"},
    {0x004352D0, 0x8, "__tiUc"},
    {0x004352D8, 0x8, "__tiSc"},
    {0x004352E0, 0x0, "_GLOBAL_$F$before__C9type_infoRC9type_info"},
    {0x00435780, 0x4, "gCommBusy"},
    {0x00435784, 0x4, "gAwaitingInts"},
    {0x00435788, 0x4, "gStreamingInited"},
    {0x0043578C, 0x4, "gCdCallback"},
    {0x00435790, 0x4, "gSSRead"},
    {0x00435794, 0x4, "gSSReadDone"},
    {0x00435798, 0x4, "gLoadingFromFS"},
    {0x004357A0, 0x8, "gSndCommandBuffePtr"},
    {0x004357A8, 0x8, "gCommandBuffeBytesAvail"},
    {0x004357B0, 0x8, "gSndCommandReturnDefPtr"},
    {0x004357B8, 0x8, "gReturnValuesPtr"},
    {0x004357C0, 0x4, "gCommandFillBuffer"},
    {0x004357C4, 0x4, "gCaching"},
    {0x004357C8, 0x4, "gLoadBusy"},
    {0x004357D0, 0x10, "gLoadReturnDef"},
    {0x004357E0, 0x4, "gLoadCB"},
    {0x004357E8, 0x8, "gLoadUserData"},
    {0x004357F0, 0x4, "gLoadCommand"},
    {0x004357F4, 0x4, "gLoadAwaits"},
    {0x00435800, 0x40, "gSyncBuffer"},
    {0x00435840, 0x40, "gSyncSendBuffer"},
    {0x00435880, 0x100, "gSyncStringBuffer"},
    {0x00435980, 0x1000, "gSndCommandBuffer1"},
    {0x00436980, 0x1000, "gSndCommandBuffer2"},
    {0x00437980, 0x1000, "gSndCommandReturnDef1"},
    {0x00438980, 0x1000, "gSndCommandReturnDef2"},
    {0x00439980, 0x420, "gActualReturnValues1"},
    {0x00439DC0, 0x420, "gActualReturnValues2"},
    {0x0043A200, 0x20, "gStats"},
    {0x0043A240, 0x10, "gLoadReturnValue"},
    {0x0043A280, 0x20, "gLoadParams"},
    {0x0043A2C0, 0xC, "_6CsPool$m_activeList"},
    {0x0043A2D0, 0xC, "_6CsPool$m_inactiveList"},
    {0x0043A2E0, 0xC, "_6CsPool$m_HPActiveList"},
    {0x0043A2F0, 0x3840, "dbsStack"},
    {0x0043DB30, 0x40, "defaultCLUT"},
    {0x0043DB80, 0x7000, "cdFileSystemToc"},
    {0x00444B80, 0x810, "cdSectorBuffer"},
    {0x00445390, 0x184, "fileStatus"},
    {0x00445518, 0x20, "gFileName"},
    {0x00445538, 0x10, "globalTimeString"},
    {0x00445580, 0x40, "fogColorCtx"},
    {0x004455C0, 0x660, "gsPkt"},
    {0x00445C80, 0x40, "fixAxes.477"},
    {0x00445CC0, 0x10, "objRange"},
    {0x00445CD0, 0x9B0, "worldCtx"},
    {0x00446680, 0x18, "dmaVu1"},
    {0x00446698, 0x18, "fakeAlphaVu1"},
    {0x00446700, 0x88000, "hierObjPkt"},
    {0x004CE700, 0x3C000, "hierAlphaPkt"},
    {0x0050A700, 0x810, "hierAlphaBins"},
    {0x0050AF10, 0x204, "lastAlpEntry"},
    {0x0050B118, 0x3E80, "hierAlpList"},
    {0x0050EF98, 0x80, "FxToKill"},
    {0x0050F020, 0xA0, "g_cameraLosPoints"},
    {0x0050F0C0, 0x14, "g_cameraLosPointsPerView"},
    {0x0050F100, 0x3E80, "texInfo"},
    {0x00512F80, 0x9600, "texBinE"},
    {0x0051C580, 0x80, "texAllBins"},
    {0x0051C600, 0x1E0, "psBlkLst"},
    {0x0051C800, 0x28000, "psObjDma"},
    {0x00544800, 0x8000, "psBlkDma"},
    {0x0054C800, 0x1E0, "psFilterDma"},
    {0x0054CA00, 0x180, "psBorderDma"},
    {0x0054CB80, 0x28, "psObjIdx"},
    {0x0054CBB0, 0x80, "psDrawEnv"},
    {0x0054CC30, 0x60, "psClearDraw"},
    {0x0054CC90, 0x10, "psLitDirWs"},
    {0x0054CCA0, 0x28, "psNumObjects"},
    {0x0054CD00, 0x240, "gLightEnv"},
    {0x0054CF40, 0x9C4, "gRand"},
    {0x0054D910, 0x40, "UNIT_MATRIX"},
    {0x0054D950, 0x18, "_11MemoryStack$global"},
    {0x0054D968, 0x14, "voBuf"},
    {0x0054D980, 0xB8, "videoDec"},
    {0x0054DA38, 0x38, "display"},
    {0x0054DA70, 0x2C, "infile"},
    {0x0054DAA0, 0x58, "audioDec"},
    {0x0054DAF8, 0x30, "th_param01"},
    {0x0054DB28, 0x30, "th_param02"},
    {0x0054DB60, 0x10, "originalTexZero"},
    {0x0054DB70, 0x30, "th_paramM"},
    {0x0054DBA0, 0x30, "th_paramD"},
    {0x0054DBD0, 0x20040, "miscSprt"},
    {0x0056DC10, 0x20040, "miscLine"},
    {0x0058DC50, 0x20040, "misc2D"},
    {0x005ADD00, 0x12E0, "VisualInfo"},
    {0x005AEFE0, 0x10, "Gusts"},
    {0x005AEFF0, 0x2F30, "FieldInfo"},
    {0x005B1F20, 0x10, "NoOffset"},
    {0x005B1F30, 0x10, "UpABit"},
    {0x005B1F40, 0x10, "FireOffset"},
    {0x005B1F50, 0x10, "Nozzle"},
    {0x005B1F60, 0x40, "Front"},
    {0x005B1FA0, 0x40, "Up"},
    {0x005B1FE0, 0x40, "Back"},
    {0x005B2020, 0x2F30, "EmitterInfo"},
    {0x005B4F50, 0x10, "fragColoring"},
    {0x005B4F60, 0x10, "trailColoring"},
    {0x005B4F70, 0x38A0, "ParticleInfo"},
    {0x005B8810, 0x14, "monArcParticle"},
    {0x005B8828, 0x40, "monArcCLUT"},
    {0x005B8868, 0x40, "roboArcCLUT"},
    {0x005B88A8, 0x10, "attractors"},
    {0x005B88B8, 0x30, "dustTexLitNormalMat"},
    {0x005B88E8, 0x20, "dustLighting"},
    {0x005B8908, 0x20, "sweepTrailHandle"},
    {0x005B8980, 0x1500, "displayList"},
    {0x005B9E80, 0x400, "TextureInfo"},
    {0x005BA280, 0x124F80, "fx"},
    {0x006DF200, 0x4B0, "fxList"},
    {0x006DF6B0, 0x400, "TEX0"},
    {0x006DFAB0, 0x100, "texId"},
    {0x006DFBB0, 0x48, "maxParticles"},
    {0x006DFC00, 0x40, "paraL"},
    {0x006DFC40, 0xC, "paraLightR"},
    {0x006DFC50, 0xC, "paraLightG"},
    {0x006DFC60, 0xC, "paraLightB"},
    {0x006DFC70, 0xC, "paraLightH"},
    {0x006DFC80, 0xC, "paraLightP"},
    {0x006DFC90, 0xC, "paraTODLightR"},
    {0x006DFCA0, 0xC, "paraTODLightG"},
    {0x006DFCB0, 0xC, "paraTODLightB"},
    {0x006DFCC0, 0xC, "paraTODLightH"},
    {0x006DFCD0, 0xC, "paraTODLightP"},
    {0x006DFCE0, 0x8A0, "pointLights"},
    {0x006E0580, 0x100, "s_sourceObjects"},
    {0x006E0680, 0x84, "ascii_special"},
    {0x006E0708, 0xC, "ascii_table"},
    {0x006E0718, 0x10, "_11TaskManager$global"},
    {0x006E0728, 0x10, "_11TaskManager$debris"},
    {0x006E0738, 0x10, "_11TaskManager$global0"},
    {0x006E0750, 0xFC, "todLink"},
    {0x006E0850, 0x600, "todInfo"},
    {0x006E0E50, 0x10, "s_todPlightsEnabled"},
    {0x006E0E60, 0x460, "todSky"},
    {0x006E12C0, 0x600, "viewDef"},
    {0x006E18C0, 0x10, "s_viewAmbientVolBlend"},
    {0x006E18D0, 0x10, "s_viewAmbientVolRed"},
    {0x006E18E0, 0x10, "s_viewAmbientVolGreen"},
    {0x006E18F0, 0x10, "s_viewAmbientVolBlue"},
    {0x006E1900, 0x64, "viewInfo"},
    {0x006E1970, 0x10, "rearViewPos"},
    {0x006E1980, 0x10, "rearViewRot"},
    {0x006E1990, 0x140, "viewScreenMats"},
    {0x006E1AD0, 0x140, "viewClipMats"},
    {0x006E1C10, 0x140, "viewFovMats"},
    {0x006E1D50, 0x140, "viewClipView"},
    {0x006E1E90, 0x140, "viewFovView"},
    {0x006E1FD0, 0x140, "worldToScreenMat"},
    {0x006E2110, 0x40, "viewShadRecMat"},
    {0x006E2150, 0x40, "viewShadWorldToScrMat"},
    {0x006E21C0, 0x10E0, "viewDb"},
    {0x006E32C0, 0x4F0, "G_codeLen"},
    {0x006E37B0, 0x70, "ctxDef"},
    {0x006E3820, 0x10, "q1.3"},
    {0x006E3830, 0x10, "q2.4"},
    {0x006E3840, 0x10, "__ps2_libinfo___006E3840"},
    {0x006E3850, 0x10, "gp.6"},
    {0x006E3860, 0x10, "init_mp3.3"},
    {0x006E3870, 0x10, "__ps2_libinfo___006E3870"},
    {0x006E3880, 0x4, "isInit"},
    {0x006E3884, 0x4, "isWarning"},
    {0x006E3888, 0x20, "PadStateStr"},
    {0x006E38A8, 0xC, "ReqStateStr"},
    {0x006E38B8, 0x10, "__ps2_libinfo___006E38B8"},
    {0x006E38C8, 0x4, "mcRunCmdNo"},
    {0x006E38CC, 0x4, "semaidRegFunc"},
    {0x006E38D0, 0xA0, "_strmap"},
    {0x006E3970, 0x10, "__ps2_libinfo___006E3970"},
    {0x006E3980, 0x40, "_defIQM"},
    {0x006E39C0, 0x40, "_defNIQM"},
    {0x006E3A00, 0x20, "_rix"},
    {0x006E3A20, 0x20, "_ri0"},
    {0x006E3A40, 0x28, "_isDirty"},
    {0x006E3A68, 0x40, "_showCount"},
    {0x006E3AB0, 0x10, "__ps2_libinfo___006E3AB0"},
    {0x006E3AC0, 0x50, "iqval"},
    {0x006E3B10, 0x20, "vqval"},
    {0x006E3B40, 0x10, "__ps2_libinfo___006E3B40"},
    {0x006E3B50, 0x4, "SCE_CD_debug"},
    {0x006E3B54, 0x4, "cb_thid"},
    {0x006E3B58, 0x4, "scmd_sema_keep_cmd"},
    {0x006E3B5C, 0x4, "ncmd_sema_keep_cmd"},
    {0x006E3B60, 0x4, "cb_semid"},
    {0x006E3B64, 0x4, "Init_seq"},
    {0x006E3B68, 0x4, "_sceCd_ncmd_semid"},
    {0x006E3B6C, 0x4, "_sceCd_scmd_semid"},
    {0x006E3B70, 0x4, "_sceCd_c_cb_sem"},
    {0x006E3B74, 0x4, "_sceCd_ee_read_mode"},
    {0x006E3B78, 0x4, "_ncmd_bind"},
    {0x006E3B7C, 0x4, "_icmd_bind"},
    {0x006E3B80, 0x4, "_sf_bind"},
    {0x006E3B84, 0x4, "_scmd_dr_bind"},
    {0x006E3B88, 0x4, "_scmd_bind"},
    {0x006E3B8C, 0x4, "_it_bind"},
    {0x006E3B90, 0x4, "c_cnt.57"},
    {0x006E3B94, 0x4, "sceCdCbfunc_num"},
    {0x006E3B98, 0x4, "sceCdCbfunc_number"},
    {0x006E3BC0, 0x60, "_sceCd_ncmdrdata"},
    {0x006E3C40, 0x1000, "_sceCd_ncmdsdata"},
    {0x006E4C40, 0x90, "_sceCd_rd_intr_data"},
    {0x006E4D00, 0x10, "_sceCd_Read_cur_pos"},
    {0x006E4D10, 0x28, "_sceCd_cd_ncmd"},
    {0x006E4D40, 0x420, "_sceCd_scmdrdata"},
    {0x006E5180, 0x408, "_sceCd_scmdsdata"},
    {0x006E5588, 0x28, "_sceCd_cd_scmd"},
    {0x006E55B0, 0x4, "stm_status"},
    {0x006E55B8, 0x2EC, "impure_data"},
    {0x006E58A4, 0x4, "_impure_ptr"},
    {0x006E58A8, 0x408, "__malloc_av_"},
    {0x006E5CB0, 0x8, "__malloc_trim_threshold"},
    {0x006E5CB8, 0x8, "__malloc_top_pad"},
    {0x006E5CC0, 0x4, "__malloc_sbrk_base"},
    {0x006E5CC8, 0x8, "__malloc_max_sbrked_mem"},
    {0x006E5CD0, 0x8, "__malloc_max_total_mem"},
    {0x006E5CD8, 0x28, "__malloc_current_mallinfo"},
    {0x006E5D00, 0x4, "__mb_cur_max"},
    {0x006E5D08, 0x8, "lc_ctype.3"},
    {0x006E5D10, 0x8, "last_lc_ctype.4"},
    {0x006E5D18, 0x0, "__ps2_klibinfo__"},
    {0x006E5D28, 0x4, "ttyinit"},
    {0x006E5D2C, 0x4, "heap_ptr.30"},
    {0x006E5D30, 0x740, "srcfile"},
    {0x006E6470, 0x28, "eenull"},
    {0x006E6498, 0x40, "SysEntry_006E6498"},
    {0x006E64D8, 0x4, "topId"},
    {0x006E64DC, 0x4, "count_006E64DC"},
    {0x006E64E0, 0x4, "_putchar"},
    {0x006E64E4, 0x4, "_cmd_init_check"},
    {0x006E64E8, 0x4, "_sceSifInitCheck"},
    {0x006E64F0, 0x80, "_sceFs_q"},
    {0x006E6570, 0x4, "_fs_init"},
    {0x006E6574, 0x4, "_fs_semid"},
    {0x006E6578, 0x4, "_fs_iob_semid"},
    {0x006E657C, 0x4, "_fs_fsq_semid"},
    {0x006E6580, 0x4, "_fswildcard"},
    {0x006E6584, 0x4, "_bind"},
    {0x006E6588, 0x4, "_bind_check"},
    {0x006E658C, 0x4, "_lfwildcard"},
    {0x006E6590, 0xC, "SysEntry_006E6590"},
    {0x006E659C, 0x4, "__sce_sema_id"},
    {0x006E65A0, 0x4, "__sce_eh_sema_id"},
    {0x006E65A4, 0x4, "_SyscallEntry"},
    {0x006E65A8, 0x7A8, "osdsrc"},
    {0x006E6D50, 0x18, "SysExecPS2Entry"},
    {0x006E6D68, 0x328, "tlbsrc"},
    {0x006E7090, 0x4, "_kTLBRefillHandler"},
    {0x006E7098, 0x40, "_kDebugHandler"},
    {0x006E70D8, 0x40, "SysEntry_006E70D8"},
    {0x006E7118, 0xD0, "kernelTLB"},
    {0x006E71E8, 0x120, "defaultTLB"},
    {0x006E7308, 0x80, "extendTLB"},
    {0x006E7388, 0x1C, "TLBInfo"},
    {0x006E73A4, 0xC0, "__CTOR_LIST__"},
    {0x006E7464, 0x1C, "__DTOR_LIST__"},
    {0x006E7580, 0x18, "_vt$18AiThrashActionList"},
    {0x006E7598, 0x18, "_vt$12AiActionList"},
    {0x006E75B0, 0x38, "_vt$13AiActionTuple"},
    {0x006E79A0, 0x38, "_vt$15AiGrappleAttack"},
    {0x006E79D8, 0x38, "_vt$14AiGrappleThrow"},
    {0x006E7A10, 0x18, "_vt$21AiGrapplingActionList"},
    {0x006E7A28, 0x18, "_vt$20AiGrappledActionList"},
    {0x006E7AF0, 0x28, "_vt$10AiPathTool"},
    {0x006E7C78, 0x38, "_vt$12AiPutOutFire"},
    {0x006E7CB0, 0x38, "_vt$7AiTaunt"},
    {0x006E7CE8, 0x38, "_vt$14AiDetonateHead"},
    {0x006E7D20, 0x38, "_vt$15AiSpecialAttack"},
    {0x006E7D58, 0x38, "_vt$14AiCrowdControl"},
    {0x006E7D90, 0x38, "_vt$11AiRamAttack"},
    {0x006E7DC8, 0x38, "_vt$13AiStompReflex"},
    {0x006E7E00, 0x38, "_vt$13AiThrowPickup"},
    {0x006E7E38, 0x38, "_vt$14AiThrowRecover"},
    {0x006E7E70, 0x38, "_vt$13AiSmashReflex"},
    {0x006E7EA8, 0x38, "_vt$15AiDisposeReflex"},
    {0x006E7EE0, 0x38, "_vt$14AiThrashReflex"},
    {0x006E7F18, 0x38, "_vt$15AiCounterReflex"},
    {0x006E7F50, 0x38, "_vt$13AiBlockReflex"},
    {0x006E7F88, 0x38, "_vt$15AiGrappleReflex"},
    {0x006E7FC0, 0x38, "_vt$16AiFireProjectile"},
    {0x006E7FF8, 0x38, "_vt$11AiBatReflex"},
    {0x006E8030, 0x38, "_vt$13AiPunchReflex"},
    {0x006E8270, 0x28, "_vt$7AiReset"},
    {0x006E82A8, 0x118, "GameLevelNames_006E82A8"},
    {0x006E83C0, 0x88, "MonsterLongNames_006E83C0"},
    {0x006E8450, 0x38, "_vt$14AiDodgeSpecial"},
    {0x006E8488, 0x38, "_vt$12AiDodgeStomp"},
    {0x006E84C0, 0x38, "_vt$10AiDodgeRam"},
    {0x006E84F8, 0x38, "_vt$10AiBatThrow"},
    {0x006E8530, 0x38, "_vt$12AiCatchThrow"},
    {0x006E8568, 0x38, "_vt$12AiDodgeThrow"},
    {0x006E85A0, 0x38, "_vt$7AiSwarm"},
    {0x006E85D8, 0x38, "_vt$13AiSeekMonster"},
    {0x006E8610, 0x38, "_vt$12AiSeekPickup"},
    {0x006E8648, 0x38, "_vt$11AiSeekCloak"},
    {0x006E8680, 0x38, "_vt$13AiSeekSpecial"},
    {0x006E86B8, 0x38, "_vt$13AiSeekStamina"},
    {0x006E86F0, 0x38, "_vt$12AiSeekHealth"},
    {0x006E8870, 0x30, "_vt$8Airplane"},
    {0x006E8920, 0x10, "_12BigShotLevel$UPDATE_FUNK"},
    {0x006E8958, 0xCC, "MonsterNewNames_006E8958"},
    {0x006E8A28, 0x50, "_6Camera$UPDATE_FUNK"},
    {0x006E8B10, 0x70, "_vt$9CarPickup"},
    {0x006E8B90, 0x28, "_vt$9charreset"},
    {0x006E8BC8, 0x88, "MonsterLongNames_006E8BC8"},
    {0x006E8D60, 0x10, "_10CrushLevel$UPDATE_FUNK"},
    {0x006E8D90, 0xCC, "MonsterNewNames_006E8D90"},
    {0x006E8F88, 0x30, "_vt$12Destructible"},
    {0x006E9060, 0x30, "_vt$9FinalBoss"},
    {0x006E9550, 0x28, "_vt$11GlobalReset"},
    {0x006E9590, 0x50, "_vt$13StateGrappled"},
    {0x006E95E0, 0x50, "_vt$12StateGrapple"},
    {0x006E9650, 0x60, "_vt$19StateMonkeyOHAttack"},
    {0x006E96B0, 0x60, "_vt$20StateGrappleOHAttack"},
    {0x006E9750, 0x60, "_vt$18StateGrappleThrow1"},
    {0x006E97B0, 0x60, "_vt$17StateGrappleThrow"},
    {0x006E9810, 0x50, "_vt$16StateGrappleLift"},
    {0x006E98F8, 0x70, "_vt$10HeliPickup"},
    {0x006E99B8, 0x30, "_vt$9HomingBug"},
    {0x006EAA78, 0xCC, "MonsterNewNames_006EAA78"},
    {0x006EAC88, 0x30, "_vt$14McSetupGameDir"},
    {0x006EACB8, 0x30, "_vt$14McScrambleIcon"},
    {0x006EACE8, 0x30, "_vt$8McFormat"},
    {0x006EAD18, 0x30, "_vt$13McUpdateState"},
    {0x006EAD48, 0x30, "_vt$15McDoesFileExist"},
    {0x006EAD78, 0x30, "_vt$19McListRootDirectory"},
    {0x006EADA8, 0x30, "_vt$10McSaveFile"},
    {0x006EADD8, 0x30, "_vt$10McLoadFile"},
    {0x006EAE08, 0x30, "_vt$9McMessage"},
    {0x006EAF10, 0x70, "_vt$6McPage"},
    {0x006EAF80, 0x40, "_vt$12BoxFormation"},
    {0x006EAFC0, 0x40, "_vt$13LineFormation"},
    {0x006EB000, 0x40, "_vt$17MilitaryFormation"},
    {0x006EB078, 0x90, "_vt$14MilitaryPickup"},
    {0x006EB160, 0x90, "_vt$18MissileTruckPickup"},
    {0x006EB268, 0x50, "_vt$12StateVictory"},
    {0x006EB2B8, 0x50, "_vt$15StateBigTakeHit"},
    {0x006EB308, 0x60, "_vt$17StateShieldAttack"},
    {0x006EB368, 0x60, "_vt$16StateTazerAttack"},
    {0x006EB3C8, 0x60, "_vt$16StateStompAttack"},
    {0x006EB428, 0x50, "_vt$12StateShocked"},
    {0x006EB478, 0x60, "_vt$19StateTwoHandedThrow"},
    {0x006EB4D8, 0x60, "_vt$10StateThrow"},
    {0x006EB538, 0x50, "_vt$10StateTaunt"},
    {0x006EB588, 0x50, "_vt$12StateStunned"},
    {0x006EB5D8, 0x50, "_vt$11StateRecoil"},
    {0x006EB628, 0x60, "_vt$10StatePunch"},
    {0x006EB688, 0x20, "_vt$Q210StatePunch11PunchConfig"},
    {0x006EB6A8, 0x50, "_vt$11StatePickUp"},
    {0x006EB6F8, 0x60, "_vt$12StateJavelin"},
    {0x006EB758, 0x50, "_vt$12StateImpaled"},
    {0x006EB7A8, 0x60, "_vt$14StateGunAttack"},
    {0x006EB808, 0x50, "_vt$16StateGetupAttack"},
    {0x006EB858, 0x50, "_vt$10StateDeath"},
    {0x006EB8A8, 0x50, "_vt$14StateCountered"},
    {0x006EB8F8, 0x50, "_vt$12StateCounter"},
    {0x006EB948, 0x50, "_vt$10StateCatch"},
    {0x006EB998, 0x50, "_vt$10StateBlock"},
    {0x006EB9E8, 0x60, "_vt$13StateBatSwipe"},
    {0x006EBA48, 0x20, "_vt$Q213StateBatSwipe11SwipeConfig"},
    {0x006EBA68, 0x50, "_vt$9StateIdle"},
    {0x006EBAB8, 0x60, "_vt$11AttackState"},
    {0x006EBB18, 0x50, "_vt$12MonsterState"},
    {0x006EC018, 0x30, "_vt$7Monster"},
    {0x006EC750, 0x70, "_vt$13McCardRemoved"},
    {0x006EC7C0, 0x70, "_vt$11McErrorPage"},
    {0x006EC830, 0x70, "_vt$11McAskIfSure"},
    {0x006EC8A0, 0x70, "_vt$11McAskToSave"},
    {0x006EC910, 0x70, "_vt$16McSelectSaveSlot"},
    {0x006EC980, 0x70, "_vt$20McSaveAdventureBlank"},
    {0x006EC9F0, 0x70, "_vt$15McSaveAdventure"},
    {0x006ECA60, 0x70, "_vt$12McSelectReel"},
    {0x006ECAD0, 0x70, "_vt$15McLoadGameBoxes"},
    {0x006ECB40, 0x70, "_vt$15McPrepBoxScreen"},
    {0x006ECBB0, 0x70, "_vt$16McCheckOverwrite"},
    {0x006ECC20, 0x70, "_vt$13McCheckFormat"},
    {0x006ECC90, 0x70, "_vt$12McCheckSpace"},
    {0x006ECD00, 0x70, "_vt$12McChooseSlot"},
    {0x006ECD70, 0x70, "_vt$14McSaveSettings"},
    {0x006ECDE0, 0x70, "_vt$14McLoadSettings"},
    {0x006ECE50, 0x70, "_vt$18McCheckForSettings"},
    {0x006ECEC0, 0x70, "_vt$20McCheckForSweetTooth"},
    {0x006ECF30, 0x70, "_vt$10McBootPage"},
    {0x006ED120, 0x1F8, "GameLevelLongNames_006ED120"},
    {0x006ED318, 0xCC, "MonsterNewNames_006ED318"},
    {0x006ED530, 0x50, "_vt$8StateRun"},
    {0x006ED580, 0x60, "_vt$14StateRamAttack"},
    {0x006ED5E0, 0x50, "_vt$9StateJump"},
    {0x006ED630, 0x50, "_vt$8StateFly"},
    {0x006ED680, 0x50, "_vt$11JumpFlyBase"},
    {0x006ED6D0, 0x50, "_vt$9StateDash"},
    {0x006ED720, 0x50, "_vt$10StateClimb"},
    {0x006ED770, 0x60, "_vt$13StateButtSlam"},
    {0x006ED870, 0x28, "_vt$8PathTool"},
    {0x006EDB90, 0x70, "_vt$6Pickup"},
    {0x006EDDE8, 0x30, "_vt$13PlantBossBase"},
    {0x006EDE18, 0x30, "_vt$13PlantBossRoot"},
    {0x006EDE48, 0x30, "_vt$13PlantBossHead"},
    {0x006EDEC0, 0x28, "_vt$11PowerUpTool"},
    {0x006EDF28, 0x28, "_vt$5reset"},
    {0x006EDF50, 0x30, "_11RigidDebris$COLLIS_FUNK"},
    {0x006EDFD8, 0x70, "_vt$11RigidPickup"},
    {0x006EEE78, 0x80, "on_bit_006EEE78"},
    {0x006EEEF8, 0x1F8, "GameLevelLongNames_006EEEF8"},
    {0x006EF748, 0x118, "GameLevelNames_006EF748"},
    {0x006EF860, 0x88, "MonsterLongNames_006EF860"},
    {0x006EF8E8, 0x80, "on_bit_006EF8E8"},
    {0x006EF968, 0x80, "off_bit"},
    {0x006EFEC8, 0xCC, "MonsterNewNames_006EFEC8"},
    {0x006F0E30, 0x60, "_vt$15StateUltraTazer"},
    {0x006F0E90, 0x60, "_vt$16StateCannonHands"},
    {0x006F0EF0, 0x60, "_vt$12StateTopSpin"},
    {0x006F0F50, 0x60, "_vt$14StateZapAttack"},
    {0x006F0FB0, 0x60, "_vt$14StateSonicRoar"},
    {0x006F1010, 0x60, "_vt$16StateRockSpecial"},
    {0x006F1070, 0x60, "_vt$17StateRobotSpecial"},
    {0x006F10D0, 0x60, "_vt$14StateLavaBlast"},
    {0x006F1130, 0x60, "_vt$16StateGrappleHook"},
    {0x006F1190, 0x60, "_vt$15StateFireBreath"},
    {0x006F11F0, 0x60, "_vt$17StateCrowdControl"},
    {0x006F1250, 0x60, "_vt$14StateBugAttack"},
    {0x006F12B0, 0x60, "_vt$14StateAirStrike"},
    {0x006F1498, 0x28, "_vt$14StartPointTool"},
    {0x006F14D8, 0x70, "_vt$12SubwayPickup"},
    {0x006F15A0, 0x90, "_vt$10TankPickup"},
    {0x006F1678, 0x70, "_vt$12TurretPickup"},
    {0x006F1768, 0x30, "_vt$13DbInteractive"},
    {0x006F1878, 0x50, "_vt$14StateKnockBack"},
    {0x006F18C8, 0x50, "_vt$17StateGrappleBreak"},
    {0x006F1918, 0x50, "_vt$16StateBeingThrown"},
    {0x006F1968, 0x50, "_vt$17StateBeingSlammed"},
    {0x006F19B8, 0x50, "_vt$14StateThrowBack"},
    {0x006F1B08, 0x30, "_vt$8UfoTokyo"},
    {0x006F1BE0, 0x70, "_vt$9UfoPickup"},
    {0x006F2208, 0x118, "GameLevelNames_006F2208"},
    {0x006F2470, 0x38, "_vt$14CNetworkScreen"},
    {0x006F24A8, 0x38, "_vt$10CDevScreen"},
    {0x006F24E0, 0x38, "_vt$7CScreen"},
    {0x006F2518, 0x60, "_vt$16CVerticalSection"},
    {0x006F2578, 0x58, "_vt$18CHorizontalSection"},
    {0x006F25D0, 0x58, "_vt$18CFunctionalSection"},
    {0x006F2628, 0x28, "_vt$14CScreenSection"},
    {0x006F26E8, 0x20, "UIDEV_P"},
    {0x006F2708, 0x118, "GameLevelNames_006F2708"},
    {0x006F2B90, 0x1, "nothrow"},
    {0x006F2B98, 0x18, "_vt$9bad_alloc"},
    {0x006F2BE0, 0x18, "_vt$17__class_type_info"},
    {0x006F2BF8, 0x18, "_vt$14__si_type_info"},
    {0x006F2C10, 0x18, "_vt$16__user_type_info"},
    {0x006F2C28, 0x18, "_vt$10bad_typeid"},
    {0x006F2C40, 0x18, "_vt$8bad_cast"},
    {0x006F2C58, 0x10, "_vt$9type_info"},
    {0x006F2CE0, 0x18, "_vt$13bad_exception"},
    {0x006F2CF8, 0x18, "_vt$9exception"},
    {0x006F2E98, 0x10, "_vt$17__array_type_info"},
    {0x006F2EA8, 0x10, "_vt$16__ptmd_type_info"},
    {0x006F2EB8, 0x10, "_vt$16__ptmf_type_info"},
    {0x006F2EC8, 0x10, "_vt$16__func_type_info"},
    {0x006F2ED8, 0x10, "_vt$19__builtin_type_info"},
    {0x006F2EE8, 0x10, "_vt$16__attr_type_info"},
    {0x006F2EF8, 0x10, "_vt$19__pointer_type_info"},
    {0x006F47D8, 0x28, "_vt$12PointToolKit"},
    {0x006F4A30, 0x28, "_vt$8resetcom"},
    {0x006F4E28, 0x22, "mask_bits"},
    {0x006F4E50, 0x4C, "c_border"},
    {0x006F4EA0, 0x3E, "c_plens"},
    {0x006F4EE0, 0x3E, "c_plext"},
    {0x006F4F20, 0x3C, "c_pdist"},
    {0x006F4F60, 0x3C, "c_pdext"},
    {0x006F4FA0, 0x800, "crc_32_tab"},
    {0x006F57E8, 0x10, "atanhi"},
    {0x006F57F8, 0x10, "atanlo"},
    {0x006F5808, 0x2C, "aT"},
    {0x006F5834, 0x4, "one_006F5834"},
    {0x006F5838, 0x4, "huge_006F5838"},
    {0x006F583C, 0x4, "huge_006F583C"},
    {0x006F5840, 0x4, "one_006F5840"},
    {0x006F5890, 0x4, "one_006F5890"},
    {0x006F5894, 0x4, "pi_006F5894"},
    {0x006F5898, 0x4, "pio2_hi_006F5898"},
    {0x006F589C, 0x4, "pio2_lo_006F589C"},
    {0x006F58A0, 0x4, "pS0_006F58A0"},
    {0x006F58A4, 0x4, "pS1_006F58A4"},
    {0x006F58A8, 0x4, "pS2_006F58A8"},
    {0x006F58AC, 0x4, "pS3_006F58AC"},
    {0x006F58B0, 0x4, "pS4_006F58B0"},
    {0x006F58B4, 0x4, "pS5_006F58B4"},
    {0x006F58B8, 0x4, "qS1_006F58B8"},
    {0x006F58BC, 0x4, "qS2_006F58BC"},
    {0x006F58C0, 0x4, "qS3_006F58C0"},
    {0x006F58C4, 0x4, "qS4_006F58C4"},
    {0x006F58C8, 0x4, "one_006F58C8"},
    {0x006F58CC, 0x4, "huge_006F58CC"},
    {0x006F58D0, 0x4, "pio2_hi_006F58D0"},
    {0x006F58D4, 0x4, "pio2_lo_006F58D4"},
    {0x006F58D8, 0x4, "pio4_hi"},
    {0x006F58DC, 0x4, "pS0_006F58DC"},
    {0x006F58E0, 0x4, "pS1_006F58E0"},
    {0x006F58E4, 0x4, "pS2_006F58E4"},
    {0x006F58E8, 0x4, "pS3_006F58E8"},
    {0x006F58EC, 0x4, "pS4_006F58EC"},
    {0x006F58F0, 0x4, "pS5_006F58F0"},
    {0x006F58F4, 0x4, "qS1_006F58F4"},
    {0x006F58F8, 0x4, "qS2_006F58F8"},
    {0x006F58FC, 0x4, "qS3_006F58FC"},
    {0x006F5900, 0x4, "qS4_006F5900"},
    {0x006F5904, 0x4, "tiny_006F5904"},
    {0x006F5908, 0x4, "zero_006F5908"},
    {0x006F590C, 0x4, "pi_o_4"},
    {0x006F5910, 0x4, "pi_o_2"},
    {0x006F5914, 0x4, "pi_006F5914"},
    {0x006F5918, 0x4, "pi_lo"},
    {0x006F5920, 0x8, "bp"},
    {0x006F5928, 0x8, "dp_h"},
    {0x006F5930, 0x8, "dp_l"},
    {0x006F5938, 0x4, "zero_006F5938"},
    {0x006F593C, 0x4, "one_006F593C"},
    {0x006F5940, 0x4, "two"},
    {0x006F5944, 0x4, "two24"},
    {0x006F5948, 0x4, "huge_006F5948"},
    {0x006F594C, 0x4, "tiny_006F594C"},
    {0x006F5950, 0x4, "L1"},
    {0x006F5954, 0x4, "L2"},
    {0x006F5958, 0x4, "L3"},
    {0x006F595C, 0x4, "L4"},
    {0x006F5960, 0x4, "L5"},
    {0x006F5964, 0x4, "L6"},
    {0x006F5968, 0x4, "P1"},
    {0x006F596C, 0x4, "P2"},
    {0x006F5970, 0x4, "P3"},
    {0x006F5974, 0x4, "P4"},
    {0x006F5978, 0x4, "P5"},
    {0x006F597C, 0x4, "lg2"},
    {0x006F5980, 0x4, "lg2_h"},
    {0x006F5984, 0x4, "lg2_l"},
    {0x006F5988, 0x4, "ovt"},
    {0x006F598C, 0x4, "cp"},
    {0x006F5990, 0x4, "cp_h"},
    {0x006F5994, 0x4, "cp_l"},
    {0x006F5998, 0x4, "ivln2"},
    {0x006F599C, 0x4, "ivln2_h"},
    {0x006F59A0, 0x4, "ivln2_l"},
    {0x006F59A8, 0x318, "two_over_pi"},
    {0x006F5CC0, 0x80, "npio2_hw"},
    {0x006F5D40, 0x4, "zero_006F5D40"},
    {0x006F5D44, 0x4, "half_006F5D44"},
    {0x006F5D48, 0x4, "two8_006F5D48"},
    {0x006F5D4C, 0x4, "invpio2"},
    {0x006F5D50, 0x4, "pio2_1"},
    {0x006F5D54, 0x4, "pio2_1t"},
    {0x006F5D58, 0x4, "pio2_2"},
    {0x006F5D5C, 0x4, "pio2_2t"},
    {0x006F5D60, 0x4, "pio2_3"},
    {0x006F5D64, 0x4, "pio2_3t"},
    {0x006F5D68, 0x4, "one_006F5D68"},
    {0x006F5D6C, 0x4, "tiny_006F5D6C"},
    {0x006F5D70, 0x4, "one_006F5D70"},
    {0x006F5D74, 0x4, "C1"},
    {0x006F5D78, 0x4, "C2"},
    {0x006F5D7C, 0x4, "C3"},
    {0x006F5D80, 0x4, "C4"},
    {0x006F5D84, 0x4, "C5"},
    {0x006F5D88, 0x4, "C6"},
    {0x006F5D90, 0xC, "init_jk"},
    {0x006F5DA0, 0x2C, "PIo2"},
    {0x006F5DCC, 0x4, "zero_006F5DCC"},
    {0x006F5DD0, 0x4, "one_006F5DD0"},
    {0x006F5DD4, 0x4, "two8_006F5DD4"},
    {0x006F5DD8, 0x4, "twon8"},
    {0x006F5DDC, 0x4, "half_006F5DDC"},
    {0x006F5DE0, 0x4, "S1"},
    {0x006F5DE4, 0x4, "S2"},
    {0x006F5DE8, 0x4, "S3"},
    {0x006F5DEC, 0x4, "S4"},
    {0x006F5DF0, 0x4, "S5"},
    {0x006F5DF4, 0x4, "S6"},
    {0x006F5DF8, 0x4, "one_006F5DF8"},
    {0x006F5DFC, 0x4, "pio4"},
    {0x006F5E00, 0x4, "pio4lo"},
    {0x006F5E08, 0x34, "T"},
    {0x006F5E40, 0x8, "__infinity"},
    {0x006F5E48, 0x4, "__fdlib_version"},
    {0x006F5E50, 0x10, "TWO52"},
    {0x006F5E60, 0x4, "huge_006F5E60"},
    {0x006F5E64, 0x4, "two25"},
    {0x006F5E68, 0x4, "twom25"},
    {0x006F5E6C, 0x4, "huge_006F5E6C"},
    {0x006F5E70, 0x4, "tiny_006F5E70"},
    {0x006F6BE8, 0x101, "_ctype_"},
    {0x006F6D00, 0x10, "blanks.12"},
    {0x006F6D10, 0x10, "zeroes.13"},
    {0x006F6F68, 0x30, "lconv"},
    {0x006F6FB0, 0xC, "p05.27"},
    {0x006F6FC0, 0xC8, "__mprec_tens"},
    {0x006F7088, 0x28, "__mprec_bigtens"},
    {0x006F70B0, 0x28, "__mprec_tinytens"},
    {0x006F7400, 0x100, "__clz_tab_006F7400"},
    {0x006F7500, 0x100, "__clz_tab_006F7500"},
    {0x006F7600, 0x100, "__clz_tab_006F7600"},
    {0x006F7700, 0x100, "__clz_tab_006F7700"},
    {0x006F7900, 0x4, "moviePlaying"},
    {0x006F7904, 0x4, "movieAborted"},
    {0x006F7908, 0x4, "s_genericEventCB"},
    {0x006F790C, 0x4, "s_numUserDefinedScripts"},
    {0x006F7910, 0x4, "s_fadeOut"},
    {0x006F7914, 0x4, "s_endingActions"},
    {0x006F7918, 0x4, "s_pathNet"},
    {0x006F791C, 0x4, "s_mode"},
    {0x006F7920, 0x4, "s_startupDelayMin"},
    {0x006F7924, 0x4, "s_startupDelayMax"},
    {0x006F7928, 0x4, "_2Ai$s_attackOtherAi"},
    {0x006F792C, 0x4, "_2Ai$s_ccAi"},
    {0x006F7930, 0x4, "_2Ai$s_ignoreZ"},
    {0x006F7934, 0x4, "TYPICAL_SLOPE_TAN.2481"},
    {0x006F7938, 0x4, "CERTAINTY_FACTOR.2482"},
    {0x006F793C, 0x2, "nodeToUpdate.2504"},
    {0x006F7960, 0x4, "assBossTier1Health"},
    {0x006F7964, 0x4, "assBossTier2Health"},
    {0x006F7968, 0x4, "cherryBombThrown.2466"},
    {0x006F796C, 0x4, "cherryBombAssigned.2467"},
    {0x006F7970, 0x4, "assBoss"},
    {0x006F7974, 0x4, "attackState"},
    {0x006F7978, 0x4, "attackSuccessful"},
    {0x006F7988, 0x4, "g_mem"},
    {0x006F798C, 0x4, "s_impossibleCost"},
    {0x006F7990, 0x4, "s_zWeight"},
    {0x006F79A8, 0x4, "LOOK_AHEAD_T"},
    {0x006F79AC, 0x4, "GET_UP_ATTACK_RANGE"},
    {0x006F79B0, 0x4, "minDepth.2436"},
    {0x006F79B4, 0x4, "maxDepth.2437"},
    {0x006F79B8, 0x4, "LOW_GRAPPLE_STAMINA"},
    {0x006F79C0, 0x4, "s_loadFromRef"},
    {0x006F79D8, 0x4, "POWERUP_TIME"},
    {0x006F79DC, 0x4, "FIRE_TIME"},
    {0x006F79E0, 0x4, "SWARM_TIME"},
    {0x006F79E4, 0x4, "PICK_UP_GRAB_RANGE"},
    {0x006F79E8, 0x4, "high.2424"},
    {0x006F79EC, 0x4, "med.2425"},
    {0x006F79F0, 0x4, "med.2444"},
    {0x006F79FC, 0x4, "s_rangeCorrectFactor.2565"},
    {0x006F7A00, 0x4, "s_catchLookAhead.2605"},
    {0x006F7A04, 0x4, "s_catchRange.2606"},
    {0x006F7A08, 0x4, "s_lookAhead.2625"},
    {0x006F7A0C, 0x4, "s_hitRange.2626"},
    {0x006F7A10, 0x4, "s_ai"},
    {0x006F7A14, 0x4, "s_net"},
    {0x006F7A18, 0x4, "ELEVATOR_Z.2513"},
    {0x006F7A1C, 0x4, "ELEVATOR_RADIUS.2514"},
    {0x006F7A20, 0x4, "UPPER_RING_RADIUS.2515"},
    {0x006F7A24, 0x4, "minSpeed.2534"},
    {0x006F7A3C, 0x4, "sPinTestFOV"},
    {0x006F7A40, 0x4, "sShakeEnabled"},
    {0x006F7A44, 0x4, "zChangeTime.2523"},
    {0x006F7A48, 0x4, "canyon2LevelProgression"},
    {0x006F7A4C, 0x4, "startMovieCount"},
    {0x006F7A50, 0x4, "finalFade.2571"},
    {0x006F7A54, 0x4, "toggleCount.2572"},
    {0x006F7A58, 0x4, "assBossActive"},
    {0x006F7A5C, 0x4, "antHillPlugged"},
    {0x006F7A60, 0x4, "hintPowerUpIndex"},
    {0x006F7A64, 0x4, "MAX_ITERATIONS.2532"},
    {0x006F7A68, 0x4, "COLLIS_EPSILON.2533"},
    {0x006F7A6C, 0x4, "s_snapDelta.2546"},
    {0x006F7A70, 0x4, "snapOffTorqueThresh.2547"},
    {0x006F7A74, 0x4, "backoutStrength.2554"},
    {0x006F7AA4, 0x4, "losTestA.2427"},
    {0x006F7AA8, 0x4, "losTestB.2428"},
    {0x006F7AAC, 0x4, "mechaShadowRadius"},
    {0x006F7AB0, 0x4, "bossMph"},
    {0x006F7AB4, 0x4, "toggleCount.2484"},
    {0x006F7AB8, 0x4, "smoothFactor.2488"},
    {0x006F7ABC, 0x4, "degreeOffset.2489"},
    {0x006F7AC0, 0x4, "inc.2490"},
    {0x006F7AC8, 0x4, "MAX_FORCE_MAG"},
    {0x006F7ACC, 0x4, "MAX_SPEED"},
    {0x006F7AD0, 0x4, "NEIGHBOR_RANGE"},
    {0x006F7AD4, 0x4, "SIDE_CORRECTION"},
    {0x006F7AD8, 0x4, "END_CORRECTION"},
    {0x006F7ADC, 0x4, "separateDistSq"},
    {0x006F7AE0, 0x4, "separateStrength"},
    {0x006F7AE4, 0x4, "cohereDistSq"},
    {0x006F7AE8, 0x4, "cohereStrength"},
    {0x006F7AEC, 0x4, "alignDistSq"},
    {0x006F7AF0, 0x4, "alignStrength"},
    {0x006F7AF4, 0x4, "fleeDist"},
    {0x006F7AF8, 0x4, "fleeStrength"},
    {0x006F7AFC, 0x4, "squashRadius"},
    {0x006F7B00, 0x4, "_8PedGroup$s_numPedGroups"},
    {0x006F7B14, 0x4, "stalling.2812"},
    {0x006F7B18, 0x4, "count.2813"},
    {0x006F7B24, 0x4, "monsterStackIdx"},
    {0x006F7B28, 0x4, "m_minUpdateRate"},
    {0x006F7B30, 0x4, "g_levelList"},
    {0x006F7B34, 0x4, "s_shoulderAnalogSensitivity.156"},
    {0x006F7B38, 0x4, "s_turnAnalogSensitivity.157"},
    {0x006F7B3C, 0x4, "s_len.158"},
    {0x006F7B40, 0x4, "s_tMod.2460"},
    {0x006F7B44, 0x4, "s_padControl_006F7B44"},
    {0x006F7B48, 0x4, "s_seekRange"},
    {0x006F7B4C, 0x4, "s_fireRange"},
    {0x006F7B50, 0x4, "s_fleeRange"},
    {0x006F7B54, 0x4, "s_safeRange"},
    {0x006F7B58, 0x4, "BURST_DELAY_MIN"},
    {0x006F7B5C, 0x4, "BURST_DELAY_MAX"},
    {0x006F7B60, 0x4, "BURST_COUNT_MIN"},
    {0x006F7B64, 0x4, "BURST_COUNT_MAX"},
    {0x006F7B68, 0x4, "FIRE_DELAY_MIN"},
    {0x006F7B6C, 0x4, "FIRE_DELAY_MAX"},
    {0x006F7B70, 0x4, "HOVER_DELAY_MIN"},
    {0x006F7B74, 0x4, "HOVER_DELAY_MAX"},
    {0x006F7B78, 0x4, "FIRE_DELAY_006F7B78"},
    {0x006F7B7C, 0x4, "FIRE_DELAY_RANDOMNESS_006F7B7C"},
    {0x006F7B80, 0x4, "s_drag"},
    {0x006F7B84, 0x4, "s_elevatorThrust"},
    {0x006F7B88, 0x4, "s_headToVel"},
    {0x006F7B8C, 0x4, "s_tiltThrust"},
    {0x006F7B90, 0x4, "s_tiltToVel"},
    {0x006F7B94, 0x4, "s_slowRange.759"},
    {0x006F7B98, 0x4, "CAR_RADIUS.763"},
    {0x006F7B9C, 0x4, "_$tmp_0.765"},
    {0x006F7BA0, 0x4, "LOOK_AHEAD_DT.766"},
    {0x006F7BA4, 0x4, "COLLECT_MAX.767"},
    {0x006F7BA8, 0x4, "s_bubbleRad.771"},
    {0x006F7BAC, 0x4, "STILL_VEL"},
    {0x006F7BB0, 0x4, "STILL_FIELDS"},
    {0x006F7BB4, 0x4, "_9HomingBug$s_numBugs"},
    {0x006F7BB8, 0x4, "gLowStaminaThreshold"},
    {0x006F7BBC, 0x4, "_12Letterboxing$letterBoxDist"},
    {0x006F7BC0, 0x4, "_12Letterboxing$letterBoxVert"},
    {0x006F7BE0, 0x4, "gMessageBuf"},
    {0x006F7BF0, 0x8, "islandMessages"},
    {0x006F7BF8, 0x4, "message.2673"},
    {0x006F7BFC, 0x4, "message.2674"},
    {0x006F7C00, 0x4, "message.2675"},
    {0x006F7C04, 0x4, "message.2676"},
    {0x006F7C08, 0x4, "hintCount.2677"},
    {0x006F7C0C, 0x4, "curMessage.2678"},
    {0x006F7C18, 0x4, "increment.2725"},
    {0x006F7C1C, 0x4, "gHudEnable"},
    {0x006F7C20, 0x4, "gRangeToNearPlane"},
    {0x006F7C24, 0x4, "gHudTextureObjNum"},
    {0x006F7C28, 0x4, "m_textBoxVertMax"},
    {0x006F7C30, 0x4, "g_numCharsInTitleBeforeLineBreak"},
    {0x006F7C34, 0x4, "funcNo.2597"},
    {0x006F7C38, 0x4, "funcNo.2612"},
    {0x006F7C44, 0x4, "funcNo.2625"},
    {0x006F7C48, 0x4, "funcNo.2640"},
    {0x006F7C4C, 0x4, "funcNo.2651"},
    {0x006F7C50, 0x4, "funcNo.2659"},
    {0x006F7C60, 0x4, "fileDesc.2666"},
    {0x006F7C64, 0x4, "result.2667"},
    {0x006F7C78, 0x4, "iconFileSize.2703"},
    {0x006F7C90, 0x5, "s_charMap"},
    {0x006F7C98, 0x4, "xoffset.2775"},
    {0x006F7C9C, 0x4, "yoffset.2776"},
    {0x006F7CA0, 0x4, "xscale.2777"},
    {0x006F7CA4, 0x4, "yscale.2778"},
    {0x006F7CA8, 0x4, "xLetterScale.2779"},
    {0x006F7CAC, 0x4, "buttonDist.2780"},
    {0x006F7CB0, 0x4, "_$tmp_0.2902"},
    {0x006F7CB4, 0x4, "_$tmp_1.2904"},
    {0x006F7CB8, 0x4, "bgDist.2914"},
    {0x006F7CC8, 0x4, "s_padControl_006F7CC8"},
    {0x006F7CCC, 0x4, "_18MissileTruckPickup$s_windUpDelayMin"},
    {0x006F7CD0, 0x4, "_18MissileTruckPickup$s_windUpDelayMax"},
    {0x006F7CD4, 0x4, "_18MissileTruckPickup$s_burstCountMin"},
    {0x006F7CD8, 0x4, "_18MissileTruckPickup$s_burstCountMax"},
    {0x006F7CDC, 0x4, "_18MissileTruckPickup$s_fireDelayMin"},
    {0x006F7CE0, 0x4, "_18MissileTruckPickup$s_fireDelayMax"},
    {0x006F7CE4, 0x4, "_18MissileTruckPickup$s_windDownDelayMin"},
    {0x006F7CE8, 0x4, "_18MissileTruckPickup$s_windDownDelayMax"},
    {0x006F7CEC, 0x4, "_18MissileTruckPickup$s_seekRange"},
    {0x006F7CF0, 0x4, "_18MissileTruckPickup$s_fireRange"},
    {0x006F7CF4, 0x4, "_18MissileTruckPickup$s_fireAngleTol"},
    {0x006F7CF8, 0x4, "_11StatePickUp$s_attemptVelFactor"},
    {0x006F7CFC, 0x4, "s_bigRecoilTrans.2826"},
    {0x006F7D00, 0x4, "WaterLevel"},
    {0x006F7D04, 0x4, "cloaker.2691"},
    {0x006F7D08, 0x4, "s_heldRad.2704"},
    {0x006F7D0C, 0x4, "s_lookAtReleaseSmoother.2753"},
    {0x006F7D10, 0x4, "s_maxVel.2799"},
    {0x006F7D1C, 0x4, "_15MonsterDynamics$s_turnModZeroAtSpeed"},
    {0x006F7D20, 0x4, "_15MonsterDynamics$s_turnModMin"},
    {0x006F7D24, 0x4, "s_aiTurnFactor.2457"},
    {0x006F7D28, 0x4, "s_pushMag.2461"},
    {0x006F7D30, 0x4, "g_monsterToUnlock"},
    {0x006F7D34, 0x4, "g_skinToUnlock"},
    {0x006F7D60, 0x4, "hasValidCard.2808"},
    {0x006F7D64, 0x4, "hasEnoughSpace.2809"},
    {0x006F7D70, 0x4, "s_tmDirName.2834"},
    {0x006F7D74, 0x4, "s_tmGlobalName.2835"},
    {0x006F7D98, 0x4, "s_mon"},
    {0x006F7D9C, 0x4, "clingSlack.2496"},
    {0x006F7DA0, 0x4, "slack.2503"},
    {0x006F7DA4, 0x4, "TOE_IN.2513"},
    {0x006F7DA8, 0x4, "FACE_ANGLE_TOL.2517"},
    {0x006F7DAC, 0x4, "TURN_ANGLE_TOL.2518"},
    {0x006F7DB0, 0x4, "bankRange.2582"},
    {0x006F7DB4, 0x4, "turnFactor.2583"},
    {0x006F7DB8, 0x4, "s_impactVel.2590"},
    {0x006F7DBC, 0x4, "s_turnCap.2669"},
    {0x006F7DC0, 0x4, "_8OgreMace$s_ballEp"},
    {0x006F7DE8, 0x4, "numDeadHeads"},
    {0x006F7DEC, 0x4, "numLoops.2562"},
    {0x006F7DF0, 0x4, "finalFade.2563"},
    {0x006F7DF4, 0x4, "junk.2582"},
    {0x006F7DF8, 0x4, "playerThrown.2619"},
    {0x006F7DFC, 0x4, "hitPowerUps"},
    {0x006F7E00, 0x4, "s_grabHealthWhenFull"},
    {0x006F7E04, 0x4, "s_grabStaminaWhenFull"},
    {0x006F7E08, 0x4, "s_regenMultiplier"},
    {0x006F7E0C, 0x4, "s_cloakTime"},
    {0x006F7E20, 0x4, "gFieldCounter"},
    {0x006F7E24, 0x4, "doTweaks"},
    {0x006F7E28, 0x4, "grappleLastFrame"},
    {0x006F7E2C, 0x4, "tweakLastFrame"},
    {0x006F7E30, 0x8, "rtOldSp"},
    {0x006F7E38, 0x4, "g_frame"},
    {0x006F7E48, 0x4, "MAX_COLLIS_ITERATIONS"},
    {0x006F7E4C, 0x4, "size.2487"},
    {0x006F7E50, 0x4, "number.2488"},
    {0x006F7E54, 0x4, "jaggedness.2489"},
    {0x006F7E58, 0x4, "whichAction_006F7E58"},
    {0x006F7E68, 0x4, "quakeDone"},
    {0x006F7E70, 0x4, "NamecardDistance"},
    {0x006F7E74, 0x4, "targetAlpha"},
    {0x006F7E78, 0x4, "numModsLeft"},
    {0x006F7E7C, 0x4, "UnlockTitle"},
    {0x006F7E80, 0x4, "screenFirstPass"},
    {0x006F7E84, 0x4, "PlayerConfig"},
    {0x006F7E88, 0x4, "currAnimationIndex"},
    {0x006F7E8C, 0x4, "betweenScreens"},
    {0x006F7E90, 0x8, "numSelectablesH"},
    {0x006F7E98, 0x4, "AINumLeft"},
    {0x006F7E9C, 0x4, "AINum"},
    {0x006F7EA0, 0x4, "mSelectWins"},
    {0x006F7EA4, 0x4, "charSelectStringOne"},
    {0x006F7EA8, 0x4, "charSelectStringTwo"},
    {0x006F7EB0, 0x4, "numeralString"},
    {0x006F7EBC, 0x4, "animq"},
    {0x006F7EC0, 0x4, "changingReel"},
    {0x006F7EC4, 0x4, "numSelectables.2784"},
    {0x006F7EC8, 0x4, "first.2788"},
    {0x006F7ECC, 0x4, "last.2789"},
    {0x006F7ED0, 0x4, "first.2793"},
    {0x006F7ED4, 0x4, "last.2794"},
    {0x006F7ED8, 0x4, "first.2798"},
    {0x006F7EDC, 0x4, "last.2799"},
    {0x006F7EE0, 0x4, "first.2803"},
    {0x006F7EE4, 0x4, "last.2804"},
    {0x006F7EE8, 0x4, "first.2808"},
    {0x006F7EEC, 0x4, "last.2809"},
    {0x006F7EF0, 0x4, "first.2813"},
    {0x006F7EF4, 0x4, "last.2814"},
    {0x006F7EF8, 0x4, "first.2818"},
    {0x006F7EFC, 0x4, "last.2819"},
    {0x006F7F00, 0x4, "numSelectables.2823"},
    {0x006F7F04, 0x4, "numSelectables.2827"},
    {0x006F7F08, 0x4, "m_textBoxAlpha.2828"},
    {0x006F7F0C, 0x4, "numAvail1.2830"},
    {0x006F7F10, 0x4, "first.2831"},
    {0x006F7F14, 0x4, "last.2832"},
    {0x006F7F18, 0x4, "curCostume1.2833"},
    {0x006F7F1C, 0x4, "stoppedMovie.2834"},
    {0x006F7F20, 0x4, "showTexCounter.2835"},
    {0x006F7F24, 0x4, "numSelectables.2839"},
    {0x006F7F28, 0x4, "numAvail1.2841"},
    {0x006F7F2C, 0x4, "stoppedMovie.2842"},
    {0x006F7F30, 0x4, "showTexCounter.2843"},
    {0x006F7F34, 0x4, "selectingIsComplete.2844"},
    {0x006F7F38, 0x4, "_$tmp_0.2846"},
    {0x006F7F3C, 0x4, "curCostume1.2847"},
    {0x006F7F40, 0x4, "first.2848"},
    {0x006F7F44, 0x4, "last.2849"},
    {0x006F7F48, 0x4, "numSelectables.2853"},
    {0x006F7F4C, 0x4, "numSelectables.2857"},
    {0x006F7F50, 0x4, "beginning.2861"},
    {0x006F7F54, 0x4, "ending.2862"},
    {0x006F7F58, 0x4, "m_textBoxAlpha.2863"},
    {0x006F7F5C, 0x4, "m_textBoxAlpha2.2864"},
    {0x006F7F60, 0x4, "m_textBoxAlpha3.2865"},
    {0x006F7F64, 0x4, "numSelectablesOne.2866"},
    {0x006F7F68, 0x4, "numSelectablesTwo.2867"},
    {0x006F7F6C, 0x4, "curCostume1.2868"},
    {0x006F7F70, 0x4, "curCostume2.2869"},
    {0x006F7F74, 0x4, "curCostume3.2870"},
    {0x006F7F78, 0x4, "playerOneIsDone.2871"},
    {0x006F7F7C, 0x4, "playerTwoIsDone.2872"},
    {0x006F7F80, 0x4, "aiOneIsDone.2873"},
    {0x006F7F84, 0x4, "aiTwoIsDone.2874"},
    {0x006F7F88, 0x4, "AINum.2875"},
    {0x006F7F8C, 0x4, "watchForASecond.2876"},
    {0x006F7F90, 0x4, "numAvail1.2877"},
    {0x006F7F94, 0x4, "numAvail2.2878"},
    {0x006F7F98, 0x4, "numAvail3.2879"},
    {0x006F7F9C, 0x4, "num1.2880"},
    {0x006F7FA0, 0x4, "num2.2881"},
    {0x006F7FA4, 0x4, "num3.2882"},
    {0x006F7FA8, 0x4, "aiOneMon.2883"},
    {0x006F7FAC, 0x4, "aiOneCost.2884"},
    {0x006F7FB0, 0x4, "beginning.2888"},
    {0x006F7FB4, 0x4, "ending.2889"},
    {0x006F7FB8, 0x4, "m_textBoxAlpha.2890"},
    {0x006F7FBC, 0x4, "m_textBoxAlpha2.2891"},
    {0x006F7FC0, 0x4, "numSelectablesOne.2892"},
    {0x006F7FC4, 0x4, "numSelectablesTwo.2893"},
    {0x006F7FC8, 0x4, "curCostume1.2894"},
    {0x006F7FCC, 0x4, "curCostume2.2895"},
    {0x006F7FD0, 0x4, "playerOneIsDone.2896"},
    {0x006F7FD4, 0x4, "playerTwoIsDone.2897"},
    {0x006F7FD8, 0x4, "AIplayerIsDone.2898"},
    {0x006F7FDC, 0x4, "watchForASecond.2899"},
    {0x006F7FE0, 0x4, "numAvail1.2900"},
    {0x006F7FE4, 0x4, "numAvail2.2901"},
    {0x006F7FE8, 0x4, "num1.2902"},
    {0x006F7FEC, 0x4, "num2.2903"},
    {0x006F7FF0, 0x4, "AiLeft.2904"},
    {0x006F7FF4, 0x4, "beginning.2908"},
    {0x006F7FF8, 0x4, "ending.2909"},
    {0x006F7FFC, 0x4, "numSelectablesOne.2910"},
    {0x006F8000, 0x4, "numSelectablesTwo.2911"},
    {0x006F8004, 0x4, "curCostume1.2912"},
    {0x006F8008, 0x4, "curCostume2.2913"},
    {0x006F800C, 0x4, "playerOneIsDone.2914"},
    {0x006F8010, 0x4, "playerTwoIsDone.2915"},
    {0x006F8014, 0x4, "watchForASecond.2916"},
    {0x006F8018, 0x4, "numAvail1.2917"},
    {0x006F801C, 0x4, "numAvail2.2918"},
    {0x006F8020, 0x4, "num1.2919"},
    {0x006F8024, 0x4, "num2.2920"},
    {0x006F8028, 0x4, "numSelectables.2924"},
    {0x006F802C, 0x4, "beginning.2925"},
    {0x006F8030, 0x4, "ending.2926"},
    {0x006F8034, 0x4, "stoppedMovie.2927"},
    {0x006F8038, 0x4, "showTexCounter.2928"},
    {0x006F803C, 0x4, "gSliderBrightness"},
    {0x006F8040, 0x4, "numSelectables.2938"},
    {0x006F8044, 0x4, "Difficulty.2939"},
    {0x006F8048, 0x4, "audio.2940"},
    {0x006F804C, 0x4, "fxVol.2941"},
    {0x006F8050, 0x4, "musVol.2942"},
    {0x006F8054, 0x4, "vib.2943"},
    {0x006F8058, 0x4, "vib2.2944"},
    {0x006F805C, 0x4, "numSelectables.2948"},
    {0x006F8060, 0x4, "numSelectables.2952"},
    {0x006F8064, 0x4, "numSelectables.2959"},
    {0x006F8068, 0x4, "numSelectables.2964"},
    {0x006F806C, 0x4, "numWins.2965"},
    {0x006F8070, 0x4, "needUpdate.2972"},
    {0x006F8074, 0x4, "waitingForAnimation.2977"},
    {0x006F8078, 0x4, "waitingForAlphaFade.2978"},
    {0x006F807C, 0x4, "fadingIn.2979"},
    {0x006F8080, 0x4, "fadingOut.2980"},
    {0x006F8084, 0x4, "halfWayMark.2981"},
    {0x006F8088, 0x4, "currentMenuSelection.2985"},
    {0x006F808C, 0x4, "visibleStart.2986"},
    {0x006F8090, 0x4, "_$tmp_1.2988"},
    {0x006F8094, 0x4, "m.2989"},
    {0x006F8098, 0x4, "s.2990"},
    {0x006F809C, 0x4, "fade.2991"},
    {0x006F80A0, 0x4, "items.2992"},
    {0x006F80A4, 0x4, "loop.2993"},
    {0x006F80A8, 0x4, "loop2.2994"},
    {0x006F80AC, 0x4, "fadingin.2995"},
    {0x006F80CC, 0x4, "m_textBoxAlpha.2999"},
    {0x006F80D0, 0x4, "m_textBoxAlpha2.3000"},
    {0x006F80D4, 0x4, "m_textBoxAlpha3.3001"},
    {0x006F80D8, 0x4, "m_textBoxAlpha4.3002"},
    {0x006F80DC, 0x4, "beginning.3003"},
    {0x006F80E0, 0x4, "ending.3004"},
    {0x006F80E4, 0x4, "numSelectablesOne.3005"},
    {0x006F80E8, 0x4, "numSelectablesTwo.3006"},
    {0x006F80EC, 0x4, "curCostume1.3007"},
    {0x006F80F0, 0x4, "curCostume2.3008"},
    {0x006F80F4, 0x4, "curCostume3.3009"},
    {0x006F80F8, 0x4, "curCostume4.3010"},
    {0x006F80FC, 0x4, "playerOneIsDone.3011"},
    {0x006F8100, 0x4, "playerTwoIsDone.3012"},
    {0x006F8104, 0x4, "aiOneIsDone.3013"},
    {0x006F8108, 0x4, "aiTwoIsDone.3014"},
    {0x006F810C, 0x4, "aiThreeIsDone.3015"},
    {0x006F8110, 0x4, "watchForASecond.3016"},
    {0x006F8114, 0x4, "numAvail1.3017"},
    {0x006F8118, 0x4, "numAvail2.3018"},
    {0x006F811C, 0x4, "numAvail3.3019"},
    {0x006F8120, 0x4, "numAvail4.3020"},
    {0x006F8124, 0x4, "num1.3021"},
    {0x006F8128, 0x4, "num2.3022"},
    {0x006F812C, 0x4, "num3.3023"},
    {0x006F8130, 0x4, "num4.3024"},
    {0x006F8134, 0x4, "aiOneMon.3025"},
    {0x006F8138, 0x4, "aiOneCost.3026"},
    {0x006F813C, 0x4, "aiTwoMon.3027"},
    {0x006F8140, 0x4, "aiTwoCost.3028"},
    {0x006F8144, 0x4, "loop.3044"},
    {0x006F8148, 0x4, "high.3045"},
    {0x006F814C, 0x4, "low.3046"},
    {0x006F8150, 0x4, "alpha.3047"},
    {0x006F8154, 0x4, "fadingIn.3048"},
    {0x006F8158, 0x4, "val1_x.3061"},
    {0x006F815C, 0x4, "val_y.3062"},
    {0x006F8160, 0x4, "val_z.3063"},
    {0x006F8164, 0x4, "val2_x.3064"},
    {0x006F8168, 0x4, "unlocked.3092"},
    {0x006F816C, 0x4, "numSelectablesOne.3096"},
    {0x006F8170, 0x4, "numSelectablesTwo.3097"},
    {0x006F8174, 0x4, "beginning.3098"},
    {0x006F8178, 0x4, "ending.3099"},
    {0x006F8180, 0x4, "g_noScripts"},
    {0x006F8184, 0x4, "UseCommandLineLevel"},
    {0x006F8188, 0x4, "NonStandardNameSelected"},
    {0x006F818C, 0x4, "altAI"},
    {0x006F8190, 0x4, "exitFromAdvStory"},
    {0x006F8194, 0x4, "clearBeforeFontDMA"},
    {0x006F8198, 0x4, "temp.2832"},
    {0x006F81EC, 0x4, "mov.2987"},
    {0x006F81F0, 0x4, "needIntro"},
    {0x006F81F4, 0x4, "needOutro"},
    {0x006F81F8, 0x4, "game"},
    {0x006F81FC, 0x4, "destructibles"},
    {0x006F8200, 0x4, "shell"},
    {0x006F8208, 0x4, "calledFrom"},
    {0x006F820C, 0x4, "gCurrentMenuSelection"},
    {0x006F8210, 0x4, "gLastMenuSelection"},
    {0x006F8214, 0x4, "gExitDialog"},
    {0x006F8218, 0x4, "gDrawOverlay"},
    {0x006F8230, 0x4, "currentMenuSelection.2733"},
    {0x006F8234, 0x4, "reset.2734"},
    {0x006F8238, 0x4, "foundSlot.2735"},
    {0x006F823C, 0x4, "firsttime.2736"},
    {0x006F8240, 0x4, "starting.2737"},
    {0x006F82B8, 0x4, "gPlayerThatPaused"},
    {0x006F82C0, 0x4, "g_onStartupFrame"},
    {0x006F82C4, 0x4, "gIntroMoviesHavePlayed"},
    {0x006F82C8, 0x4, "gInnerPed"},
    {0x006F82CC, 0x4, "gOutterPed"},
    {0x006F82D0, 0x4, "gUseEarsOnMonster"},
    {0x006F82D4, 0x4, "COLLECT_MAX.3000"},
    {0x006F82D8, 0x4, "mTimeOfLastFinalBossStep"},
    {0x006F82F4, 0x4, "mPlayingBossMusic"},
    {0x006F82F8, 0x4, "mTimeOfMusicFadeOut"},
    {0x006F82FC, 0x4, "mOrigBattleFadeOutVol"},
    {0x006F8300, 0x4, "mOrigAmbientFadeOutVol"},
    {0x006F8308, 0x4, "size.2853"},
    {0x006F830C, 0x4, "number.2854"},
    {0x006F8310, 0x4, "jaggedness.2855"},
    {0x006F8314, 0x4, "_10SpecFxAnim$s_numFxs"},
    {0x006F8318, 0x4, "quantizer"},
    {0x006F8330, 0x4, "s_padControl_006F8330"},
    {0x006F8334, 0x4, "_10TankPickup$s_windUpDelayMin"},
    {0x006F8338, 0x4, "_10TankPickup$s_windUpDelayMax"},
    {0x006F833C, 0x4, "_10TankPickup$s_burstCountMin"},
    {0x006F8340, 0x4, "_10TankPickup$s_burstCountMax"},
    {0x006F8344, 0x4, "_10TankPickup$s_fireDelayMin"},
    {0x006F8348, 0x4, "_10TankPickup$s_fireDelayMax"},
    {0x006F834C, 0x4, "_10TankPickup$s_windDownDelayMin"},
    {0x006F8350, 0x4, "_10TankPickup$s_windDownDelayMax"},
    {0x006F8354, 0x4, "_10TankPickup$s_seekRange"},
    {0x006F8358, 0x4, "_10TankPickup$s_fireRange"},
    {0x006F835C, 0x4, "_10TankPickup$s_fireAngleTol"},
    {0x006F8360, 0x4, "_10TankPickup$s_fleeRange"},
    {0x006F8364, 0x4, "_10TankPickup$s_safeRange"},
    {0x006F8368, 0x4, "RECOIL_SPEED"},
    {0x006F836C, 0x4, "FIRE_DELAY_006F836C"},
    {0x006F8370, 0x4, "FIRE_DELAY_RANDOMNESS_006F8370"},
    {0x006F8374, 0x4, "_12TurretPickup$s_fireAngleTol"},
    {0x006F8378, 0x4, "_12TurretPickup$s_weapType"},
    {0x006F8380, 0x4, "monstersRespawned"},
    {0x006F8390, 0x4, "lastCountdownSecs.2663"},
    {0x006F839C, 0x4, "craterSwitched"},
    {0x006F83A0, 0x4, "craterTransitionInProgress"},
    {0x006F83A4, 0x4, "stackCountDownActive"},
    {0x006F83A8, 0x4, "stackCountDown"},
    {0x006F83B0, 0x4, "s_vFactor"},
    {0x006F83B8, 0x4, "s_counterUpright.2448"},
    {0x006F83BC, 0x4, "s_maxCounterAttempts.2449"},
    {0x006F83C0, 0x4, "s_snapAlignThresh.2477"},
    {0x006F83C4, 0x4, "s_snapW.2478"},
    {0x006F83C8, 0x4, "s_collisX.2479"},
    {0x006F83CC, 0x4, "s_collisY.2480"},
    {0x006F83D0, 0x4, "s_collisZ.2481"},
    {0x006F83D4, 0x4, "s_collisForce.2482"},
    {0x006F83D8, 0x4, "s_pushMagX.2483"},
    {0x006F83DC, 0x4, "s_pushMagY.2484"},
    {0x006F83E0, 0x4, "pedsTurnedOff"},
    {0x006F83E4, 0x4, "whichAction.2505"},
    {0x006F83E8, 0x4, "finalFade.2506"},
    {0x006F83EC, 0x4, "toggleCount.2507"},
    {0x006F83F0, 0x4, "tidalWaveActive"},
    {0x006F83F4, 0x4, "tidalWaveCount"},
    {0x006F83F8, 0x4, "tidalSource"},
    {0x006F83FC, 0x4, "movieActive"},
    {0x006F8400, 0x4, "movieCount"},
    {0x006F8404, 0x4, "finalFade.2448"},
    {0x006F8408, 0x4, "toggleCount.2449"},
    {0x006F840C, 0x4, "s_moveFract.2475"},
    {0x006F8410, 0x4, "s_seekMoveFract.2476"},
    {0x006F8414, 0x4, "s_specialSeekMoveFract.2477"},
    {0x006F8418, 0x4, "s_moveStepMin.2481"},
    {0x006F841C, 0x4, "s_moveStepMax.2482"},
    {0x006F8420, 0x4, "_9UfoPickup$s_windUpDelayMin"},
    {0x006F8424, 0x4, "_9UfoPickup$s_windUpDelayMax"},
    {0x006F8428, 0x4, "_9UfoPickup$s_burstCountMin"},
    {0x006F842C, 0x4, "_9UfoPickup$s_burstCountMax"},
    {0x006F8430, 0x4, "_9UfoPickup$s_fireDelayMin"},
    {0x006F8434, 0x4, "_9UfoPickup$s_fireDelayMax"},
    {0x006F8438, 0x4, "_9UfoPickup$s_seekRange"},
    {0x006F843C, 0x4, "_9UfoPickup$s_fireRange"},
    {0x006F8440, 0x4, "_9UfoPickup$s_windDownDelayMin"},
    {0x006F8444, 0x4, "_9UfoPickup$s_windDownDelayMax"},
    {0x006F8448, 0x4, "specialBurstCount"},
    {0x006F844C, 0x4, "specialFireDelay"},
    {0x006F8450, 0x4, "specialWindDelay"},
    {0x006F8454, 0x4, "numSaucers"},
    {0x006F8458, 0x4, "continueDecoding"},
    {0x006F845C, 0x4, "pMode"},
    {0x006F8460, 0x4, "prevScreen"},
    {0x006F8464, 0x4, "currScreen"},
    {0x006F8468, 0x4, "lastScreen"},
    {0x006F846C, 0x4, "nextScreen"},
    {0x006F8470, 0x4, "nextMovie"},
    {0x006F8474, 0x4, "whichPlayerIsPicking"},
    {0x006F8478, 0x4, "incogTimer"},
    {0x006F847C, 0x4, "currentMonster"},
    {0x006F8480, 0x4, "lastMonster"},
    {0x006F8484, 0x4, "currentLevel"},
    {0x006F8488, 0x4, "uiWhichPad"},
    {0x006F848C, 0x4, "clutterFrame"},
    {0x006F8490, 0x4, "clutterDelay"},
    {0x006F8494, 0x4, "clutterDelayCnt"},
    {0x006F8498, 0x4, "testPattern"},
    {0x006F849C, 0x4, "AItoChoose"},
    {0x006F84A0, 0x4, "initialAnimation"},
    {0x006F84A4, 0x4, "movieEnable"},
    {0x006F84A8, 0x4, "select"},
    {0x006F84AC, 0x4, "goLoadLevel"},
    {0x006F84B0, 0x4, "exitUi"},
    {0x006F84B4, 0x4, "gManualMonsterChangeHappened"},
    {0x006F84B8, 0x4, "gRealWorldEpNode"},
    {0x006F84BC, 0x4, "lastTicCount"},
    {0x006F84C0, 0x4, "thisTicCount"},
    {0x006F84C4, 0x4, "startTime"},
    {0x006F84C8, 0x4, "frameTime"},
    {0x006F84CC, 0x4, "movieTime"},
    {0x006F84D0, 0x4, "innerFrameTime"},
    {0x006F84D4, 0x4, "g_fadeFrameOffset"},
    {0x006F84E0, 0x4, "movieHasStarted.2745"},
    {0x006F84E4, 0x4, "movieHasStarted.2749"},
    {0x006F84E8, 0x4, "g_shellOddEven"},
    {0x006F84F0, 0x8, "userintOldSp"},
    {0x006F84F8, 0x8, "numPlayerStrings"},
    {0x006F8530, 0x4, "z.510"},
    {0x006F8548, 0x4, "exitDevUi"},
    {0x006F8550, 0x4, "whichAction_006F8550"},
    {0x006F8554, 0x4, "toggleCount.2518"},
    {0x006F8558, 0x4, "vegasSpawnMilitary"},
    {0x006F855C, 0x4, "s_hatField"},
    {0x006F8560, 0x4, "s_hatCounter"},
    {0x006F8564, 0x4, "CAR_RADIUS.792"},
    {0x006F8568, 0x4, "_$tmp_0.794"},
    {0x006F856C, 0x4, "LOOK_AHEAD_DT.795"},
    {0x006F8570, 0x4, "COLLECT_MAX.796"},
    {0x006F8578, 0x4, "gNumViews"},
    {0x006F857C, 0x4, "s_enabled"},
    {0x006F8580, 0x4, "stat_enlarge"},
    {0x006F8584, 0x4, "stat_intensity"},
    {0x006F8588, 0x4, "fb0U0"},
    {0x006F858C, 0x4, "fb0V0"},
    {0x006F8590, 0x4, "fb0U1"},
    {0x006F8594, 0x4, "fb0V1"},
    {0x006F8598, 0x4, "fb0X0"},
    {0x006F859C, 0x4, "fb0Y0"},
    {0x006F85A0, 0x4, "fb0X1"},
    {0x006F85A4, 0x4, "fb0Y1"},
    {0x006F85A8, 0x4, "fb1U0"},
    {0x006F85AC, 0x4, "fb1V0"},
    {0x006F85B0, 0x4, "fb1U1"},
    {0x006F85B4, 0x4, "fb1V1"},
    {0x006F85B8, 0x4, "fb1X0"},
    {0x006F85BC, 0x4, "fb1Y0"},
    {0x006F85C0, 0x4, "fb1X1"},
    {0x006F85C4, 0x4, "fb1Y1"},
    {0x006F85C8, 0x4, "okToBlend"},
    {0x006F85CC, 0x4, "s_blendCurve"},
    {0x006F85D0, 0x4, "linearChar"},
    {0x006F85D4, 0x4, "smoothChar"},
    {0x006F85E8, 0x2, "g_dbsDisableNodeMask"},
    {0x006F8630, 0x4, "dbsStackIdx"},
    {0x006F8638, 0x4, "isCountVblank"},
    {0x006F863C, 0x4, "isFrameEnd"},
    {0x006F8640, 0x4, "isUp"},
    {0x006F8644, 0x4, "handler_error"},
    {0x006F8648, 0x4, "VramAddrForMovies"},
    {0x006F864C, 0x4, "VramWidthForMovies"},
    {0x006F8650, 0x4, "movieIsPlaying"},
    {0x006F8654, 0x4, "vbhEnabled"},
    {0x006F8658, 0x4, "movieHasPlayed.42"},
    {0x006F865C, 0x4, "lastValidTag"},
    {0x006F8660, 0x4, "tempflag.58"},
    {0x006F8664, 0x4, "lastWriteValue"},
    {0x006F8668, 0x4, "localSeed"},
    {0x006F8670, 0x4, "cdHasBeenInitialized"},
    {0x006F8674, 0x4, "FileReadStatus.9"},
    {0x006F86C0, 0x4, "cdReadMode"},
    {0x006F86C8, 0x4, "fogMinRange"},
    {0x006F86CC, 0x4, "fogMaxRange"},
    {0x006F86D0, 0x4, "fogMaxVal"},
    {0x006F86D4, 0x4, "fogFarClip"},
    {0x006F86D8, 0x4, "fogFA"},
    {0x006F86DC, 0x4, "fogFB"},
    {0x006F86E0, 0x4, "fogColorRed"},
    {0x006F86E4, 0x4, "fogColorGreen"},
    {0x006F86E8, 0x4, "fogColorBlue"},
    {0x006F86EC, 0x4, "fogBackgroundColorIsSlaved"},
    {0x006F86F0, 0x4, "fogChanged"},
    {0x006F86F4, 0x4, "fogTOD"},
    {0x006F86F8, 0x4, "fogTODMinRange"},
    {0x006F86FC, 0x4, "fogTODMaxRange"},
    {0x006F8700, 0x4, "fogTODMaxVal"},
    {0x006F8704, 0x4, "fogTODFarClip"},
    {0x006F8708, 0x4, "fogTODFA"},
    {0x006F870C, 0x4, "fogTODFB"},
    {0x006F8710, 0x4, "fogTODColorRed"},
    {0x006F8714, 0x4, "fogTODColorGreen"},
    {0x006F8718, 0x4, "fogTODColorBlue"},
    {0x006F871C, 0x4, "fontFirstFrame"},
    {0x006F8720, 0x4, "spriteTESTlocation"},
    {0x006F8724, 0x4, "spriteFBAlocation"},
    {0x006F8728, 0x4, "spritePRIMlocation"},
    {0x006F872C, 0x4, "colGrid"},
    {0x006F8730, 0x4, "hdTraceId"},
    {0x006F8734, 0x4, "csEpNodeNull"},
    {0x006F8738, 0x4, "hdDefaultHeight"},
    {0x006F873C, 0x4, "g_traceId"},
    {0x006F874C, 0x4, "s_checkBackface"},
    {0x006F8768, 0x4, "gTraversalCallback"},
    {0x006F876C, 0x4, "g_dontTraverseObjects"},
    {0x006F8770, 0x4, "g_doFOVGroups"},
    {0x006F8774, 0x4, "dirty.476"},
    {0x006F8778, 0x4, "matStack"},
    {0x006F877C, 0x4, "fovNorms"},
    {0x006F8780, 0x4, "fovNorms1"},
    {0x006F8784, 0x4, "lightDir"},
    {0x006F8788, 0x4, "skelMatStack"},
    {0x006F878C, 0x4, "skelStack"},
    {0x006F8790, 0x4, "eoCnt"},
    {0x006F8794, 0x4, "lastEoCnt"},
    {0x006F8798, 0x4, "lastGsCtx"},
    {0x006F879C, 0x4, "objectCount"},
    {0x006F87A0, 0x4, "objsInPacket"},
    {0x006F87A4, 0x4, "objsInAlphaPacket"},
    {0x006F87A8, 0x4, "vertexCount"},
    {0x006F87AC, 0x4, "hierClipRange"},
    {0x006F87B0, 0x4, "firstDmaObj"},
    {0x006F87B4, 0x4, "ldmaVu1"},
    {0x006F87B8, 0x4, "dmaAlphaVu1"},
    {0x006F87BC, 0x4, "tracingShadow"},
    {0x006F87C0, 0x4, "shadowLodRangeSq"},
    {0x006F87C4, 0x4, "world"},
    {0x006F87C8, 0x4, "gCs"},
    {0x006F87CC, 0x4, "g_CameraLos"},
    {0x006F87D0, 0x4, "g_CameraLosPerView"},
    {0x006F87D4, 0x4, "interactiveState"},
    {0x006F87D8, 0x4, "lensFlareListPtr"},
    {0x006F87DC, 0x4, "lensFlareListIndex"},
    {0x006F87E0, 0x4, "iBookmark"},
    {0x006F87E4, 0x4, "texmNumTextures"},
    {0x006F87E8, 0x4, "texBins"},
    {0x006F87EC, 0x4, "texLastUsedCnt"},
    {0x006F87F0, 0x4, "nextVramAddr"},
    {0x006F87F4, 0x4, "startVramAddr"},
    {0x006F87F8, 0x4, "displayBuffer"},
    {0x006F87FC, 0x4, "movieBuffer"},
    {0x006F8800, 0x4, "lastVramTexAddr"},
    {0x006F8804, 0x4, "loadingShellTex"},
    {0x006F8808, 0x4, "texmK"},
    {0x006F880C, 0x4, "texmL"},
    {0x006F8810, 0x4, "texmAdjustId"},
    {0x006F8814, 0x4, "texmFilter"},
    {0x006F8818, 0x4, "tempVramTexAddr"},
    {0x006F881C, 0x4, "psZoomT"},
    {0x006F8820, 0x4, "psShadowIntens"},
    {0x006F8824, 0x4, "psShowObjects"},
    {0x006F8828, 0x4, "psBlkIdx"},
    {0x006F882C, 0x4, "psViewCs"},
    {0x006F8830, 0x4, "psBufNum"},
    {0x006F8834, 0x4, "psObjectCnt"},
    {0x006F8838, 0x4, "sg_do2Circuit"},
    {0x006F883C, 0x4, "sg_doHalfTex"},
    {0x006F8840, 0x4, "movieRequested"},
    {0x006F8844, 0x4, "movieIsCompletelyFinished"},
    {0x006F8848, 0x4, "AlmostInVram"},
    {0x006F884C, 0x4, "IsInVram"},
    {0x006F8850, 0x4, "voBufData"},
    {0x006F8854, 0x4, "voBufTag"},
    {0x006F8858, 0x4, "viBufData"},
    {0x006F885C, 0x4, "readBuf"},
    {0x006F8860, 0x4, "mpegWork"},
    {0x006F8864, 0x4, "isWithAudio"},
    {0x006F8868, 0x4, "readrest"},
    {0x006F8870, 0x4, "writerest"},
    {0x006F8874, 0x4, "isStarted"},
    {0x006F8878, 0x4, "isPaused"},
    {0x006F887C, 0x4, "flushedBuffers"},
    {0x006F8880, 0x4, "videoDecTh"},
    {0x006F8884, 0x4, "defaultTh"},
    {0x006F8888, 0x4, "bank"},
    {0x006F888C, 0x4, "put_ptr"},
    {0x006F8890, 0x4, "get_ptr"},
    {0x006F8894, 0x4, "putsize"},
    {0x006F8898, 0x4, "getsize"},
    {0x006F889C, 0x4, "controller_val"},
    {0x006F88A0, 0x4, "originalTexId"},
    {0x006F88A4, 0x4, "originalNumTextures"},
    {0x006F88A8, 0x4, "gO"},
    {0x006F88AC, 0x4, "alreadyPlaying"},
    {0x006F88B0, 0x4, "ScreenObjectPending"},
    {0x006F88B4, 0x4, "gMovObj"},
    {0x006F88B8, 0x4, "gMovWid"},
    {0x006F88BC, 0x4, "gMovHei"},
    {0x006F88C0, 0x4, "movieFrameCounter"},
    {0x006F88C4, 0x4, "moviePreFrameCounter"},
    {0x006F88C8, 0x4, "CurrentMovieScreen"},
    {0x006F88D0, 0x8, "seed"},
    {0x006F88D8, 0x4, "firstTimeAround"},
    {0x006F88DC, 0x4, "texIndex"},
    {0x006F88E0, 0x4, "count_006F88E0"},
    {0x006F88E4, 0x4, "numActiveFires"},
    {0x006F88E8, 0x4, "displayListTop"},
    {0x006F88EC, 0x4, "fxActiveTop"},
    {0x006F88F0, 0x4, "fxInactiveTop"},
    {0x006F88F8, 0x8, "lensFlareIndex"},
    {0x006F8900, 0x8, "lensFlareList"},
    {0x006F8908, 0x8, "lensFlarePacket"},
    {0x006F8910, 0x4, "splats"},
    {0x006F8918, 0x4, "paraLightColor1"},
    {0x006F8920, 0x4, "paraLightColor2"},
    {0x006F8928, 0x4, "paraLightColor3"},
    {0x006F892C, 0x4, "doParaLights"},
    {0x006F8930, 0x4, "visualizeLights"},
    {0x006F8934, 0x4, "visLength"},
    {0x006F8938, 0x4, "specularColor"},
    {0x006F8958, 0x4, "s_allocatedBufferSpace"},
    {0x006F895C, 0x4, "s_numSourceObjects"},
    {0x006F8968, 0x4, "nextSector"},
    {0x006F896C, 0x4, "finalSector"},
    {0x006F8970, 0x4, "isStrFileInit"},
    {0x006F8974, 0x4, "partialMovie"},
    {0x006F8990, 0x4, "mode"},
    {0x006F8998, 0x4, "pkt"},
    {0x006F899C, 0x4, "GIFstart"},
    {0x006F89A0, 0x4, "GIFeopBit"},
    {0x006F89A4, 0x4, "GIFnumRegs"},
    {0x006F89A8, 0x4, "DMAbegin"},
    {0x006F89AC, 0x4, "DMAlink"},
    {0x006F89B0, 0x4, "DIRECTstart"},
    {0x006F89B4, 0x4, "TestReg"},
    {0x006F89B8, 0x4, "todTimeOfDayOn"},
    {0x006F89BC, 0x4, "todSetTimeOfDay"},
    {0x006F89C0, 0x4, "todTimeOfDayMinutesPerSecond"},
    {0x006F89C4, 0x4, "todTimeOfDay"},
    {0x006F89C8, 0x4, "todFrame"},
    {0x006F89CC, 0x4, "todAddFrame"},
    {0x006F89D0, 0x4, "todDeleteFrame"},
    {0x006F89D4, 0x4, "startTOD.144"},
    {0x006F89D8, 0x4, "_$tmp_0.146"},
    {0x006F89DC, 0x4, "todAmbientRed"},
    {0x006F89E0, 0x4, "todAmbientGreen"},
    {0x006F89E4, 0x4, "todAmbientBlue"},
    {0x006F89E8, 0x4, "todDirHeading"},
    {0x006F89EC, 0x4, "todDirPitch"},
    {0x006F89F0, 0x4, "todDirRed"},
    {0x006F89F4, 0x4, "todDirGreen"},
    {0x006F89F8, 0x4, "todDirBlue"},
    {0x006F89FC, 0x4, "todBackHeading"},
    {0x006F8A00, 0x4, "todBackPitch"},
    {0x006F8A04, 0x4, "todBackIntensity"},
    {0x006F8A08, 0x4, "todBackRed"},
    {0x006F8A0C, 0x4, "todBackGreen"},
    {0x006F8A10, 0x4, "todBackBlue"},
    {0x006F8A14, 0x4, "todFogMinRange"},
    {0x006F8A18, 0x4, "todFogMaxRange"},
    {0x006F8A1C, 0x4, "todFogMaxVal"},
    {0x006F8A20, 0x4, "todFogFarClip"},
    {0x006F8A24, 0x4, "todFogRed"},
    {0x006F8A28, 0x4, "todFogGreen"},
    {0x006F8A2C, 0x4, "todFogBlue"},
    {0x006F8A30, 0x4, "todBackgroundRed"},
    {0x006F8A34, 0x4, "todBackgroundGreen"},
    {0x006F8A38, 0x4, "todBackgroundBlue"},
    {0x006F8A3C, 0x4, "todSkyTarget"},
    {0x006F8A40, 0x4, "todSkyUMod"},
    {0x006F8A44, 0x4, "todSkyVMod"},
    {0x006F8A48, 0x4, "todSkyFogBurnThru"},
    {0x006F8A4C, 0x4, "todSkyRedMod"},
    {0x006F8A50, 0x4, "todSkyGreenMod"},
    {0x006F8A54, 0x4, "todSkyBlueMod"},
    {0x006F8A58, 0x4, "todCloudRedMod"},
    {0x006F8A5C, 0x4, "todCloudGreenMod"},
    {0x006F8A60, 0x4, "todCloudBlueMod"},
    {0x006F8A64, 0x4, "todCloudAlphaMod"},
    {0x006F8A68, 0x4, "todNumSkys"},
    {0x006F8A6C, 0x4, "todSkySetFound"},
    {0x006F8A70, 0x4, "whichBuildBar"},
    {0x006F8A74, 0x4, "gWhichMacroSection"},
    {0x006F8A78, 0x4, "gWhichMicroSection"},
    {0x006F8A7C, 0x4, "gOkToDrawProgressBar"},
    {0x006F8A88, 0x4, "viewTimeOfDay"},
    {0x006F8A8C, 0x4, "gUseUnifiedView"},
    {0x006F8A90, 0x4, "viewFovH"},
    {0x006F8A94, 0x4, "viewDegrees"},
    {0x006F8A98, 0x4, "viewBgR"},
    {0x006F8A9C, 0x4, "viewBgG"},
    {0x006F8AA0, 0x4, "viewBgB"},
    {0x006F8AA4, 0x4, "viewBgA"},
    {0x006F8AA8, 0x4, "viewTODBgR"},
    {0x006F8AAC, 0x4, "viewTODBgG"},
    {0x006F8AB0, 0x4, "viewTODBgB"},
    {0x006F8AB4, 0x4, "viewTODBgA"},
    {0x006F8AB8, 0x4, "viewAmbientRed"},
    {0x006F8ABC, 0x4, "viewAmbientGreen"},
    {0x006F8AC0, 0x4, "viewAmbientBlue"},
    {0x006F8AC4, 0x4, "viewTODAmbientRed"},
    {0x006F8AC8, 0x4, "viewTODAmbientGreen"},
    {0x006F8ACC, 0x4, "viewTODAmbientBlue"},
    {0x006F8AD0, 0x4, "viewAnimAmbientRed"},
    {0x006F8AD4, 0x4, "viewAnimAmbientGreen"},
    {0x006F8AD8, 0x4, "viewAnimAmbientBlue"},
    {0x006F8ADC, 0x4, "viewAmbientChanged"},
    {0x006F8AE0, 0x4, "viewNumViews"},
    {0x006F8AE4, 0x4, "viewCurView"},
    {0x006F8AE8, 0x4, "viewRearViewConfig"},
    {0x006F8AEC, 0x4, "viewRearLargeActive"},
    {0x006F8AF0, 0x4, "whichHalfMeg"},
    {0x006F8AF4, 0x4, "G_windowPos"},
    {0x006F8AF8, 0x4, "G_bitCount"},
    {0x006F8AFC, 0x4, "G_bitBucket"},
    {0x006F8B00, 0x8, "G_crc32val"},
    {0x006F8B08, 0x4, "G_fixedTlen"},
    {0x006F8B0C, 0x4, "G_FileName"},
    {0x006F8B10, 0x4, "G_FileAddr"},
    {0x006F8B18, 0x4, "outFileWindow"},
    {0x006F8B1C, 0x4, "tmpLong"},
    {0x006F8B20, 0x8, "G_outSize"},
    {0x006F8B28, 0x4, "G_fixedTdist"},
    {0x006F8B2C, 0x4, "G_fixedBlen"},
    {0x006F8B30, 0x4, "G_fixedBdist"},
    {0x006F8B34, 0x4, "howManyBlocks"},
    {0x006F8B38, 0x4, "zipFileBuf"},
    {0x006F8B3C, 0x4, "outFileBuf"},
    {0x006F8B40, 0x4, "gRet"},
    {0x006F8B80, 0x0, "_fbss"},
    {0x006F8DF8, 0x4, "_12Interactives$s_numInteractives"},
    {0x006F8E00, 0x8, "__ti13AiActionTuple"},
    {0x006F8E08, 0x8, "__ti12AiActionList"},
    {0x006F8E10, 0x8, "__ti12PointToolKit"},
    {0x006F8E18, 0x8, "__ti8resetcom"},
    {0x006F8E20, 0x8, "__ti13DbInteractive"},
    {0x006F8E28, 0x4, "_7Cameras$m_ticsAtUnifiedInit"},
    {0x006F8E2C, 0x4, "_7Cameras$m_leaveTriggered"},
    {0x006F8E30, 0x4, "_7Cameras$m_numCameras"},
    {0x006F8E34, 0x4, "_7Cameras$m_currentFadeAlpha"},
    {0x006F8E38, 0x4, "_7Cameras$m_leaveReason"},
    {0x006F8E3C, 0x4, "_7Cameras$m_animDisableSwitch"},
    {0x006F8E40, 0x4, "_9HomingBug$s_homingFactor"},
    {0x006F8E44, 0x4, "_9HomingBug$s_avoidanceFactor"},
    {0x006F8E48, 0x4, "_9HomingBug$s_maxForce"},
    {0x006F8E4C, 0x4, "_9HomingBug$s_maxVel"},
    {0x006F8E50, 0x4, "_9HomingBug$s_initialVel"},
    {0x006F8E54, 0x4, "_9HomingBug$s_initialHealth"},
    {0x006F8E58, 0x4, "_9HomingBug$s_attachRange"},
    {0x006F8E5C, 0x4, "_9HomingBug$s_attachMinAngle"},
    {0x006F8E60, 0x4, "_9HomingBug$s_attachFields"},
    {0x006F8E64, 0x4, "_9HomingBug$s_maxSeekFields"},
    {0x006F8E68, 0x4, "_9HomingBug$s_headRadius"},
    {0x006F8E6C, 0x4, "_9HomingBug$s_feelerRadius"},
    {0x006F8E70, 0x4, "_9HomingBug$s_collisRadius"},
    {0x006F8E74, 0x4, "_9HomingBug$s_damageRadius"},
    {0x006F8E78, 0x4, "_9HomingBug$s_damagePerSecond"},
    {0x006F8E7C, 0x4, "_9HomingBug$s_maxMonsterDamage"},
    {0x006F8E80, 0x4, "_9HomingBug$s_targetLockMaxRange"},
    {0x006F8E84, 0x4, "_9HomingBug$s_targetLockMinAngle"},
    {0x006F8E88, 0x4, "_6Debris$m_damageMod"},
    {0x006F8E8C, 0x4, "_6Debris$m_damageThreshold"},
    {0x006F8E90, 0x8, "__ti12MonsterState"},
    {0x006F8E98, 0x4, "_12LevelPickups$s_militaryStillAlive"},
    {0x006F8E9C, 0x4, "_9HomingBug$s_targetLockDotMin"},
    {0x006F8EA0, 0x8, "__ti6McTask"},
    {0x006F8EA8, 0x8, "__ti6McPage"},
    {0x006F8EB0, 0x8, "__ti17MilitaryFormation"},
    {0x006F8EB8, 0x8, "__ti20PunchSwipeConfigBase"},
    {0x006F8EC0, 0x1, "_12LevelPickups$s_colorInc"},
    {0x006F8EC2, 0x2, "_12LevelPickups$s_color"},
    {0x006F8EC4, 0x2, "_12LevelPickups$s_green"},
    {0x006F8EC8, 0x8, "_12LevelPickups$s_highlight"},
    {0x006F8ED0, 0x4, "_11ShellRtLoop$m_oddEven"},
    {0x006F8ED8, 0x8, "__ti14CScreenSection"},
    {0x006F8EE0, 0x8, "__ti7CScreen"},
    {0x006F8EE8, 0x8, "__ti9exception"},
    {0x006F8EF0, 0x8, "__ti9type_info"},
    {0x006F8EF8, 0x4, "errno"},
    {0x0077A280, 0x1010, "viBufTag"},
    {0x0077B2C0, 0x800, "defStack"},
    {0x0077BAC0, 0x4000, "videoDecStack"},
    {0x0077FAC0, 0xC000, "audioBuff"},
    {0x007BBED0, 0x1C00, "_11DelayAction$m_pool"},
    {0x007BDAD0, 0x2718, "_8PowerUps$instance"},
    {0x007C01F0, 0x3AC0, "_7Cameras$m_cameras"},
    {0x007C3CB0, 0x5B80, "_9AiPathNet$monster"},
    {0x007C9830, 0xC, "__ti18AiThrashActionList"},
    {0x007C9840, 0xC, "__ti20AiGrappledActionList"},
    {0x007C9850, 0xC, "__ti21AiGrapplingActionList"},
    {0x007C9860, 0xC, "__ti14AiGrappleThrow"},
    {0x007C9870, 0xC, "__ti15AiGrappleAttack"},
    {0x007C9880, 0xC, "__ti10AiPathTool"},
    {0x007C9890, 0xC, "__ti13AiPunchReflex"},
    {0x007C98A0, 0xC, "__ti11AiBatReflex"},
    {0x007C98B0, 0xC, "__ti16AiFireProjectile"},
    {0x007C98C0, 0xC, "__ti15AiGrappleReflex"},
    {0x007C98D0, 0xC, "__ti13AiBlockReflex"},
    {0x007C98E0, 0xC, "__ti15AiCounterReflex"},
    {0x007C98F0, 0xC, "__ti14AiThrashReflex"},
    {0x007C9900, 0xC, "__ti15AiDisposeReflex"},
    {0x007C9910, 0xC, "__ti13AiSmashReflex"},
    {0x007C9920, 0xC, "__ti14AiThrowRecover"},
    {0x007C9930, 0xC, "__ti13AiThrowPickup"},
    {0x007C9940, 0xC, "__ti13AiStompReflex"},
    {0x007C9950, 0xC, "__ti11AiRamAttack"},
    {0x007C9960, 0xC, "__ti14AiCrowdControl"},
    {0x007C9970, 0xC, "__ti15AiSpecialAttack"},
    {0x007C9980, 0xC, "__ti14AiDetonateHead"},
    {0x007C9990, 0xC, "__ti7AiTaunt"},
    {0x007C99A0, 0xC, "__ti12AiPutOutFire"},
    {0x007C99B0, 0xC, "__ti7AiReset"},
    {0x007C99C0, 0xC, "__ti12AiSeekHealth"},
    {0x007C99D0, 0xC, "__ti13AiSeekStamina"},
    {0x007C99E0, 0xC, "__ti13AiSeekSpecial"},
    {0x007C99F0, 0xC, "__ti11AiSeekCloak"},
    {0x007C9A00, 0xC, "__ti12AiSeekPickup"},
    {0x007C9A10, 0xC, "__ti13AiSeekMonster"},
    {0x007C9A20, 0xC, "__ti7AiSwarm"},
    {0x007C9A30, 0xC, "__ti12AiDodgeThrow"},
    {0x007C9A40, 0xC, "__ti12AiCatchThrow"},
    {0x007C9A50, 0xC, "__ti10AiBatThrow"},
    {0x007C9A60, 0xC, "__ti10AiDodgeRam"},
    {0x007C9A70, 0xC, "__ti12AiDodgeStomp"},
    {0x007C9A80, 0xC, "__ti14AiDodgeSpecial"},
    {0x007C9A90, 0xC, "__ti8Airplane"},
    {0x007C9AA0, 0xEC, "_12BigShotLevel$instance"},
    {0x007C9B90, 0x930, "_12LevelPickups$s_info"},
    {0x007CA4C0, 0xC, "__ti9CarPickup"},
    {0x007CA4D0, 0x3010, "_7PathNet$fodder"},
    {0x007CD4E0, 0xC, "__ti6Pickup"},
    {0x007CD4F0, 0xC, "__ti9charreset"},
    {0x007CD500, 0x14, "_10CrushLevel$instance"},
    {0x007CD518, 0xC, "__ti12Destructible"},
    {0x007CD528, 0x40, "_8HitEvent$s_takeHitInfo"},
    {0x007CD570, 0x2A30, "_9EnemyInfo$s_info"},
    {0x007CFFA0, 0xC, "__ti9FinalBoss"},
    {0x007CFFB0, 0x50, "_8PedGroup$s_allPedGroups"},
    {0x007D0000, 0x3010, "_7PathNet$ped"},
    {0x007D3010, 0x560, "_11StartPoints$m_instance"},
    {0x007D3570, 0xC, "__ti11GlobalReset"},
    {0x007D3580, 0xC, "__ti12StateGrapple"},
    {0x007D3590, 0xC, "__ti13StateGrappled"},
    {0x007D35A0, 0xC, "__ti20StateGrappleOHAttack"},
    {0x007D35B0, 0xC, "__ti19StateMonkeyOHAttack"},
    {0x007D35C0, 0xC, "__ti11AttackState"},
    {0x007D35D0, 0xC, "__ti16StateGrappleLift"},
    {0x007D35E0, 0xC, "__ti17StateGrappleThrow"},
    {0x007D35F0, 0xC, "__ti18StateGrappleThrow1"},
    {0x007D3600, 0xC, "__ti10HeliPickup"},
    {0x007D3610, 0xC, "__ti9HomingBug"},
    {0x007D3620, 0x1000, "_12Interactives$s_dbInteractive"},
    {0x007D4620, 0xC, "__ti9McMessage"},
    {0x007D4630, 0xC, "__ti10McLoadFile"},
    {0x007D4640, 0xC, "__ti10McSaveFile"},
    {0x007D4650, 0xC, "__ti19McListRootDirectory"},
    {0x007D4660, 0xC, "__ti15McDoesFileExist"},
    {0x007D4670, 0xC, "__ti13McUpdateState"},
    {0x007D4680, 0xC, "__ti8McFormat"},
    {0x007D4690, 0xC, "__ti14McScrambleIcon"},
    {0x007D46A0, 0xC, "__ti14McSetupGameDir"},
    {0x007D46B0, 0x28, "_6McPage$s_selectedFont"},
    {0x007D46D8, 0x28, "_6McPage$s_unselectedFont"},
    {0x007D4700, 0x28, "_6McPage$s_giantFont"},
    {0x007D4728, 0x28, "_6McPage$s_menuFont"},
    {0x007D4750, 0x28, "_6McPage$s_titleFont"},
    {0x007D4778, 0x28, "_6McPage$s_bottomFont"},
    {0x007D47A0, 0x28, "_6McPage$s_font"},
    {0x007D47C8, 0xC, "__ti13LineFormation"},
    {0x007D47D8, 0xC, "__ti12BoxFormation"},
    {0x007D47E8, 0xC, "__ti14MilitaryPickup"},
    {0x007D47F8, 0xC, "__ti18MissileTruckPickup"},
    {0x007D4808, 0xC, "__ti9StateIdle"},
    {0x007D4818, 0xC, "__tiQ213StateBatSwipe11SwipeConfig"},
    {0x007D4828, 0xC, "__ti13StateBatSwipe"},
    {0x007D4838, 0xC, "__ti10StateBlock"},
    {0x007D4848, 0xC, "__ti10StateCatch"},
    {0x007D4858, 0xC, "__ti12StateCounter"},
    {0x007D4868, 0xC, "__ti14StateCountered"},
    {0x007D4878, 0xC, "__ti10StateDeath"},
    {0x007D4888, 0xC, "__ti16StateGetupAttack"},
    {0x007D4898, 0xC, "__ti14StateGunAttack"},
    {0x007D48A8, 0xC, "__ti12StateImpaled"},
    {0x007D48B8, 0xC, "__ti12StateJavelin"},
    {0x007D48C8, 0xC, "__ti11StatePickUp"},
    {0x007D48D8, 0xC, "__tiQ210StatePunch11PunchConfig"},
    {0x007D48E8, 0xC, "__ti10StatePunch"},
    {0x007D48F8, 0xC, "__ti11StateRecoil"},
    {0x007D4908, 0xC, "__ti12StateStunned"},
    {0x007D4918, 0xC, "__ti10StateTaunt"},
    {0x007D4928, 0xC, "__ti10StateThrow"},
    {0x007D4938, 0xC, "__ti19StateTwoHandedThrow"},
    {0x007D4948, 0xC, "__ti12StateShocked"},
    {0x007D4958, 0xC, "__ti16StateStompAttack"},
    {0x007D4968, 0xC, "__ti16StateTazerAttack"},
    {0x007D4978, 0xC, "__ti17StateShieldAttack"},
    {0x007D4988, 0xC, "__ti15StateBigTakeHit"},
    {0x007D4998, 0xC, "__ti12StateVictory"},
    {0x007D49A8, 0xC, "__ti7Monster"},
    {0x007D49B8, 0xC, "__ti10McBootPage"},
    {0x007D49C8, 0xC, "__ti20McCheckForSweetTooth"},
    {0x007D49D8, 0xC, "__ti18McCheckForSettings"},
    {0x007D49E8, 0xC, "__ti14McLoadSettings"},
    {0x007D49F8, 0xC, "__ti14McSaveSettings"},
    {0x007D4A08, 0xC, "__ti12McChooseSlot"},
    {0x007D4A18, 0xC, "__ti12McCheckSpace"},
    {0x007D4A28, 0xC, "__ti13McCheckFormat"},
    {0x007D4A38, 0xC, "__ti16McCheckOverwrite"},
    {0x007D4A48, 0xC, "__ti15McPrepBoxScreen"},
    {0x007D4A58, 0xC, "__ti15McLoadGameBoxes"},
    {0x007D4A68, 0xC, "__ti12McSelectReel"},
    {0x007D4A78, 0xC, "__ti15McSaveAdventure"},
    {0x007D4A88, 0xC, "__ti20McSaveAdventureBlank"},
    {0x007D4A98, 0xC, "__ti16McSelectSaveSlot"},
    {0x007D4AA8, 0xC, "__ti11McAskToSave"},
    {0x007D4AB8, 0xC, "__ti11McAskIfSure"},
    {0x007D4AC8, 0xC, "__ti11McErrorPage"},
    {0x007D4AD8, 0xC, "__ti13McCardRemoved"},
    {0x007D4AE8, 0xC, "__ti13StateButtSlam"},
    {0x007D4AF8, 0xC, "__ti10StateClimb"},
    {0x007D4B08, 0xC, "__ti9StateDash"},
    {0x007D4B18, 0xC, "__ti11JumpFlyBase"},
    {0x007D4B28, 0xC, "__ti8StateFly"},
    {0x007D4B38, 0xC, "__ti9StateJump"},
    {0x007D4B48, 0xC, "__ti14StateRamAttack"},
    {0x007D4B58, 0xC, "__ti8StateRun"},
    {0x007D4B68, 0xC, "__ti8PathTool"},
    {0x007D4B78, 0xC, "__ti13PlantBossHead"},
    {0x007D4B88, 0xC, "__ti13PlantBossRoot"},
    {0x007D4B98, 0xC, "__ti13PlantBossBase"},
    {0x007D4BA8, 0xC, "__ti11PowerUpTool"},
    {0x007D4BB8, 0xC, "__ti5reset"},
    {0x007D4BC8, 0xC, "__ti11RigidPickup"},
    {0x007D4BD8, 0xC, "__ti14StateAirStrike"},
    {0x007D4BE8, 0xC, "__ti14StateBugAttack"},
    {0x007D4BF8, 0xC, "__ti17StateCrowdControl"},
    {0x007D4C08, 0xC, "__ti15StateFireBreath"},
    {0x007D4C18, 0xC, "__ti16StateGrappleHook"},
    {0x007D4C28, 0xC, "__ti14StateLavaBlast"},
    {0x007D4C38, 0xC, "__ti17StateRobotSpecial"},
    {0x007D4C48, 0xC, "__ti16StateRockSpecial"},
    {0x007D4C58, 0xC, "__ti14StateSonicRoar"},
    {0x007D4C68, 0xC, "__ti14StateZapAttack"},
    {0x007D4C78, 0xC, "__ti12StateTopSpin"},
    {0x007D4C88, 0xC, "__ti16StateCannonHands"},
    {0x007D4C98, 0xC, "__ti15StateUltraTazer"},
    {0x007D4CA8, 0xC, "__ti14StartPointTool"},
    {0x007D4CB8, 0xC, "__ti12SubwayPickup"},
    {0x007D4CC8, 0xC, "__ti10TankPickup"},
    {0x007D4CD8, 0xC, "__ti12TurretPickup"},
    {0x007D4CE8, 0xC, "__ti14StateThrowBack"},
    {0x007D4CF8, 0xC, "__ti17StateBeingSlammed"},
    {0x007D4D08, 0xC, "__ti16StateBeingThrown"},
    {0x007D4D18, 0xC, "__ti17StateGrappleBreak"},
    {0x007D4D28, 0xC, "__ti14StateKnockBack"},
    {0x007D4D38, 0xC, "__ti8UfoTokyo"},
    {0x007D4D48, 0xC, "__ti9UfoPickup"},
    {0x007D4D58, 0xC, "__ti18CFunctionalSection"},
    {0x007D4D68, 0xC, "__ti18CHorizontalSection"},
    {0x007D4D78, 0xC, "__ti16CVerticalSection"},
    {0x007D4D88, 0xC, "__ti10CDevScreen"},
    {0x007D4D98, 0xC, "__ti14CNetworkScreen"},
    {0x007D4DA8, 0xC, "__ti9bad_alloc"},
    {0x007D4DB8, 0xC, "__ti17__class_type_info"},
    {0x007D4DC8, 0xC, "__ti16__user_type_info"},
    {0x007D4DD8, 0xC, "__ti14__si_type_info"},
    {0x007D4DE8, 0xC, "__ti10bad_typeid"},
    {0x007D4DF8, 0xC, "__ti8bad_cast"},
    {0x007D4E08, 0xC, "__ti13bad_exception"},
    {0x007D4E18, 0xC, "__ti17__array_type_info"},
    {0x007D4E28, 0xC, "__ti16__ptmd_type_info"},
    {0x007D4E38, 0xC, "__ti16__ptmf_type_info"},
    {0x007D4E48, 0xC, "__ti16__func_type_info"},
    {0x007D4E58, 0xC, "__ti19__builtin_type_info"},
    {0x007D4E68, 0xC, "__ti16__attr_type_info"},
    {0x007D4E78, 0xC, "__ti19__pointer_type_info"},
    {0x007D4E90, 0x2C000, "_6CsPool$m_csPool"},
    {0x00800E90, 0x80000, "_11MemoryStack$globalPool"},
    {0x00880E90, 0x80, "_9DebugDraw$s_ep"},
};
inline constexpr size_t kCount = sizeof(kData) / sizeof(kData[0]);
// Entrada que contem `a` (ou a ultima que comeca antes dele, quando nao tem tamanho); nullptr antes da primeira.
inline const Entry *find(uint32_t a) {
    size_t lo = 0, hi = kCount;
    while (lo < hi) { const size_t mid = (lo + hi) / 2; if (kData[mid].addr <= a) lo = mid + 1; else hi = mid; }
    if (!lo) return nullptr;
    const Entry *e = &kData[lo - 1];
    return (!e->size || a < e->addr + e->size) ? e : nullptr;
}
} // namespace symtab
} // namespace wotm
#endif
