/*
 * Engine/cmd.cpp of the engine3d: the reader of the command file engine.txt (models loaded, animations added and
 * played, pivots, joined nodes and the mesh view), on the token layer of cmd_lex (scrap/lex.h). The handlers
 * read their arguments from the current token on.
 */
#ifndef SCRAP_CMD_H
#define SCRAP_CMD_H

#include "scrap/mathlib.h"

struct CModel;

void Cmd_ReadFile(const char *name);

#endif
