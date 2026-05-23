# Embed commit SHA + message in built `index.html`

When sharing the artifact, it was previously impossible to tell which
commit a given `build/index.html` was produced from. The artifact now
carries that info inline.

## What changed

- `src/platform/shell.html` gains a small `<p id="build-info">` line at
  the bottom of the `#info` block: `build: {{BUILD_INFO}}`. Styled
  subtle (`#666`, 11px) so it doesn't compete with the legend.
- `Makefile`: the `$(OUT)` recipe now generates `build/shell.html` from
  `src/platform/shell.html` with `{{BUILD_INFO}}` substituted to
  `<short-sha>[(dirty)] — <commit subject>`. `emcc --shell-file` is
  pointed at the generated copy.
- HTML-special chars (`&`, `<`, `>`) in the commit subject are escaped
  before sed substitution, so a `<` in a commit message won't break the
  page.
- `(dirty)` is appended when the working tree or index differs from
  HEAD, so it's obvious when a shared artifact wasn't built from a
  clean commit.

## What is *not* tracked

If you commit without changing any source file and re-run `make`, the
existing `build/index.html` is up-to-date by make's dep graph and won't
be regenerated — its embedded commit info stays at the previous build's
commit. To force a refresh: `make clean && make`. This is an accepted
trade-off; adding `.git/HEAD` as a dep was considered but rejected as
fragile (packed-refs, worktrees, detached HEAD).

## Verification

`grep -ao 'build:[^<]*' build/index.html` after `make clean && make`
shows e.g. `build: c469cac (dirty) — change up creeps to be doubling`.
emcc minifies the shell, so single-line `grep` without `-a` won't match
— this is expected.
