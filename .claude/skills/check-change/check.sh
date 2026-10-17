#!/usr/bin/env bash
#
# Syntax-check RetrOS-32 sources with clang, using the flags the build compiles them with, and
# report only diagnostics that HEAD's version of the same file doesn't have. For files under
# apps/, also report runtime-library helpers (libgcc) the object would need, since apps don't
# link libgcc.
#
# Usage: .claude/skills/check-change/check.sh [file...]
#        With no files, checks the .c/.cpp/.s files changed since HEAD, plus new untracked ones.
#
# This is a quick check, not a build: GCC (which the real build uses) warns about some things
# clang doesn't. Exit status is 1 if anything new was found.

set -u
cd "$(git rev-parse --show-toplevel)" || exit 2

FREESTANDING=(--target=i386-elf -m32 -ffreestanding -nostdinc -fno-builtin -fno-omit-frame-pointer -fno-pie -fno-stack-protector)
WARNINGS=(-Wall -Wextra -Wpedantic -Wno-pointer-arith -Wno-unused-parameter)
LIBGCC_HELPERS='^__(u?(div|mod)di3|u?divmoddi4|(a|l)sh[lr]di3|ashrdi3|muldi3|negdi2|u?cmpdi2|fix|float|popcount|clz|ctz|bswap|powi)'

files=("$@")
if [ ${#files[@]} -eq 0 ]; then
    while IFS= read -r f; do
        [ -n "$f" ] && files+=("$f")
    done < <(git diff --name-only HEAD -- '*.c' '*.cpp' '*.s'; git ls-files --others --exclude-standard -- '*.c' '*.cpp' '*.s')
fi
if [ ${#files[@]} -eq 0 ]; then
    echo "No changed .c/.cpp/.s files."
    exit 0
fi

tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT
found=0

# Prints diagnostics for the source named $1 ("<stdin>" for piped input), without file name,
# line and column, so the same warning on a moved line still matches.
diagnostics() {
    local name=$1; shift
    "$@" 2>&1 | grep -E "^$(printf '%s' "$name" | sed 's/[][\.*^$/]/\\&/g'):[0-9]+:[0-9]+: (warning|error)" \
        | sed -E 's/^[^:]+:[0-9]+:[0-9]+: //' | sort
}

for f in "${files[@]}"; do
    if [ ! -f "$f" ]; then
        echo "$f: deleted or missing, skipped"
        continue
    fi
    case "$f" in
        apps/C-Compiler/*) echo "$f: submodule, skipped"; continue ;;
    esac

    dir=$(dirname "$f")
    is_app=0
    case "$f" in
        *.s)
            out=$(clang --target=i386-elf -m32 -c -x assembler "$f" -o "$tmp/asm.o" 2>&1)
            if [ -n "$out" ]; then
                echo "$f: assembler output:"; echo "$out" | sed 's/^/    /'; found=1
            else
                echo "$f: assembles"
            fi
            continue ;;
        apps/*.cpp)
            is_app=1; lang=c++
            app_dir=$(echo "$f" | cut -d/ -f1-2)
            cmd=(clang++ "${FREESTANDING[@]}" -fno-exceptions -fno-rtti -O2 -I include -I apps -I "$app_dir" -I "$dir" "${WARNINGS[@]}") ;;
        apps/*.c)
            is_app=1; lang=c
            app_dir=$(echo "$f" | cut -d/ -f1-2)
            cmd=(clang "${FREESTANDING[@]}" -std=gnu11 -O2 -I include -I apps -I "$app_dir" -I "$dir" "${WARNINGS[@]}") ;;
        lib/*.c)
            lang=c
            cmd=(clang "${FREESTANDING[@]}" -std=gnu11 -O1 -I include -I "$dir" "${WARNINGS[@]}") ;;
        *.c)
            lang=c
            cmd=(clang "${FREESTANDING[@]}" -std=gnu11 -O1 -D__KERNEL -I include -I "$dir" "${WARNINGS[@]}") ;;
        *)
            echo "$f: not a .c, .cpp or .s file, skipped"; continue ;;
    esac

    diagnostics "$f" "${cmd[@]}" -fsyntax-only "$f" > "$tmp/after"
    if git cat-file -e "HEAD:$f" 2>/dev/null; then
        git show "HEAD:$f" > "$tmp/head-source"
        diagnostics "<stdin>" "${cmd[@]}" -fsyntax-only -x "$lang" - < "$tmp/head-source" > "$tmp/before"
    else
        : > "$tmp/before"
    fi

    new=$(comm -13 "$tmp/before" "$tmp/after")
    existing=$(wc -l < "$tmp/before" | tr -d ' ')
    if [ -n "$new" ]; then
        echo "$f: NEW diagnostics:"
        while IFS= read -r msg; do
            "${cmd[@]}" -fsyntax-only "$f" 2>&1 | grep -F -- "$msg" | grep -E "^$f:" | head -3 | sed 's/^/    /'
        done <<< "$new"
        found=1
    else
        echo "$f: no new diagnostics ($existing already in HEAD)"
    fi

    if [ $is_app -eq 1 ]; then
        if "${cmd[@]}" -c "$f" -o "$tmp/app.o" 2>/dev/null; then
            helpers=$(nm -u "$tmp/app.o" | awk '{print $NF}' | grep -E "$LIBGCC_HELPERS")
            if [ -n "$helpers" ]; then
                echo "$f: needs libgcc helpers, which apps don't link:"; echo "$helpers" | sed 's/^/    /'
                echo "    (usually 64-bit division or modulo; use 32-bit math)"
                found=1
            fi
        fi
    fi
done

exit $found
