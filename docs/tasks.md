# EncFS — Task Breakdown (dependency-ordered)

Chunks are ordered so each one only depends on chunks above it. Size/team
doesn't change the ordering — only how many chunks run in parallel.
Status reflects what's actually in the working tree right now, not intent.

| # | Chunk | Depends on | Status |
|---|-------|------------|--------|
| 1 | **Design doc** — architecture, on-disk format, key management, threat model | — | Done — [docs/design.md](design.md) |
| 2 | **Repo scaffolding** — git repo scoped correctly, `.gitignore` for build artifacts | — | Done |
| 3 | **Shared headers** — `struct encfs_context`, function signatures fixed in `include/*.h` so pieces plug together without renegotiating interfaces | 1 | Done |
| 4 | **Path utils** — `get_backing_path` (`src/path_utils.c`) | 3 | **Open — [issue #2](https://github.com/jpsiddharth2008/cryptovault-fs/issues/2)**, skeleton only |
| 5 | **Crypto module** — `encfs_encrypt` / `encfs_decrypt` (`src/crypto.c`) | 3 | **Open — [issue #3](https://github.com/jpsiddharth2008/cryptovault-fs/issues/3) / [#4](https://github.com/jpsiddharth2008/cryptovault-fs/issues/4)**, skeleton only |
| 6 | **Crypto unit tests** — `tests/test_crypto.c`, `make test-crypto` | 5 | Done — pre-built test harness, will fail until 5 is implemented (expected) |
| 7 | **Metadata & directory ops** — `getattr`, `readdir`, `mkdir`, `rmdir`, `unlink`, `chmod`, `chown`, `utimens` | 4 | **Open — [issues #6-#9](https://github.com/jpsiddharth2008/cryptovault-fs/issues)**, skeleton only |
| 8 | **File handles & read** — `create`, `open`, `release`, `read` | 4, 5 | **Open — [issues #10-#12](https://github.com/jpsiddharth2008/cryptovault-fs/issues)**, skeleton only |
| 9 | **Write & truncate** — `write`, `truncate` | 4, 5, 8 | **Open — [issues #13-#14](https://github.com/jpsiddharth2008/cryptovault-fs/issues)**, skeleton only |
| 10 | **Program setup** — `main.c`: arg parsing, passphrase → key derivation, `fuse_main` wiring | 5 | **Open — [issue #15](https://github.com/jpsiddharth2008/cryptovault-fs/issues/15)**, skeleton only |
| 11 | **Wire up `fuse_operations` table** | 7, 8, 9 | **Open — [issue #16](https://github.com/jpsiddharth2008/cryptovault-fs/issues/16)**, currently zero-initialized |
| 12 | **Integration test** — `tests/test_fs.sh`, `make test-fs` | 7, 8, 9, 10, 11 | Done — pre-built test harness, will fail until 7-11 are implemented (expected) |
| 13 | **CI** — run `make` + `make test` on every push/PR | 6, 12 | Done — [.github/workflows/ci.yml](../.github/workflows/ci.yml). Will show **red** until the issues above are implemented — that's expected, not a bug in the CI config. |
| 14 | **CONTRIBUTING.md** — branch naming, commit style, review expectations | — | Done — [CONTRIBUTING.md](../CONTRIBUTING.md) |
| 15 | **Branch protection + PR review flow** on the remote | 13, 14 | **Not started** — rules documented in CONTRIBUTING.md, CI exists to require, but branch protection itself isn't turned on in GitHub's repo settings yet |
| 16 | **Future work** (from design doc §7): filename encryption, Argon2 KDF, block-based read/write, key rotation | 1–12 | Backlog, not scheduled |

## Reading the table

- Chunks 4–11 are the actual filesystem implementation, and they are
  **skeletons only** — function signatures and TODO comments, no working
  logic. This is intentional: they're the actual learning exercise, tracked
  one-for-one as GitHub Issues (see the Issues tab), and are meant to be
  implemented by the team, not read off a reference solution.
- Chunks 6 and 12 (the two test files) are the exception — they're
  pre-built, ready-made test harnesses given to the team up front (per
  `assignments.md`), not something anyone implements. They exist so you
  have immediate pass/fail feedback (`make test-crypto`, `make test-fs`)
  as you fill in chunks 4–11. They will fail until those chunks are done —
  that's the whole point of having them early.
- Chunk 5 (crypto) is the one everything downstream needs — correctly,
  `assignments.md` already flags it as the priority to finish first.
- Chunks 4, 5, and 10 have no dependency on each other and can start in
  parallel; 7 only needs 4; 8 and 9 need both 4 and 5.
- Chunk 16 is deliberately last and unscheduled: it's scope the design doc
  flagged as a known limitation, not something blocking a working v1.

## Suggested immediate order

Foundation (1–3) and process setup (13, 14) are done. What's left before
the filesystem actually works is entirely chunks 4–11 — pick an issue,
follow [CONTRIBUTING.md](../CONTRIBUTING.md)'s branch/PR flow, and open a
PR that closes it. CI (chunk 13) will run automatically on that PR and
tell you if `make`/`make test-crypto` still pass.

```
4 (path utils) ──┬──> 7 (metadata ops)
                  ├──> 8 (file handles + read) ──┐
5 (crypto)     ───┴──> 9 (write + truncate) ─────┼──> 11 (wire fuse_operations) ──> 12 (integration test passes)
                       10 (main.c setup) ─────────┘
```

15 (branch protection) is the last process piece — turn it on once CI
(13) is green at least once, so "require CI to pass" has something real
to point at.
