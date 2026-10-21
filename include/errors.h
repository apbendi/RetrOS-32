#ifndef EDD28A84_235C_41BD_B175_C5224DEECC55
#define EDD28A84_235C_41BD_B175_C5224DEECC55

/* For the macros below, which only the kernel's C uses. Apps include this for the codes and their messages. */
#ifndef __cplusplus
#include <serial.h>
#include <kutils.h>
#endif

/* error code from errors.h */
typedef int error_t;

enum errors {
    ERROR_OK,
    ERROR_UNKNOWN,
    ERROR_NULL_POINTER,
    ERROR_INVALID_ARGUMENTS,
    ERROR_WORK_QUEUE_FULL,
    ERROR_ALLOC,
    ERROR_INDEX,
    ERROR_FILE_NOT_FOUND,
    ERROR_PCB_FULL,
    ERROR_KTHREAD_CREATE,
    ERROR_KTHREAD_START,
    ERROR_INVALID_SOCKET,
    ERROR_INVALID_SOCKET_TYPE,
    ERROR_MSS_SIZE,
    ERROR_RBUFFER_FULL,
    ERROR_RBUFFER_EMPTY,
    ERROR_PCB_NULL,
    ERROR_PCB_QUEUE_NULL,
    ERROR_PCB_QUEUE_CREATE,
    ERROR_PCB_QUEUE_EMPTY,
    ERROR_SCHED_EXISTS,
    ERROR_SCHED_INVALID,
    ERROR_WINDOW_NOT_FOUND,
    ERROR_OPS_CORRUPTED,
    ERROR_OUT_OF_MEMORY,
    ERROR_ACCESS_DENIED,
    ERROR_TIMEOUT,
    ERROR_FILE_TOO_LARGE,
    ERROR_PATH_NOT_FOUND,
    ERROR_FILE_EXISTS,
    ERROR_IS_DIRECTORY,
    ERROR_NOT_DIRECTORY,
    ERROR_DIRECTORY_NOT_EMPTY,
    ERROR_NO_SPACE,
    ERROR_INVALID_NAME,
    ERROR_DISK,
    ERROR_NO_FILESYSTEM,
    ERROR_UNTERMINATED_QUOTE,
    ERROR_ARGUMENTS_TOO_LONG,
    ERROR_MOVE_INTO_ITSELF,
};

#ifdef __cplusplus
extern "C" {
#endif

/* The message for an error code, negative as functions return them, or positive. Apps get it from libcore. */
char* error_get_string(error_t err);

#ifdef __cplusplus
}
#endif

#define RETURN_ON_ERR(exp) do{int ret; ret = exp; if(ret < 0){dbgprintf("WARNING: %s\n", error_get_string(ret)); return ret;}}while (0);
#define PANIC_ON_ERR(exp) do{int ret; ret = exp; if(ret < 0){kernel_panic(error_get_string(ret));}}while (0);
#define ERR_ON_NULL(exp) do{if(exp == NULL){dbgprintf("WARNING: %s\n", error_get_string(-ERROR_NULL_POINTER)); return -ERROR_NULL_POINTER;}}while (0);
#define ERR_ON_NULL_PTR(exp) do{if(exp == NULL){dbgprintf("WARNING: %s\n", error_get_string(-ERROR_NULL_POINTER)); return NULL;}}while (0);

#endif /* EDD28A84_235C_41BD_B175_C5224DEECC55 */
