---
name: debug-kernel-panic
description: Use when someone shares a RetrOS-32 serial log or screen showing a kernel panic, exception, page fault, "Out of memory!", assert, or a thread crash, and wants to know why it happened.
---

# Debugging a RetrOS-32 kernel panic

Work from the serial log. Don't boot QEMU yourself unless asked; ask for the full log instead. When you may boot it, the `run-in-qemu` skill can reproduce a crash without a display and stop in the kernel with lldb. See `AGENTS.md` ("Debugging crashes") for background.

## 1. Collect

- Get the **whole** serial log from boot, not just the panic. The lines before the crash show what was happening, e.g. `Clicked resize` or a window being created, and where allocations landed: `Stack: 0x…`, `Created new window … inner 0x…`.
- Ask what the user was doing at the time, and whether they can make it happen again.

## 2. Classify the crash

| Log shows | Meaning | Where the location comes from |
|---|---|---|
| `[exception] N <name> (<thread>)` followed by `eip:` | A CPU exception. If there's no `eip:` line, the kernel predates #5 | `eip`, and the backtrace's first line is the faulting function |
| `print_page_fault_info(): … address 0x…` | A page fault. CR2 is the address it touched | There's no `eip` (#8). The backtrace's second entry is a return address in the faulting function's *caller* |
| `kalloc(): Out of memory: N` | The 4 MB kernel heap has no contiguous block big enough | The backtrace, from inside `kalloc` |
| `assert` / `kernel_panic(): <reason>` | An explicit check failed | The reason string and the backtrace |

The PCB dump's `entry point` and its `esp`/`ebp` "at last switch" are **not** the crash location.

## 3. Map addresses to code

1. Check that `bin/symbols` comes from the kernel that booted: compare its timestamp with the last build and with `git log -1`. Code addresses in `symbols.map` and `bin/symbols` match `bin/kernelout`. String and data addresses can differ by a few KB.
2. Find the function containing an address with `rootfs/sysutil/symbols.map`, or with the backtrace's own `name: addr - base = offset` lines.
3. Disassemble around the address:
   ```
   objdump -d --no-show-raw-insn --start-address=0x<addr-0x30> --stop-address=0x<addr+0x10> bin/symbols
   ```
   - **For a page fault:** the instruction just before the return address is a `call`. The function it calls is where the fault happened.
   - **Then read the source** of that function. Work out which pointer it was dereferencing, which is often the first memory access.
4. To look up a global's address and its neighbors, use `nm -n -S bin/symbols`.

## 4. Decide whether it's corruption

If the faulting code is simple and correct, something else overwrote its memory. Look at the bad value:

- **Pixels:** small repeated bytes, especially the terminal's text color `0x1c`, or palette indices, mean drawing went past a buffer.
- **ASCII:** printable bytes mean a string overflowed.
- **Heap or stack addresses** in the wrong place mean a stale pointer or a use after free.

Then find what sits next to the victim:

- The heap is first fit, in boot order, and the log shows where many allocations landed.
- At boot, netd's stack comes right before the window manager's screen buffer, so anything drawn above the screen's top edge used to crash netd.
- Window buffers, including apps', all come from this heap.

Common writers, from past bugs:
- window drawing past a buffer's edge (#4, #7);
- a buffer swapped during a resize while drawing code wrote to it without holding `buffer_lock` (#9);
- a window frame drawn off screen.

## 5. Narrow it down and confirm

- **Reproduce reliably** if you can, and write down the exact steps.
- **Test exception output** by adding a temporary fault to `about()` (`kernel/kthreads/about.c`), which runs when the user opens About. Remove it afterwards.
  - Invalid opcode: `asm volatile("ud2");`
  - An exception with an error code: `asm volatile("movw $0x1234, %%ax\n\tmovw %%ax, %%ds" ::: "eax");`, which raises #GP with error code `0x1234`.
- **If you can't confirm it,** say so, and say what evidence would. For page faults, fixing #8 would show the exact instruction.

## 6. Report

Lead with the cause and how sure you are. Then give the evidence:
- the log lines;
- the address mapping;
- the code path;
- for corruption, why the bad value looks like what you think it is.

List secondary bugs you noticed separately. Propose a fix or an issue (see the `file-issue` skill). Don't change code unless asked.
