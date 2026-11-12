#!/bin/bash
# Host tests, a trace and a benchmark of the Editor (apps/editor), built with the host's C++ compiler around the real
# edit.cpp, with tests/editor/stubs standing in for libgraphic, the system calls and the other apps/utils classes.
# Run from anywhere:
#   tests/editor/run.sh               the tests, then the trace against HEAD
#   tests/editor/run.sh tests         the tests, of the Editor and of the real TreeView, under AddressSanitizer and UBSan
#   tests/editor/run.sh trace [REF]   the trace of the working tree's Editor against REF's (default HEAD)
#   tests/editor/run.sh bench [REF]   the benchmark, of the working tree's Editor, or REF's
# Its builds go in tests/editor/out.
set -u
cd "$(dirname "$0")/../.."
T=tests/editor
OUT=$T/out
CXX=${HOST_CXX:-c++}
CC=${HOST_CC:-cc}
SAN="-O1 -g -fsanitize=address,undefined -fno-sanitize-recover=all"
mkdir -p "$OUT"

# The real headers the Editor needs that compile on the host, copied fresh each run so they're never stale.
# include/ itself can't go on the path: its string.h and the rest would hide the host's own
INC=$OUT/inc
rm -rf "$INC"
mkdir -p "$INC/gfx" "$INC/fs" "$INC/utils"
cp include/keyboard.h include/colors.h include/syscall_helper.h include/errors.h include/clipboard.h include/rtc.h \
    include/image.h "$INC/"
cp include/gfx/events.h "$INC/gfx/"
cp include/fs/fs_types.h "$INC/fs/"
# What the real TreeView needs from apps/utils, which the tree's tests build
cp apps/utils/StringHelper.hpp apps/utils/Function.hpp "$INC/utils/"

# build NAME SOURCE EDITOR_DIR FLAGS...: SOURCE, with the Editor whose edit.cpp and edit.hpp are in EDITOR_DIR
build() {
    local name=$1 source=$2 dir=$3
    shift 3
    $CC -c lib/errors.c -DERRORS_CODES_ONLY -I "$INC" "$@" -o "$OUT/$name-errors.o" &&
        $CXX -std=gnu++17 -w "$@" -I "$T/stubs" -I "$INC" -DEDIT_CPP="\"$PWD/$dir/edit.cpp\"" \
            "$source" "$T/host.cpp" "$OUT/$name-errors.o" -o "$OUT/$name"
}

# export_editor REF DIR: REF's edit.cpp and edit.hpp, into DIR
export_editor() {
    mkdir -p "$2" &&
        git show "$1:apps/editor/edit.cpp" > "$2/edit.cpp" &&
        git show "$1:apps/editor/edit.hpp" > "$2/edit.hpp"
}

run_tests() {
    echo "== tests"
    build tests "$T/tests.cpp" apps/editor $SAN || return 1
    "$OUT/tests" || return 1
    build treeview "$T/treeview.cpp" apps/editor $SAN -DTREEVIEW_HPP="\"$PWD/apps/utils/TreeView.hpp\"" || return 1
    "$OUT/treeview"
}

# The same scripted sessions through REF's Editor and the working tree's. Each prints a line per event, with a hash
# of what was drawn; at the first line that differs, both print that event's draw calls in full.
run_trace() {
    local ref=${1:-HEAD}
    echo "== trace: the working tree against $ref"
    export_editor "$ref" "$OUT/ref" || return 1
    build trace-ref "$T/trace.cpp" "$OUT/ref" $SAN || return 1
    build trace-new "$T/trace.cpp" apps/editor $SAN || return 1
    "$OUT/trace-ref" > "$OUT/trace-ref.txt" || return 1
    "$OUT/trace-new" > "$OUT/trace-new.txt" || return 1
    if cmp -s "$OUT/trace-ref.txt" "$OUT/trace-new.txt"; then
        echo "ok   $(wc -l < "$OUT/trace-new.txt" | tr -d ' ') events, the same in both"
        return 0
    fi
    local line
    line=$(cmp "$OUT/trace-ref.txt" "$OUT/trace-new.txt" | sed -E 's/.* line ([0-9]+).*/\1/')
    echo "FAIL the traces differ first at line $line:"
    echo "  $ref:    $(sed -n "${line}p" "$OUT/trace-ref.txt")"
    echo "  working: $(sed -n "${line}p" "$OUT/trace-new.txt")"
    local event
    event=$(sed -n "${line}p" "$OUT/trace-ref.txt" | cut -d' ' -f1)
    "$OUT/trace-ref" --dump "$event" > "$OUT/dump-ref.txt"
    "$OUT/trace-new" --dump "$event" > "$OUT/dump-new.txt"
    echo "  the draw calls of event $event, $ref's then the working tree's (all in $OUT/dump-*.txt):"
    diff "$OUT/dump-ref.txt" "$OUT/dump-new.txt" | head -40
    return 1
}

# The benchmark counts characters laid out (TextLayout::place) and compared with a newline, with counters added to a
# copy of the Editor, and EDITOR_BUFFER_SIZE raised there so it can open bigger texts
run_bench() {
    local dir=apps/editor
    if [ $# -gt 0 ]; then
        export_editor "$1" "$OUT/bench-ref" || return 1
        dir=$OUT/bench-ref
    fi
    echo "== benchmark: ${1:-the working tree}"
    mkdir -p "$OUT/bench-editor"
    python3 - "$dir" "$OUT/bench-editor" <<'EOF' || return 1
import re, sys
src, dst = sys.argv[1], sys.argv[2]
cpp = open(src + '/edit.cpp').read()
hpp = open(src + '/edit.hpp').read()
hpp, n = re.subn(r'#define EDITOR_BUFFER_SIZE \(.*\)', '#define EDITOR_BUFFER_SIZE (2 * 1024 * 1024)', hpp)
if n != 1:
    sys.exit('EDITOR_BUFFER_SIZE not found')
cpp = cpp.replace('#include "edit.hpp"', '#include "edit.hpp"\nlong long g_placed = 0;\nlong long g_compared = 0;', 1)
cpp, n = re.subn(r'(void place\(unsigned char ch\)\s*\{)', r'\1\n        g_placed++;', cpp)
if n != 1:
    print('warning: TextLayout::place not found, so nothing is counted as laid out', file=sys.stderr)
cpp, n = re.subn(r"(m_textBuffer\[[^\]]+\] == '\\n')", r'(g_compared++, \1)', cpp)
if n == 0:
    print('warning: no newline comparisons found to count', file=sys.stderr)
open(dst + '/edit.cpp', 'w').write(cpp)
open(dst + '/edit.hpp', 'w').write(hpp)
EOF
    build bench "$T/bench.cpp" "$OUT/bench-editor" -O2 -DNDEBUG || return 1
    "$OUT/bench" libc/musl/include/*.h
}

case "${1:-all}" in
tests) run_tests ;;
trace) run_trace "${2:-HEAD}" ;;
bench) shift; run_bench "$@" ;;
all) run_tests && run_trace HEAD ;;
*)
    sed -n '2,9p' "$0"
    exit 2
    ;;
esac
