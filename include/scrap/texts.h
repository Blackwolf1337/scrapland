/*
 * The player-visible texts ScrapC adds (the menus of the worlds of the new network and their messages), which the
 * language files of the game do not have: one table in src/game/Script/Texts.c, English only for now. Script_Lang
 * (and so {Key} in a .def file) falls back to it for a key the language files lack. Not in the original.
 */
#ifndef SCRAP_TEXTS_H
#define SCRAP_TEXTS_H

/* the English text of key, NULL when ScrapC has none */
const char *Texts_Get(const char *key);
/* fmt with %s (the names in order), %d or %i (the number) and %% filled in: no printf format string from a table or a
 * language file */
void Texts_Fill(char *out, int cap, const char *fmt, int num, const char *name, const char *name2);
/* the text of key (Script_Lang: the language's, else the table's) with its %s or %d replaced by arg */
void Texts_Format(char *out, int size, const char *key, const char *arg);

#endif
