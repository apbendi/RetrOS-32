#!/bin/bash
#
# Installs RetrOS-32 on this computer's internal drive, erasing everything on it. It runs in SystemRescue, started
# from the install drive, which holds this script and the RetrOS image:
#
#   bash /run/archiso/bootmnt/install-retros.sh [IMAGE] [DRIVE]
#
# IMAGE defaults to RetrOS-32-debug.img next to this script. DRIVE, like sda, defaults to the only drive that isn't
# the install drive or another USB drive; with none or several, the script lists them and asks for one. Nothing is
# written until you type the drive's name to confirm.
#
set -euo pipefail

die() { echo "install-retros: $*" >&2; exit 1; }

[ "$(id -u)" = 0 ] || die "Run this as root, as SystemRescue's shell is."

HERE=$(dirname "$(readlink -f "$0")")
IMAGE=${1:-$HERE/RetrOS-32-debug.img}
[ -f "$IMAGE" ] || die "There's no RetrOS image at $IMAGE. Copy RetrOS-32-debug.img onto the install drive."
IMAGE_SIZE=$(stat -c %s "$IMAGE")

# The install drive itself, which is never a choice
BOOT_DRIVE=$(lsblk -no PKNAME "$(findmnt -no SOURCE /run/archiso/bootmnt 2>/dev/null)" 2>/dev/null || true)

if [ -n "${2:-}" ]; then
	DRIVE=${2#/dev/}
else
	# Whole drives that aren't USB, removable (a floppy or a card reader), the install drive, or one of the kernel's
	# own (loop, zram, a CD). TRAN, the connection, goes last, since it's empty for some.
	CANDIDATES=$(lsblk -dnrb -o NAME,TYPE,RM,SIZE,TRAN | awk -v boot="$BOOT_DRIVE" \
		'$2 == "disk" && $3 == 0 && $4 > 0 && $5 != "usb" && $1 != boot && $1 !~ /^zram/ { print $1 }')
	if [ "$(echo "$CANDIDATES" | grep -c .)" != 1 ]; then
		lsblk -d -o NAME,SIZE,RM,TRAN,MODEL
		die "Couldn't tell which drive is the internal one. Run it again with the drive's name, like: bash $0 $IMAGE sda"
	fi
	DRIVE=$CANDIDATES
fi

[ -b "/dev/$DRIVE" ] || die "There's no drive /dev/$DRIVE."
[ "$DRIVE" != "$BOOT_DRIVE" ] || die "/dev/$DRIVE is the install drive."
[ "$(lsblk -dno TYPE "/dev/$DRIVE")" = disk ] || die "/dev/$DRIVE isn't a whole drive."
# grep without -q, which reads all of findmnt's output: with -q, findmnt can die of SIGPIPE and pipefail fail the test
if findmnt -rno SOURCE | grep -E "^/dev/$DRIVE(p?[0-9]+)?$" >/dev/null; then
	die "Something on /dev/$DRIVE is mounted. Unmount it first."
fi

echo
echo "This erases everything on /dev/$DRIVE:"
echo
lsblk -o NAME,SIZE,TYPE,FSTYPE,LABEL,MODEL "/dev/$DRIVE"
echo
echo "and writes $(basename "$IMAGE") ($((IMAGE_SIZE / 1048576)) MB, $(date -r "$IMAGE" '+%Y-%m-%d %H:%M')) onto it."
echo
read -r -p "Type $DRIVE to go ahead: " ANSWER
[ "$ANSWER" = "$DRIVE" ] || die "Nothing was written."

# The old partition tables go first. A GPT keeps a copy in the drive's last sectors, which the image doesn't reach.
wipefs -a "/dev/$DRIVE"
dd if="$IMAGE" of="/dev/$DRIVE" bs=1M conv=fsync status=progress
cmp -n "$IMAGE_SIZE" "$IMAGE" "/dev/$DRIVE" || die "What's on /dev/$DRIVE doesn't match the image."

echo
echo "RetrOS is on /dev/$DRIVE, and it matches the image."
echo "Turn the computer off (poweroff), take out the install drive, and start it from the internal drive."
echo "On a Dell Inspiron 11 3147, that takes these BIOS settings (F2): Boot List Option: Legacy, SATA Operation: ATA."
