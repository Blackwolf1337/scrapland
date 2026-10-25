/*
 * Functions of Scrap.exe replaced by C code: SD_HOOK(name, rva, size). The original gets a jmp to the C function at
 * startup (5 bytes, so size must be at least 5). Every hook can be switched off in scrapdecomp.ini:
 *   [hooks]
 *   Scrap_GetMoney=0
 */

/* src/py/scrap.c */
SD_HOOK(Scrap_GetLevelPath, 0x197fc0, 50)
SD_HOOK(Scrap_GetMoney, 0x199ea0, 53)

/* src/py/sweap.c */
SD_HOOK(SWeap_GetNWeap, 0x19f230, 53)
