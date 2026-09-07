# EncFS — Task Breakdown (dependency-ordered)

Chunks are ordered so each one only depends on chunks above it. Size/team
doesn't change the ordering — only how many chunks run in parallel.
Status reflects what's actually in the working tree right now, not intent.

| # | Chunk | Depends on | Status |
|---|-------|------------|--------|
| 1 | **Design doc** — architecture, on-disk format, key management, threat model | — | Done — [docs/design.md](design.md) |
| 2 | **Repo scaffolding** — git repo scoped correctly, `.gitignore` for build artifacts | — | Done |
| 3 | **Shared headers** — `struct encfs_context`, function signatures fixed in `include/*.h` so pieces plug together without renegotiating interfaces | 1 | Done |
| 4 | **Path utils** — `get_backing_path` (`src/path_utils.c`) | 3 | Done |
| 5 | **Crypto module** — `encfs_encrypt` / `encfs_decrypt` (`src/crypto.c`) | 3 | Done |
| 6 | **Crypto unit tests** — `tests/test_crypto.c`, `make test-crypto` | 5 | Done |
| 7 | **Metadata & directory ops** — `getattr`, `readdir`, `mkdir`, `rmdir`, `unlink`, `chmod`, `chown`, `utimens` | 4 | Done |
| 8 | **File handles & read** — `create`, `open`, `release`, `read` | 4, 5 | Done |
| 9 | **Write & truncate** — `write`, `truncate` | 4, 5, 8 | Done |
| 10 | **Program setup** — `main.c`: arg parsing, passphrase → key derivation, `fuse_main` wiring | 5 | Done |
| 11 | **Integration test** — `tests/test_fs.sh`, `make test-fs` | 7, 8, 9, 10 | Done |
| 12 | **CI** — run `make test` on every push/PR | 6, 11 | **Not started** |
| 13 | **CONTRIBUTING.md** — branch naming, commit style, review expectations | — | **Not started** |
| 14 | **Branch protection + PR review flow** on the remote | 12, 13 | **Not started** |
| 15 | **Future work** (from design doc §7): filename encryption, Argon2 KDF, block-based read/write, key rotation | 1–11 | Backlog, not scheduled |

## Reading the table

- Chunks 3–11 are the actual filesystem: they mirror `assignments.md`'s
  per-person split, but ordered by what unblocks what rather than by who's
  doing it. Chunk 5 (crypto) is the one everything downstream needs —
  correctly, `assignments.md` already flags it as the priority to finish
  first.
- Chunks 4, 5, and 10 have no dependency on each other and can start in
  parallel; 7 only needs 4; 8 and 9 need both 4 and 5.
- Everything through chunk 11 is **already implemented** in the working
  tree — there's just no CI, no PR flow, and no commit history yet, which
  is what chunks 12–14 close.
- Chunk 15 is deliberately last and unscheduled: it's scope the design doc
  flagged as a known limitation, not something blocking a working v1.

## Suggested immediate order

Since 1–11 are done, the next dependency chain to actually execute is:

```
12 (CI) ──┐
13 (CONTRIBUTING) ──┴──> 14 (branch protection / PR review)
```

12 and 13 have no dependency on each other — do them in parallel (or in
either order). 14 needs both, since branch protection requiring "CI passes"
and "review required" only makes sense once there's a CI job and a
documented review expectation to point to.
