/* Host stand-in for apps/utils/Thread.hpp: a thread never starts, so the Help window and Save As popup report failing */
#ifndef EDITOR_TEST_THREAD_HPP
#define EDITOR_TEST_THREAD_HPP
typedef void (*ThreadFunc)(void*);
class Thread {
public:
    Thread(ThreadFunc, int) {}
    int start(void*) { return -1; }
};
#endif
