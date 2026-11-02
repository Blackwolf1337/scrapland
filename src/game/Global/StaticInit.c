/*
 * The engine variables that only the scripts and the config read: the C registers them here and never reads them.
 * Their initializers (scrap/staticinit.h) run in the order of the original, src/standalone/crtxc.inc, like the ones
 * in the files of the other globals. Also three objects the original makes at the start and never uses.
 */
#include "scrap/material.h"
#include "scrap/net.h"
#include "scrap/staticinit.h"

#define g_Fraglimit (GVar_Fraglimit.value.i)

/* a material the original constructs at the start and never uses */
static CMaterial g_UnusedMaterial;
static cGVar GVar_QuickMessageString4;
static cGVar GVar_QuickMessageString2;
static cGVar GVar_QuickMessageString6;
static cGVar GVar_QuickMessageString3;
static cGVar GVar_QuickMessageString5;
static cGVar GVar_QuickMessageString1;
static cGVar GVar_R_SkipModelFX;
static cGVar GVar_NumRadarIcons;
static cGVar GVar_MovieFramerate;
static cGVar GVar_MovieJpegQuality;
static cGVar GVar_MovieUseMemory;
static cGVar GVar_MovieCacheSize;
static cGVar GVar_ScorerMarginV;
static cGVar GVar_OptAnimPosMax;
static cGVar GVar_MinTimeStoppedTurnPatrolCharAI;
static cGVar GVar_MaxDistStopVisObjChaseWithOutActionAI;
static cGVar GVar_MaxRotCharAI;
static cGVar GVar_MaxDistStopVisObjChaseWithActionAI;
static cGVar GVar_LimVelTurnCharAI;
static cGVar GVar_MinDistStrafeCharAI;
static cGVar GVar_FileArrowNextAlpha;
static cGVar GVar_DimWindowReposYAI;
static cGVar GVar_MaxMarginObjTrafficAI;
static cGVar GVar_DistMinItemAI;
static cGVar GVar_MinDistBetweenTrafficAI;
static cGVar GVar_DimWindowReposXAI;
static cGVar GVar_ShootAI;
static cGVar GVar_MaxAngleReposAI;
static cGVar GVar_MaxRotAI;
static cGVar GVar_MinRotAI;
static cGVar GVar_ASEPublic;

/* two random generators the original seeds at the start and never uses */
static cNetRandom g_UnusedNetRandom1;
static cGVar GVar_ViewerModel;
static cGVar GVar_ViewerModelOffsetY;
static cGVar GVar_ViewerAnim;
static cGVar GVar_BatchBuildGraph3DAI;
static cGVar GVar_ViewerIniAI;
static cGVar GVar_BatchBuildGraph2DAI;
static cNetRandom g_UnusedNetRandom2;
static cGVar GVar_SoundFrequency;
static cGVar GVar_SoundChannels;
static cGVar GVar_S_PriorityMode;
static cGVar GVar_SoundBits;
static cGVar GVar_SoundStereo;
static cGVar GVar_SoundUseHardware;
static cGVar GVar_S_PlayMute;
static cGVar GVar_MaxPlayerLives;
static cGVar GVar_BishopLivePrice;
static cGVar GVar_FXCharacterPoliceGearConversionFile;
static cGVar GVar_ShowTSCMode;
static cGVar GVar_FXVehicleAcceleratorFile;
static cGVar GVar_XboxSavePath;
static cGVar GVar_XboxSaveName6;
static cGVar GVar_XboxSaveName3;
static cGVar GVar_XboxSaveName0;
static cGVar GVar_Fraglimit;
static cGVar GVar_XboxProfileName;
static cGVar GVar_XboxSaveName1;
static cGVar GVar_XboxSaveName5;
static cGVar GVar_XBRumblePadPad;
static cGVar GVar_XboxCanSave;
static cGVar GVar_ShowCredits;
static cGVar GVar_XboxSaveName2;
static cGVar GVar_NumProfiles;
static cGVar GVar_XboxSaveName4;
static cGVar GVar_XBNumControllers;
static cGVar GVar_isXboxDebug;
static cGVar GVar_XboxFileCacheSize;
static cGVar GVar_DefaultServerAddress;
static cGVar GVar_MakeDedicated;
static cGVar GVar_JoinServerOnStartup;
static cGVar GVar_MapTriCacheRecalc;
static cGVar GVar_FXVehicleSpeedTrailFollowFactor;
static cGVar GVar_HangarShip5;
static cGVar GVar_HangarShip3;
static cGVar GVar_HangarShip9;
static cGVar GVar_HangarShip8;
static cGVar GVar_HangarShip2;
static cGVar GVar_iHangarShip;
static cGVar GVar_HangarShip4;
static cGVar GVar_HangarShip7;
static cGVar GVar_HangarShip1;
static cGVar GVar_HangarShip6;
static cGVar GVar_STrafficCapsule;
static cGVar GVar_EMIRadius;
static cGVar GVar_InfernoFastSpeed;
static cGVar GVar_SwarmDamage;
static cGVar GVar_SwarmRangeUPG;
cGVar GVar_SwarmNumber3;
static cGVar GVar_SwarmRange;
cGVar GVar_SwarmNumber4;
cGVar GVar_SwarmNumber2;
static cGVar GVar_BishopSpecialActionTime;
static cGVar GVar_RelaxSeatsFunctionaryListName;
static cGVar GVar_GenCharSpecialActionRadius;
static cGVar GVar_GetAwayHumanDist;
static cGVar GVar_SecondaryMissionDesc;
static cGVar GVar_PrimaryMissionDesc;
static cGVar GVar_PlayerHelpedPoliceReward;

/* the dynamic initializers of the globals here, run in the order of src/standalone/crtxc.inc */
INIT_GVAR_FLOAT(Init_GVar_MinTimeStoppedTurnPatrolCharAI, &GVar_MinTimeStoppedTurnPatrolCharAI, "MinTimeStoppedTurnPatrolCharAI", 4, "Time min. duration turn to center of patrol char.\n", 5.0f)
INIT_GVAR_FLOAT(Init_GVar_LimVelTurnCharAI, &GVar_LimVelTurnCharAI, "LimVelTurnCharAI", 4, "Limit velocity to turn.\n", 2.0f)
INIT_GVAR_FLOAT(Init_GVar_MaxRotCharAI, &GVar_MaxRotCharAI, "MaxRotCharAI", 4, "Max. vel. rotation.\n", 7.5f)
INIT_GVAR_FLOAT(Init_GVar_MinDistStrafeCharAI, &GVar_MinDistStrafeCharAI, "MinDistStrafeCharAI", 4, "Min. distance to do while straffing.\n", 300.0f)
INIT_GVAR_FLOAT(Init_GVar_MaxDistStopVisObjChaseWithActionAI, &GVar_MaxDistStopVisObjChaseWithActionAI, "MaxDistStopVisObjChaseWithActionAI", 4, "Max. distance stopped while is visible objective.\n", 2000.0f)
INIT_GVAR_FLOAT(Init_GVar_MaxDistStopVisObjChaseWithOutActionAI, &GVar_MaxDistStopVisObjChaseWithOutActionAI, "MaxDistStopVisObjChaseWithOutActionAI", 4, "Max. distance stopped while is visible objective.\n", 1000.0f)
INIT_GVAR_INT(Init_GVar_ShootAI, &GVar_ShootAI, "ShootAI", 4, "0 = AI cannot shoot, 1 = AI can shoot.", 0)
INIT_GVAR_FLOAT(Init_GVar_DistMinItemAI, &GVar_DistMinItemAI, "DistMinItemAI", 4, "Distance Min. for AI's purposes.", 5000.0f)
INIT_GVAR_FLOAT(Init_GVar_MinDistBetweenTrafficAI, &GVar_MinDistBetweenTrafficAI, "MinDistBetweenTrafficAI", 4, "Distance Min. between two ships of traffic.\n", 5000.0f)
INIT_GVAR_FLOAT(Init_GVar_MaxMarginObjTrafficAI, &GVar_MaxMarginObjTrafficAI, "MaxMarginObjTrafficAI", 4, ".\n", 1000.0f)
INIT_GVAR_FLOAT(Init_GVar_MaxAngleReposAI, &GVar_MaxAngleReposAI, "MaxAngleReposAI", 4, "Angle max. to reposition the agent.\n", 0.3926991f)
INIT_GVAR_FLOAT(Init_GVar_DimWindowReposXAI, &GVar_DimWindowReposXAI, "DimWindowReposXAI", 4, "Dimension X window to reposition agent.\n", 20000.0f)
INIT_GVAR_FLOAT(Init_GVar_DimWindowReposYAI, &GVar_DimWindowReposYAI, "DimWindowReposYAI", 4, "Dimension Y window to reposition agent.\n", 20000.0f)
INIT_GVAR_FLOAT(Init_GVar_MinRotAI, &GVar_MinRotAI, "MinRotAI", 4, "Factor to control rotation.\n", 2.0f)
INIT_GVAR_FLOAT(Init_GVar_MaxRotAI, &GVar_MaxRotAI, "MaxRotAI", 4, "Factor to control rotation.\n", 20.0f)
INIT_GVAR_INT(Init_GVar_BatchBuildGraph3DAI, &GVar_BatchBuildGraph3DAI, "BatchBuildGraph3DAI", 4, "To Build Graph 3D.\n", 0)
INIT_GVAR_INT(Init_GVar_ViewerIniAI, &GVar_ViewerIniAI, "ViewerIniAI", 4, "To initialize AI in Viewer.\n", 0)
INIT_GVAR_INT(Init_GVar_BatchBuildGraph2DAI, &GVar_BatchBuildGraph2DAI, "BatchBuildGraph2DAI", 4, "To Build Graph 2D.\n", 0)
INIT_GVAR_INT(Init_GVar_MaxPlayerLives, &GVar_MaxPlayerLives, "MaxPlayerLives", 0, "Max player lives", 50)
INIT_GVAR_INT(Init_GVar_BishopLivePrice, &GVar_BishopLivePrice, "BishopLivePrice", 0, "Bishop live price", 1000)
INIT_GVAR_FLOAT(Init_GVar_FXVehicleSpeedTrailFollowFactor, &GVar_FXVehicleSpeedTrailFollowFactor, "FXVehicleSpeedTrailFollowFactor", 0, "Vehicle Speed Trails follow factor (0...1)", 0.65f)
FINI_GVAR_STRING(Fini_GVar_HangarShip1, &GVar_HangarShip1)
INIT_GVAR_STRING(Init_GVar_HangarShip1, Fini_GVar_HangarShip1, &GVar_HangarShip1, "HangarShip1", 10, "1st Hangar ship values", "SArchbishop1<-MARCHBISHOP1<-MARCHBISHOP1<-<-<-3,0,15,0,15,3,1<-150<-0,0,0,0,0,0")
FINI_GVAR_STRING(Fini_GVar_HangarShip2, &GVar_HangarShip2)
INIT_GVAR_STRING(Init_GVar_HangarShip2, Fini_GVar_HangarShip2, &GVar_HangarShip2, "HangarShip2", 10, "2nd Hangar ship values", "SBankMaster1<-MBANKMASTER1<-MBANKMASTER1<-<-<-15,15,3,15,0,6,1<-150<-0,0,0,0,0,0")
FINI_GVAR_STRING(Fini_GVar_HangarShip3, &GVar_HangarShip3)
INIT_GVAR_STRING(Init_GVar_HangarShip3, Fini_GVar_HangarShip3, &GVar_HangarShip3, "HangarShip3", 10, "3rd Hangar ship values", "SMerc3<-MMERC3<-MMERC3<-<-<-0,15,7,0,7,0,1<-150<-0,0,0,0,0,0")
FINI_GVAR_STRING(Fini_GVar_HangarShip4, &GVar_HangarShip4)
INIT_GVAR_STRING(Init_GVar_HangarShip4, Fini_GVar_HangarShip4, &GVar_HangarShip4, "HangarShip4", 10, "4th Hangar ship values", "SBetty1<-MBETTY1<-<-<-<-1,1,3,0,3,0,1<-80<-0,0,0,0,0,0")
FINI_GVAR_STRING(Fini_GVar_HangarShip5, &GVar_HangarShip5)
INIT_GVAR_STRING(Init_GVar_HangarShip5, Fini_GVar_HangarShip5, &GVar_HangarShip5, "HangarShip5", 10, "5th Hangar ship values", "SMayor1<-MMAYOR1<-<-<-<-0,0,3,3,3,1,1<-80<-0,0,0,0,0,0")
FINI_GVAR_STRING(Fini_GVar_HangarShip6, &GVar_HangarShip6)
INIT_GVAR_STRING(Init_GVar_HangarShip6, Fini_GVar_HangarShip6, &GVar_HangarShip6, "HangarShip6", 10, "6th Hangar ship values", "SBanker1<-MBANKER1<-<-<-<-3,1,1,3,0,0,1<-80<-0,0,0,0,0,0")
FINI_GVAR_STRING(Fini_GVar_HangarShip7, &GVar_HangarShip7)
INIT_GVAR_STRING(Init_GVar_HangarShip7, Fini_GVar_HangarShip7, &GVar_HangarShip7, "HangarShip7", 10, "7th Hangar ship values", "SMerc1<-MMERC1<-<-<-<-1,3,1,0,0,3,1<-80<-0,0,0,0,0,0")
FINI_GVAR_STRING(Fini_GVar_HangarShip8, &GVar_HangarShip8)
INIT_GVAR_STRING(Init_GVar_HangarShip8, Fini_GVar_HangarShip8, &GVar_HangarShip8, "HangarShip8", 10, "8th Hangar ship values", "SPoliBoss1<-MPOLIBOSS1<-MPOLIBOSS1<-<-<-0,15,0,7,7,6,1<-150<-0,0,0,0,0,0")
FINI_GVAR_STRING(Fini_GVar_HangarShip9, &GVar_HangarShip9)
INIT_GVAR_STRING(Init_GVar_HangarShip9, Fini_GVar_HangarShip9, &GVar_HangarShip9, "HangarShip9", 10, "9th Hangar ship values", "SFunc1<-MFUNC1<-<-<-<-1,0,0,0,1,0,1<-45<-0,0,0,0,0,0")
INIT_GVAR_INT(Init_GVar_iHangarShip, &GVar_iHangarShip, "iHangarShip", 10, "The selected hangar ship", 3)
INIT_GVAR_FLOAT(Init_GVar_STrafficCapsule, &GVar_STrafficCapsule, "STrafficCapsule", 0, "Small/Minimal Radius of the Traffic Segment", 1500.0f)
INIT_GVAR_FLOAT(Init_GVar_EMIRadius, &GVar_EMIRadius, "EMIRadius", 0, "Radius that have the EMI", 1250.0f)
INIT_GVAR_FLOAT(Init_GVar_InfernoFastSpeed, &GVar_InfernoFastSpeed, "InfernoFastSpeed", 0, "The Fast Speed that have the Inferno", 16000.0f)
INIT_GVAR_FLOAT(Init_GVar_SwarmRange, &GVar_SwarmRange, "SwarmRange", 0, "The maximun range that have the swarm", 60000.0f)
INIT_GVAR_FLOAT(Init_GVar_SwarmRangeUPG, &GVar_SwarmRangeUPG, "SwarmRangeUPG", 0, "The maximun range that have the swarm", 120000.0f)
INIT_GVAR_INT(Init_GVar_SwarmDamage, &GVar_SwarmDamage, "SwarmDamage", 0, "Damage per mis.", 4)
INIT_GVAR_INT(Init_GVar_SwarmNumber2, &GVar_SwarmNumber2, "SwarmNumber2", 0, " Swarm Number 2 ", 6)
INIT_GVAR_INT(Init_GVar_SwarmNumber3, &GVar_SwarmNumber3, "SwarmNumber3", 0, " Swarm Number 3 ", 5)
INIT_GVAR_INT(Init_GVar_SwarmNumber4, &GVar_SwarmNumber4, "SwarmNumber4", 0, " Swarm Number 4 ", 4)
INIT_GVAR_FLOAT(Init_GVar_BishopSpecialActionTime, &GVar_BishopSpecialActionTime, "BishopSpecialActionTime", 0, "Bishop special action time", 5.0f)
FINI_GVAR_STRING(Fini_GVar_RelaxSeatsFunctionaryListName, &GVar_RelaxSeatsFunctionaryListName)
INIT_GVAR_STRING(Init_GVar_RelaxSeatsFunctionaryListName, Fini_GVar_RelaxSeatsFunctionaryListName, &GVar_RelaxSeatsFunctionaryListName, "RelaxSeatsFunctionaryListName", 1, "Relax seats functionary list name", "RelaxSeatsFunctionary")
INIT_GVAR_FLOAT(Init_GVar_GenCharSpecialActionRadius, &GVar_GenCharSpecialActionRadius, "GenCharSpecialActionRadius", 0, "GenChar special action radius", 6250000.0f)
INIT_GVAR_FLOAT(Init_GVar_GetAwayHumanDist, &GVar_GetAwayHumanDist, "GetAwayHumanDist", 0, "Get away human dist", 600.0f)
FINI_GVAR_STRING(Fini_GVar_PrimaryMissionDesc, &GVar_PrimaryMissionDesc)
INIT_GVAR_STRING(Init_GVar_PrimaryMissionDesc, Fini_GVar_PrimaryMissionDesc, &GVar_PrimaryMissionDesc, "PrimaryMissionDesc", 0, "Main Mission Descriptor.", "")
FINI_GVAR_STRING(Fini_GVar_SecondaryMissionDesc, &GVar_SecondaryMissionDesc)
INIT_GVAR_STRING(Init_GVar_SecondaryMissionDesc, Fini_GVar_SecondaryMissionDesc, &GVar_SecondaryMissionDesc, "SecondaryMissionDesc", 0, "Secondary Mission Descriptor.", "")
INIT_GVAR_INT(Init_GVar_PlayerHelpedPoliceReward, &GVar_PlayerHelpedPoliceReward, "PlayerHelpedPoliceReward", 0, "Player helped police reward", 2000)
FINI_GVAR_STRING(Fini_GVar_QuickMessageString6, &GVar_QuickMessageString6)
INIT_GVAR_STRING(Init_GVar_QuickMessageString6, Fini_GVar_QuickMessageString6, &GVar_QuickMessageString6, "QuickMessageString6", 2, "Quick chat messsage\n", "I´m the one and only")
FINI_GVAR_STRING(Fini_GVar_QuickMessageString5, &GVar_QuickMessageString5)
INIT_GVAR_STRING(Init_GVar_QuickMessageString5, Fini_GVar_QuickMessageString5, &GVar_QuickMessageString5, "QuickMessageString5", 2, "Quick chat messsage", "There is no rules")
FINI_GVAR_STRING(Fini_GVar_QuickMessageString4, &GVar_QuickMessageString4)
INIT_GVAR_STRING(Init_GVar_QuickMessageString4, Fini_GVar_QuickMessageString4, &GVar_QuickMessageString4, "QuickMessageString4", 2, "Quick chat messsage", "That´s no fair")
FINI_GVAR_STRING(Fini_GVar_QuickMessageString3, &GVar_QuickMessageString3)
INIT_GVAR_STRING(Init_GVar_QuickMessageString3, Fini_GVar_QuickMessageString3, &GVar_QuickMessageString3, "QuickMessageString3", 2, "Quick chat messsage", "Good game")
FINI_GVAR_STRING(Fini_GVar_QuickMessageString2, &GVar_QuickMessageString2)
INIT_GVAR_STRING(Init_GVar_QuickMessageString2, Fini_GVar_QuickMessageString2, &GVar_QuickMessageString2, "QuickMessageString2", 2, "Quick chat messsage", "This map sucks")
FINI_GVAR_STRING(Fini_GVar_QuickMessageString1, &GVar_QuickMessageString1)
INIT_GVAR_STRING(Init_GVar_QuickMessageString1, Fini_GVar_QuickMessageString1, &GVar_QuickMessageString1, "QuickMessageString1", 2, "Quick chat messsage", "Die bastard!")
INIT_GVAR_INT(Init_GVar_NumRadarIcons, &GVar_NumRadarIcons, "NumRadarIcons", 0, "The number of radar icons", 9)
INIT_GVAR_INT(Init_GVar_ScorerMarginV, &GVar_ScorerMarginV, "ScorerMarginV", 0, "Vertical Margin for Scorer", 36)
FINI_GVAR_STRING(Fini_GVar_FileArrowNextAlpha, &GVar_FileArrowNextAlpha)
INIT_GVAR_STRING(Init_GVar_FileArrowNextAlpha, Fini_GVar_FileArrowNextAlpha, &GVar_FileArrowNextAlpha, "FileArrowNextAlpha", 0, "FileArrowNextAlpha bitmap file", "2D/Dialog/NextOption.alpha.tga")
INIT_GVAR_INT(Init_GVar_ASEPublic, &GVar_ASEPublic, "ASEPublic", 6, "'The All-Seeing Eye' server public.\n", 1)
INIT_CALL(Init_UnusedNetRandom1, &g_UnusedNetRandom1, cNetRandom_ctorSeeded)
INIT_CALL(Init_UnusedNetRandom2, &g_UnusedNetRandom2, cNetRandom_ctorSeeded)
INIT_GVAR_INT(Init_GVar_SoundStereo, &GVar_SoundStereo, "SoundStereo", 2, "0 = Mono, 1 = Stereo\n", 1)
INIT_GVAR_INT(Init_GVar_SoundBits, &GVar_SoundBits, "SoundBits", 2, "Bits (8/16)", 16)
INIT_GVAR_INT(Init_GVar_SoundFrequency, &GVar_SoundFrequency, "SoundFrequency", 2, "Frequency (11025/22050/44100)", 44100)
INIT_GVAR_INT(Init_GVar_SoundUseHardware, &GVar_SoundUseHardware, "SoundUseHardware", 2, "0 = Software only, 1 = Use hardware if available", 1)
INIT_GVAR_INT(Init_GVar_SoundChannels, &GVar_SoundChannels, "SoundChannels", 2, "Sound channels (max. 32)", 24)
INIT_GVAR_INT(Init_GVar_S_PriorityMode, &GVar_S_PriorityMode, "S_PriorityMode", 0, "Priority mode (0=time,1=vol,2=time&vol)", 1)
FINI_GVAR_STRING(Fini_GVar_S_PlayMute, &GVar_S_PlayMute)
INIT_GVAR_STRING(Init_GVar_S_PlayMute, Fini_GVar_S_PlayMute, &GVar_S_PlayMute, "S_PlayMute", 0, "Mute all sounds but those specified here", "")
FINI_GVAR_STRING(Fini_GVar_FXCharacterPoliceGearConversionFile, &GVar_FXCharacterPoliceGearConversionFile)
INIT_GVAR_STRING(Init_GVar_FXCharacterPoliceGearConversionFile, Fini_GVar_FXCharacterPoliceGearConversionFile, &GVar_FXCharacterPoliceGearConversionFile, "FXCharacterPoliceGearConversionFile", 0, "Default Model for Gear Conversion Effects", "Models/GFX/Gear/Conversion.M3D")
FINI_GVAR_STRING(Fini_GVar_FXVehicleAcceleratorFile, &GVar_FXVehicleAcceleratorFile)
INIT_GVAR_STRING(Init_GVar_FXVehicleAcceleratorFile, Fini_GVar_FXVehicleAcceleratorFile, &GVar_FXVehicleAcceleratorFile, "FXVehicleAcceleratorFile", 0, "Default Model for Vehicle Accelerator Effect", "Models/GFX/VehicleAccelerator.M3D")
INIT_GVAR_INT(Init_GVar_Fraglimit, &GVar_Fraglimit, "Fraglimit", 14, "Number of frags to win a match...\n", 20)
FINI_GVAR_STRING(Fini_GVar_XboxSavePath, &GVar_XboxSavePath)
INIT_GVAR_STRING(Init_GVar_XboxSavePath, Fini_GVar_XboxSavePath, &GVar_XboxSavePath, "XboxSavePath", 0, "the current save game directory. If '' no savegame file.", "")
FINI_GVAR_STRING(Fini_GVar_XboxProfileName, &GVar_XboxProfileName)
INIT_GVAR_STRING(Init_GVar_XboxProfileName, Fini_GVar_XboxProfileName, &GVar_XboxProfileName, "XboxProfileName", 0, "the current save game profile name. If '' no savegame file.", "")
FINI_GVAR_STRING(Fini_GVar_XboxSaveName0, &GVar_XboxSaveName0)
INIT_GVAR_STRING(Init_GVar_XboxSaveName0, Fini_GVar_XboxSaveName0, &GVar_XboxSaveName0, "XboxSaveName0", 8, "Save game isues.", "")
FINI_GVAR_STRING(Fini_GVar_XboxSaveName1, &GVar_XboxSaveName1)
INIT_GVAR_STRING(Init_GVar_XboxSaveName1, Fini_GVar_XboxSaveName1, &GVar_XboxSaveName1, "XboxSaveName1", 8, "Save game isues.", "")
FINI_GVAR_STRING(Fini_GVar_XboxSaveName2, &GVar_XboxSaveName2)
INIT_GVAR_STRING(Init_GVar_XboxSaveName2, Fini_GVar_XboxSaveName2, &GVar_XboxSaveName2, "XboxSaveName2", 8, "Save game isues.", "")
FINI_GVAR_STRING(Fini_GVar_XboxSaveName3, &GVar_XboxSaveName3)
INIT_GVAR_STRING(Init_GVar_XboxSaveName3, Fini_GVar_XboxSaveName3, &GVar_XboxSaveName3, "XboxSaveName3", 8, "Save game isues.", "")
FINI_GVAR_STRING(Fini_GVar_XboxSaveName4, &GVar_XboxSaveName4)
INIT_GVAR_STRING(Init_GVar_XboxSaveName4, Fini_GVar_XboxSaveName4, &GVar_XboxSaveName4, "XboxSaveName4", 8, "Save game isues.", "")
FINI_GVAR_STRING(Fini_GVar_XboxSaveName5, &GVar_XboxSaveName5)
INIT_GVAR_STRING(Init_GVar_XboxSaveName5, Fini_GVar_XboxSaveName5, &GVar_XboxSaveName5, "XboxSaveName5", 8, "Save game isues.", "")
FINI_GVAR_STRING(Fini_GVar_XboxSaveName6, &GVar_XboxSaveName6)
INIT_GVAR_STRING(Init_GVar_XboxSaveName6, Fini_GVar_XboxSaveName6, &GVar_XboxSaveName6, "XboxSaveName6", 8, "Save game isues.", "")
INIT_GVAR_INT(Init_GVar_NumProfiles, &GVar_NumProfiles, "NumProfiles", 0, "Maximun number of profiles.", 3)
INIT_GVAR_INT(Init_GVar_XboxCanSave, &GVar_XboxCanSave, "XboxCanSave", 0, "Can save games.", 1)
INIT_GVAR_INT(Init_GVar_XBNumControllers, &GVar_XBNumControllers, "XBNumControllers", 0, "number of plugged controllers", 0)
INIT_GVAR_FLOAT(Init_GVar_XBRumblePadPad, &GVar_XBRumblePadPad, "XBRumblePadPad", 10, "[0..1] rumble power of pad.", 1.0f)
INIT_GVAR_INT(Init_GVar_ShowCredits, &GVar_ShowCredits, "ShowCredits", 4, "Show credits on main menu?", 0)
INIT_GVAR_INT(Init_GVar_MakeDedicated, &GVar_MakeDedicated, "MakeDedicated", 4, "", 0)
FINI_GVAR_STRING(Fini_GVar_DefaultServerAddress, &GVar_DefaultServerAddress)
INIT_GVAR_STRING(Init_GVar_DefaultServerAddress, Fini_GVar_DefaultServerAddress, &GVar_DefaultServerAddress, "DefaultServerAddress", 6, "Default conection address.", "<Server IP>")
INIT_GVAR_INT(Init_GVar_JoinServerOnStartup, &GVar_JoinServerOnStartup, "JoinServerOnStartup", 4, "if 1 automaticaly joins in a server.", 0)
INIT_GVAR_INT(Init_GVar_isXboxDebug, &GVar_isXboxDebug, "isXboxDebug", 0, "true if can be acessed the debug info.", 0)
INIT_GVAR_INT(Init_GVar_XboxFileCacheSize, &GVar_XboxFileCacheSize, "XboxFileCacheSize", 0, "if value > 0 uses a file cache of size 'value' Kbs.", 0)
INIT_GVAR_FLOAT(Init_GVar_ViewerModelOffsetY, &GVar_ViewerModelOffsetY, "ViewerModelOffsetY", 0, "Viewer model offset (Y axis)", 0.0f)
FINI_GVAR_STRING(Fini_GVar_ViewerAnim, &GVar_ViewerAnim)
INIT_GVAR_STRING(Init_GVar_ViewerAnim, Fini_GVar_ViewerAnim, &GVar_ViewerAnim, "ViewerAnim", 4, "Model animation used in viewer mode", "")
FINI_GVAR_STRING(Fini_GVar_ViewerModel, &GVar_ViewerModel)
INIT_GVAR_STRING(Init_GVar_ViewerModel, Fini_GVar_ViewerModel, &GVar_ViewerModel, "ViewerModel", 4, "Model used in viewer mode", "")
INIT_GVAR_INT(Init_GVar_ShowTSCMode, &GVar_ShowTSCMode, "ShowTSCMode", 0, "Show TSC info (0=instant,1=by seconds)", 1)
INIT_GVAR_INT(Init_GVar_MapTriCacheRecalc, &GVar_MapTriCacheRecalc, "MapTriCacheRecalc", 0, "Always recalc planes", 0)
FINI_CALL(Fini_UnusedMaterial, &g_UnusedMaterial, CMaterial_dtor)
INIT_OBJECT(Init_UnusedMaterial, Fini_UnusedMaterial, &g_UnusedMaterial, CMaterial_ctor)
INIT_GVAR_INT(Init_GVar_MovieUseMemory, &GVar_MovieUseMemory, "MovieUseMemory", 0, "Movie uses RAM instead of HD (0/1)", 0)
INIT_GVAR_INT(Init_GVar_MovieCacheSize, &GVar_MovieCacheSize, "MovieCacheSize", 0, "File buffer size for movies (Megabytes)", 256)
INIT_GVAR_INT(Init_GVar_MovieJpegQuality, &GVar_MovieJpegQuality, "MovieJpegQuality", 0, "JPEG quality for movies (0..100)", 95)
INIT_GVAR_INT(Init_GVar_MovieFramerate, &GVar_MovieFramerate, "MovieFramerate", 0, "Movie FPS for avi recording", 30)
INIT_GVAR_FLOAT(Init_GVar_OptAnimPosMax, &GVar_OptAnimPosMax, "OptAnimPosMax", 0, "AnimPos compression max dist", 2000.0f)
INIT_GVAR_INT(Init_GVar_R_SkipModelFX, &GVar_R_SkipModelFX, "R_SkipModelFX", 0, "Disable model effects (0/1)", 0)
