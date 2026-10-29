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
/* the lexer output into the debug log */
void Cmd_Out(const char *text);
/* "Load name file pos [scale] bool": a model (scale 1 when missing), its first animation played */
void Cmd_Load(void);
/* "AddAnim model anim": the animation added to the model and played */
void Cmd_AddAnim(void);
/* "PivotPos model node" */
void Cmd_PivotPos(void);
/* "PlayAnim model anim slot" */
void Cmd_PlayAnim(void);
/* "EFEC_MALLA pantalla|modelo model" */
void Cmd_MeshEffect(void);
/* "FUNDIR_NODOS model": the nodes of its scene joined */
void Cmd_MergeNodes(void);
/* the model named by the token (an identifier or a string), with an error NULL; then the next token */
struct CModel *Cmd_GetModel(void);
/* "( x , y , z )" or "$node" (its world position); 0 or -1 with an error. next: the token before is skipped */
int Cmd_GetVec(byte next, vec3_t *out, int unused);
/* "true" or "false", else an error and def */
byte Cmd_GetBool(byte def);
/* a number, then the character end unless 0; 0 or -1 */
int Cmd_GetFloat(byte next, float *out, int end);
/* the tokens up to the next identifier skipped with an error each */
void Cmd_SkipToIdent(void);
/* the next token, CHECK an integer, its value */
int Cmd_NextInt(void);

#endif
