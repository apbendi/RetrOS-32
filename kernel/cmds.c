/**
 * @file cmds.c
 * @author Joe Bayer (joexbayer)
 * @brief All commands for the kernel
 * @version 0.1
 * @date 2023-12-24
 * 
 * Commands can be executed using exec_cmd(char *cmd)
 * Take in argc and argv, and can be run in parallel.
 * 
 * @copyright Copyright (c) 2023
 */

#include <kutils.h>
#include <kernel.h>
#include <ksyms.h>
#include <terminal.h>
#include <memory.h>
#include <fs/fs.h>
#include <work.h>
#include <conf.h>
#include <gfx/theme.h>
#include <kevents.h>
#include <lib/lz.h>
#include <timefmt.h>
#include <errors.h>

#define COMMAND(name, func) \
	int name\
		func\
	EXPORT_KSYMBOL(name);

static int __read_file(char* filename, char** buffer){
    int fd = fs_open(filename, FS_FILE_FLAG_READ);
    if(fd < 0){
        twritef("Failed to open file %s\n", filename);
        return -1;
    }

    int size = fs_size(fd);
    *buffer = kalloc(size);
    int len = fs_read(fd, *buffer, size);
    fs_close(fd);
    if(len < 0){
        twritef("Failed to read file %s\n", filename);
        return -1;
    }

    return len;
}

static int __write_file(char* filename, char* buffer, int size){
    int fd = fs_open(filename, FS_FILE_FLAG_CREATE | FS_FILE_FLAG_WRITE | FS_FILE_FLAG_TRUNCATE);
    if(fd < 0){
        twritef("Failed to open file %s\n", filename);
        return -1;
    }

    int len = fs_write(fd, buffer, size);
    fs_close(fd);
    if(len < 0){
        twritef("Failed to write file %s\n", filename);
        return -1;
    }

    return len;
}


/**
 * @brief Create a file or directory
 * Specified by second argument "file" or "dir"
 */
static int new(int argc, char* argv[])
{
    if(argc < 3) {
        twritef("Usage: new [file, dir] <name>\n");
        return 1;
    }

    if(strcmp(argv[1], "file") == 0) {

        int fd = fs_open(argv[2], FS_FILE_FLAG_CREATE);
        if(fd < 0) {
            twritef("Failed to create file %s\n", argv[2]);
            return 1;
        }
        fs_close(fd);

    } else if(strcmp(argv[1], "dir") == 0) {

        twritef("Currently unsupported.\n");
    
    } else {
        twritef("Usage: new <file, dir> <name>\n");
        return 1;
    }

    return 0;
}
EXPORT_KSYMBOL(new);

/* Prints why a file system command failed on path, e.g. "rm: /bin: Is a directory." */
static void __fs_cmd_error(const char* cmd, const char* path, int err)
{
    twritef("%s: %s: %s\n", cmd, path, error_get_string(err));
}

/* Spaces that pad text out to width characters, for lining up columns. twritef has no field widths. */
static const char* __padding(const char* text, int width)
{
    static const char spaces[] = "                ";
    int pad = width - strlen(text);

    if(pad < 0) pad = 0;
    if(pad > (int)sizeof(spaces) - 1) pad = sizeof(spaces) - 1;
    return spaces + (sizeof(spaces) - 1 - pad);
}

/* One line of list: the entry's size, the date and time it was last written, and its name, with "/" for a directory */
static void __list_entry(const struct fs_dirent* entry)
{
    char date[TIME_FORMAT_DATE_MAX + 1] = "";
    char time[TIME_FORMAT_TIME_MAX + 1] = "";
    if(entry->written.centuary != 0){
        time_format_date(date, sizeof(date), &entry->written, TIME_DATE_NUMERIC);
        time_format_time(time, sizeof(time), &entry->written, false);
    }

    struct unit size_unit = calculate_size_unit(entry->size);

    /* The time is right-aligned, so 12-hour times line up */
    twritef("%p%s %s%s %s%s  %s%s\n",
        size_unit.size,
        size_unit.unit,
        date, __padding(date, TIME_FORMAT_NUMERIC_DATE_MAX),
        __padding(time, TIME_FORMAT_TIME_MAX), time,
        entry->name,
        entry->attributes & FS_ATTR_DIRECTORY ? "/" : ""
    );
}

/* Lists a directory's entries, or one file's, with their long names, sizes and dates */
static int __list_path(const char* path)
{
    struct fs_dirent entry;
    int ret = fs_stat(path, &entry);
    if(ret < 0){
        __fs_cmd_error("list", path, ret);
        return -1;
    }

    twritef("Size    Date%s %sTime  Name\n", __padding("Date", TIME_FORMAT_NUMERIC_DATE_MAX), __padding("Time", TIME_FORMAT_TIME_MAX));
    if(!(entry.attributes & FS_ATTR_DIRECTORY)){
        __list_entry(&entry);
        return 0;
    }

    int fd = fs_open(path, FS_FILE_FLAG_READ);
    if(fd < 0){
        __fs_cmd_error("list", path, fd);
        return -1;
    }

    int entries = 0;
    while((ret = fs_readdir(fd, &entry)) == 1){
        __list_entry(&entry);
        entries++;
    }
    fs_close(fd);

    twritef("%d directory entries.\n", entries);
    if(ret < 0){
        __fs_cmd_error("list", path, ret);
        return -1;
    }
    return 0;
}

/* Lists each path given, or the current directory */
static int list(int argc, char* argv[])
{
    if(argc < 2) return __list_path(".");

    int failed = 0;
    for (int i = 1; i < argc; i++){
        if(argc > 2) twritef("%s%s:\n", i > 1 ? "\n" : "", argv[i]);
        if(__list_path(argv[i]) < 0) failed = -1;
    }
    return failed;
}
EXPORT_KSYMBOL(list);

static int ls(int argc, char* argv[])
{
    return list(argc, argv);
}
EXPORT_KSYMBOL(ls);

/* Changes the shell's current directory, to the root with no path */
static int cd(int argc, char* argv[])
{
    if(argc > 2){
        twritef("cd: too many arguments. Quote a name with spaces: cd \"my files\"\n");
        return -1;
    }
    const char* path = argc < 2 ? "/" : argv[1];

    int ret = fs_chdir(path);
    if(ret < 0){
        __fs_cmd_error("cd", path, ret);
        return -1;
    }
    return 0;
}
EXPORT_KSYMBOL(cd);

static int pwd(int argc, char* argv[])
{
    char cwd[FS_PATH_MAX];
    if(fs_getcwd(cwd, sizeof(cwd)) < 0) return -1;

    twritef("%s\n", cwd);
    return 0;
}
EXPORT_KSYMBOL(pwd);

/* Runs a file system function on each path, reporting each one that fails */
static int __fs_each_path(const char* cmd, int argc, char* argv[], int (*op)(const char* path))
{
    if(argc < 2){
        twritef("Usage: %s <path> [path ...]\n", cmd);
        return -1;
    }

    int failed = 0;
    for (int i = 1; i < argc; i++){
        int ret = op(argv[i]);
        if(ret < 0){
            __fs_cmd_error(cmd, argv[i], ret);
            failed = -1;
        }
    }
    return failed;
}

static int mkdir(int argc, char* argv[])
{
    return __fs_each_path("mkdir", argc, argv, fs_mkdir);
}
EXPORT_KSYMBOL(mkdir);

/* Deletes files. Directories are for rmdir. */
static int rm(int argc, char* argv[])
{
    return __fs_each_path("rm", argc, argv, fs_unlink);
}
EXPORT_KSYMBOL(rm);

/* Deletes empty directories */
static int rmdir(int argc, char* argv[])
{
    return __fs_each_path("rmdir", argc, argv, fs_rmdir);
}
EXPORT_KSYMBOL(rmdir);

/**
 * Renames or moves a file or directory. A destination that's an existing directory gets the source moved into it,
 * under the same name, as on Unix, unless it's the source itself in another case: mv docs Docs renames it. An
 * existing file isn't replaced.
 */
static int mv(int argc, char* argv[])
{
    if(argc != 3){
        twritef("Usage: mv <from> <to>\n");
        return -1;
    }

    const char* from = argv[1];
    const char* to = argv[2];
    char into[FS_PATH_MAX];

    struct fs_dirent entry;
    if(fs_stat(to, &entry) == 0 && (entry.attributes & FS_ATTR_DIRECTORY) && fs_same_entry(from, to) != 1){
        /* Into the directory, under the source's name as it's stored, whatever case it was typed in */
        int ret = fs_stat(from, &entry);
        if(ret < 0){
            __fs_cmd_error("mv", from, ret);
            return -1;
        }

        int to_len = strlen(to);
        int name_len = strlen(entry.name);
        if(to_len + 1 + name_len + 1 > FS_PATH_MAX){
            __fs_cmd_error("mv", to, -ERROR_INVALID_ARGUMENTS);
            return -1;
        }
        memcpy(into, to, to_len);
        into[to_len] = '/';
        memcpy(into + to_len + 1, entry.name, name_len + 1);
        to = into;
    }

    int ret = fs_rename(from, to);
    if(ret < 0){
        __fs_cmd_error("mv", from, ret);
        return -1;
    }
    return 0;
}
EXPORT_KSYMBOL(mv);

/* prints content of compressed file */
static int lzcat(int argc, char* argv[]){
    if(argc < 2) {
        twritef("Usage: lz <file>\n");
        return 1;
    }
    int8_t* buf;
    int len = __read_file(argv[1], (char**)&buf);

    int8_t* output;
    uint32_t output_size = lz_decompress((uint8_t*)buf, len, (uint8_t**)&output);
    if(output_size == 0) {
        twritef("Failed to decompress file\n");
        return -1;
    }

    twritef("%s\n", output);

    free(output);
    kfree(buf);
    
    return 0;
}
EXPORT_KSYMBOL(lzcat);

static int lz(int argc, char* argv[]){
    if(argc < 3) {
        twritef("Usage: lz <file> <outfile>\n");
        return 1;
    }
    int8_t* buf;
    int len = __read_file(argv[1], (char**)&buf);

    int8_t* output;
    uint32_t output_size = lz_compress((uint8_t*)buf, len, (uint8_t**)&output, 0);
    if(output_size == 0) {
        twritef("Failed to compress file\n");
        return -1;
    }

    int ret = __write_file(argv[2], (char *)output, output_size);
    if(ret < 0) {
        twritef("Failed to write compressed file\n");
        return -1;
    }

    free(output);
    kfree(buf);
    
    return 0;
}
EXPORT_KSYMBOL(lz);

static int color(int argc, char* argv[]){
    if(argc < 2) {
        twritef("Usage: color <text> <background>\n Use hex values\n");
        return 1;
    }

    color_t text = htoi(argv[1]);
    color_t bg = 255;

    if(argc == 3){
        bg = htoi(argv[2]);
    }

    TERM_CONTEXT({
        term->text_color = text;
        term->org_text_color = text;
        if(bg != 255) {
            term->bg_color = bg;
        }
    });

    return 0;
}
EXPORT_KSYMBOL(color);

static int about(){

    struct memory_map* map = memory_map_get();

    /* neofetch style twritef */
    #define RETROS_ASCII_ART_INFO \
    "                  .----.      Kernel: RetrOS-32 0.0.1 alpha\n" \
    "      .---------. | == |      Build Date: " __DATE__ "\n" \
    "      |.-\"\"\"\"\"-.| |----|      Build Time: " __TIME__ "\n" \
    "      ||       || | == |      Memory: %d bytes\n" \
    "      ||RetrOS || |----|      Display: VGA\n" \
    "      |'-.....-'| |::::|                \n" \
    "      `\"\")---(\"\"` |___.|                \n" \
    "     /:::::::::::\" _  \"                \n" \
    "    /:::=======:::\\`\\`\\             \n" \
    "    `\"\"\"\"\"\"\"\"\"\"\"\"\"`  '-'                \n"

    twritef(RETROS_ASCII_ART_INFO, map->total);

    return 0;
}
EXPORT_KSYMBOL(about);

static int help(int argc, char* argv[])
{
    twritef("Available commands:\n\n");
    twritef("Filesystem:      System:      Etc:\n");
    twritef("  ls, list        ps           help\n");
    twritef("  cd, pwd         tree         reset\n");
    twritef("  mkdir, rmdir    kill         about\n");
    twritef("  rm, mv          exec         xxd\n");
    twritef("  new, view       fdisk        bg\n");
    twritef("  file            meminfo      sh\n");
    twritef("Network:          conf         echo\n");
    twritef(" socks            services     cc\n");
    twritef(" ifconfig         admin        color\n");
    twritef(" dns, tcp         clear\n\n");
    twritef("Quote names with spaces: cd \"my files\"\n");
    return 0;
}
EXPORT_KSYMBOL(help);

int joe(int argc, char* argv[])
{
    int a;
    int b;
    int c;

    a = argc;
    b = 2;
    c = a + b;

    return c;
}

static int dump(int argc, char* argv[])
{
    if(argc < 2) {
        twritef("Usage: dump <address>\n");
        return 1;
    }

    void* addr = (void*) htoi(argv[1]);
    hexdump(addr, 256);

    return 0;
}
EXPORT_KSYMBOL(dump);

static int dumpfn(int argc, char* argv[])
{
    if(argc < 2) {
        twritef("Usage: dumpfn <function>\n");
        return 1;
    }

    uintptr_t addr = ksyms_resolve_symbol(argv[1]);
    if(addr == 0) {
        twritef("Symbol not found\n");
        return 1;
    }

    int sz = kfunc_size((void (*)()) addr);
    twritef("Dumping %s (addr: 0x%x, size: %d)\n", argv[1], addr, sz);
    hexdump((void*) addr, sz);

    return 0;
}
EXPORT_KSYMBOL(dumpfn);

static int regs(int argc, char* argv[])
{
    struct registers_dump regs;
    kregisters(&regs);

    twritef("Registers:\n");
    twritef("EAX: %x\n", regs.eax);
    twritef("EBX: %x\n", regs.ebx);
    twritef("ECX: %x\n", regs.ecx);
    twritef("EDX: %x\n", regs.edx);
    twritef("ESI: %x\n", regs.esi);
    twritef("EDI: %x\n", regs.edi);
    twritef("EBP: %x\n", regs.ebp);
    twritef("ESP: %x\n", regs.esp);
    twritef("EIP: %x\n", regs.eip);
    twritef("EFLAGS: %x\n", regs.eflags);
    twritef("CR0: %x\n", regs.cr0);
    twritef("CR2: %x\n", regs.cr2);
    twritef("CR3: %x\n", regs.cr3);
    twritef("CR4: %x\n", regs.cr4);
    twritef("GS: %x\n", regs.gs);
    
    return 0;
}
EXPORT_KSYMBOL(regs);

int syms(int args, char* argv[])
{
    ksyms_list();
    return 0;
}
EXPORT_KSYMBOL(syms);

int disas(int argc, char* argv[])
{
    if(argc < 2) {
        twritef("Usage: disassemble <function>\n");
        return 1;
    }

    uintptr_t addr = ksyms_resolve_symbol(argv[1]);
    if(addr == 0) {
        twritef("Symbol not found\n");
        return 1;
    }

    int sz = kfunc_size((void (*)()) addr);
    twritef("Disassembling %s (addr: 0x%x, size: %d)\n", argv[1], addr, sz);
    disassemble((uint8_t*) addr, sz, (void*) addr);

    return 0;
}
EXPORT_KSYMBOL(disas);

int resolve(int argc, char* argv[])
{
    if(argc < 2) {
        twritef("Usage: resolve <symbol>\n");
        return 1;
    }

    uintptr_t addr = ksyms_resolve_symbol(argv[1]);
    if(addr == 0) {
        twritef("Symbol not found\n");
        return 1;
    }

    twritef("Symbol %s resolved to 0x%x\n", argv[1], addr);
    return 0;
}
EXPORT_KSYMBOL(resolve);

static int kevents(int argc, char* argv[])
{
    if(IS_AUTHORIZED(ADMIN_FULL_ACCESS) == 0) {
        twritef("You are not authorized to use this command\n");
        return 1;
    }

    if(argc < 2) {
        twritef("Usage: kevents <list>\n");
        return 1;
    }

    if(strcmp(argv[1], "list") == 0) {
        $services->kevents->ops->list($services->kevents);
    } else {
        twritef("Usage: kevents <list>\n");
        return 1;
    }

    return 0;
}
EXPORT_KSYMBOL(kevents);


/* Prints a file's text */
static int view(int argc, char* argv[]){
    if(argc < 2) {
        twritef("Usage: view <path>\n");
        return 1;
    }

    struct fs_dirent entry;
    int ret = fs_stat(argv[1], &entry);
    if(ret == 0 && (entry.attributes & FS_ATTR_DIRECTORY)) ret = -ERROR_IS_DIRECTORY;
    if(ret < 0){
        __fs_cmd_error("view", argv[1], ret);
        return -1;
    }

    int fd = fs_open(argv[1], FS_FILE_FLAG_READ);
    if(fd < 0) {
        __fs_cmd_error("view", argv[1], fd);
        return -1;
    }

    int size = fs_size(fd);
    if(size == 0) {
        twritef("File %s is empty\n", argv[1]);
        fs_close(fd);
        return -1;
    }

    /* kalloc_try, so a file bigger than the free memory is an error rather than a panic */
    char* buf = kalloc_try(size + 1);
    if(buf == NULL){
        __fs_cmd_error("view", argv[1], -ERROR_ALLOC);
        fs_close(fd);
        return -1;
    }

    int len = fs_read(fd, buf, size);
    fs_close(fd);
    if(len < 0) {
        __fs_cmd_error("view", argv[1], len);
        kfree(buf);
        return -1;
    }

    /* The text ends where the file does, or at its first zero byte */
    buf[len] = 0;
    twritef("%s\n", buf);

    kfree(buf);
    return 0;
}
EXPORT_KSYMBOL(view);

static int __lower(int c)
{
    return c >= 'A' && c <= 'Z' ? c + ('a' - 'A') : c;
}

/* Whether needle appears in name, ignoring case, as FAT does */
static int __contains(const char* name, const char* needle)
{
    for(int i = 0; name[i] != 0; i++){
        int j = 0;
        while(needle[j] != 0 && __lower(name[i + j]) == __lower(needle[j])) j++;
        if(needle[j] == 0) return 1;
    }
    return needle[0] == 0;
}

/* How many directories deep find goes. Each level keeps a directory open, out of a process's 32 files. */
#define FIND_DEPTH 16

/* One level of find's state, on the heap: a kernel stack doesn't have room for 16 */
struct find_level {
    struct fs_dirent entry;
    char child[FS_PATH_MAX];
};

/* Prints every path under path, a normalized directory, whose name contains needle */
static void __find(const char* path, const char* needle, int depth)
{
    struct find_level* level = kalloc_try(sizeof(struct find_level));
    if(level == NULL){
        __fs_cmd_error("find", path, -ERROR_ALLOC);
        return;
    }

    int fd = fs_open(path, FS_FILE_FLAG_READ);
    if(fd < 0){
        __fs_cmd_error("find", path, fd);
        kfree(level);
        return;
    }

    int path_len = strlen(path);
    while(fs_readdir(fd, &level->entry) == 1){
        int is_directory = level->entry.attributes & FS_ATTR_DIRECTORY;
        int name_len = strlen(level->entry.name);
        if(path_len + 1 + name_len + 1 > FS_PATH_MAX) continue;

        /* The root is "/", and every other directory needs a "/" after it */
        int len = path_len;
        memcpy(level->child, path, path_len);
        if(path_len > 1) level->child[len++] = '/';
        memcpy(level->child + len, level->entry.name, name_len + 1);

        if(__contains(level->entry.name, needle)){
            twritef("%s%s\n", level->child, is_directory ? "/" : "");
        }
        if(is_directory && depth < FIND_DEPTH){
            __find(level->child, needle, depth + 1);
        }
    }
    fs_close(fd);
    kfree(level);
}

/* Prints the path of every file and directory whose name contains the text, ignoring case, under a directory */
static int find(int argc, char* argv[]){    
    if(argc < 2 || argc > 3) {
        twritef("Usage: find <text> [directory]\n");
        return 1;
    }

    const char* start = argc == 3 ? argv[2] : ".";
    char path[FS_PATH_MAX];
    if(fs_path_normalize(start, path) < 0){
        __fs_cmd_error("find", start, -ERROR_INVALID_ARGUMENTS);
        return -1;
    }

    struct fs_dirent entry;
    int ret = fs_stat(path, &entry);
    if(ret == 0 && !(entry.attributes & FS_ATTR_DIRECTORY)) ret = -ERROR_NOT_DIRECTORY;
    if(ret < 0){
        __fs_cmd_error("find", start, ret);
        return -1;
    }

    __find(path, argv[1], 0);
    return 0;
}
EXPORT_KSYMBOL(find);

static int file(int argc, char* argv[]){
    if(argc < 2) {
        twritef("Usage: file <path>\n");
        return 1;
    }

    int fd = fs_open(argv[1], FS_FILE_FLAG_READ);
    if(fd < 0) {
        twritef("Failed to open file %s\n", argv[1]);
        return -1;
    }

    twritef("File %s:\n", argv[1]);
    twritef("Size: %d\n", fs_size(fd));

    fs_close(fd);
    return 0;
}
EXPORT_KSYMBOL(file);

#include <net/socket.h>
#include <net/net.h>
#include <net/ipv4.h>
#include <net/utils.h>

/**
 * @brief Part of the TCP client
 * 
 * @param argc Should always be 1
 * @param argv Contains socket as int
 * @return int 
 */
static void __kthread_entry __tcp_reader(int argc, char **argv)
{
    if(argc != 1){
        dbgprintf("Invalid arguments\n");
        return;
    }

    struct sock* socket = sock_get(atoi(argv[0]));
    if(socket == NULL) {
        dbgprintf("Invalid socket\n");
        return;
    }

    dbgprintf("TCP reader started\n");

    int ret;
    char buffer[1024];
    while(1){
        ret = kernel_recv(socket, buffer, 1024, 0);
        if(ret < 0){
            twritef("Unable to recv TCP packet\n");
            break;
        }

        buffer[ret] = 0;
        twritef("%s", buffer);

        $process->current->term->ops->commit($process->current->term);
    }

    sock_deref(socket);
    return;
}

static int tcp(int argc, char *argv[])
{
    if(argc < 3) {
        twritef("Usage: tcp <ip,domain> <port>\n");
        return 1;
    }

    struct sock* socket = kernel_socket_create(AF_INET, SOCK_STREAM, 0);
    if(socket == NULL) {
        twritef("Failed to create socket\n");
        return 1;
    }

    uint32_t ip = net_is_ipv4(argv[1]) ? ip_to_int(argv[1]) : htonl(gethostname(argv[1]));
    if(ip == (uint32_t)(-1)){
        twritef("Unable to resolve %s\n", argv[1]);
        return 1;
    }

    struct sockaddr_in dest_addr;
    dest_addr.sin_addr.s_addr = ip == LOOPBACK_IP ? htonl(ip) : ip;
    dest_addr.sin_port = htons(atoi(argv[2]));
    dest_addr.sin_family = AF_INET;

    int con = kernel_connect(socket, (struct sockaddr*) &dest_addr, sizeof(dest_addr));
    if(con < 0) {
        twritef("Unable to connect to %s:%s\n", argv[1], argv[2]);
        return 1;
    }

    twritef("Connected to %s:%s\n", argv[1], argv[2]);
    $process->current->term->ops->commit($process->current->term); 

    char socket_str[10] = {0};
    itoa(socket->socket, socket_str);

    pid_t reader = pcb_create_kthread(__tcp_reader, "tcp_reader", 1, (char**)&socket_str);
    
    int ret;
    char* buffer = kalloc(1024);
    while(1){
        ret = $process->current->term->ops->scan($process->current->term, (ubyte_t *)buffer, 255);
        if(ret < 0){
            break;
        }
        ret = kernel_send(socket, buffer, strlen(buffer), 0);
        if(ret < 0){
            twritef("Unable to send TCP packet\n");
            break;
        }
    }

    pcb_kill(reader);

    kernel_sock_close(socket);
    return 0;
}
EXPORT_KSYMBOL(tcp);

static int conf(int argc, char *argv[])
{
    int ret;

    if(IS_AUTHORIZED(ADMIN_FULL_ACCESS) == 0) {
        twritef("You are not authorized to use this command\n");
        return 1;
    }

    if(argc < 2) {
        twritef("Usage: conf <load, get, list>\n");
        return 1;
    }

    if(strcmp(argv[1], "list") == 0) {
        config_list();
    } else if(strcmp(argv[1], "load") == 0) {
        if(argc < 3) {
            twritef("Usage: conf load <filename>\n");
            return 1;
        }
        ret = kernel_config_load(argv[2]);
        if(ret < 0) {
            twritef("Failed to load config file: %d\n", ret);
            return 1;
        }
        twritef("Loaded config file %s\n", argv[2]);

    } else if(strcmp(argv[1], "get") == 0) {
        if(argc < 4) {
            twritef("Usage: conf get <section> <value>\n");
            return 1;
        }
        char* value = kernel_config_get_value(argv[2], argv[3]);
        if(value == NULL) {
            twritef("Value not found\n");
            return 1;
        }
        twritef("%s\n", value);
    } else {
        twritef("Usage: conf <load, get, list>\n");
        return 1;
    }

    return 0;
}
EXPORT_KSYMBOL(conf);

static int clear(){
    $process->current->term->ops->reset($process->current->term);
    return 0;
}
EXPORT_KSYMBOL(clear);

static int panic(int argc, char *argv[])
{
    if(argc < 2) {
        twritef("Usage: panic <message>\n");
        return 1;
    }

    kernel_panic(argv[1]);
    return 0;
}
EXPORT_KSYMBOL(panic);

static int services(int argc, char *argv[])
{
    if(argc < 2) {
        twritef("Usage: services <list, start, stop>\n");
        return 1;
    }

    if(strcmp(argv[1], "list") == 0) {
        twritef("Services:\n");
        
        twritef("UserManager:    %s\n", $services->usermanager != NULL ? "running" : "stopped");
        twritef("Scheduler:      %s\n", $services->scheduler != NULL ? "running" : "stopped");
        twritef("NetworkManager: %s\n", $services->networking != NULL ? "running" : "stopped");
        return 0;   
    }

    if(strcmp(argv[1], "start") == 0) {
        
        if(IS_AUTHORIZED(ADMIN_FULL_ACCESS) == 0) {
            twritef("You are not authorized to use this command\n");
            return 1;
        }

        if(argc < 3) {
            twritef("Usage: services start <service>\n");
            return 1;
        }


        return 1;
    }

    return 0;
}
EXPORT_KSYMBOL(services);

static int reboot(){
    twritef("Rebooting...\n");
    system_reboot();
    return 0;
}
EXPORT_KSYMBOL(reboot);

/* Process management */

/* System management */

/* Network management */

/* Administration management */

/* package? */
