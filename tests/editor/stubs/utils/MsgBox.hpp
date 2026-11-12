/* Host stand-in for apps/utils/MsgBox.hpp: every question is answered No, so "Save changes?" discards them */
#ifndef EDITOR_TEST_MSGBOX_HPP
#define EDITOR_TEST_MSGBOX_HPP
enum { MSGBOX_OK_ONLY, MSGBOX_YES_NO, MSGBOX_YES_NO_CANCEL };
typedef enum { MSGBOX_YES, MSGBOX_NO, MSGBOX_CANCEL, MSGBOX_OK } MsgBoxResult;
class MsgBox {
public:
    MsgBox(const char*, const char*, int) {}
    MsgBoxResult show() { return MSGBOX_NO; }
};
#endif
