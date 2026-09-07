# EncFS — Team Assignments

Everyone works against the same skeleton. Function signatures are already
fixed in the headers so your pieces plug together without renegotiating
interfaces later. Work each problem out from the signature, the comments,
and the constraints below — don't look up a finished solution.

Shared struct (`include/encfs.h`):
```c
struct encfs_context {
    char backing_path[PATH_MAX];
    unsigned char key[32];
};
#define ENCFS_CTX ((struct encfs_context *)fuse_get_context()->private_data)
```

---

## Q0 — Warm-up (`src/path_utils.c`) — whoever wants it first

**Problem**: Every FUSE callback needs to translate a *virtual* path (what
the user sees, e.g. `/notes.txt`) into the *real* path on the backing disk.
Implement `get_backing_path(char *dest, const char *path)` using
`ENCFS_CTX->backing_path`.

Constraint: use a bounded string function, not raw concatenation — what
goes wrong if you don't bound it?

---

## Person 1 — Crypto module (`src/crypto.c`)

You're not implementing an encryption algorithm — libsodium does that.
You're implementing the packaging: how a nonce and ciphertext combine into
one blob on disk, and unpack again. `tests/test_crypto.c` is a ready-made
test harness — build it with `make test-crypto` to check your work as you go.

### Q1 — `encfs_encrypt`
```c
int encfs_encrypt(const unsigned char *plaintext, size_t plaintext_len,
                   unsigned char *out_backing_data, const unsigned char *key);
```
Work through:
- Where does the nonce come from — what breaks if two writes reuse one?
- `out_backing_data`'s required size, in terms of `crypto.h`'s constants —
  who's responsible for allocating that, this function or its caller?

### Q2 — `encfs_decrypt`
```c
int encfs_decrypt(const unsigned char *backing_data, size_t backing_len,
                   unsigned char *out_plaintext, const unsigned char *key);
```
Work through:
- How do you split `backing_data` into "the nonce part" and "the ciphertext
  part"?
- What should happen if `backing_len` is smaller than the minimum valid
  blob size — attempt decryption anyway, or reject immediately?
- `test_crypto.c` checks that a tampered blob *fails* to decrypt rather than
  producing corrupted output silently — why does that distinction matter
  for a filesystem specifically?

---

## Person 2 — Metadata & directory operations (`src/fuse_ops.c`)

No encryption here — you're delegating to the real filesystem underneath,
with one adjustment.

### Q1 — `encfs_getattr`
The file on disk is bigger than the real file (nonce + MAC overhead). What
correction has to happen to `st_size` before returning, and what's the
formula? What if the corrected size would go negative?

### Q2 — `encfs_readdir`
Pure passthrough — list the real directory's entries and report each one to
FUSE via `filler`. Look up: `opendir`/`readdir`.

### Q3-Q5 — `encfs_mkdir`, `encfs_rmdir`, `encfs_unlink`
Each is "call the matching real syscall on the backing path, translate a
failure into the right return value via `errno`."

### Q6-Q8 — `encfs_chmod`, `encfs_chown`, `encfs_utimens`
Same pattern. Look up: `chmod()`, `lchown()` (why the `l`?), `utimensat()`.

---

## Person 3 — File handles & read (`src/fuse_ops.c`)

### Q1 — `encfs_create` / Q2 — `encfs_open`
Open the backing file with the right flags, store the descriptor in `fi->fh`
for later calls. What's different between "creating new" vs "opening
existing"?

### Q3 — `encfs_release`
What resource opened in Q1/Q2 needs cleanup here?

### Q4 — `encfs_read` (the big one)
You can't decrypt "just the requested slice" — why not, given how
`encfs_encrypt` packaged the data?

Work through: get the full encrypted blob → decrypt the whole thing (calls
Person 1's function) → extract just `[offset, offset+size)` from the result.
Edge cases: offset past EOF, offset+size past EOF.

---

## Person 4 — Write, truncate, and program setup

### Q1 — `encfs_write` (the big one, `src/fuse_ops.c`)
A write to the middle of an existing file requires: decrypt what's there,
apply your edit in memory, re-encrypt and rewrite the *entire* thing — the
nonce/MAC cover the whole file, not a delta.

Work through: get current plaintext (reuse Person 3's decrypt-the-whole-file
logic) → what fills the gap on a sparse write past current EOF? → re-encrypt
with a *fresh* nonce → replace the backing file's contents entirely.

### Q2 — `encfs_truncate` (`src/fuse_ops.c`)
Same decrypt → modify → re-encrypt → rewrite pattern, different
modification step (pad with zeros when growing, cut off when shrinking).
Special case: `size == 0` — do you need to decrypt anything first?

### Q3 — `main.c` setup
Work through:
- Where does the key come from (env var, per the README's `VAULT_KEY`) —
  what's the security downside of a command-line argument instead?
- How do you turn an arbitrary-length passphrase string into exactly 32
  key bytes? (Look at libsodium's hashing functions, not its encryption
  ones.)
- The backing directory argument isn't a FUSE option — how do you resolve
  it to an absolute path and keep it out of what gets passed to
  `fuse_main`?

---

## Combining everyone's work

1. Start in parallel — no one needs to wait to begin.
2. Person 1 should finish first if possible: Q1/Q2 don't depend on FUSE at
   all, and `make test-crypto` gives immediate pass/fail feedback.
3. Person 3 and Person 4 both call Person 1's functions — coordinate on
   when that's ready.
4. Once everything compiles, `make` builds the real binary and
   `make test-fs` runs the full mount/write/read/diff integration script —
   that's when bugs from mismatched assumptions between people surface.
