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
#define SHELL_POSITION shell_height-8
#define SHELL_MAX_SIZE SHELL_WIDTH/2

void __kthread_entry shell(int argc, char* argv[]);

/* TODO: Move this into a shell struct */
static uint16_t shell_width = SHELL_WIDTH;
static uint16_t shell_height = SHELL_HEIGHT;
static uint8_t shell_column = 0;
static char previous_shell_buffer[SHELL_MAX_SIZE];
static char shell_buffer[SHELL_MAX_SIZE];
static uint8_t shell_buffer_length = 0;

static const char newline = '\n';
static const char backspace = '\b';

/* The most of the current directory the GUI shell's prompt shows, so a deep directory leaves room to type */
#define SHELL_PROMPT_DIR_MAX 20
static char shell_prompt[SHELL_PROMPT_DIR_MAX + 16];
static char* about_text = "\nRetrOS-32 - 32bit Operating System\n    " KERNEL_RELEASE " " KERNEL_VERSION " - " KERNEL_DATE "\n";

/*
 *	IMPLEMENTATIONS
 */
void shell_clear()
{
	//struct gfx_theme* theme = kernel_gfx_current_theme();
	kernel_gfx_draw_rectangle($process->current->gfx_window, 0, SHELL_POSITION, gfx_get_window_width(), 8, $process->current->term->bg_color);
}

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

void reset_shell()
{
	struct terminal* term = $process->current->term;

	/* The prompt shows the current directory, which a command may have changed */
	char dir[SHELL_PROMPT_DIR_MAX + 1];
	__shell_cwd(dir, SHELL_PROMPT_DIR_MAX);
	int dir_len = strlen(dir);
	memcpy(shell_prompt, "kernel:", 7);
	memcpy(shell_prompt + 7, dir, dir_len);
	memcpy(shell_prompt + 7 + dir_len, ">", 2);

	shell_clear();
	memset(&shell_buffer, 0, SHELL_MAX_SIZE);
	shell_column = strlen(shell_prompt)+1;
	shell_buffer_length = 0;
	kernel_gfx_draw_text($process->current->gfx_window, 0, SHELL_POSITION, shell_prompt, term->text_color);
	shell_column += 1;
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


void xxd(int argc, char* argv[])
{
	if(argc == 1){
		twritef("usage: xxd <file>\n");
		return;
	}

	int inode = fs_open(argv[1], FS_FILE_FLAG_READ);
	if(inode < 0){
		twritef("File %s not found.\n", argv[1]);
		return;
	}

	char* buf = kalloc(32*1024);
	int ret = fs_read(inode, buf, 32*1024);
	if(ret < 0){
		fs_close(inode);
		kfree(buf);
		twritef("Error reading file\n");
		return;
	}
	
	hexdump(buf, ret);
	
	fs_close(inode);
	kfree(buf);

	return;
}
EXPORT_KSYMBOL(xxd);

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

	pid = pcb_create_process(argv[idx], argc-idx, &argv[idx], 0 /* PCB_FLAG_KERNEL */);
	if(pid > 0){
		if(!kthread_as_deamon) pcb_await(pid);
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

void kill(int argc, char* argv[])
{
	int id = atoi(argv[1]);
	pcb_kill(id);
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
	kernel_gfx_draw_rectangle($process->current->gfx_window, 0,0, gfx_get_window_width(), gfx_get_window_height(), $process->current->term->bg_color);
	$process->current->term->ops->reset($process->current->term);
	reset_shell();
}
EXPORT_KSYMBOL(reset);


/**
 * @brief Puts a character c into the shell line 
 * at correct position. Also detects enter.
 * 
 * @param c character to put to screen.
 */
void shell_put(unsigned char c)
{
	unsigned char uc = c;
	struct terminal* term = $process->current->term;

	if(uc == ARROW_UP){
		/* Only the characters: replaying the terminating 0 would end the command there (#67) */
		int len = strlen(previous_shell_buffer);
		for (int i = 0; i < len; i++){
			shell_put(previous_shell_buffer[i]);
		}
		return;
	}

	if(uc == newline){
		memcpy(previous_shell_buffer, shell_buffer, strlen(shell_buffer)+1);
		__shell_echo(shell_buffer);
		
		/* exec_cmd reports its own errors, like an unknown command */
		exec_cmd(shell_buffer);
		
		terminal_commit();
		reset_shell();
		return;
	}

	if(uc == backspace)
	{
		if(shell_buffer_length < 1)
			return;
		shell_column -= 1;
		kernel_gfx_draw_rectangle($process->current->gfx_window, shell_column*8, SHELL_POSITION, 8, 8, term->bg_color);
		gfx_commit();
		/* Clear the deleted character, the last one, not the 0 after it (#66) */
		shell_buffer_length--;
		shell_buffer[shell_buffer_length] = 0;
		return;
	}

	/* Only printable ASCII goes in the command. Arrows, F keys and Ctrl+key are 128 and up (#83) */
	if(uc < 32 || uc > 126)
		return;

	if(shell_column == SHELL_MAX_SIZE)
		return;

	kernel_gfx_draw_char($process->current->gfx_window, shell_column*8, SHELL_POSITION, uc, term->text_color);
	gfx_commit();
	shell_buffer[shell_buffer_length] = uc;
	shell_buffer_length++;
	shell_column++;
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

	reset_shell();

	dbgprintf("shell: entering event loop\n");
	while(1)
	{
		struct gfx_event event;
		gfx_event_loop(&event, GFX_EVENT_BLOCKING);

		switch (event.event){
		case GFX_EVENT_KEYBOARD:
			switch (event.data){
			case CTRLC:
				reset_shell();
				gfx_commit();
				break;
			default:
				shell_put(event.data);
				c_test++;
				gfx_commit();
				break;
			}
			break;
		case GFX_EVENT_RESOLUTION:
			shell_height  = event.data2;
			shell_width = event.data;
			terminal_commit();
			reset_shell();
			break;
		case GFX_EVENT_EXIT:{
				terminal_destroy(term);
				kernel_exit();
			}
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
		twritef("Username: ");
		term->ops->commit(term);
		term->ops->scan(term, (ubyte_t *)username, 32);
		twritef("\n");
		
		twritef("Password: ");
		term->ops->commit(term);
		term->ops->scan(term, (ubyte_t *)password, 32);
		twritef("\n");

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
