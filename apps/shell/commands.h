#ifndef _COMMANDS_H
#define _COMMANDS_H

int cmd_exit(char *args);
int cmd_exec(char *args);
int cmd_ls(char *args);
int cmd_stat(char *args);
int cmd_mount(char *args);
int cmd_unmount(char *args);
int cmd_mkdir(char *args);
int cmd_cat(char *args);
int cmd_cd(char *args);
int cmd_pwd(char *args);
int cmd_help(char *args);

#endif
