/* menu.h - menu bar built in code (no resource file needed) */
#ifndef MENU_H
#define MENU_H

void menu_build(void);          /* creates tree and installs it with menu_bar() */
void menu_sync(void);           /* enable/disable items, refresh Window list   */
int  menu_cmd_for_obj(int obj); /* object index -> CMD_xxx                      */

#endif
