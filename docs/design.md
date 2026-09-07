# EncFS — Design Document

## 1. Overview

EncFS is a user-space encrypting filesystem. It mounts a directory (the
"mountpoint") backed by a second, hidden directory (the "backing store") via
FUSE3. Every file a user reads or writes through the mountpoint is
transparently decrypted or encrypted with libsodium's authenticated
encryption on the way through, so the backing store never holds plaintext
or an unauthenticated (tamperable) ciphertext.

## 2. Architecture

```
Application --> VFS --> FUSE kernel module --> our userspace program (encfs) --> backing store
```

1. An application calls a normal syscall (`open`, `read`, `write`, ...) on a
   path under the mountpoint.
2. The kernel VFS routes it to the FUSE kernel module because the
   mountpoint is a FUSE mount.
3. The kernel module forwards the request over `/dev/fuse` to our userspace
   process, which dispatches it to the matching callback in
   `encfs_oper` (`src/fuse_ops.c`).
4. The callback maps the virtual path to the real path on the backing disk
   (`get_backing_path`, `src/path_utils.c`) and performs the real syscall
   there, running plaintext through `encfs_encrypt`/`encfs_decrypt`
   (`src/crypto.c`) on the way.
5. The result is returned back up through FUSE to the kernel, and from the
   kernel back to the application, indistinguishable from a normal
   filesystem call.

**Read round trip:** `encfs_read` opens the backing file (fd already cached
in `fi->fh` from `encfs_open`/`encfs_create`), reads the entire encrypted
blob, calls `encfs_decrypt` on the whole thing, then copies out just
`[offset, offset+size)` of the resulting plaintext to satisfy the caller's
request.

**Write round trip:** `encfs_write` reads and decrypts the current file in
full (or starts from empty), applies the caller's edit to the in-memory
plaintext buffer at the given offset (zero-padding any gap for a sparse
write past current EOF), re-encrypts the whole buffer under a **fresh**
nonce, and overwrites the backing file's contents entirely. There is no
in-place/delta write — the MAC covers the whole file, so any change
requires a full re-encrypt.

## 3. On-disk file format

Each file in the backing store is laid out as:

```
[nonce (NONCE_SIZE bytes)][ciphertext + MAC (plaintext_len + MAC_SIZE bytes)]
```

Concretely, with libsodium's `crypto_secretbox` (XSalsa20-Poly1305):
`NONCE_SIZE` = `crypto_secretbox_NONCEBYTES`, `MAC_SIZE` =
`crypto_secretbox_MACBYTES`, and `CRYPTO_OVERHEAD = NONCE_SIZE + MAC_SIZE`
bytes are added to every file (`include/crypto.h`).

- **The nonce is stored, not secret.** `crypto_secretbox` requires a
  unique nonce per encryption under the same key, but nonce secrecy buys
  nothing — security comes entirely from the key. Storing it in the clear
  next to the ciphertext lets decryption reconstruct exactly what was used
  at encrypt time, with no separate key-management problem for it.
- **A fresh, random nonce is generated on every write.** Reusing a nonce
  with the same key breaks XSalsa20-Poly1305's confidentiality and
  authentication guarantees. Since every write rewrites the entire file
  (see Architecture), generating a new random nonce per write is cheap and
  removes any risk of nonce reuse across versions of the same file.
- The MAC lets `encfs_decrypt` detect any bit-flip, truncation, or
  corruption of the stored blob and fail closed (return -1) rather than
  hand back garbage plaintext.

## 4. Key management

- The key is derived once, at mount time, in `main.c`: a passphrase comes
  from the `VAULT_KEY` environment variable (or an interactive
  `getpass()` prompt if unset), and `crypto_generichash` (BLAKE2b) hashes
  it down to exactly `KEY_SIZE` (32) bytes. The passphrase buffer is
  zeroed (`sodium_memzero`) immediately after derivation.
- The derived key is stored once in `struct encfs_context` for the
  lifetime of the mount and reused for every file's encrypt/decrypt call.
- **Limitations for production use:**
  - `crypto_generichash` is a fast hash, not a deliberately slow
    password-based KDF (e.g. Argon2). A short or weak passphrase is
    brute-forceable at hashing speed, not KDF speed.
  - There's no salt, so the same passphrase always derives the same key —
    no protection against rainbow-table-style precomputation across
    vaults.
  - Passing the passphrase via `VAULT_KEY` env var is visible to any
    process that can read `/proc/<pid>/environ` for the mount process, and
    may leak into shell history or process listings depending on how it's
    set.
  - One key for the whole vault: no per-file or per-user keys, no key
    rotation, no revocation.
- **With more time**, we'd derive the key with Argon2id
  (`crypto_pwhash`) using a random per-vault salt stored alongside the
  backing directory, and consider per-user keys layered under a
  vault-wide key so access could be revoked without re-encrypting
  everything.

## 5. VFS operations implemented

| FUSE callback      | What it does                                                              | Delegates to                          |
|---------------------|----------------------------------------------------------------------------|----------------------------------------|
| `encfs_getattr`     | Stats the backing file, corrects `st_size` to the plaintext size          | `lstat` + size correction (`- CRYPTO_OVERHEAD`, floored at 0) |
| `encfs_readdir`     | Lists real directory entries, reports each to FUSE                       | `opendir`/`readdir`                   |
| `encfs_mkdir`       | Creates the backing directory                                            | `mkdir`                               |
| `encfs_rmdir`       | Removes the backing directory                                            | `rmdir`                               |
| `encfs_unlink`      | Deletes the backing file                                                 | `unlink`                              |
| `encfs_chmod`       | Changes permissions on the backing file                                 | `chmod`                               |
| `encfs_chown`       | Changes ownership on the backing file                                   | `lchown`                              |
| `encfs_utimens`     | Updates access/modify times on the backing file                         | `utimensat`                           |
| `encfs_create`      | Creates + opens a new backing file, stashes fd in `fi->fh`               | `open(O_CREAT\|...)`                  |
| `encfs_open`        | Opens an existing backing file, stashes fd in `fi->fh`                   | `open`                                |
| `encfs_release`     | Closes the fd opened in `create`/`open`                                  | `close`                               |
| `encfs_read`        | Reads + decrypts the whole backing file, slices out `[offset, offset+size)` | `read` + `encfs_decrypt`          |
| `encfs_write`       | Decrypts whole file, applies edit in memory, re-encrypts, rewrites whole file | `read`/`write`/`ftruncate` + `encfs_encrypt`/`encfs_decrypt` |
| `encfs_truncate`    | Decrypt → pad with zeros (grow) or cut (shrink) → re-encrypt → rewrite   | same pattern as write                 |

## 6. Threat model

**What this protects against:**
- Someone with offline access to the backing disk (a stolen drive, a
  leaked backup, a snapshot) cannot read file contents without the key.
- Undetected tampering with a file's ciphertext: `encfs_decrypt` fails
  closed on any modification to the stored blob, rather than returning
  corrupted plaintext silently.

**What this does NOT protect against:**
- Anyone with access to the *live, mounted* filesystem — mounting is an
  all-or-nothing unlock, not a per-request auth check.
- A weak or reused passphrase (see Key management limitations above).
- `VAULT_KEY` leaking through the environment, shell history, or process
  inspection tools.
- Memory dumps or swap while the process holds the derived key or
  plaintext buffers.
- Filename or directory-structure disclosure — see Known limitations.
- Traffic/metadata analysis: file sizes, mtimes, and access patterns are
  all visible on the backing store.

## 7. Known limitations / future work

- Filenames and directory structure are **not** encrypted — only file
  *contents* are.
- No per-user access control; the whole vault shares one key.
- No password-based key derivation (Argon2/scrypt) or per-vault salt —
  see Key management.
- Every read/write reprocesses the *entire* file rather than operating on
  blocks/pages — fine for small files, but writes get expensive and
  memory-hungry as file size grows. A block-based format (independent
  nonce+MAC per fixed-size chunk) would fix this at the cost of a more
  complex on-disk layout.
- No key rotation or re-encryption tooling if `VAULT_KEY` is ever
  suspected compromised.
- No concurrent-access locking beyond what the OS gives file descriptors
  for free; two processes writing the same file through the mount
  simultaneously can race on the decrypt-modify-encrypt cycle.

## 8. Testing

- `tests/test_crypto.c` (run via `make test-crypto`) is a unit-test
  harness for the crypto module in isolation, independent of FUSE. It
  checks: a plaintext round-trips correctly through
  `encfs_encrypt`/`encfs_decrypt`, and that a tampered/corrupted backing
  blob is *rejected* (`encfs_decrypt` returns -1) rather than silently
  producing corrupted plaintext — the property that matters most for a
  filesystem, since silent corruption is worse than a hard failure.
- `tests/test_fs.sh` (run via `make test-fs`) is an integration script:
  it mounts the filesystem, performs real file operations (write, read,
  mkdir, etc.) through the mountpoint, and diffs results against
  expectations, plus confirms the backing store's raw bytes are not
  plaintext.
- `make test` runs both.
