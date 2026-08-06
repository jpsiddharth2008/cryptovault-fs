# VaultFS: Encrypted Filesystem via FUSE

VaultFS is a stackable filesystem that automatically encrypts and decrypts files as they are written to and read from a backing directory. It uses **FUSE 3** for filesystem operations and **libsodium** for authenticated encryption (XSalsa20-Poly1305).

---

## 🛠️ Prerequisites

Ensure your system has the required development libraries installed:

```bash
sudo apt-get update
sudo apt-get install -y libfuse3-dev libsodium-dev gcc pkg-config make
```

---

## 🏗️ Build Instructions

To compile VaultFS, run the following command in the project root:

```bash
make
```

This compiles all files in `src/` and generates the `vaultfs` executable.

---

## 🚀 How to Run

1. **Create the Directories**:
   Create a backing directory (where encrypted files reside) and a mount point (where plaintext is accessed):
   ```bash
   mkdir -p /tmp/vault_backing /tmp/vault_mount
   ```

2. **Export your Encryption Key**:
   VaultFS reads the environment variable `VAULT_KEY` to derive the encryption key:
   ```bash
   export VAULT_KEY="my-super-secret-passphrase"
   ```

3. **Mount the Filesystem**:
   Run the executable with the backing directory and mount point. We run it in the foreground (`-f`) for testing so we can see print logs:
   ```bash
   ./vaultfs /tmp/vault_backing /tmp/vault_mount -f
   ```

4. **Verify operations in a separate terminal**:
   Try writing to the mount point and verify that it appears encrypted in the backing directory:
   ```bash
   echo "Top Secret Document" > /tmp/vault_mount/secret.txt
   cat /tmp/vault_mount/secret.txt
   
   # Read the backing directory (should be encrypted)
   xxd /tmp/vault_backing/secret.txt
   ```

5. **Unmount the Filesystem**:
   When finished, unmount the mount point:
   ```bash
   fusermount3 -u /tmp/vault_mount
   ```

---

## 🧪 Running under Valgrind

To check for memory leaks during operation, run the mount command under Valgrind:

```bash
valgrind --leak-check=full --show-leak-kinds=all ./vaultfs /tmp/vault_backing /tmp/vault_mount -f
```
