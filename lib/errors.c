/**
 * @file errors.c
 * @author Joe Bayer (joexbayer)
 * @brief Error handling.
 * @version 0.1
 * @date 2024-01-10
 * 
 * @copyright Copyright (c) 2024
 * 
 */

#include <errors.h>
#include <assert.h>

static char* error_string[] = {
    "No error.",
    "Unspecified generic error.",
    "Null pointer.",
    "Invalid arguments.",
    "Work queue full.",
    "Unable to allocate memory.",
    "Invalid index.",
    "File not found.",
    "No more free PCBs.",
    "Unable to create kernel thread.",
    "Unable to start kernel thread.",
    "Invalid socket.",
    "Invalid socket type.",
    "Invalid size of packet (probably too high).",
    "Ring buffer full.",
    "Ring buffer empty.",
    "PCB is NULL.",
    "PCB queue is NULL.",
    "Unable to create PCB queue.",
    "PCB queue is empty.",
    "Scheduler already exists and is initiated.",
    "Invalid scheduler, probably not initiated.",
    "Window not found.",
    "Window operations are corrupted.",
    "Out of memory.",
    "Access denied.",
    "Timed out.",
    "File too large.",
    "Directory not found.",
    "Already exists.",
    "Is a directory.",
    "Not a directory.",
    "Directory not empty.",
    "No space left in the directory or on the disk.",
    "Invalid file name.",
    "Disk error.",
    "No file system.",
    "Missing closing quote.",
    "Arguments too long."
};

char* error_get_string(error_t err)
{
    assert(err < 0);
    return error_string[-err];
}
