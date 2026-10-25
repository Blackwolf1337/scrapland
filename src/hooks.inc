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
