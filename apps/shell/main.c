/*
** Copyright 2001, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#include <string.h>
#include <ctype.h>
#include <sys/syscalls.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "commands.h"
#include "parse.h"
#include "statements.h"
#include "shell_defs.h"
#include "shell_vars.h"
#include "args.h"

static int readline(char *buf, int len)
{
	int i = 0;
	char ch;

	while(true){

		ch = getchar();

		switch(ch){
		case  8 :
			if(i>0){
				printf("%c", ch);
				i --;
			}
			break;

		case '\n':
			buf[i] = 0;
			printf("\n");
			return i;

		default:
			buf[i] = ch;
			printf("%c",ch);
			i++;
		}
	}
}

int main(int argc,char *argv[])
{
	char buf[1024];

	init_vars();
	init_statements();
	init_arguments(argc,argv);

	if(af_script_file_name != NULL){
		run_script(af_script_file_name);
		if(af_exit_after_script) sys_exit(0);
	}

	for(;;) {
		int chars_read;

		printf("> ");

		chars_read = readline(buf, sizeof(buf));
 		if(chars_read > 0) {
			parse_string(buf);
		}
	}

	return 0;
}


