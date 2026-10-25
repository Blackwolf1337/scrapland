/*
 * Functions of Scrap.exe replaced by C code: SD_HOOK(name, rva, size). The original gets a jmp to the C function at
 * startup (5 bytes, so size must be at least 5). Every hook can be switched off in scrapdecomp.ini:
 *   [hooks]
 *   Scrap_GetMoney=0
 */

/* src/game/PyInterface/ScrapPy.c */
SD_HOOK(Scrap_GetLevelPath, 0x197fc0, 50)
SD_HOOK(Scrap_GetMoney, 0x199ea0, 53)

/* src/game/PyInterface/SWeapPy.c */
SD_HOOK(SWeap_GetNWeap, 0x19f230, 53)
SD_HOOK(SWeap_GetNAmmo, 0x19f270, 53)
SD_HOOK(SWeap_InitWeap, 0x19f2b0, 93)
SD_HOOK(SWeap_InitAmmo, 0x19f310, 73)
SD_HOOK(SWeap_GetFAmmo, 0x19f360, 141)
SD_HOOK(SWeap_GetSAmmo, 0x19f3f0, 128)
SD_HOOK(SWeap_SetFAmmo, 0x19f470, 142)
SD_HOOK(SWeap_SetSAmmo, 0x19f500, 122)
SD_HOOK(SWeap_GetFWeap, 0x19f580, 144)
SD_HOOK(SWeap_GetSWeap, 0x19f610, 131)
SD_HOOK(SWeap_SetFWeap, 0x19f6a0, 145)
SD_HOOK(SWeap_SetSWeap, 0x19f740, 133)
SD_HOOK(SWeap_FillPriority, 0x19f7d0, 51)
SD_HOOK(SWeap_GetFirstiWeap, 0x19f810, 53)
SD_HOOK(SWeap_NetExec, 0x19f850, 487)

/* src/game/PyInterface/SVecPy.c */
SD_HOOK(SVec_Add, 0x19e810, 123)
SD_HOOK(SVec_Sub, 0x19e890, 123)
SD_HOOK(SVec_Mod, 0x19e910, 118)
SD_HOOK(SVec_ModSqr, 0x19e990, 104)
SD_HOOK(SVec_Norm, 0x19ea00, 229)
SD_HOOK(SVec_Prod, 0x19eaf0, 117)
SD_HOOK(SVec_DProd, 0x19eb70, 122)
SD_HOOK(SVec_CProd, 0x19ebf0, 368)
SD_HOOK(SVec_Rotate3D, 0x19ed60, 262)
SD_HOOK(SVec_GetAngle, 0x19ee70, 568)
SD_HOOK(SVec_GetRotAng, 0x19f0b0, 220)
SD_HOOK(SVec_NormAng, 0x19f190, 119)

/* src/game/PyInterface/SActPy.c */
SD_HOOK(SAct_CreateClass, 0x194790, 139)
SD_HOOK(SAct_DelClass, 0x194820, 73)
SD_HOOK(SAct_GetClass, 0x194870, 139)
SD_HOOK(SAct_SetCls, 0x194900, 164)
SD_HOOK(SAct_GetCls, 0x1949b0, 198)
SD_HOOK(SAct_CreateAction, 0x194a80, 127)
SD_HOOK(SAct_GetAction, 0x194b00, 127)
SD_HOOK(SAct_SetAct, 0x194b80, 171)
SD_HOOK(SAct_GetAct, 0x194c30, 198)
