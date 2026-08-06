# VaultFS: Design Document and Threat Model

VaultFS is an encrypted, stackable userspace filesystem. It utilizes FUSE (`libfuse3`) to intercept filesystem calls and `libsodium` to apply authenticated symmetric encryption to files before they are written to a backing directory.

---

## 1. System Architecture

```text
+------------------------------------------+
|            User Application              |
+------------------------------------------+
                     |  (Standard Syscalls: read, write, etc.)
                     v
+------------------------------------------+
|            VFS / Mount Point             |
|             (/tmp/mount)                 |
+------------------------------------------+
                     |
                     v
+------------------------------------------+
|          FUSE Kernel Module              |
+------------------------------------------+
                     |  (FUSE Protocol messages)
                     v
+------------------------------------------+
|          VaultFS Daemon (Userspace)      |
|                                          |
|  +--------------+      +--------------+  |
|  | FUSE Handler | ---> | Crypto Engine|  |
|  +--------------+      +--------------+  |
|         |                     ^          |
|         |                     | Key      |
|         v                     v          |
|  +------------------------------------+  |
|  |         libsodium Wrapper          |  |
|  +------------------------------------+  |
+------------------------------------------+
                     |  (Encrypted Syscalls)
                     v
+------------------------------------------+
|           Backing Directory              |
|             (/tmp/backing)               |
+------------------------------------------+
```

---

## 2. On-Disk File Format

For simplicity and reliability, VaultFS encrypts the entire contents of a file as a single block of ciphertext. 

Each file stored in the backing directory layout is formatted as follows:

```
+------------------------+------------------------------------------+
|    Nonce (24 bytes)    |       Ciphertext (Plaintext + 16B)       |
+------------------------+------------------------------------------+
|  crypto_secretbox_     |  crypto_secretbox_MACBYTES               |
|  NONCEBYTES            |                                          |
+------------------------+------------------------------------------+
```

### Encryption Metadata
* **Nonce (24 bytes)**: Generated using a cryptographically secure random number generator (`randombytes_buf`) on every write. This guarantees nonces are never reused, preventing cryptographic key-stream reuse attacks.
* **Authentication Tag / MAC (16 bytes)**: Integrated Poly1305 MAC tag that protects ciphertext integrity. Decryption will fail instantly if the ciphertext or nonce is altered.
* **Size Overhead**: Exactly 40 bytes per file. An empty file is represented as a 0-byte file on disk.

---

## 3. Key Management

VaultFS employs the following key management practices:
1. **Passphrase Input**: The passphrase is passed to the daemon through the `VAULT_KEY` environment variable during mount time.
2. **Key Derivation**: The passphrase string is hashed using SHA-256 (`crypto_generichash`) to derive a secure, uniformly distributed 256-bit (32-byte) key.
3. **In-Memory Security**: The key is stored in the daemon's private heap space and is never written to disk. The backing store contains only ciphertext and public nonces.

---

## 4. Threat Model

This threat model outlines the security guarantees and boundaries of VaultFS.

### What VaultFS Protects Against
* **Offline Storage Theft (Confidentiality)**: If the storage medium (HDD, SSD, SD card) containing the backing directory is stolen or read offline, the attacker cannot read any file content.
* **Offline Tampering (Integrity)**: If an attacker modifies the ciphertext or nonces in the backing directory, the authenticated decryption process (`crypto_secretbox_open_easy`) will fail during a `read`, returning an input/output error (`-EIO`) to the OS rather than returning corrupted or manipulated data.

### What VaultFS Does NOT Protect Against
* **Compromised Host Memory**: Since the filesystem driver must hold the key in the host machine's RAM to perform cryptographic operations, an attacker with root access can dump the daemon process's memory and extract the key.
* **Active Mounted Access**: When the filesystem is mounted, any running process on the machine with appropriate permissions on the mount point can read files in their plaintext form.
* **Traffic Analysis / Metadata Leakage**: Directory structures, file names, file count, and encrypted file sizes (adjusted by 40 bytes) are not obfuscated or encrypted. An attacker with access to the backing directory can see the file tree and trace activity.
