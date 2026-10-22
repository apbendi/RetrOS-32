#!/bin/sh
# Builds TinyCC's own tests (apps/tcc/tinycc/tests/tests2) for the C harness, with the cross compiler, as tests2.conf
# says. For each test NAME that isn't skipped, out/tests2/ gets:
# - NAME, the program, unless it didn't compile;
# - NAME.compile, what the compiler printed. It's compiled from the tests' own directory, so messages name the file
#   alone, as the tests' Makefile has them;
# - NAME.expect, what the test should print in all, the compiler's messages first.
# Runs in the Docker builder (tests/c/Makefile).
set -u
HERE=$(cd "$(dirname "$0")" && pwd)
ROOT=$(cd "$HERE/../.." && pwd)
SRC=$ROOT/apps/tcc/tinycc/tests/tests2
TCC=$ROOT/apps/tcc/bin/i386-retros-tcc
CONF=$HERE/tests2.conf
OUT=$HERE/out/tests2

# The rest of the conf line for SETTING NAME
setting() {
	awk -v k="$1" -v n="$2" '$1 == k && $2 == n { sub(/^[^ ]+ +[^ ]+ */, ""); print }' "$CONF"
}

rm -rf "$OUT"
mkdir -p "$OUT"
built=0
for c in "$SRC"/[0-9][0-9]_*.c "$SRC"/[0-9][0-9][0-9]_*.c; do
	name=$(basename "$c" .c)
	[ -n "$(setting skip "$name")" ] && continue
	(cd "$SRC" && $TCC $(setting flags "$name") "$name.c" $(setting files "$name") -o "$OUT/$name") > "$OUT/$name.compile" 2>&1
	cp "$SRC/$name.expect" "$OUT/"
	built=$((built + 1))
done
# 46_grep reads its own source
cp "$SRC/46_grep.c" "$OUT/"
echo "[c harness] built $built of TinyCC's tests into out/tests2"
