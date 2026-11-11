/* commands.h - every user action, reachable from menu or keyboard */
#ifndef COMMANDS_H
#define COMMANDS_H

#include "doc.h"

#define CMD_SEP (-1)            /* separator line in menu tables */

enum {
    CMD_NONE = 0,
    CMD_ABOUT,
    CMD_NEW, CMD_OPEN, CMD_CLOSE, CMD_SAVE, CMD_SAVEAS, CMD_SAVEALL, CMD_QUIT,
    CMD_UNDO, CMD_REDO, CMD_CUT, CMD_COPY, CMD_PASTE, CMD_SELALL,
    CMD_FIND, CMD_FINDNEXT, CMD_REPLACE, CMD_FINDALL, CMD_REPLFILES,
    CMD_BMCLEAR,
    CMD_NEXTWIN,
    CMD_WIN0,                       /* CMD_WIN0 .. +7  = pick window n        */
    CMD_BMGO0  = CMD_WIN0 + 8,      /* CMD_BMGO0 .. +9 = go to bookmark F1..F10 */
    CMD_BMSET0 = CMD_BMGO0 + 10,    /* CMD_BMSET0 .. +9 = set bookmark          */
    CMD_COUNT  = CMD_BMSET0 + 10
};

void cmd_execute(int cmd);
int  cmd_close_doc(Doc *d);             /* asks about unsaved changes; 1 = closed */
void cmd_check_error(Doc *d);           /* reports out-of-memory flag */

#endif
