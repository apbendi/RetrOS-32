# The cases run.sh runs, from basic to advanced. Each is a function, registered with
#   add NAME "what it checks" FUNCTION
# A case starts R and M from a base image (begin), optionally sets both up with mtools (seed, then ready), makes
# the same change to both (put, put_chunks, put_at, create, or rfs and ref directly; refused for a change RetrOS
# should refuse), and checks the result (check, or check_fsck, check_same and check_reads). See run.sh for the
# helpers and base images.
# Sizes are picked around the 512-byte cluster RetrOS formats with.

# --- Format and mount ------------------------------------------------------------------------------------------

format_clean() {
    begin fresh || return
    [ -s "$CDIR/fsck-before" ] && fail fsck "$(paste -sd ';' "$CDIR/fsck-before")"
    rfs tree >/dev/null
}
add format.clean "A 32 MB disk RetrOS formats has nothing for fsck.fat to fix" format_clean

format_small() {
    begin small || return
    [ -s "$CDIR/fsck-before" ] && fail fsck "$(paste -sd ';' "$CDIR/fsck-before")"
    rfs tree >/dev/null
}
add format.small "The same for a 3 MB disk" format_small

format_virtual() {
    begin virtual || return
    [ -s "$CDIR/fsck-before" ] && fail fsck "$(paste -sd ';' "$CDIR/fsck-before")"
    rfs tree >/dev/null
}
add format.virtual-disk "The same for a 1 MB disk, the size of the virtual disk login formats" format_virtual

# --- Reading what other systems wrote --------------------------------------------------------------------------

read_real() {
    begin real || return
    rfs tree >/dev/null
    check_reads
}
add read.real-image "RetrOS reads the image make img builds, whose files Linux copied in" read_real

read_standard() {
    begin standard || return
    seed mcopy "$(data 3000 a)" ::/A.TXT
    seed mmd ::/SUB
    seed mcopy "$(data 100 b)" ::/SUB/B.TXT
    rfs tree >/dev/null && check_reads
}
add read.standard "RetrOS reads a FAT16 disk another system formatted (mkfs.fat's defaults)" read_standard

read_standard_oem() {
    begin standard-oem || return
    seed mcopy "$(data 3000 a)" ::/A.TXT
    seed mmd ::/SUB
    seed mcopy "$(data 100 b)" ::/SUB/B.TXT
    rfs tree >/dev/null && check_reads
}
add read.standard-geometry "The same, with the boot sector's name changed to RetrOS's, so only the layout differs" read_standard_oem

read_partitioned() {
    begin partitioned || return
    # mtools reaches the partition with image@@offset; RetrOS has to find it from the partition table
    local img
    for img in "$R" "$M"; do
        { mmd -i "$img@@1M" ::/SUB &&
            mcopy -i "$img@@1M" "$(data 3000 a)" ::/A.TXT &&
            mcopy -i "$img@@1M" "$(data 100 b)" ::/SUB/B.TXT; } >/dev/null 2>&1 || fail setup "mtools couldn't write the partition"
    done
    rfs tree >/dev/null || return
    rfs_tree "$R" > "$CDIR/tree-R-retros"
    ref_tree "$M@@1M" > "$CDIR/tree-M-mtools"
    diff -q "$CDIR/tree-R-retros" "$CDIR/tree-M-mtools" >/dev/null ||
        fail reads "$(diff "$CDIR/tree-M-mtools" "$CDIR/tree-R-retros" | grep '^[<>]' | head -6 | paste -sd ';' -)"
}
add read.partitioned "RetrOS finds and reads a FAT16 partition on a disk with a partition table" read_partitioned

read_multi_cluster() {
    begin fresh || return
    seed mcopy "$(data 20000 big)" ::/BIG.BIN
    rfs tree >/dev/null
    check_reads
}
add read.multi-cluster "RetrOS reads a 20,000-byte file, 40 clusters" read_multi_cluster

read_fragmented() {
    begin fresh || return
    seed mcopy "$(data 1500 a)" ::/A.TXT
    seed mcopy "$(data 1500 b)" ::/B.TXT
    seed mdel ::/A.TXT
    seed mcopy "$(data 4000 c)" ::/C.TXT
    rfs tree >/dev/null
    check_reads
}
add read.fragmented "RetrOS reads a file whose clusters aren't next to each other" read_fragmented

read_empty() {
    begin fresh || return
    : > "$WORK/empty"
    seed mcopy "$WORK/empty" ::/E.TXT
    rfs tree >/dev/null
    check_reads
}
add read.empty-file "RetrOS reads an empty file as other systems write it, with no cluster" read_empty

read_nested() {
    begin fresh || return
    seed mmd ::/D1
    seed mmd ::/D1/D2
    seed mcopy "$(data 700 f)" ::/D1/D2/F.TXT
    rfs tree >/dev/null
    check_reads
}
add read.nested "RetrOS reads a file two directories down" read_nested

read_deleted_gap() {
    begin fresh || return
    seed mmd ::/SUB
    seed mcopy "$(data 100 x)" ::/SUB/X.TXT
    seed mcopy "$(data 100 y)" ::/SUB/Y.TXT
    seed mcopy "$(data 100 z)" ::/SUB/Z.TXT
    seed mdel ::/SUB/Y.TXT
    rfs tree >/dev/null
    check_reads
}
add read.deleted-entry "RetrOS skips a deleted entry between two live ones" read_deleted_gap

read_big_directory() {
    begin fresh || return
    seed mmd ::/SUB
    for i in $(seq -w 1 20); do
        seed mcopy "$(data 100 "f$i")" "::/SUB/F$i.TXT"
    done
    rfs tree >/dev/null
    check_reads
}
add read.big-directory "RetrOS reads a directory of 20 files, which takes two clusters" read_big_directory

read_long_names() {
    begin fresh || return
    seed mmd ::/SUB
    MTOOLS_NO_VFAT=0 seed mcopy "$(data 100 long)" ::/SUB/longfilename.txt
    MTOOLS_NO_VFAT=0 seed mcopy "$(data 100 mixed)" ::/SUB/Mixed.Txt
    rfs tree >/dev/null
    check_reads
}
add read.long-names "RetrOS reads files that have long names by their short names" read_long_names

read_dotdot() {
    begin real || return
    local direct through
    direct=$("$FSTOOL" "$R" cat /LICENSE.TXT 2>/dev/null | sha256sum)
    through=$("$FSTOOL" "$R" cat /BIN/../LICENSE.TXT 2>/dev/null | sha256sum)
    [ "$direct" = "$through" ] || fail path "/BIN/../LICENSE.TXT doesn't read as /LICENSE.TXT"
    [ "$("$FSTOOL" "$R" ls /BIN/.. 2>/dev/null)" = "$("$FSTOOL" "$R" ls / 2>/dev/null)" ] ||
        fail path "ls /BIN/.. doesn't list / "
}
add read.dotdot "A path through .. back to the root directory reaches it" read_dotdot

# --- Writing --------------------------------------------------------------------------------------------------

write_small() {
    begin fresh || return
    put /A.TXT 100
    check
}
add write.small "RetrOS writes a 100-byte file" write_small

write_one_cluster() {
    begin fresh || return
    put /A.TXT 512
    check
}
add write.one-cluster "RetrOS writes a file of exactly one cluster" write_one_cluster

write_cluster_plus_one() {
    begin fresh || return
    put /A.TXT 513
    check
}
add write.cluster-plus-one "RetrOS writes a file one byte into its second cluster" write_cluster_plus_one

write_large() {
    begin fresh || return
    put /BIG.BIN 20000
    check
}
add write.large "RetrOS writes a 20,000-byte file" write_large

write_subdirectory() {
    begin fresh || return
    seed mmd ::/SUB
    ready
    put /SUB/A.TXT 700
    check
}
add write.subdirectory "RetrOS writes a file in a subdirectory" write_subdirectory

write_real() {
    begin real || return
    put /NEW.TXT 1500
    check
}
add write.real-image "RetrOS writes a file on the image make img builds" write_real

write_standard() {
    begin standard || return
    put /A.TXT 3000
    put /B.TXT 100
    check
}
add write.standard "RetrOS writes files on a FAT16 disk another system formatted, keeping both FAT copies the same" write_standard

write_lowercase() {
    begin fresh || return
    put /lower.txt 100
    check
}
add write.lowercase-name "RetrOS writes a file named in lowercase as the uppercase 8.3 name" write_lowercase

write_empty() {
    begin fresh || return
    create /E.TXT
    check
}
add write.empty-file "RetrOS creates an empty file, as the shell's new file does (#93)" write_empty

write_grow() {
    begin fresh || return
    put /A.TXT 100
    put /A.TXT 3000
    check
}
add write.grow "RetrOS replaces a file with a longer one" write_grow

write_shrink() {
    begin fresh || return
    put /A.TXT 3000
    put /A.TXT 100
    check
}
add write.shrink "RetrOS replaces a file with a shorter one (#94)" write_shrink

write_zero() {
    begin fresh || return
    put /A.TXT 100
    put /A.TXT 0
    check
}
add write.zero-bytes "RetrOS empties a file by writing 0 bytes (#85)" write_zero

write_pieces() {
    begin fresh || return
    put_chunks /A.TXT 1500 100
    check
}
add write.in-pieces "RetrOS writes a file as 15 writes of 100 bytes on one open file" write_pieces

write_at_offset() {
    begin fresh || return
    put /A.TXT 1500
    put_at /A.TXT 500 100
    check
}
add write.at-offset "RetrOS overwrites 100 bytes in the middle of a file, across a cluster boundary" write_at_offset

write_to_empty() {
    begin fresh || return
    : > "$WORK/empty"
    seed mcopy "$WORK/empty" ::/E.TXT
    seed mcopy "$(data 100 keep)" ::/KEEP.TXT
    ready
    put /E.TXT 100
    check
}
add write.to-empty-file "RetrOS writes to an empty file another system made, with no cluster (#93)" write_to_empty

write_to_directory() {
    begin fresh || return
    seed mmd ::/SUB
    seed mcopy "$(data 100 inside)" ::/SUB/IN.TXT
    ready
    refused write /SUB < "$(data 50 over)"
    check
}
add write.to-directory "RetrOS refuses to write over a directory, and it stays intact (#86)" write_to_directory

# --- Directory and disk limits ---------------------------------------------------------------------------------

limits_directory() {
    begin fresh || return
    seed mmd ::/SUB
    ready
    for i in $(seq -w 1 20); do
        put "/SUB/F$i.TXT" 100
    done
    check
}
add limits.full-directory "RetrOS writes 20 files to a directory, past one cluster's 16 entries (#88)" limits_directory

limits_root() {
    begin root16 || return
    # A FAT16 root directory has a fixed size, here 16 entries
    for i in $(seq -w 1 16); do
        put "/F$i.TXT" 100
    done
    refused write /F17.TXT < "$(data 100 f17)"
    check
}
add limits.full-root "RetrOS reports a full root directory and leaves the disk consistent" limits_root

limits_disk() {
    begin small || return
    refused write /HUGE.BIN < "$(data 4000000 huge)"
    check_fsck
}
add limits.full-disk "RetrOS reports a full disk, touching nothing outside it, and leaves it consistent" limits_disk

# --- Delete, mkdir, rmdir, rename ------------------------------------------------------------------------------

ops_remove() {
    begin fresh || return
    seed mcopy "$(data 1500 a)" ::/A.TXT
    ready
    rfs rm /A.TXT
    ref mdel ::/A.TXT
    check
}
add ops.remove "RetrOS deletes a file and frees its clusters (#88)" ops_remove

ops_mkdir() {
    begin fresh || return
    rfs mkdir /NEWDIR
    ref mmd ::/NEWDIR
    check
}
add ops.mkdir "RetrOS makes a directory (#88)" ops_mkdir

ops_rmdir() {
    begin fresh || return
    seed mmd ::/OLD
    ready
    rfs rmdir /OLD
    ref mrd ::/OLD
    check
}
add ops.rmdir "RetrOS removes an empty directory (#88)" ops_rmdir

ops_rename() {
    begin fresh || return
    seed mcopy "$(data 100 a)" ::/A.TXT
    ready
    rfs mv /A.TXT /B.TXT
    ref mren ::/A.TXT B.TXT
    check
}
add ops.rename "RetrOS renames a file" ops_rename
