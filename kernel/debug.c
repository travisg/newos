#include <string.h>
#include <ctype.h>

#include "kernel.h"
#include "debug.h"
#include "int.h"
#include "spinlock.h"
#include "smp.h"
#include "console.h"
#include "vm.h"

#include "arch_dbg_console.h"
#include "arch_debug.h"
#include "arch_cpu.h"

#include <stdarg.h>
#include <printf.h>

static bool serial_debug_on = false;
static int dbg_spinlock = 0;

struct debugger_command
{
	struct debugger_command *next;
	void (*func)(int, char **);
	const char *cmd;
	const char *description;
};

static struct debugger_command *commands;

#define LINE_BUF_SIZE 1024
#define MAX_ARGS 16

static char line_buf[LINE_BUF_SIZE] = "";
static char *args[MAX_ARGS] = { NULL, };

static int debug_read_line(char *buf, int max_len)
{
	char c;
	int ptr = 0;
	
	while(1) {
		c = arch_dbg_con_read();
		if(c == '\n' || c == '\r' || ptr >= max_len - 1) {
			buf[ptr++] = '\0';
			dbg_puts("\n");
			break;
		} else {
			buf[ptr++] = c;
			dbg_putch(c);
		}
	}
	return ptr;
}

static int debug_parse_line(char *buf, char **argv, int *argc, int max_args)
{
	int pos = 0;

	if(!isspace(buf[0])) {
		argv[0] = buf;
		*argc = 1;
	} else {
		*argc = 0;
	}

	while(buf[pos] != '\0') {
		if(isspace(buf[pos])) {
			buf[pos] = '\0';
			// scan all of the whitespace out of this
			while(isspace(buf[++pos])) 
				;
			if(buf[pos] == '\0')
				break;
			argv[*argc] = &buf[pos];
			(*argc)++;
			
			if(*argc >= max_args - 1)
				break;
		}
		pos++;		
	}
	
	return *argc;
}

void kernel_debugger()
{
	int argc;
	struct debugger_command *cmd;
	
	int_disable_interrupts();

	dprintf("kernel debugger on cpu %d\n", smp_get_current_cpu());

	while(1) {
		dprintf(": ");
		debug_read_line(line_buf, LINE_BUF_SIZE);		
		debug_parse_line(line_buf, args, &argc, MAX_ARGS);
		if(argc <= 0) continue;

		cmd = commands;
		while(cmd != NULL) {
			if(strcmp(args[0], cmd->cmd) == 0) {
				cmd->func(argc, args);
			}
			cmd = cmd->next;
		}
	}
}

int panic(const char *fmt, ...)
{
	int ret = 0; 
	va_list args;
	char temp[128];

	if(serial_debug_on) {
		va_start(args, fmt);
		ret = vsprintf(temp, fmt, args);
		va_end(args);

		dbg_puts("PANIC: ");
		dbg_puts(temp);
		
		// halt all of the other cpus
		smp_send_broadcast_ici(SMP_MSG_CPU_HALT, 0, NULL);
		
		kernel_debugger();
	}
	return ret;
}

int dprintf(const char *fmt, ...)
{
	int ret = 0; 
	va_list args;
	char temp[128];

	if(serial_debug_on) {
		va_start(args, fmt);
		ret = vsprintf(temp, fmt, args);
		va_end(args);
	
		dbg_puts(temp);
	}
	return ret;
}

char dbg_putch(char c)
{
	char ret;
	int flags = int_disable_interrupts();
	acquire_spinlock(&dbg_spinlock);
	
	if(serial_debug_on)
		ret = arch_dbg_con_putch(c);
	else
		ret = c;

	release_spinlock(&dbg_spinlock);
	int_restore_interrupts(flags);

	return ret;
}

void dbg_puts(const char *s)
{
	int flags = int_disable_interrupts();
	acquire_spinlock(&dbg_spinlock);
	
	if(serial_debug_on)
		arch_dbg_con_puts(s);

	release_spinlock(&dbg_spinlock);
	int_restore_interrupts(flags);
}

int dbg_add_command(void (*func)(int, char **), const char *name, const char *desc)
{
	int flags;
	struct debugger_command *cmd;
	
	cmd = (struct debugger_command *)kmalloc(sizeof(struct debugger_command));
	if(cmd == NULL)
		return -1;

	cmd->func = func;
	cmd->cmd = name;
	cmd->description = desc;

	flags = int_disable_interrupts();
	acquire_spinlock(&dbg_spinlock);
	
	cmd->next = commands;
	commands = cmd;

	release_spinlock(&dbg_spinlock);
	int_restore_interrupts(flags);

	return 0;
}	

static void cmd_reboot(int argc, char **argv)
{
	TOUCH(argc);TOUCH(argv);

	reboot();
}

static void cmd_help(int argc, char **argv)
{
	struct debugger_command *cmd;
	TOUCH(argc);TOUCH(argv);


	dprintf("debugger commands:\n");
	cmd = commands;
	while(cmd != NULL) {
		dprintf("%s\t\t%s\n", cmd->cmd, cmd->description);
		cmd = cmd->next;
	}
}

int dbg_init(struct kernel_args *ka)
{
	commands = NULL;
	
	return arch_dbg_con_init(ka);
}

int dbg_init2(struct kernel_args *ka)
{
	TOUCH(ka);

	dbg_add_command(&cmd_help, "help", "List all debugger commands");
	dbg_add_command(&cmd_reboot, "reboot", "Reboot");
	
	return 0;
}

bool dbg_set_serial_debug(bool new_val)
{
	int temp = serial_debug_on;
	serial_debug_on = new_val;
	return temp;
}

