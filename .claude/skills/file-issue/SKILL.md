---
name: file-issue
description: Use when asked to file, write up or update a GitHub issue for a RetrOS-32 bug or follow-up, so a future agent can pick it up and implement it without the original conversation.
---

# Filing a RetrOS-32 issue

Issues go on the fork, with `--repo apbendi/RetrOS-32`. Write each one so that an agent with no other context can understand the problem, find the code, implement the fix and verify it. Be concise, but leave nothing out that's needed.

## Body outline

```
## Summary
What goes wrong, in a sentence or a few bullets. The user-visible symptom first, then the cause.
**Status:** what's reproduced (and how), and what's only from reading the code.

Line links point at `<short sha>`.

## Where
- [`function`](https://github.com/apbendi/RetrOS-32/blob/<full sha>/path/file.c#L10-L20): what it does wrong.
  - Sub-bullets for the details that matter (types, order of operations, who calls it).

## Repro steps            (when known; numbered, with the expected bad result)

## Proposed fix
1. Numbered steps, each naming the function to change.
   - Constraints, and things that look tempting but are wrong, with why.

## How to verify
- What to do and what should happen, including regression checks.
- Temporary test code to add and remove, if useful.
```

Add a short log excerpt in a code block when it's the evidence. Keep the full log out of the issue.

## Line links

1. **Pin links to a commit that's already pushed:** `git rev-parse HEAD`, after `git status -sb` shows no unpushed commits. Links to an unpushed SHA don't resolve.
2. **Take line numbers from that commit,** not from your working tree:
   ```
   git show <sha>:graphics/window.c | grep -n "gfx_window_resize\|kfree(old)"
   git show <sha>:graphics/window.c | awk 'NR>=194 && /^}/ {print NR; exit}'   # end of a function
   ```
3. **Check every range before filing.** An off-by-a-few link sends the next agent to the wrong code.

## Creating it

```
gh issue create --repo apbendi/RetrOS-32 --label bug --title "<symptom-first title>" --body-file <file>
```

- **Titles** describe the problem, not the fix, e.g. "Resizing a window leaves its new buffer uninitialized".
- **Write the body to a scratch file,** not inline, so quoting and backticks survive.
- **When several issues reference each other:**
  1. Create them all with a placeholder body, which assigns their numbers.
  2. Replace placeholders like `#LINE_ISSUE` in the bodies with the real numbers.
  3. Run `gh issue edit <n> --repo apbendi/RetrOS-32 --body-file <file>` for each.
  4. `grep -c "_ISSUE"` to confirm no placeholder is left.

## Updating and closing

- **If an existing issue is wrong,** edit its body with `gh issue edit --body-file`. If it's already closed, add a correcting comment with `gh issue comment`. Say what was wrong and what's true.
- **A commit whose message ends in `Fixes #N.`** closes the issue when it's pushed to `development`. Check afterwards with `gh issue view N --json state`.
- **If one issue covers several fixes,** say which ones a commit does and doesn't include, so nothing is silently dropped when it closes.
