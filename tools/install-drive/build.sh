#!/bin/sh
#
# Builds tools/install-drive/out/install-drive.img: a USB drive image that boots SystemRescue in legacy BIOS mode
# from an ordinary FAT32 partition, with install-retros.sh and, if it's been built, RetrOS-32-debug.img on it.
# Runs in the container docker-compose.yml's install-drive service gives it (README.md).
#
# The drive, from its first sector:
#   sector 0       syslinux's MBR boot code, which loads the active partition's first sector
#   from 1 MiB     one FAT32 partition, active, with SystemRescue's volume label, holding SystemRescue's files.
#                  Its boot sector has syslinux's boot code, which loads ldlinux.sys from the partition, which reads
#                  SystemRescue's syslinux menu and boots Linux. Linux finds the partition by its label.
#
set -eu

HERE=$(cd "$(dirname "$0")" && pwd)
ROOT=$(cd "$HERE/../.." && pwd)
CACHE=$HERE/cache
OUT=$HERE/out
. "$HERE/systemrescue.conf"

ISO=systemrescue-$SYSRESCUE_VERSION-amd64.iso
ISO_URL=https://fastly-cdn.system-rescue.org/releases/$SYSRESCUE_VERSION/$ISO
SIG_URL=https://www.system-rescue.org/releases/$SYSRESCUE_VERSION/$ISO.asc
KEY_URL=https://www.system-rescue.org/security/signing-keys/$SYSRESCUE_KEY
RETROS_IMAGE=$ROOT/RetrOS-32-debug.img

# Free space on the partition beyond SystemRescue, for RetrOS images
ROOM_MB=512

say() { echo "install-drive: $*"; }
die() { echo "install-drive: $*" >&2; exit 1; }

WORK=$(mktemp -d)
trap 'rm -rf "$WORK"' EXIT

# Downloads a file into the cache, unless it's there already. A download that's cut off, as the CDN's often are, is
# picked up where it stopped, by a new curl each time: curl's own --retry starts again from where its run began.
fetch() {
	[ -f "$CACHE/$1" ] && return
	say "Downloading $2"
	TRIES=1
	until curl -fL -C - -o "$CACHE/$1.part" "$2"; do
		[ "$TRIES" -lt 20 ] || die "The download keeps stopping. Run this again to pick it up where it left off."
		TRIES=$((TRIES + 1))
		say "The download stopped. Picking it up where it left off."
		sleep 2
	done
	mv "$CACHE/$1.part" "$CACHE/$1"
}

mkdir -p "$CACHE" "$OUT"
fetch "$ISO" "$ISO_URL"
fetch "$ISO.asc" "$SIG_URL"
fetch "$SYSRESCUE_KEY" "$KEY_URL"

say "Checking $ISO"
echo "$SYSRESCUE_SHA256  $CACHE/$ISO" | sha256sum -c --quiet - ||
	die "$ISO isn't the one systemrescue.conf names. Delete $CACHE/$ISO to download it again."
export GNUPGHOME="$WORK/gnupg"
mkdir -m 700 "$GNUPGHOME"
FINGERPRINT=$(gpg --show-keys --with-colons "$CACHE/$SYSRESCUE_KEY" 2>/dev/null | awk -F: '$1 == "fpr" { print $10; exit }')
[ "$FINGERPRINT" = "$SYSRESCUE_KEY_FINGERPRINT" ] ||
	die "$SYSRESCUE_KEY's fingerprint is $FINGERPRINT, not the $SYSRESCUE_KEY_FINGERPRINT systemrescue.conf names."
gpg --dearmor < "$CACHE/$SYSRESCUE_KEY" > "$WORK/key.gpg"
gpgv --keyring "$WORK/key.gpg" "$CACHE/$ISO.asc" "$CACHE/$ISO" 2>/dev/null ||
	die "$ISO's signature doesn't check out against $SYSRESCUE_KEY."

say "Unpacking it"
FILES=$WORK/files
if ! XORRISO=$(xorriso -report_about SORRY -osirrox on -indev "$CACHE/$ISO" -extract / "$FILES" 2>&1); then
	echo "$XORRISO" >&2
	die "Couldn't unpack $ISO."
fi
chmod -R u+w "$FILES"

# SystemRescue's syslinux directory: its menu and modules, and a syslinux.cfg for a drive with syslinux installed there.
# The CD boots with isolinux, syslinux's version for CDs, from /isolinux, which is left as it is and isn't used.
SYSLINUX_CFG=$(find "$FILES" -name syslinux.cfg)
[ "$(echo "$SYSLINUX_CFG" | grep -c .)" = 1 ] || die "Expected one syslinux.cfg in the ISO, found: $SYSLINUX_CFG"
SYSLINUX_DIR=$(dirname "$SYSLINUX_CFG")
SYSLINUX_PATH=${SYSLINUX_DIR#"$FILES"}

# The partition's label has to be the one SystemRescue's boot menu tells Linux to look for
LABEL=$(grep -rhoE --include='*.cfg' 'archisolabel=[^ ]+' "$SYSLINUX_DIR" | sort -u | cut -d= -f2)
[ "$(echo "$LABEL" | grep -c .)" = 1 ] || die "Expected one archisolabel in $SYSLINUX_PATH, found: $LABEL"
[ "${#LABEL}" -le 11 ] || die "The label $LABEL is longer than FAT's 11 characters."

# syslinux's modules have to be the same version as its installer, so SystemRescue's are replaced with this one's
for MODULE in "$SYSLINUX_DIR"/*.c32; do
	NAME=$(basename "$MODULE")
	[ -f "/usr/lib/syslinux/modules/bios/$NAME" ] || die "This syslinux has no $NAME, which SystemRescue's menu uses."
	cp "/usr/lib/syslinux/modules/bios/$NAME" "$MODULE"
done

cp "$HERE/install-retros.sh" "$FILES/"
cp "$HERE/retros.yaml" "$FILES/sysrescue.d/900-retros.yaml"
if [ -f "$RETROS_IMAGE" ]; then
	# -p keeps when it was built, which install-retros.sh shows
	cp -p "$RETROS_IMAGE" "$FILES/"
	RETROS_NOTE="with RetrOS-32-debug.img from $(date -u -r "$RETROS_IMAGE" '+%Y-%m-%d %H:%M') UTC"
else
	RETROS_NOTE="without a RetrOS image: there's no RetrOS-32-debug.img to copy. Build it, then copy it onto the drive"
fi

say "Making the FAT32 partition, $LABEL"
FILES_MB=$(du -sm "$FILES" | cut -f1)
PART_MB=$(( (FILES_MB + ROOM_MB + 63) / 64 * 64 ))
PART=$WORK/partition.img
truncate -s "${PART_MB}M" "$PART"
mkfs.fat -F 32 -n "$LABEL" "$PART" > /dev/null
export MTOOLS_SKIP_CHECK=1
find "$FILES" -mindepth 1 -maxdepth 1 -exec mcopy -s -m -i "$PART" {} ::/ \;

say "Installing syslinux in $SYSLINUX_PATH"
syslinux --install --directory "$SYSLINUX_PATH" "$PART"
fsck.fat -n "$PART" > /dev/null || die "fsck.fat found problems in the partition."

say "Putting the drive together"
DRIVE=$OUT/install-drive.img
rm -f "$DRIVE"
truncate -s "$((PART_MB + 1))M" "$DRIVE"
printf 'label: dos\nlabel-id: 0x52455452\nstart=2048, type=c, bootable\n' | sfdisk --quiet "$DRIVE"
dd if=/usr/lib/syslinux/mbr/mbr.bin of="$DRIVE" bs=440 count=1 conv=notrunc status=none
dd if="$PART" of="$DRIVE" bs=1M seek=1 conv=notrunc status=none

say "Done: tools/install-drive/out/install-drive.img, $((PART_MB + 1)) MB, SystemRescue $SYSRESCUE_VERSION,"
say "$RETROS_NOTE."
