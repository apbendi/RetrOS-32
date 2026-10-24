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
add read.long-names "RetrOS reads files that have long names, by those names" read_long_names

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
add write.empty-file "RetrOS creates an empty file, as the shell's touch does (#93)" write_empty

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

write_append() {
    begin fresh || return
    put /A.TXT 1500
    append /A.TXT 700
    check
}
add write.append "RetrOS adds 700 bytes to the end of a 1,500-byte file, opened to append" write_append

write_past_end() {
    begin fresh || return
    put /A.TXT 100
    refused pwrite /A.TXT 200 < "$(data 50 gap)"
    check
}
add write.past-end "RetrOS refuses a write that starts past the end of the file, and leaves the file as it was" write_past_end

write_truncate() {
    begin fresh || return
    put /A.TXT 3000
    rfs truncate /A.TXT
    : > "$WORK/empty"
    ref mcopy -o "$WORK/empty" ::/A.TXT
    check
}
add write.truncate "RetrOS empties a 3,000-byte file by opening it with the truncate flag" write_truncate

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

write_long_names() {
    begin fresh || return
    ready
    local file
    file=$(data 1500 long)
    rfs mkdir "/My Projects"
    MTOOLS_NO_VFAT=0 ref mmd "::/My Projects"
    rfs write "/My Projects/Long File Name.txt" < "$file"
    MTOOLS_NO_VFAT=0 ref mcopy -o "$file" "::/My Projects/Long File Name.txt"
    check
}
add write.long-names "RetrOS makes a directory and writes a file, both with long names with spaces" write_long_names

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

limits_long_names() {
    begin fresh || return
    seed mmd ::/SUB
    ready
    local file
    for i in $(seq -w 1 20); do
        file=$(data 100 "long$i")
        rfs write "/SUB/A long file name, number $i.txt" < "$file"
        MTOOLS_NO_VFAT=0 ref mcopy -o "$file" "::/SUB/A long file name, number $i.txt"
    done
    check
}
add limits.long-names "RetrOS writes 20 files with long names to a directory, each taking 4 entries" limits_long_names

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

ops_rename_move() {
    begin fresh || return
    seed mcopy "$(data 700 a)" ::/A.TXT
    seed mmd ::/SUB
    ready
    rfs mv /A.TXT /SUB/A.TXT
    ref mmove ::/A.TXT ::/SUB/A.TXT
    check
}
add ops.rename-into-directory "RetrOS moves a file into a subdirectory by renaming it" ops_rename_move

ops_rename_existing() {
    begin fresh || return
    seed mcopy "$(data 100 a)" ::/A.TXT
    seed mcopy "$(data 100 b)" ::/B.TXT
    ready
    refused mv /A.TXT /B.TXT
    check
}
add ops.rename-onto-existing "RetrOS refuses to rename a file onto one that exists, and changes neither" ops_rename_existing

ops_rename_into_itself() {
    begin fresh || return
    seed mmd ::/SUB
    seed mmd ::/SUB/INNER
    MTOOLS_NO_VFAT=0 seed mmd "::/SUB/INNER/Long Name"
    ready
    refused mv /SUB /SUB/INNER/SUB
    refused mv /sub /Sub/inner/SUB
    refused mv /SUB "/SUB/INNER/long name/SUB"
    refused mv /SUB/INNER /SUB/INNER/X
    check
}
add ops.rename-into-itself "RetrOS refuses to move a directory inside itself, however the path spells it" ops_rename_into_itself

ops_rename_directory() {
    begin fresh || return
    seed mmd ::/SUB
    seed mmd ::/SUB/INNER
    seed mcopy "$(data 900 in)" ::/SUB/INNER/IN.TXT
    seed mmd ::/OTHER
    ready
    rfs mv /SUB/INNER /OTHER/INNER
    ref mmove ::/SUB/INNER ::/OTHER/INNER
    check
}
add ops.rename-directory "RetrOS moves a directory, with a file in it, into another" ops_rename_directory

ops_rename_case() {
    begin fresh || return
    seed mmd ::/DOCS
    seed mcopy "$(data 100 a)" ::/DOCS/A.TXT
    ready
    rfs mv /DOCS /Docs
    rfs mv /docs/A.TXT /docs/a.Txt
    check
    [ "$("$FSTOOL" "$R" stat /docs 2>/dev/null)" = "D Docs" ] || fail names "/DOCS wasn't renamed Docs"
    [ "$("$FSTOOL" "$R" stat /docs/a.txt 2>/dev/null)" = "F a.Txt 100" ] || fail names "/DOCS/A.TXT wasn't renamed a.Txt"
}
add ops.rename-case "RetrOS changes only the case of a directory's or a file's name" ops_rename_case

ops_remove_directory() {
    begin fresh || return
    seed mmd ::/SUB
    ready
    refused rm /SUB
    check
}
add ops.remove-directory "RetrOS refuses to delete a directory as a file" ops_remove_directory

ops_rmdir_not_empty() {
    begin fresh || return
    seed mmd ::/SUB
    seed mcopy "$(data 100 in)" ::/SUB/IN.TXT
    ready
    refused rmdir /SUB
    check
}
add ops.rmdir-not-empty "RetrOS refuses to remove a directory that has files in it" ops_rmdir_not_empty

ops_mkdir_exists() {
    begin fresh || return
    seed mmd ::/SUB
    ready
    refused mkdir /SUB
    check
}
add ops.mkdir-exists "RetrOS refuses to make a directory that already exists" ops_mkdir_exists

ops_readdir() {
    begin fresh || return
    seed mmd ::/SUB
    MTOOLS_NO_VFAT=0 seed mcopy "$(data 100 long)" ::/SUB/longfilename.txt
    MTOOLS_NO_VFAT=0 seed mcopy "$(data 200 mixed)" ::/SUB/Mixed.Txt
    MTOOLS_NO_VFAT=0 seed mcopy "$(data 300 space)" "::/SUB/My Notes.txt"
    seed mmd ::/SUB/INNER
    printf '%s\n' "D INNER" "F Mixed.Txt 200" "F My Notes.txt 300" "F longfilename.txt 100" > "$CDIR/dir-expected"
    "$FSTOOL" "$R" dir /SUB 2> "$CDIR/err" | sort > "$CDIR/dir-retros"
    diff -q "$CDIR/dir-expected" "$CDIR/dir-retros" >/dev/null ||
        fail names "$(diff "$CDIR/dir-expected" "$CDIR/dir-retros" | grep '^[<>]' | head -6 | paste -sd ';' -)"
    [ "$("$FSTOOL" "$R" stat "/sub/my notes.TXT" 2>/dev/null)" = "F My Notes.txt 300" ] ||
        fail names "stat /sub/my notes.TXT didn't report My Notes.txt, 300 bytes"
    [ "$("$FSTOOL" "$R" stat /SUB/INNER 2>/dev/null)" = "D INNER" ] || fail names "stat /SUB/INNER didn't report a directory"
}
add ops.readdir-long-names "RetrOS lists a directory with its long names, and stats files by them, ignoring case" ops_readdir

# same_entry same|different PATH OTHER: checks what fs_same_entry says about two paths on R
same_entry() {
    local got
    got=$("$FSTOOL" "$R" same-entry "$2" "$3" 2>"$CDIR/err")
    [ "$got" = "$1" ] || fail entry "$2 and $3: fs_same_entry said ${got:-nothing}, not $1"
}

ops_same_entry() {
    begin fresh || return
    seed mmd ::/SUB
    MTOOLS_NO_VFAT=0 seed mmd "::/SUB/My Docs"
    MTOOLS_NO_VFAT=0 seed mcopy "$(data 300 notes)" "::/SUB/My Notes.txt"
    seed mcopy "$(data 0 one)" ::/SUB/E1.TXT
    seed mcopy "$(data 0 two)" ::/SUB/E2.TXT
    ready
    same_entry same "/SUB/My Notes.txt" "/sub/my notes.TXT"
    same_entry same "/SUB/My Notes.txt" /SUB/MYNOTE~1.TXT
    same_entry same "/SUB/My Docs" "/sub/../SUB/mydocs~1"
    same_entry same / /SUB/..
    same_entry different /SUB/E1.TXT /SUB/E2.TXT
    same_entry different /SUB "/SUB/My Docs"
    same_entry different "/SUB/My Notes.txt" "/SUB/My Docs"
    refused same-entry /SUB/NONE.TXT /SUB/E1.TXT
    check
}
add ops.same-entry "RetrOS tells whether two paths are one file or directory, however they spell it" ops_same_entry

# --- The current directory ------------------------------------------------------------------------------------

paths_relative() {
    begin fresh || return
    seed mmd ::/SUB
    seed mmd ::/SUB/INNER
    ready
    local file
    file=$(data 600 rel)
    FSTOOL_CWD=/SUB rfs write a.txt < "$file"
    ref mcopy -o "$file" ::/SUB/A.TXT
    check
    [ "$(FSTOOL_CWD=/SUB/INNER "$FSTOOL" "$R" cat ../a.txt 2>/dev/null | sha256sum)" = "$(sha256sum < "$file")" ] ||
        fail path "../a.txt from /SUB/INNER doesn't read /SUB/A.TXT"
    [ "$(FSTOOL_CWD=/SUB/INNER "$FSTOOL" "$R" pwd 2>/dev/null)" = "/SUB/INNER" ] || fail path "pwd in /SUB/INNER isn't /SUB/INNER"
    [ "$(FSTOOL_CWD=sub/./inner/.. "$FSTOOL" "$R" pwd 2>/dev/null)" = "/sub" ] || fail path "cd sub/./inner/.. from / doesn't end in /sub"
}
add paths.relative "Relative paths start from the current directory, and .. leaves it" paths_relative

paths_chdir_file() {
    begin fresh || return
    seed mcopy "$(data 100 a)" ::/A.TXT
    ready
    FSTOOL_CWD=/A.TXT "$FSTOOL" "$R" pwd > /dev/null 2> "$CDIR/err" && fail op "cd to the file /A.TXT succeeded"
    FSTOOL_CWD=/NOPE "$FSTOOL" "$R" pwd > /dev/null 2>> "$CDIR/err" && fail op "cd to /NOPE, which doesn't exist, succeeded"
    check
}
add paths.chdir-refused "The current directory can't be a file, or something that doesn't exist" paths_chdir_file
