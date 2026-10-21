#!/usr/bin/env bash
# Checks RetrOS's FAT16 code against mtools and fsck.fat. Run it in the Docker builder, from the repo root:
#
#   docker compose run --rm --entrypoint tests/fs/run.sh builder [-k] [-v] [PATTERN...]
#
# PATTERN picks cases by name with shell globs, e.g. 'write.*'. Failing cases leave their notes, trees and fsck
# findings in tests/fs/out/CASE/; -k keeps every case's, with its R and M images. -v shows the details of every
# failure, not only of results that changed.
#
# Each case starts two copies of the same image: R, which RetrOS's code (fstool) changes, and M, which mtools
# changes the same way. Then it checks, as the case asks:
#   fsck   fsck.fat finds no problems in R that weren't there before RetrOS touched it
#   same   mtools reads R and M the same: every path, size and file content
#   reads  RetrOS reads R the same as mtools reads M
#   tool   fstool saw no read or write outside the disk and no write past a kernel allocation
#   lock   fstool saw no lock released that wasn't held (#38), kept apart since every new cluster does it today
#   op     RetrOS's operations succeeded, or failed when they should have
#   mount  RetrOS mounted the image
# The results go to tests/fs/out/results.txt, one line per case with the checks that failed, and are compared
# with tests/fs/expected.txt.
set -u
cd "$(dirname "$0")/../.." || exit 1
ROOT=$PWD
HERE=$ROOT/tests/fs
OUT=$HERE/out
WORK=/tmp/fstest
FSTOOL=$HERE/bin/fstool
export MTOOLS_SKIP_CHECK=1   # RetrOS writes 0 heads and sectors per track, which mtools otherwise refuses
export MTOOLS_NO_VFAT=1      # 8.3 names only, like RetrOS: mtools neither writes nor lists long names

KEEP=0
VERBOSE=0
PATTERNS=()
while [ $# -gt 0 ]; do
    case $1 in
        -k) KEEP=1 ;;
        -v) VERBOSE=1 ;;
        *) PATTERNS+=("$1") ;;
    esac
    shift
done

# fstool, from the current fs/ sources
make -s -C fs >/dev/null || { echo "Building fs/ failed"; exit 1; }
make -s -C tests/fs >/dev/null || { echo "Building fstool failed"; exit 1; }

rm -rf "$WORK" "$OUT"
mkdir -p "$WORK/base" "$OUT"

# --- Images ---------------------------------------------------------------------------------------------------

# base_image NAME: the path of a starting image, made the first time it's asked for
base_image() {
    local img=$WORK/base/$1.img
    if [ ! -f "$img" ]; then
        case $1 in
            real)           # The image make img builds: bootloader, kernel and rootfs/
                [ -f "$ROOT/RetrOS-32-debug.img" ] || return 1
                cp "$ROOT/RetrOS-32-debug.img" "$img" ;;
            fresh)          # 32 MB formatted by RetrOS, as login formats the virtual disk
                truncate -s 32M "$img" && "$FSTOOL" "$img" format 4 >/dev/null ;;
            small)          # 3 MB formatted by RetrOS, for filling up
                truncate -s 3M "$img" && "$FSTOOL" "$img" format 4 >/dev/null ;;
            virtual)        # 1 MB formatted by RetrOS, the size of the virtual disk login makes
                truncate -s 1M "$img" && "$FSTOOL" "$img" format 1 >/dev/null ;;
            standard)       # 32 MB FAT16 as other systems make it: 2 FATs, 2 KB clusters, 512 root entries
                mkfs.fat -F 16 -C "$img" 32768 >/dev/null ;;
            standard-oem)   # The same, with RetrOS's name in the boot sector, which RetrOS requires to mount
                mkfs.fat -F 16 -C "$img" 32768 >/dev/null && printf 'RetrOS32' | dd of="$img" bs=1 seek=3 conv=notrunc status=none ;;
            *) return 1 ;;
        esac || return 1
    fi
    echo "$img"
}

# data N [TAG]: a file of N bytes that never repeats, so misplaced pieces show. Prints its path.
data() {
    local tag=${2:-x}
    local file=$WORK/data-$1-${tag//[^A-Za-z0-9]/_}
    [ -f "$file" ] || { printf '%s:' "$tag"; seq 1 2000000 | tr '\n' ','; } 2>/dev/null | head -c "$1" > "$file"
    echo "$file"
}

# fsck_problems IMG: fsck.fat's findings, one per line, without its banner and summary
fsck_problems() {
    fsck.fat -n "$1" 2>&1 | grep -v -E '^fsck\.fat [0-9]|^Leaving filesystem unchanged|: [0-9]+ files, [0-9]+/[0-9]+ clusters$|^$' | sort -u
}

# ref_tree IMG: every directory and file as mtools reads them, as "D /PATH" and "F /PATH SIZE SHA"
ref_tree() {
    ref_walk "$1" "" | tr '[:lower:]' '[:upper:]' | sort
}

# ref_walk IMG DIR: one directory and everything under it, by their short 8.3 names, which is all RetrOS reads.
# The names come from mdir's columns: the first 8 characters of an entry's line are its name and the next 3 its
# extension, even when the entry has a long name, which mdir shows at the end of the line.
ref_walk() {
    local img=$1 dir=$2 line name ext path
    local entry='^(.{8}) (.{3}) +(<DIR>|[0-9][0-9 ]*) +[0-9]{4}-[0-9]{2}-[0-9]{2}'
    mdir -i "$img" "::$dir/" 2>/dev/null | tr -d '\0' | while IFS= read -r line; do
        [[ $line =~ $entry ]] || continue
        name=${BASH_REMATCH[1]%"${BASH_REMATCH[1]##*[! ]}"}
        ext=${BASH_REMATCH[2]%"${BASH_REMATCH[2]##*[! ]}"}
        { [ "$name" = . ] || [ "$name" = .. ]; } && continue
        path=$dir/$name${ext:+.$ext}
        if [ "${BASH_REMATCH[3]}" = "<DIR>" ]; then
            echo "D $path"
            ref_walk "$img" "$path"
        else
            mcopy -i "$img" "::$path" - > "$WORK/content" 2>/dev/null
            echo "F $path $(stat -c %s "$WORK/content") $(sha256sum < "$WORK/content" | cut -c1-16)"
        fi
    done
}

# rfs_tree IMG: the same, as RetrOS reads them
rfs_tree() {
    if ! "$FSTOOL" "$1" tree > "$WORK/tree" 2>/dev/null; then
        echo "(fstool tree failed)"
    fi
    while read -r kind path size; do
        if [ "$kind" = D ]; then
            echo "D $path"
        else
            "$FSTOOL" "$1" cat "$path" > "$WORK/content" 2>/dev/null
            echo "F $path $size $(sha256sum < "$WORK/content" | cut -c1-16)"
        fi
    done < "$WORK/tree" | tr '[:lower:]' '[:upper:]' | sort
}

# --- Cases -----------------------------------------------------------------------------------------------------

CASE=""
CDIR=""
R=""
M=""
FAILED=""
NOTES=()

# fail CHECK DETAIL: records a failed check
fail() {
    local check=$1 note
    shift
    [[ " $FAILED " == *" $check "* ]] || FAILED="$FAILED $check"
    for note in "${NOTES[@]}"; do
        [ "$note" = "$check: $*" ] && return
    done
    NOTES+=("$check: $*")
}

# begin BASE: R and M start as copies of a base image
begin() {
    local base
    base=$(base_image "$1") || { fail setup "can't make the $1 image"; return 1; }
    cp "$base" "$R"
    cp "$base" "$M"
    ready
}

# ready: records fsck's findings before RetrOS changes anything. begin calls it; call it again after seeding.
ready() {
    fsck_problems "$R" > "$CDIR/fsck-before"
    fsck_problems "$M" > "$CDIR/ref-fsck-before"
}

# seed MTOOLS-COMMAND ARGS...: runs an mtools command on both images, to set up a case
seed() {
    local cmd=$1
    shift
    "$cmd" -i "$R" "$@" >/dev/null 2>&1 || fail setup "$cmd $* failed on R"
    "$cmd" -i "$M" "$@" >/dev/null 2>&1 || fail setup "$cmd $* failed on M"
}

# problems: records the problems fstool reported in $CDIR/err, under lock or tool
problems() {
    grep -q '^fstool: problem: release of a lock' "$CDIR/err" && fail lock "a lock was released that wasn't held (#38)"
    local other
    other=$(grep '^fstool: problem:' "$CDIR/err" | grep -v 'release of a lock' | sed 's/^fstool: problem: //; s/block [0-9]*/block N/g' |
        sort | uniq -c | sed 's/^ *\([0-9]*\) /\1x /' | head -4 | paste -sd ';' -)
    [ -z "$other" ] || fail tool "$other"
}

# rfs ARGS...: fstool on R, recording a failure, a refused mount and any problems
rfs() {
    local status
    "$FSTOOL" "$R" "$@" 2> "$CDIR/err"
    status=$?
    case $status in
        0|4) ;;
        3) fail mount "RetrOS didn't mount the image" ;;
        *) fail op "fstool $1 ${2:-} exited $status: $(grep -m1 -v '^fstool: problem:' "$CDIR/err")" ;;
    esac
    problems
    return $status
}

# refused ARGS...: fstool on R for an operation that should fail, recording success as an op failure
refused() {
    local status
    timeout 120 "$FSTOOL" "$R" "$@" 2> "$CDIR/err"
    status=$?
    case $status in
        0|4) fail op "fstool $* succeeded, but should have failed" ;;
        124) fail op "fstool $1 ${2:-} hadn't finished after 120 seconds" ;;
    esac
    problems
}

# ref MTOOLS-COMMAND ARGS...: an mtools command on M
ref() {
    local cmd=$1
    shift
    "$cmd" -i "$M" "$@" >/dev/null 2>"$CDIR/ref-err" || fail ref "$cmd $* failed: $(head -c 200 "$CDIR/ref-err")"
}

# The same change on both images. RetrOS through fstool, the reference through mtools.

put() {         # put PATH N: write N bytes in one write, creating PATH if needed
    local file
    file=$(data "$2" "$1")
    rfs write "$1" < "$file"
    ref mcopy -o "$file" "::$1"
}

put_chunks() {  # put_chunks PATH N CHUNK: write N bytes as CHUNK-byte writes on one open file
    local file
    file=$(data "$2" "$1")
    rfs writes "$1" "$3" < "$file"
    ref mcopy -o "$file" "::$1"
}

put_at() {      # put_at PATH OFFSET N: overwrite N bytes of an existing file at OFFSET
    local file
    file=$(data "$3" "$1@$2")
    rfs pwrite "$1" "$2" < "$file"
    mcopy -i "$M" "::$1" - > "$WORK/patched" 2>/dev/null
    dd if="$file" of="$WORK/patched" bs=1 seek="$2" conv=notrunc status=none
    ref mcopy -o "$WORK/patched" "::$1"
}

create() {      # create PATH: an empty file
    : > "$WORK/empty"
    rfs new "$1"
    ref mcopy -o "$WORK/empty" "::$1"
}

# Checks

check_fsck() {
    local new
    new=$(fsck_problems "$R" | comm -13 "$CDIR/fsck-before" -)
    [ -z "$new" ] || fail fsck "$(echo "$new" | head -6 | paste -sd ';' -)"
    new=$(fsck_problems "$M" | comm -13 "$CDIR/ref-fsck-before" -)
    [ -z "$new" ] || fail ref "fsck on M: $(echo "$new" | head -3 | paste -sd ';' -)"
}

check_same() {
    ref_tree "$R" > "$CDIR/tree-R-mtools"
    ref_tree "$M" > "$CDIR/tree-M-mtools"
    diff -q "$CDIR/tree-R-mtools" "$CDIR/tree-M-mtools" >/dev/null ||
        fail same "$(diff "$CDIR/tree-M-mtools" "$CDIR/tree-R-mtools" | grep '^[<>]' | head -6 | paste -sd ';' -)"
}

check_reads() {
    [ -f "$CDIR/tree-M-mtools" ] || ref_tree "$M" > "$CDIR/tree-M-mtools"
    rfs_tree "$R" > "$CDIR/tree-R-retros"
    diff -q "$CDIR/tree-R-retros" "$CDIR/tree-M-mtools" >/dev/null ||
        fail reads "$(diff "$CDIR/tree-M-mtools" "$CDIR/tree-R-retros" | grep '^[<>]' | head -6 | paste -sd ';' -)"
}

check() {
    check_fsck
    check_same
    check_reads
}

# A case: a function, and a line in CASES with its name and what it checks
CASES=()
add() {
    CASES+=("$1|$2|$3")
}

source "$HERE/cases.sh"

# --- Run ----------------------------------------------------------------------------------------------------

selected() {
    [ ${#PATTERNS[@]} -eq 0 ] && return 0
    local p
    for p in "${PATTERNS[@]}"; do
        # shellcheck disable=SC2053
        [[ $1 == $p ]] && return 0
    done
    return 1
}

declare -A EXPECTED=()
if [ -f "$HERE/expected.txt" ]; then
    while read -r name result; do
        [ -n "$name" ] && [[ $name != \#* ]] && EXPECTED[$name]=$result
    done < "$HERE/expected.txt"
fi

passed=0
failed=0
changed=0
for entry in "${CASES[@]}"; do
    IFS='|' read -r CASE description fn <<< "$entry"
    selected "$CASE" || continue

    CDIR=$WORK/cases/$CASE
    mkdir -p "$CDIR"
    R=$CDIR/R.img
    M=$CDIR/M.img
    FAILED=""
    NOTES=()
    "$fn"

    if [ -z "$FAILED" ]; then
        result="pass"
        passed=$((passed + 1))
    else
        result="fail:$(echo "$FAILED" | tr ' ' '\n' | sort | paste -sd ',' - | sed 's/^,//')"
        failed=$((failed + 1))
    fi
    echo "$CASE $result" >> "$OUT/results.txt"

    expected=${EXPECTED[$CASE]:-}
    marker=""
    if [ -n "$expected" ] && [ "$expected" != "$result" ]; then
        marker="  <- was $expected"
        changed=$((changed + 1))
    elif [ -z "$expected" ] && [ ${#EXPECTED[@]} -gt 0 ]; then
        marker="  <- new case"
    fi
    printf '%-34s %s%s\n' "$CASE" "$result" "$marker"
    if [ -n "$FAILED" ] && { [ $VERBOSE = 1 ] || [ -n "$marker" ]; }; then
        echo "    $description"
        for note in "${NOTES[@]}"; do
            echo "    - $note" | cut -c1-240
        done
    fi

    # The trees, fsck findings and notes of failing cases; the 32 MB images only with -k
    if [ -n "$FAILED" ] || [ $KEEP = 1 ]; then
        mkdir -p "$OUT/$CASE"
        printf '%s\n' "${NOTES[@]}" > "$OUT/$CASE/notes.txt"
        for f in "$CDIR"/*; do
            [[ $f == *.img ]] && [ $KEEP = 0 ] && continue
            cp "$f" "$OUT/$CASE/"
        done
    fi
    rm -rf "$CDIR"
done

echo
echo "$passed passed, $failed failed, $changed changed from tests/fs/expected.txt. Results: tests/fs/out/results.txt"
[ $changed -eq 0 ]
