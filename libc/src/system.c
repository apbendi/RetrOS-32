/* sysconf(), realpath(), and what RetrOS can't do yet: mprotect, and replacing a program with exec */
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include "retros.h"

long sysconf(int name)
{
	switch(name){
	case _SC_PAGESIZE: return 4096;
	case _SC_CLK_TCK: return 1000;
	case _SC_OPEN_MAX: return FS_MAX_PROCESS_FILES;
	case _SC_NPROCESSORS_CONF:
	case _SC_NPROCESSORS_ONLN: return 1;
	default:
		errno = EINVAL;
		return -1;
	}
}

/**
 * The absolute path of path, without "." and ".." or repeated slashes. RetrOS has no links to follow. Like the
 * kernel's own normalizing (fs.c), ".." at the root stays at the root. The path must exist.
 */
char* realpath(const char* restrict path, char* restrict resolved)
{
	char out[FS_PATH_MAX];
	size_t len = 0;

	if(path == 0){
		errno = EINVAL;
		return 0;
	}
	if(path[0] != '/'){
		if(!getcwd(out, sizeof(out))) return 0;
		len = strlen(out);
		if(len == 1) len = 0;
	}

	for (const char* p = path; *p;){
		while(*p == '/') p++;
		const char* end = p;
		while(*end && *end != '/') end++;
		size_t n = end - p;
		if(n == 0 || (n == 1 && p[0] == '.')){
			/* nothing */
		} else if(n == 2 && p[0] == '.' && p[1] == '.'){
			while(len > 0 && out[len - 1] != '/') len--;
			if(len > 0) len--;
		} else {
			if(len + 1 + n + 1 > sizeof(out)){
				errno = ENAMETOOLONG;
				return 0;
			}
			out[len++] = '/';
			memcpy(out + len, p, n);
			len += n;
		}
		p = end;
	}
	if(len == 0) out[len++] = '/';
	out[len] = 0;

	struct stat st;
	if(stat(out, &st) < 0) return 0;

	if(resolved == 0) return strdup(out);
	memcpy(resolved, out, len + 1);
	return resolved;
}

/* RetrOS can't make memory read-only or non-executable, so there's nothing to change. tcc -run calls it. */
int mprotect(void* addr, size_t len, int prot)
{
	(void)addr;
	(void)len;
	(void)prot;
	return 0;
}

/* RetrOS can't replace a running program with another */
int execve(const char* path, char* const argv[], char* const envp[])
{
	(void)path;
	(void)argv;
	(void)envp;
	errno = ENOSYS;
	return -1;
}

int execv(const char* path, char* const argv[])
{
	return execve(path, argv, 0);
}

int execvp(const char* file, char* const argv[])
{
	return execve(file, argv, 0);
}
