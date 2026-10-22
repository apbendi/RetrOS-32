/* rename(), with POSIX's meaning: an existing file at the new name is replaced. RetrOS refuses that, so it's removed
 * first, which isn't atomic. musl's rename() makes a SYS_rename system call, which src/linux.c sends here. */
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <sys/stat.h>
#include "retros.h"

int __retros_rename(const char* old, const char* new)
{
	int ret = __retros_syscall(SYSCALL_RENAME, (int)old, (int)new, 0);
	if(ret == -ERROR_FILE_EXISTS){
		struct stat from, to;
		if(stat(old, &from) < 0 || stat(new, &to) < 0) return -1;
		if(S_ISDIR(to.st_mode)){
			errno = S_ISDIR(from.st_mode) ? ENOTEMPTY : EISDIR;
			return -1;
		}
		if(S_ISDIR(from.st_mode)){
			errno = ENOTDIR;
			return -1;
		}
		if(unlink(new) < 0) return -1;
		ret = __retros_syscall(SYSCALL_RENAME, (int)old, (int)new, 0);
	}
	return __retros_ret(ret);
}
