/**
 * How a key's work in the Editor grows with the size of its text (#213). For texts of 16 KB to 1 MB, made from the
 * files given (run.sh gives libc/musl/include's headers), it times keys pressed with the cursor in the middle, and
 * counts the characters each lays out (TextLayout::place) and compares with a newline. run.sh adds those counters, and
 * raises EDITOR_BUFFER_SIZE so the bigger texts fit, in a copy of the Editor, which this includes as EDIT_CPP.
 */
#define private public
#define main editor_main
#include EDIT_CPP
#undef main

#include "harness.hpp"

#include <chrono>

static std::string corpus;

static std::string textOf(size_t size)
{
    std::string text;
    while (text.size() < size) {
        text += corpus;
    }
    text.resize(size);
    return text;
}

static void key(Editor* e, unsigned char c, int mods = 0)
{
    e->putChar(c, mods);
}

static void typeLine(Editor* e, int line)
{
    key(e, CTRLG);
    for (char c : std::to_string(line)) {
        key(e, (unsigned char)c);
    }
    key(e, '\n');
}

/* Runs setup, then times run, reps times, and prints the mean time and counts per run */
template <typename S, typename R> static void measure(const char* name, int reps, S setup, R run)
{
    double us = 0;
    long long placed = 0;
    long long compared = 0;
    for (int i = 0; i < reps; i++) {
        setup();
        g_calls.clear();
        long long placed_before = g_placed;
        long long compared_before = g_compared;
        auto start = std::chrono::steady_clock::now();
        run();
        auto end = std::chrono::steady_clock::now();
        us += std::chrono::duration<double, std::micro>(end - start).count();
        placed += g_placed - placed_before;
        compared += g_compared - compared_before;
    }
    printf("  %-26s %9.2f ms %11lld laid out %11lld compared\n", name, us / reps / 1000.0, placed / reps, compared / reps);
}

int main(int argc, char** argv)
{
    for (int i = 1; i < argc; i++) {
        FILE* f = fopen(argv[i], "rb");
        if (f == nullptr) {
            continue;
        }
        char buf[65536];
        size_t n;
        while ((n = fread(buf, 1, sizeof(buf), f)) > 0) {
            corpus.append(buf, n);
        }
        fclose(f);
    }
    if (corpus.empty()) {
        fprintf(stderr, "usage: bench FILE...: the files to make texts of\n");
        return 2;
    }

    Editor* e = new Editor();
    int cols = e->visibleColumns();
    int rows = e->visibleLines();
    printf("The default window, %d rows of %d columns, with the cursor in the middle of the text\n", rows, cols);

    for (size_t size : {16 * 1024, 64 * 1024, 256 * 1024, 1024 * 1024}) {
        std::string text = textOf(size);
        int lines = 1 + (int)std::count(text.begin(), text.end(), '\n');
        printf("%d KB, %d lines\n", (int)(size / 1024), lines);

        /* Esc first, as a user would, since the last measurement may have left Find's prompt open, taking every key */
        auto opened = [&]() {
            key(e, 27);
            openText(e, text);
        };
        auto middle = [&]() {
            opened();
            typeLine(e, lines / 2);
        };
        auto none = [&]() {};

        measure("open", 3, none, opened);
        measure("type a character", 5, middle, [&]() { key(e, 'x'); });
        measure("Down", 5, middle, [&]() { key(e, KEY_DOWN); });
        measure("Page Up", 3, middle, [&]() { key(e, KEY_PAGE_UP); });
        measure("Page Down", 3, middle, [&]() { key(e, KEY_PAGE_DOWN); });
        measure("Ctrl+G to the middle", 3, opened, [&]() { typeLine(e, lines / 2); });
        /* The cursor to the text's end, which the view follows */
        measure("Ctrl+A from the top", 1, opened, [&]() { key(e, CTRLA); });
        measure("Find, Enter", 3,
                [&]() {
                    opened();
                    key(e, CTRLF);
                    for (const char* p = "printf"; *p; p++) {
                        key(e, (unsigned char)*p);
                    }
                },
                [&]() { key(e, '\n'); });
    }
    delete e;
    return 0;
}
