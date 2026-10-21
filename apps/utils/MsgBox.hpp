/**
 * @file MsgBox.hpp
 * @author Joe Bayer (joexbayer)
 * @brief A message box widget
 * @version 0.1
 * @date 2024-01-10
 * 
 * @copyright Copyright (c) 2024
 * 
 */

#ifndef MSGBOX_HPP
#define MSGBOX_HPP

#include <utils/Graphics.hpp>
#include <lib/syscall.h>
#include <utils/Thread.hpp>
#include <syscall_helper.h>
#include <libc.h>

typedef enum _msgbox_types {
    MSGBOX_OK,
    MSGBOX_OK_CANCEL,
    MSGBOX_YES_NO,
    MSGBOX_YES_NO_CANCEL
} MsgBoxType;

typedef enum _msgbox_results {
    MSGBOX_RESULT_OK,
    MSGBOX_CANCEL,
    MSGBOX_YES,
    MSGBOX_NO
} MsgBoxResult;

/* Room for a 21-character message, and three 60-pixel buttons, inside the window's 8-pixel borders */
#define MSGBOX_WIDTH 210
#define MSGBOX_HEIGHT 100

#define MSGBOX_BUTTON_Y 70
#define MSGBOX_BUTTON_WIDTH 60
#define MSGBOX_BUTTON_HEIGHT 20

/* The longest title or message a box shows, with its terminating 0 */
#define MSGBOX_TEXT_MAX 64

/**
 * Everything the box's thread uses. It's on the heap because a thread can't see its parent's stack: it gets new
 * stack pages at the same address, so a pointer to a local variable points into the thread's own stack there (#91).
 */
struct MsgBoxShared {
    char title[MSGBOX_TEXT_MAX];
    char message[MSGBOX_TEXT_MAX];
    MsgBoxType type;
    volatile MsgBoxResult result;
};

/* Forward declaration of threading function */
void __msgbox_thread_fn(void* args);

/**
 * @brief A message box widget
 * @details Shows a message in a window of its own, with buttons for the answers its type allows, and waits for
 * one. The Editor asks with it whether to save changes.
 *
 * @param title The title of the message box
 * @param message The message to display
 * @param type The type of message box to display
 * @warning Assumes that a window is present.
 */
class MsgBox {
public:
    MsgBox(const char* title, const char* message, MsgBoxType type) {
        mType = type;
        mTitle = title;
        mMessage = message;
    }

    /**
     * Shows the box and waits for its thread to end. A box that couldn't be shown, that was closed, or whose
     * thread died answers MSGBOX_CANCEL, so the caller does nothing it wasn't told to.
     */
    MsgBoxResult show() {
        MsgBoxShared* shared = (MsgBoxShared*) malloc(sizeof(MsgBoxShared));
        if (shared == nullptr) {
            return MSGBOX_CANCEL;
        }

        memset(shared, 0, sizeof(MsgBoxShared));
        copyText(shared->title, mTitle);
        copyText(shared->message, mMessage);
        shared->type = mType;
        shared->result = MSGBOX_CANCEL;

        Thread thread(__msgbox_thread_fn, 0);
        int id = thread.start(shared);
        if (id < 0) {
            free(shared);
            return MSGBOX_CANCEL;
        }

        invoke_syscall(SYSCALL_AWAIT_PROCESS, id, 0, 0);

        MsgBoxResult result = shared->result;
        free(shared);
        return result;
    }

private:
    const char* mTitle;
    const char* mMessage;
    MsgBoxType mType;

    static void copyText(char* out, const char* text) {
        int len = 0;
        while (text != nullptr && text[len] != 0 && len < MSGBOX_TEXT_MAX - 1) {
            out[len] = text[len];
            len++;
        }
        out[len] = 0;
    }
};

/* A button: its place in the row (0 to 2), its label, and the answer it gives */
struct MsgBoxButton {
    int slot;
    const char* label;
    MsgBoxResult result;
};

static int __msgbox_button_x(int slot)
{
    return 10 + slot * MSGBOX_BUTTON_WIDTH;
}

/**
 * @brief Thread function for the message box
 * Draws the box and waits for a click on one of its buttons, which it records in the shared result. Closing the
 * box leaves the result at MSGBOX_CANCEL.
 * @param args The box's MsgBoxShared, on the heap
 */
void __msgbox_thread_fn(void* args)
{
    MsgBoxShared* box = static_cast<MsgBoxShared*>(args);

    MsgBoxButton buttons[3];
    int count = 0;
    switch (box->type) {
        case MSGBOX_OK:
            buttons[count++] = { 1, "OK", MSGBOX_RESULT_OK };
            break;
        case MSGBOX_OK_CANCEL:
            buttons[count++] = { 0, "OK", MSGBOX_RESULT_OK };
            buttons[count++] = { 2, "Cancel", MSGBOX_CANCEL };
            break;
        case MSGBOX_YES_NO:
            buttons[count++] = { 0, "Yes", MSGBOX_YES };
            buttons[count++] = { 2, "No", MSGBOX_NO };
            break;
        case MSGBOX_YES_NO_CANCEL:
            buttons[count++] = { 0, "Yes", MSGBOX_YES };
            buttons[count++] = { 1, "No", MSGBOX_NO };
            buttons[count++] = { 2, "Cancel", MSGBOX_CANCEL };
            break;
        default:
            break;
    }

    Window win(MSGBOX_WIDTH, MSGBOX_HEIGHT, box->title, 0);
    win.drawContouredRect(0, 0, MSGBOX_WIDTH, MSGBOX_HEIGHT);

    gfx_draw_format_text(10, 30, COLOR_BLACK, "%s", box->message);

    for (int i = 0; i < count; i++) {
        int x = __msgbox_button_x(buttons[i].slot);
        int label_x = x + (MSGBOX_BUTTON_WIDTH - (int)strlen(buttons[i].label) * 8) / 2;
        win.drawContouredRect(x, MSGBOX_BUTTON_Y, MSGBOX_BUTTON_WIDTH, MSGBOX_BUTTON_HEIGHT);
        gfx_draw_format_text(label_x, MSGBOX_BUTTON_Y + 6, COLOR_BLACK, "%s", buttons[i].label);
    }

    struct gfx_event event;
    while (1){
        int ret = gfx_get_event(&event, GFX_EVENT_BLOCKING);
        if(ret == -1) continue;
        if (event.event == GFX_EVENT_EXIT) return;
        if (event.event != GFX_EVENT_MOUSE) continue;

        for (int i = 0; i < count; i++) {
            int x = __msgbox_button_x(buttons[i].slot);
            if (event.data > x && event.data < x + MSGBOX_BUTTON_WIDTH &&
                event.data2 > MSGBOX_BUTTON_Y && event.data2 < MSGBOX_BUTTON_Y + MSGBOX_BUTTON_HEIGHT) {
                box->result = buttons[i].result;
                return;
            }
        }
    }
}

#endif // !MSGBOX_HPP
