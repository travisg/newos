/* 
** Copyright 2001, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#include <types.h>
#include <libc/string.h>
#include <libc/printf.h>
#include <libc/ctype.h>
#include <libsys/syscalls.h>
#include <libsys/stdio.h>

#include "commands.h"

struct command {
	char *cmd_text;
	int (*cmd_handler)(char *);
	int needs_args;
};

struct command cmds[] = {
	{"exit", &cmd_exit, 0},
	{"exec", &cmd_exec, 1},
	{NULL, NULL, 0}
};

int shell_parse(char *buf, int len)
{
	int i;
	bool found_command = false;

	// trim off the trailing carriage returns
	for(i=strlen(buf)-1; i>=0; i--) {
		if(buf[i] == 0x0a || buf[i] == 0x0d) 
			buf[i] = 0;
	}
	
	// trim off the trailing spaces
	for(i=strlen(buf)-1; i>=0; i--) {
		if(isspace(buf[i])) 
			buf[i] = 0;
		else 
			break;
	}

	// if the command is now zero length, quit
	if(strlen(buf) == 0)
		return 0;

	// search for the command
	i=0;
	while(cmds[i].cmd_text != NULL) {
		if(strncmp(cmds[i].cmd_text, buf, strlen(cmds[i].cmd_text)) == 0) {
			// see if it has an argument, if it needs it
			if(cmds[i].needs_args && !isspace(buf[strlen(cmds[i].cmd_text)])) {
				printf("command requires arguments\n");
			} else {
				// call the command handler function
				int j;
				for(j = strlen(cmds[i].cmd_text); isspace(buf[j]); j++);
					if(cmds[i].cmd_handler(&buf[j]) != 0)
					return -1;
			}
			found_command = true;
			break;
		}
		i++;
	}	
	if(found_command == false) {
		printf("Invalid command, please try again.\n");
	}
	return 0;
}

int readline(char *buf, int len)
{
	int i;
	
	for(i=0; i<len-1; i++) {
		buf[i] = getc();
		printf("%c", buf[i]);
		if(buf[i] == '\n') {
			buf[i] = 0;
			break;
		}
	}
	if(i == len-1) {
		buf[i] = 0;
	}
	return i;
}

int main()
{
	char buf[1024];
	
	printf("Welcome to the NewOS shell\n");
	
	while(1) {
		int chars_read;
		
		printf("> ");
		
		chars_read = readline(buf, sizeof(buf));
		if(chars_read > 0) {
			if(shell_parse(buf, chars_read) < 0)
				break;
		}
	}
	
	printf("shell exiting\n");
	
	return 0;
}


