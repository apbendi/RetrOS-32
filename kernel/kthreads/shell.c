/**
 * @file shell.c
 * @author Joe Bayer (joexbayer)
 * @brief Simple program handling input from user, mainly used to handles process management.
 * @version 0.1
 * @date 2022-06-01
 * 
 * @copyright Copyright (c) 2022
 * 
 */
#include <memory.h>
#include <kernel.h>
#include <pci.h>
#include <keyboard.h>
#include <terminal.h>
#include <scheduler.h>
#include <pcb.h>
#include <rtc.h>
#include <kthreads.h>
#include <ksyms.h>
#include <arch/io.h>

#include <windowmanager.h>
#include <net/dns.h>
#include <net/icmp.h>

#include <serial.h>

#include <diskdev.h>

#include <gfx/gfxlib.h>
#include <gfx/theme.h>
#include <gfx/composition.h>
#include <gfx/events.h>

#include <fs/fs.h>

#include <net/socket.h>
#include <net/tcp.h>
#include <net/net.h>
#include <conf.h>

#include <kutils.h>
#include <script.h>
#include <vbe.h>

#define SHELL_HEIGHT 225 /* 275 */
#define SHELL_WIDTH 400 /* 300 */
/* The longest command line, with its terminating 0: room for a command and two paths (#107) */
#define SHELL_LINE_MAX (2*FS_PATH_MAX + 64)
/* The most commands a shell's history keeps */
#define SHELL_HISTORY_MAX 32

void __kthread_entry shell(int argc, char* argv[]);

static const char newline = '\n';
static const char backspace = '\b';

/**
 * A GUI shell's state. Each Terminal window runs its own shell thread with its own, which the thread's PCB points to,
 * so commands that run on the thread, like reset, can find it (#42).
 */
struct shell {
	char buffer[SHELL_LINE_MAX];
	int length;

	/* The commands run, oldest first, each kalloc'd at its own length. Up and Down go back and forth through them */
	char* history[SHELL_HISTORY_MAX];
	int history_count;
	/* The command Up and Down have brought back: -1 while typing a new line */
	int history_position;
	/* The new line being typed when Up was first pressed, which Down brings back past the newest command */
	char draft[SHELL_LINE_MAX];

	/* "kernel:<current directory>> " */
	char prompt[FS_PATH_MAX + 16];
	/* Set by the exit command: the shell exits once the line has run */
	bool_t exiting;
};

static void __shell_free(void* _sh)
{
	struct shell* sh = (struct shell*) _sh;
	for (int i = 0; i < sh->history_count; i++){
		kfree(sh->history[i]);
	}
	kfree(sh);
}

/* The calling thread's shell, or NULL when it isn't a GUI shell, e.g. a program running a command with system */
static struct shell* __shell_current()
{
	if($process->current->local_free != __shell_free) return NULL;
	return (struct shell*) $process->current->local;
}

static char* about_text = "\nRetrOS-32 - 32bit Operating System\n    " KERNEL_RELEASE " " KERNEL_VERSION " - " KERNEL_DATE "\n";

/*
 *	IMPLEMENTATIONS
 */
/**
 * Writes the current directory into out, which holds max+1 bytes. One longer than max keeps its end, after "...",
 * since that names the directory you're in.
 */
static void __shell_cwd(char* out, int max)
{
	char cwd[FS_PATH_MAX];
	if(fs_getcwd(cwd, sizeof(cwd)) < 0) memcpy(cwd, "/", 2);

	int len = strlen(cwd);
	if(len <= max){
		memcpy(out, cwd, len + 1);
		return;
	}
	memcpy(out, "...", 3);
	memcpy(out + 3, cwd + len - (max - 3), max - 3 + 1);
}

/* Adds a command to the terminal's history after its prompt, with the whole current directory */
static void __shell_echo(const char* line)
{
	char cwd[FS_PATH_MAX];
	if(fs_getcwd(cwd, sizeof(cwd)) < 0) memcpy(cwd, "/", 2);
	twritef("kernel:%s> %s\n", cwd, line);
}

/* Shows the prompt and the command typed so far on the terminal's input line, after the output */
static void __shell_show_line(struct shell* sh)
{
	terminal_set_input($process->current->term, sh->prompt, sh->buffer);
}

/* Starts a new command line: a prompt after the output, with nothing typed */
static void reset_shell(struct shell* sh)
{
	/* After a program's unfinished last line, the prompt starts a row of its own, as zsh's does */
	terminal_end_line($process->current->term);

	/**
	 * The prompt shows the current directory, which a command may have changed. It's the same text __shell_echo leaves
	 * in the output when the command runs, so Enter leaves the line on screen as it was.
	 */
	char cwd[FS_PATH_MAX];
	if(fs_getcwd(cwd, sizeof(cwd)) < 0) memcpy(cwd, "/", 2);
	int cwd_len = strlen(cwd);
	memcpy(sh->prompt, "kernel:", 7);
	memcpy(sh->prompt + 7, cwd, cwd_len);
	memcpy(sh->prompt + 7 + cwd_len, "> ", 3);

	memset(sh->buffer, 0, SHELL_LINE_MAX);
	sh->length = 0;
	sh->history_position = -1;
	__shell_show_line(sh);
}

/* Replaces the line being typed with text */
static void __shell_set_line(struct shell* sh, const char* text)
{
	int length = strlen(text);
	if(length > SHELL_LINE_MAX - 1) length = SHELL_LINE_MAX - 1;

	memcpy(sh->buffer, text, length);
	sh->buffer[length] = 0;
	sh->length = length;
	__shell_show_line(sh);
}

/* Adds a command to the history, unless it's blank or the same as the newest. The oldest goes when it's full */
static void __shell_history_add(struct shell* sh, const char* line)
{
	const char* c = line;
	while(*c == ' ' || *c == '\t') c++;
	if(*c == 0) return;

	if(sh->history_count > 0 && strcmp(sh->history[sh->history_count-1], line) == 0) return;

	int length = strlen(line);
	char* entry = kalloc(length + 1);
	if(entry == NULL) return;
	memcpy(entry, line, length + 1);

	if(sh->history_count == SHELL_HISTORY_MAX){
		kfree(sh->history[0]);
		memmove(&sh->history[0], &sh->history[1], (SHELL_HISTORY_MAX - 1) * sizeof(char*));
		sh->history_count--;
	}
	sh->history[sh->history_count++] = entry;
}

/* Up: brings back the command before the one shown, keeping the new line being typed for Down to bring back */
static void __shell_history_up(struct shell* sh)
{
	if(sh->history_count == 0) return;

	if(sh->history_position == -1){
		memcpy(sh->draft, sh->buffer, sh->length + 1);
		sh->history_position = sh->history_count - 1;
	} else if(sh->history_position > 0){
		sh->history_position--;
	} else {
		/* At the oldest */
		return;
	}
	__shell_set_line(sh, sh->history[sh->history_position]);
}

/* Down: brings back the command after the one shown, or past the newest, the line that was being typed */
static void __shell_history_down(struct shell* sh)
{
	if(sh->history_position == -1) return;

	if(sh->history_position < sh->history_count - 1){
		sh->history_position++;
		__shell_set_line(sh, sh->history[sh->history_position]);
		return;
	}
	sh->history_position = -1;
	__shell_set_line(sh, sh->draft);
}

void ifconfig()
{
	net_list_ifaces();
}
EXPORT_KSYMBOL(ifconfig);

/* Shell commands */
void ps(int argc, char* argv[])
{
	ubyte_t spin = 0;
	if(argc == 2){
		if(memcmp(argv[1], "-s", 2) == 0){
			spin = 1;
		}
	}

	if(spin){
		while(1){
			$process->current->term->ops->reset($process->current->term);
			twritef("\nPID  USER    USAGE    TYPE     STATE    NAME    \n");
			for (int i = 1; i < MAX_NUM_OF_PCBS; i++){
				struct pcb_info info;
				int ret = pcb_get_info(i, &info);
				if(ret < 0) continue;
				int usage = (int)(info.usage*100);
				twritef(" %d   %8s %s%d%      %s  %s  %s\n", info.pid, info.user, usage < 10 ? " ": "", usage, info.is_process ? "process" : "kthread", pcb_status[info.state], info.name);
			}

			$process->current->term->ops->commit($process->current->term);
			
			struct gfx_event event;
			gfx_event_loop(&event, GFX_EVENT_NONBLOCKING);

			switch (event.event){
			case GFX_EVENT_KEYBOARD:
				switch (event.data){
				case CTRLC:
					return;
				}
				break;
			case GFX_EVENT_EXIT:
				kernel_exit();
				return;
			default:
				break;
			}
			kernel_sleep(1000);
		}
	}

	//int usage;
	twritef("\nPID  USER    USAGE    TYPE     STATE     NAME    \n");
	for (int i = 1; i < MAX_NUM_OF_PCBS; i++){
		struct pcb_info info;
		int ret = pcb_get_info(i, &info);
		if(ret < 0) continue;
		int usage = (int)(info.usage*100);
		twritef(" %d   %8s %s%d%      %s  %s  %s\n", info.pid, info.user, usage < 10 ? " ": "", usage, info.is_process ? "process" : "kthread", pcb_status[info.state], info.name);
	}
}
EXPORT_KSYMBOL(ps);

/* Lists the windows on screen, front to back. F12 logs the same list to the serial log, whatever has focus. */
void windows(int argc, char* argv[])
{
	struct wm_window_info list[WM_LIST_MAX_WINDOWS];
	int count = gfx_get_windows(list, WM_LIST_MAX_WINDOWS);
	if(count < 0){
		twritef("No windows: the window server isn't running\n");
		return;
	}

	twritef("%d windows, front to back:\n", count);
	for (int i = 0; i < count && i < WM_LIST_MAX_WINDOWS; i++){
		twritef("%d %s x=%d y=%d w=%d h=%d pid=%d\n", i, list[i].name, list[i].x, list[i].y, list[i].width, list[i].height, list[i].pid);
	}
}
EXPORT_KSYMBOL(windows);

/* Function to print the lines and corners */
static void print_branches(int level) {
    for (int i = 0; i < level; i++) {
        if (i == level - 1) {
            twritef(":---");
        } else {
            twritef(":   ");
        }
    }
}

/* Recursive function to print PCB tree */
static void print_pcb_tree(struct pcb *pcb, int level) {
    if (pcb == NULL || pcb->pid == -1) {
        return;
    }

    /* Print branches and nodes */
    print_branches(level);
    twritef(">%s (%d)\n", pcb->name, pcb->pid);

    /* Recursively print child PCBs */
    for (int i = 1; i < MAX_NUM_OF_PCBS; i++) {
        struct pcb *child_pcb = pcb_get_by_pid(i);
        if (child_pcb && child_pcb->parent == pcb) {
            print_pcb_tree(child_pcb, level + 1);
        }
    }
}

/* Function to visualize the PCBs as a tree */
void tree() {
    /* Hierarchical visualization of the PCBs based on their parent */
    for (int i = 1; i < MAX_NUM_OF_PCBS; i++) {
        struct pcb *pcb = pcb_get_by_pid(i);
        if (pcb && pcb->parent == NULL) { /* Start with root PCBs */
            print_pcb_tree(pcb, 0);
        }
    }
}
EXPORT_KSYMBOL(tree);


void sh(int argc, char* argv[])
{
	if(argc == 1){
		twritef("usage: sh <file>\n");
		return;
	}

	int inode = fs_open(argv[1], FS_FILE_FLAG_READ);
	if(inode < 0){
		twritef("File %s not found.\n", argv[1]);
		return;
	}

	/* script_parse stops at a 0, so the script ends where the file does, not at leftover heap text (#80) */
	char* buf = kalloc(32*1024);
	int ret = fs_read(inode, buf, 32*1024 - 1);
	if(ret < 0){
		fs_close(inode);
		kfree(buf);
		twritef("Error reading file\n");
		return;
	}
	buf[ret] = 0;
	
	script_parse(buf);
	
	fs_close(inode);
	kfree(buf);

	return;
}
EXPORT_KSYMBOL(sh);

void ed()
{
	int pid = pcb_create_process("/bin/edit.o", 0, NULL, PCB_FLAG_KERNEL);
	if(pid < 0)
		twritef("%s does not exist\n", "edit.o");
}
EXPORT_KSYMBOL(ed);

void exec(int argc, char* argv[])
{
	pid_t pid;
	ubyte_t idx = 1;
	bool_t kthread_as_deamon = false;

	if(argc == 1){
		twritef("usage: exec [options] <file | kfunc> [args ...]\n Note: Any command can be executed by as a thread\n Example: exec echo hi\n");
		return;
	}

	/* check for potential options */
	if(argv[1][0] == '-'){
		switch (argv[1][1]){ 	
		case 'd':{
				kthread_as_deamon = true;	
			}
			break;
		default:
			break;
		}
		idx++;
	}

	pid = start(argv[idx], argc-idx, &argv[idx]);
	if(pid >= 0){
		twritef("Kernel thread started\n");
		return;
	}
	/* -ERROR_KTHREAD_START means there's no kernel thread by that name. Anything else, like running out of memory, is why it didn't start. */
	if(pid != -ERROR_KTHREAD_START){
		twritef("Couldn't start %s: %s\n", argv[idx], error_get_string(pid));
		return;
	}

	/**
	 * A program the shell waits for reads what's typed at the terminal, since the shell isn't reading it meanwhile. It
	 * becomes the terminal's reader before it can run, so its first read doesn't find there's none.
	 */
	struct terminal* term = $process->current->term;
	int reader = term != NULL ? term->reader : TERMINAL_NO_READER;
	if(term != NULL && !kthread_as_deamon) term->reader = TERMINAL_READER_STARTING;

	pid = pcb_create_process(argv[idx], argc-idx, &argv[idx], 0 /* PCB_FLAG_KERNEL */);
	if(pid > 0 && !kthread_as_deamon){
		if(term != NULL) term->reader = pid;
		pcb_await(pid);
	}
	if(term != NULL) term->reader = reader;
	if(pid > 0){
		return;
	}

	void (*ptr)(int argc, char* argv[]) = (void (*)(int argc, char* argv[])) ksyms_resolve_symbol(argv[idx]);
	if(ptr == NULL){
		/* Not a kernel function either, so say why the program didn't start, unless there's no such program */
		if(pid < 0 && pid != -ERROR_FILE_NOT_FOUND){
			twritef("Couldn't start %s: %s\n", argv[idx], error_get_string(pid));
			return;
		}
		twritef("Unknown command\n");
		return;
	}

	pid = pcb_create_kthread(ptr, argv[idx], argc-idx, &argv[idx]);
	if(pid > 0){
		if(kthread_as_deamon) pcb_await(pid);
		return;
	}
	if(pid < 0){
		twritef("Couldn't start %s: %s\n", argv[idx], error_get_string(pid));
		return;
	}
	
	twritef("Unknown command\n");
    return;
}
EXPORT_KSYMBOL(exec);

/**
 * @brief cmd: command [opts] [args]
 * 
 * command:
 * 	resolved from ksyms
 * 
 * opts:
 *  -t run as a thread (long)
 *  -w run as a worker (short)
 * 
 * args: 
 * 	arguments passed to command
 * 
 */

void ths()
{
	int total_themes = gfx_total_themes();
	for (int i = 0; i < total_themes; i++){
		twritef("%d) %s\n", i, kernel_gfx_get_theme(i)->name);
	}
}
EXPORT_KSYMBOL(ths);

#define COMMAND(name, func) \
	void name(int argc, char* argv[])\
		func\
	EXPORT_KSYMBOL(name);

COMMAND(dns, {

	if(argc == 1){
		twritef("usage: dns <domain>\n");
		return;
	}

	int val = gethostname(argv[1]);
	twritef("%s IN (A) %i\n", argv[1], val);
})

void th(int argc, char* argv[])
{
	int id = atoi(argv[1]);
	kernel_gfx_set_theme(id);
}
EXPORT_KSYMBOL(th);

/* Kills a thread or program by the pid ps lists */
void kill(int argc, char* argv[])
{
	/* atoi finds 0, idled's pid, in anything without digits, and overflows on too many */
	int length = argc == 2 ? strlen(argv[1]) : 0;
	bool_t number = length > 0 && length < 10;
	for (int i = 0; i < length && number; i++){
		number = isdigit(argv[1][i]);
	}
	if(!number){
		twritef("usage: kill <pid>\n");
		return;
	}

	int pid = atoi(argv[1]);
	switch (pcb_kill(pid)){
	case ERROR_OK:
		break;
	case -ERROR_ACCESS_DENIED:
		twritef("%d is a system thread, which can't be killed.\n", pid);
		break;
	case -ERROR_INVALID_ARGUMENTS:
		twritef("%d is the thread running this command. Use exit to close a shell.\n", pid);
		break;
	default:
		twritef("No thread or program has pid %d.\n", pid);
		break;
	}
}
EXPORT_KSYMBOL(kill);

/* Prints the arguments, separated by single spaces. With none, it prints an empty line. */
void echo(int argc, char* argv[])
{	
	for (int i = 1; i < argc; i++){
		twritef(i < argc-1 ? "%s " : "%s", argv[i]);
	}
	twritef("\n");
}
EXPORT_KSYMBOL(echo);

void fdisk(int argc, char* argv[])
{
	struct diskdev* dev = disk_device_get();
	if(dev == NULL){
		twritef("No disk device attached\n");
		return;
	}

	twritef("fdisk:      \n");
	twritef("Disk:     %s\n", dev->dev->model);
	twritef("Size:     %d\n", dev->dev->size*512);
	twritef("Attached: %d\n", dev->attached);
	twritef("Read:     %x\n", dev->read);
	twritef("Write:    %x\n", dev->write);
}
EXPORT_KSYMBOL(fdisk);

void meminfo(int argc, char* argv[])
{
	struct mem_info minfo;
	get_mem_info(&minfo);

	/* write mem for all 3 types */
	struct unit kernel = calculate_size_unit(minfo.kernel.used);
	struct unit kernel_total = calculate_size_unit(minfo.kernel.total);

	struct unit permanent = calculate_size_unit(minfo.permanent.used);
	struct unit permanent_total = calculate_size_unit(minfo.permanent.total);

	struct unit virtual = calculate_size_unit(minfo.virtual_memory.used);
	struct unit virtual_total = calculate_size_unit(minfo.virtual_memory.total);

	struct unit total = calculate_size_unit(minfo.kernel.total+minfo.permanent.total+minfo.virtual_memory.total);

	twritef("Memory:\n");
	twritef("  Kernel:    %d%s/%d%s\n", kernel.size, kernel.unit, kernel_total.size, kernel_total.unit);
	twritef("  Permanent: %d%s/%d%s\n", permanent.size, permanent.unit, permanent_total.size, permanent_total.unit);
	twritef("  Virtual:   %d%s/%d%s\n", virtual.size, virtual.unit, virtual_total.size, virtual_total.unit);
	twritef("  Total:     %d%s\n", total.size, total.unit);
}
EXPORT_KSYMBOL(meminfo);

void res(int argc, char* argv[])
{
	twritef("Screen resolution: %dx%d\n", vbe_info->width, vbe_info->height);
}
EXPORT_KSYMBOL(res);

void bg(int argc, char* argv[]){
	if(argc == 1){
		twritef("usage: bg [<hex>, reset]\n");
		return;
	}

	if(memcmp(argv[1], "reset", 6) == 0){
		gfx_set_background_color(3);
		return;
	}

	uint32_t color = htoi(argv[1]);
	gfx_set_background_color(color);
}
EXPORT_KSYMBOL(bg);

void exit()
{
	twritef("Shell exited.\n");

	/* A shell exits once the line has run. Exiting here would never return to exec_cmd, which frees the line's words */
	struct shell* sh = __shell_current();
	if(sh != NULL){
		sh->exiting = true;
		return;
	}
	kernel_exit();
}
EXPORT_KSYMBOL(exit);

void socks(void)
{
	struct sockets socks;
	net_get_sockets(&socks);

	twritef("%s)\n", socket_type_to_str(SOCK_STREAM));
	for (int i = 0; i < socks.total_sockets; i++){
		struct sock* sock = socks.sockets[i];
		if(sock == NULL || sock->bound_port == 0 || sock->type == SOCK_DGRAM) continue;

		twritef(" %i:%d %i:%d %s  tx: %d  rx: %d\n\n", sock->bound_ip == 1 ? 0 : sock->bound_ip, ntohs(sock->bound_port), ntohl(sock->recv_addr.sin_addr.s_addr), ntohs(sock->recv_addr.sin_port), sock->tcp ? tcp_state_to_str(sock->tcp->state) : "", sock->tx, sock->rx);
	}

	twritef("%s)\n", socket_type_to_str(SOCK_DGRAM));
	for (int i = 0; i < socks.total_sockets; i++){
		struct sock* sock = socks.sockets[i];
		if(sock == NULL || sock->bound_port == 0 || sock->type == SOCK_STREAM) continue;

		twritef(" %i:%d %i:%d %s  tx: %d  rx: %d\n\n", sock->bound_ip == 1 ? 0 : sock->bound_ip, ntohs(sock->bound_port), ntohl(sock->recv_addr.sin_addr.s_addr), ntohs(sock->recv_addr.sin_port), sock->tcp ? tcp_state_to_str(sock->tcp->state) : "", sock->tx, sock->rx);
	}
}
EXPORT_KSYMBOL(socks);


void reset(int argc, char* argv[])
{
	/* Only a shell has its own window to clear, and a line to start again */
	struct shell* sh = __shell_current();
	if(sh == NULL || $process->current->term == NULL){
		twritef("reset: only works in a Terminal window\n");
		return;
	}

	$process->current->term->ops->reset($process->current->term);
	reset_shell(sh);
}
EXPORT_KSYMBOL(reset);


/**
 * @brief Puts a character c into the shell line 
 * at correct position. Also detects enter.
 * 
 * @param c character to put to screen.
 */
static void shell_put(struct shell* sh, unsigned char c)
{
	unsigned char uc = c;

	/* Editing a command brought back changes only the line, not the history: Enter adds it as a new command */
	if(uc == ARROW_UP){
		__shell_history_up(sh);
		return;
	}
	if(uc == ARROW_DOWN){
		__shell_history_down(sh);
		return;
	}

	if(uc == newline){
		__shell_history_add(sh, sh->buffer);

		/* The line moves from the input line into the output, so what the command prints goes after it */
		terminal_hide_input($process->current->term);
		__shell_echo(sh->buffer);
		
		/* exec_cmd reports its own errors, like an unknown command */
		exec_cmd(sh->buffer);

		/* A command may have drawn into the window itself, so the prompt comes with a full redraw */
		terminal_invalidate($process->current->term);
		reset_shell(sh);
		return;
	}

	if(uc == backspace)
	{
		if(sh->length < 1)
			return;
		/* Clear the deleted character, the last one, not the 0 after it (#66) */
		sh->length--;
		sh->buffer[sh->length] = 0;
		__shell_show_line(sh);
		return;
	}

	/* Only printable ASCII goes in the command. Arrows, F keys and Ctrl+key are 128 and up (#83) */
	if(uc < 32 || uc > 126)
		return;

	/* Room for the terminating 0 */
	if(sh->length == SHELL_LINE_MAX - 1)
		return;

	sh->buffer[sh->length] = uc;
	sh->length++;
	__shell_show_line(sh);
}

/**
 * Types what was pasted into the terminal, as if it was typed: each newline runs the line so far, as Enter does. A
 * command that starts a program reading lines leaves the rest for the program, which takes it first. Characters
 * between newlines go into the line together, and the line is shown once for them.
 */
static void __shell_type_pending(struct shell* sh)
{
	struct terminal* term = $process->current->term;
	bool_t shown = true;
	int c;
	while(!sh->exiting && (c = terminal_take_pending(term)) >= 0){
		if(c == newline){
			shell_put(sh, newline);
			shown = true;
			continue;
		}

		/* Room for the terminating 0 */
		if(sh->length < SHELL_LINE_MAX - 1){
			sh->buffer[sh->length++] = c;
			sh->buffer[sh->length] = 0;
			shown = false;
		}
	}

	if(!shown){
		__shell_show_line(sh);
	}
}

#include <gfx/api.h>

#define CLEANUP_FUNCTION __attribute__((cleanup(cleanup_function)))
void cleanup_function(int** ptr)
{
    dbgprintf("Cleaning up...\n");
    kfree(*ptr);
}

void testfn()
{
	dbgprintf("Testfn...\n");
	CLEANUP_FUNCTION int* ptr = kalloc(sizeof(int) * 50);
}

int c_test = 0;
void __kthread_entry shell(int argc, char* argv[])
{
	dbgprintf("shell is running %d!\n", __cli_cnt);
	//testfn();
	struct window* window = gfx_new_window(SHELL_WIDTH, SHELL_HEIGHT, GFX_IS_RESIZABLE);
	if(window == NULL){
		warningf("Failed to create window for shell");
		return;
	}
	dbgprintf("shell: window 0x%x\n", window);

	
	struct terminal* term = terminal_create(TERMINAL_GRAPHICS_MODE);
	term->ops->attach(term);

	/* Freed when the thread exits: closing the window, the exit command, or being killed */
	struct shell* sh = create(struct shell);
	$process->current->local = sh;
	$process->current->local_free = __shell_free;

	kernel_gfx_draw_rectangle($process->current->gfx_window, 0,0, gfx_get_window_width(), gfx_get_window_height(), term->bg_color);
	struct mem_info minfo;
    get_mem_info(&minfo);

	struct unit used = calculate_size_unit(minfo.kernel.used+minfo.permanent.used);
	struct unit total = calculate_size_unit(minfo.kernel.total+minfo.permanent.total);

	twritef("\n");
	twritef("%s\nWelcome %s!\n\n", about_text, $process->current->user->name);
	twritef("Memory: %d%s/%d%s\n", used.size, used.unit, total.size, total.unit);
	twritef("Type 'help' for a list of commands\n");
	terminal_commit();

	reset_shell(sh);

	dbgprintf("shell: entering event loop\n");
	while(1)
	{
		struct gfx_event event;
		gfx_event_loop(&event, GFX_EVENT_BLOCKING);

		switch (event.event){
		case GFX_EVENT_KEYBOARD:
			/* Keys that select, copy and paste the terminal's text don't reach the line. What's pasted is typed now */
			if(terminal_key(term, event.data, event.flags)){
				__shell_type_pending(sh);
				gfx_commit();
				if(sh->exiting){
					kernel_exit();
				}
				break;
			}

			switch (event.data){
			case CTRLC:
				/* As in bash and zsh, the line stays in the output with ^C after it, and a new prompt starts below */
				terminal_hide_input(term);
				twritef("%s%s^C\n", sh->prompt, sh->buffer);
				reset_shell(sh);
				break;
			default:
				shell_put(sh, event.data);
				c_test++;
				gfx_commit();
				if(sh->exiting){
					kernel_exit();
				}
				break;
			}
			break;
		case GFX_EVENT_RESOLUTION:
			/* The text and the input line are laid out again at the new size */
			terminal_commit();
			break;
		case GFX_EVENT_MOUSE_DOWN:
		case GFX_EVENT_MOUSE_MOVE:
		case GFX_EVENT_MOUSE_UP:
			terminal_mouse(term, &event);
			break;
		case GFX_EVENT_EXIT:
			/* The terminal is freed when the last thread using it exits, which may be a program this shell started */
			kernel_exit();
			return;
		default:
			break;
		}
	}
	
	kernel_exit();
}
EXPORT_KTHREAD(shell);

#include <screen.h>

void draw_box(int x, int y, int width, int height, uint8_t border_color) {
    // Extended ASCII characters for double line box drawing
    unsigned char top_left = 201;     // '╔'
    unsigned char top_right = 187;    // '╗'
    unsigned char bottom_left = 200;  // '╚'
    unsigned char bottom_right = 188; // '╝'
    unsigned char horizontal = 205;   // '═'
    unsigned char vertical = 186;     // '║'

    // Draw corners
    scrput(x, y, top_left, border_color);
    scrput(x + width - 1, y, top_right, border_color);
    scrput(x, y + height - 1, bottom_left, border_color);
    scrput(x + width - 1, y + height - 1, bottom_right, border_color);

    // Draw top and bottom borders with connectors
    for (int i = x + 1; i < x + width - 1; ++i) {
        scrput(i, y, horizontal, border_color); // Top border
        scrput(i, y + height - 1, horizontal, border_color); // Bottom border
    }

    // Draw left, right borders, and connectors
    for (int i = y + 1; i < y + height - 1; ++i) {
        scrput(x, i, vertical, border_color); // Left border
        scrput(x + width - 1, i, vertical, border_color); // Right border
    }
}

static int __textshell_reset_box()
{
	ubyte_t color = VGA_COLOR_WHITE | VGA_COLOR_BLUE << 4;
	draw_box(0, 1, SCREEN_WIDTH, SCREEN_HEIGHT-1,  color);
	scrput(3, 1, '[', color);
	scrput(4, 1, 'R', VGA_COLOR_WHITE | VGA_COLOR_GREEN << 4);
	scrput(5, 1, ']', color);
	scrwrite(3, SCREEN_HEIGHT-1, "[              ]", color);
	
	/* header, with the current directory, as there's no room for it before the line typed */
	char dir[57];
	char header[72];
	__shell_cwd(dir, sizeof(dir) - 1);
	int dir_len = strlen(dir);
	memcpy(header, " Terminal: ", 11);
	memcpy(header + 11, dir, dir_len);
	memcpy(header + 11 + dir_len, " ", 2);
	scrwrite(40-(strlen(header)/2) , 1, header, color);

	scr_set_cursor(3, SCREEN_HEIGHT-1);
	return 0;
}


/* Reads a line into name, without its newline: an empty name for end of input */
static void __textshell_read_name(struct terminal* term, char* name, int size)
{
	int length = term->ops->scan(term, (ubyte_t *)name, size);
	if(length < 0) length = 0;
	if(length > 0 && name[length-1] == '\n') length--;
	name[length] = 0;
}

static int textshell_login(struct terminal* term)
{
	struct usermanager* usrman = $services->usermanager;

	char* logon = kernel_config_get_value("system", "logon");
	if(logon != NULL){
		if(strcmp(logon, "disabled") == 0){
			char* user = kernel_config_get_value("system", "user");
			if(user != NULL){
				struct user* u = usrman->ops->get(usrman, user);
				if(u != NULL){
					$process->current->user = u;
					return 0;
				}
			}
		}
	}

	char username[32];
	char password[32];

	while(1){
		/* scan ends the line on screen, and in what it reads, which the names don't include */
		twritef("Username: ");
		term->ops->commit(term);
		__textshell_read_name(term, username, 32);
		
		twritef("Password: ");
		term->ops->commit(term);
		__textshell_read_name(term, password, 32);

		struct user* user =  usrman->ops->authenticate(usrman, username, password);
		if(user == NULL){
			twritef("Invalid username or password\n");
			term->ops->commit(term);
			continue;
		}

		$process->current->user = user;
		return 0;
	}

	return -1;
}

static byte_t* text_shell_buffer = NULL;
static void __kthread_entry textshell()
{
	ubyte_t c;
	short x = 0;

	struct terminal* term = terminal_create(TERMINAL_TEXT_MODE);
	term->ops->attach(term);

	/* Zeroed, since a line is read up to its first zero byte */
	text_shell_buffer = kcalloc(1024);
	if(text_shell_buffer == NULL){
		warningf("Failed to allocate text shell buffer");
		return;
	}


	struct mem_info minfo;
    get_mem_info(&minfo);

	struct unit used = calculate_size_unit(minfo.kernel.used+minfo.permanent.used);
	struct unit total = calculate_size_unit(minfo.kernel.total+minfo.permanent.total);

	twritef("\n");
	twritef("%s\n", about_text);
	__textshell_reset_box();
	textshell_login(term);


	twritef("Memory: %d%s/%d%s\n", used.size, used.unit, total.size, total.unit);
	twritef("Type 'help' for a list of commands\n");
	term->ops->commit(term);
	while (1){
		scrwrite(0, 0, "                              RetrOS 32 - Textmode                              ", VGA_COLOR_BLUE | VGA_COLOR_LIGHT_GREY << 4);
		c = scr_keyboard_get(1);
		if(c == 0) continue;

		if(c == '\b'){
			if(x == 0) continue;
			x--;
			scrput(4+x, SCREEN_HEIGHT-1, ' ', VGA_COLOR_WHITE | VGA_COLOR_BLUE << 4);
			text_shell_buffer[x] = 0;
			scr_set_cursor(4+x, SCREEN_HEIGHT-1);
			continue;
		}

		if(c == '\n'){
			x = 0;
			/* After output that didn't end its line, the command starts a row of its own */
			terminal_end_line(term);
			__shell_echo((char*)text_shell_buffer);

			exec_cmd(text_shell_buffer);

			memset(text_shell_buffer, 0, 1024);
			term->ops->commit(term);
			__textshell_reset_box();
			continue;
		}

		/**
		 * The line is typed on the frame's bottom row, from column 4 to the right border at column
		 * SCREEN_WIDTH-1, so it holds 75 characters. Ignore keys past that; Enter and backspace still work (#44).
		 */
		if(4+x >= SCREEN_WIDTH-1){
			continue;
		}

		/* Only printable ASCII goes in the command. Arrows, F keys and Ctrl+key are 128 and up (#83) */
		if(c < 32 || c > 126){
			continue;
		}

		scrput(4+x, SCREEN_HEIGHT-1, c, VGA_COLOR_WHITE);
		text_shell_buffer[x] = c;
		x++;
		scr_set_cursor(4+x, SCREEN_HEIGHT-1);
	}
}
EXPORT_KTHREAD(textshell);
