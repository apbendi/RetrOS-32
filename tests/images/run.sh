#!/bin/bash
# Host tests for RetrOS's images: lib/bmp.c, the reader the kernel and apps use, and bmptool (tools/bmp.c), both
# built here with the host's compiler, under AddressSanitizer. Run from anywhere:
#   tests/images/run.sh
# Its build goes in tests/images/out, never bin/, which has to hold the build container's binaries (AGENTS.md).
set -u
cd "$(dirname "$0")/../.."
OUT=tests/images/out
mkdir -p "$OUT"
CC=${HOST_CC:-cc}
TOOL="$OUT/bmptool"

SAN="-O1 -g -Wall -Wextra -fsanitize=address,undefined -fno-sanitize-recover=all"
$CC tools/bmp.c lib/bmp.c lib/palette.c -idirafter include $SAN -o "$TOOL" || exit 1
$CC tests/images/test_bmp.c lib/bmp.c lib/palette.c -idirafter include $SAN -o "$OUT/test_bmp" || exit 1

failed=0
pass() { echo "ok   $1"; }
fail() { echo "FAIL $1"; failed=1; }

# The reader: files of every layout, damaged files, and every image in images/
"$OUT/test_bmp" images/*.bmp || failed=1

# Round trips: palette indexes to a BMP and back, at widths that need each amount of row padding
for size in 1x1 2x3 3x5 4x4 16x16 16x21 32x32 85x95 255x2 256x256; do
    w=${size%x*}; h=${size#*x}
    head -c $((w * h)) /dev/urandom > "$OUT/in.raw"
    if "$TOOL" raw2bmp "$w" "$h" "$OUT/in.raw" "$OUT/rt.bmp" &&
        [ "$("$TOOL" bmp2raw "$OUT/rt.bmp" "$OUT/rt.raw")" = "$w $h" ] &&
        cmp -s "$OUT/in.raw" "$OUT/rt.raw" &&
        "$TOOL" check "$OUT/rt.bmp" > /dev/null; then
        pass "round trip $size"
    else
        fail "round trip $size"
    fi
done

# Sizes and data the tool must refuse
refuse() {
    if "$@" > /dev/null 2>&1; then fail "refuses: $*"; else pass "refuses: ${*:2}"; fi
}
head -c 16 /dev/urandom > "$OUT/16.raw"
refuse "$TOOL" raw2bmp 0 16 "$OUT/16.raw" "$OUT/x.bmp"
refuse "$TOOL" raw2bmp 257 1 "$OUT/16.raw" "$OUT/x.bmp"
refuse "$TOOL" raw2bmp 4 5 "$OUT/16.raw" "$OUT/x.bmp"
head -c 4 /dev/urandom > "$OUT/4.raw"
"$TOOL" raw2bmp 2 2 "$OUT/4.raw" "$OUT/ok.bmp"
refuse "$TOOL" check "$OUT/16.raw"
head -c 100 "$OUT/ok.bmp" > "$OUT/short.bmp"
refuse "$TOOL" check "$OUT/short.bmp"
# A color table that isn't the VGA palette, entry 1's blue changed, is read with its colors mapped to the palette
cp "$OUT/ok.bmp" "$OUT/table.bmp"
printf '\x00' | dd of="$OUT/table.bmp" bs=1 seek=$((54 + 4)) conv=notrunc 2>/dev/null
if "$TOOL" check "$OUT/table.bmp" | grep -q "mapped to the palette"; then pass "check: other colors"; else fail "check: other colors"; fi
# 24 bits a pixel
cp "$OUT/ok.bmp" "$OUT/bpp.bmp"
printf '\x18' | dd of="$OUT/bpp.bmp" bs=1 seek=28 conv=notrunc 2>/dev/null
refuse "$TOOL" check "$OUT/bpp.bmp"
# An image name that isn't a C identifier
cp "$OUT/ok.bmp" "$OUT/1bad-name.bmp"
refuse "$TOOL" bmp2c "$OUT/x.c" "$OUT/x.h" "$OUT/1bad-name.bmp"

# bmp2c: arrays named after the files, which compile, with the image's size and pixels
"$TOOL" raw2bmp 4 5 <(head -c 20 /dev/urandom | tee "$OUT/gen.raw") "$OUT/gen_test.bmp"
if "$TOOL" bmp2c "$OUT/images.c" "$OUT/images.h" "$OUT/gen_test.bmp" &&
    cat > "$OUT/gen_main.c" <<'C' &&
#include <stdio.h>
#include "images.h"
int main(void)
{
    FILE* f = fopen("tests/images/out/gen.raw", "rb");
    unsigned char raw[20];
    if (f == NULL || fread(raw, 1, 20, f) != 20) {
        return 1;
    }
    for (int i = 0; i < 20; i++) {
        if (gen_test.pixels[i] != raw[i]) {
            return 1;
        }
    }
    return gen_test.width == IMAGE_GEN_TEST_WIDTH && gen_test.height == IMAGE_GEN_TEST_HEIGHT && gen_test.width == 4 &&
        gen_test.height == 5 ? 0 : 1;
}
C
    $CC -Wall -Wextra -idirafter include -I "$OUT" "$OUT/gen_main.c" "$OUT/images.c" -o "$OUT/gen_main" &&
    "$OUT/gen_main"; then
    pass "bmp2c"
else
    fail "bmp2c"
fi

[ $failed = 0 ] && echo "All passed." || echo "Some failed."
exit $failed
