/*
 * Network (original source: Net/): server, client, browser and master server, as the SNet script module uses
 * them. All plain cdecl functions.
 */
#ifndef SCRAP_NET_H
#define SCRAP_NET_H

#include "scrap/game.h"

typedef struct cNetShip {
	byte pad_0[0xc];
	char name[1];			/* 0x0c */
} cNetShip;

/* cNetClient.state */
#define NETCLIENT_CONNECTING 0
#define NETCLIENT_WAITING 2		/* "Waiting server response" */
#define NETCLIENT_TIMEOUT 3
#define NETCLIENT_DISCONNECTED 4

typedef struct cNetClient {
	byte pad_0[0x10];
	int state;			/* 0x10 NETCLIENT_ */
	byte pad_14[0x1c - 0x14];
	int ping;			/* 0x1c */
	byte pad_20[0x28 - 0x20];
	cNetShip *ship;			/* 0x28 */
} cNetClient;
SD_OFFSET(cNetClient, state, 0x10);
SD_OFFSET(cNetClient, ship, 0x28);

byte Net_IsServer(void);
byte Net_ModifyUsrData(int arg);
byte Net_ServerChangeLevel(const char *level);
int Net_AddResource(const char *name);
byte Net_InitServer(const char *name, int a, int b, int c);
byte Net_CloseServer(const char *msg);
const char *Net_GetServerName(void);
const char *Net_GetServerMap(void);
uint16_t Net_GetServerPort(void);
void Net_GetBotName(char *buf);
void Net_GetObjName(char *buf);
byte Net_Kick(const char *player);
int Net_InitBrowser(int a, const char *address);
int Net_DoneBrowser(void);
byte Net_IsClient(void);
cNetClient *Net_GetClient(void);
byte Net_InitClient(const char *address, int port);
byte Net_CloseClientMsg(const char *msg);
byte Net_CloseClient(void);
const char *Net_GetClientName(void);
uint16_t Net_GetClientPort(void);
byte Net_SendUsrString(const char *text, int arg);
byte Net_SendChatString(const char *text, int arg);
byte Net_SendCtrlString(const char *text);
int Net_SendMasterString(const char *text);
int Net_IsMaster(void);
int Net_PingInetSvrs(void);
int Net_GetPing(const char *player);

#endif
