#ifndef _COMMANDS_H
#define _COMMANDS_H

int cmd_exit(int argc, char *argv[]);
int cmd_exec(int argc, char *argv[]);
int cmd_ls(int argc, char *argv[]);
int cmd_stat(int argc, char *argv[]);
int cmd_mount(int argc, char *argv[]);
int cmd_unmount(int argc, char *argv[]);
int cmd_mkdir(int argc, char *argv[]);
int cmd_cat(int argc, char *argv[]);
int cmd_cd(int argc, char *argv[]);
int cmd_pwd(int argc, char *argv[]);
int cmd_help(int argc, char *argv[]);
int cmd_ps(int argc, char *argv[]);
int cmd_echo(int argc, char *argv[]);

#endif
