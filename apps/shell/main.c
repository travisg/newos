/*
** Copyright 2001, Travis Geiselbrecht. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#include <types.h>
#include <string.h>
#include <unistd.h>
#include <libc/printf.h>
#include <libc/ctype.h>
#include <sys/syscalls.h>
#include <libsys/stdio.h>

#include "commands.h"

struct command {
	char *cmd_text;
	int (*cmd_handler)(int argc, char *argv[]);
};

struct command cmds[] = {
	{"exit", &cmd_exit},
	{"exec", &cmd_exec},
	{"ls", &cmd_ls},
	{"stat", &cmd_stat},
	{"mount", &cmd_mount},
	{"unmount", &cmd_unmount},
	{"mkdir", &cmd_mkdir},
	{"cat", &cmd_cat},
	{"cd", &cmd_cd},
	{"pwd", &cmd_pwd},
	{"ps", &cmd_ps},
	{"echo", &cmd_echo},
	{"help", &cmd_help},
	{NULL, &cmd_exec},
	{NULL, NULL}
};

struct path {
	const char *path;
} paths[] = {
	{"/boot/"},
	{NULL}
};

enum {
	PARSE_STATE_INITIAL = 0,
	PARSE_STATE_SCANNING_SPACE,
	PARSE_STATE_NEW_FIELD,
	PARSE_STATE_SCANNING_FIELD,
	PARSE_STATE_FIELD_DONE,
	PARSE_STATE_SCANNING_STRING,
	PARSE_STATE_DONE
};

enum {
	FIELD_ARG = 0,
	FIELD_INPUT,
	FIELD_OUTPUT
};

static int parse_line(char *buf, char *argv[], int max_args, char *redirect_in, char *redirect_out)
{
	int state = PARSE_STATE_INITIAL;
	int curr_char = 0;
	int curr_field = 0;
	int field_start = 0;
	int field_end = 0;
	int field_type = 0;
	int c = 0;


	redirect_in[0]= 0;
	redirect_out[0]= 0;
	for(;;) {
		// read in the next char if we're in the proper state
		switch(state) {
			case PARSE_STATE_SCANNING_SPACE:
			case PARSE_STATE_SCANNING_FIELD:
			case PARSE_STATE_SCANNING_STRING:
				c = buf[curr_char++];
				break;
		}

//		printf("state %d\n", state);

		switch(state) {
			case PARSE_STATE_INITIAL:
				curr_char = 0;
				curr_field = 0;
				state = PARSE_STATE_SCANNING_SPACE;
				break;
			case PARSE_STATE_SCANNING_SPACE:
				if(!isspace(c)) {
					// we see a non-space char, we gots to transition
					state = PARSE_STATE_NEW_FIELD;
					switch(c) {
						case '>':
							field_type = FIELD_OUTPUT;
							state = PARSE_STATE_SCANNING_SPACE;
							break;
						case '<':
							field_type = FIELD_INPUT;
							state = PARSE_STATE_SCANNING_SPACE;
							break;
						default:
							break;
					}
				} else if(c == '\0')
					state = PARSE_STATE_DONE;
				break;
			case PARSE_STATE_NEW_FIELD:
				if(c == '"') {
					// start of a string
					state = PARSE_STATE_SCANNING_STRING;
					field_start = curr_char;
				} else {
					state = PARSE_STATE_SCANNING_FIELD;
					field_start = curr_char-1;
				}
				break;
			case PARSE_STATE_SCANNING_FIELD:
				if(isspace(c) || c == '\0') {
					// time to break outta here
					field_end = curr_char-1;
					state = PARSE_STATE_FIELD_DONE;
				}
				break;
			case PARSE_STATE_FIELD_DONE:
				// point the args at the spot in the buffer
				buf[field_end] = 0;
				switch(field_type) {
					case FIELD_OUTPUT:
						strcpy(redirect_out, buf+field_start);
						break;
					case FIELD_INPUT:
						strcpy(redirect_in, buf+field_start);
						break;
					case FIELD_ARG:
						argv[curr_field++] = &buf[field_start];
						if(curr_field == max_args)
							return curr_field;
				}
				if(c != '\0')
					state = PARSE_STATE_SCANNING_SPACE;
				else
					state = PARSE_STATE_DONE;
				field_type = FIELD_ARG;
				break;
			case PARSE_STATE_SCANNING_STRING:
				if(c == '"' || c == '\0') {
					field_end = curr_char-1;
					state = PARSE_STATE_FIELD_DONE;
				}
				break;
			case PARSE_STATE_DONE:
				return curr_field;
				break;
		}
	}
	return -1;
}

static int launch(int (*cmd)(int, char **), int argc, char **argv, char *r_in, char *r_out)
{
	int saved_in;
	int saved_out;
	int new_in;
	int new_out;
	int retval= 0;
	int err = 0;

	if(strcmp(r_in, "")!= 0) {
		new_in = open(r_in, STREAM_TYPE_ANY, 0);
	} else {
		new_in = dup(0);
	}
	if(new_in < 0) {
		printf("cannot open redirection source %s: %s\n", r_in, strerror(new_in));
		goto err;
	}

	if(strcmp(r_out, "")!= 0) {
		new_out = open(r_out, STREAM_TYPE_ANY, 0);

		if(new_out < 0) {
			err = sys_create(r_out,STREAM_TYPE_FILE);
			if(err < 0) {
				printf("cannot create redirection target %s: %s\n", r_out, strerror(err));
				goto err;
			}
			new_out = open(r_out,STREAM_TYPE_ANY, 0);
		}
	} else {
		new_out = dup(1);
	}

	if(new_out < 0) {
		err = new_out;
		printf("cannot open redirection target %s: %s\n", r_out, strerror(err));
		goto err;
	}
	saved_in = dup(0);
	saved_out= dup(1);

	dup2(new_in, 0);
	dup2(new_out, 1);
	close(new_in);
	close(new_out);

	retval= cmd(argc, argv);

	dup2(saved_in, 0);
	dup2(saved_out, 1);
	close(saved_in);
	close(saved_out);

	return retval;

err:
	close(new_in);

	return 0;
}

static int shell_parse(char *buf, int len)
{
	int i;
	bool found_command = false;

	// trim off the trailing carriage returns
	for(i=strlen(buf)-1; i>=0; i--) {
		if(buf[i] == 0x0a || buf[i] == 0x0d)
			buf[i] = 0;
	}

	// trim off any trailing spaces
	for(i=strlen(buf)-1; i>=0; i--) {
		if(isspace(buf[i]))
			buf[i] = 0;
		else
			break;
	}

	// trim off any leading spaces
	while(isspace(*buf))
		buf++;

	// if the command is now zero length, quit
	if(strlen(buf) == 0)
		return 0;

	// search for the command
	for(i=0; cmds[i].cmd_handler != NULL; i++) {
		if(cmds[i].cmd_text != NULL) {
			if(strncmp(cmds[i].cmd_text, buf, strlen(cmds[i].cmd_text)) == 0) {
				if(buf[strlen(cmds[i].cmd_text)] != 0 && !isspace(buf[strlen(cmds[i].cmd_text)])) {
					// we're actually looking at the leading edge of a larger word, skip it
					continue;
				}
				{
					// we need to parse the command
					int argc;
					char *argv[64];
					char redirect_in[256];
					char redirect_out[256];

					argc = parse_line(buf, argv, 64, redirect_in, redirect_out);
					launch(cmds[i].cmd_handler, argc, argv, redirect_in, redirect_out);
				}
				found_command = true;
				break;
			}
		} else {
			// we need to parse the command
			int argc;
			char *argv[64];
			char redirect_in[256];
			char redirect_out[256];

			argv[0]= "exec";
			argc = 1+parse_line(buf, argv+1, 63, redirect_in, redirect_out);
			launch(cmds[i].cmd_handler, argc, argv, redirect_in, redirect_out);
			found_command = true;
			break;

		}
	}
	if(found_command == false) {
		printf("Invalid command, please try again.\n");
	}
	return 0;
}

static int readline(char *buf, int len)
{
	int i;

	for(i=0; i<len-1; i++) {
		buf[i] = getc();

		/* knock out backspaces from the buffer */
		if (buf[i] == 8) {
			if (i>=1) {
				printf("%c", buf[i]);
				i -= 2;
			}
			else {
				i--;
			}
			continue;
		}

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

	for(;;) {
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


