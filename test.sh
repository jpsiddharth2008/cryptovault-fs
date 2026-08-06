#!/bin/bash

# Exit immediately if a command exits with a non-zero status
set -e

# Define directories
BACKING_DIR="/tmp/vaultfs_test_backing"
MOUNT_DIR="/tmp/vaultfs_test_mount"
VALGRIND_LOG="/tmp/vaultfs_valgrind.log"

echo "=== VaultFS Automated Test Script ==="

# Clean up any leftover mounts or files
if mountpoint -q "$MOUNT_DIR"; then
    echo "Cleaning up existing mount..."
    fusermount3 -u "$MOUNT_DIR"
fi
rm -rf "$BACKING_DIR" "$MOUNT_DIR" "$VALGRIND_LOG"
mkdir -p "$BACKING_DIR" "$MOUNT_DIR"

# 1. Build project
echo "Building VaultFS..."
make clean && make

# 2. Start VaultFS daemon
export VAULT_KEY="test-super-secret-key-12345!"
echo "Mounting VaultFS..."
./vaultfs "$BACKING_DIR" "$MOUNT_DIR"

# Ensure cleanup on script exit
cleanup() {
    echo "=== Cleaning Up ==="
    if mountpoint -q "$MOUNT_DIR"; then
        echo "Unmounting..."
        fusermount3 -u "$MOUNT_DIR"
    fi
    rm -rf "$BACKING_DIR" "$MOUNT_DIR"
    echo "Cleanup finished."
}
trap cleanup EXIT

# Wait a moment for mount to initialize
sleep 1

# 3. Test mkdir and rmdir
echo "Testing directory creation..."
mkdir "$MOUNT_DIR/testdir"
[ -d "$BACKING_DIR/testdir" ] || { echo "Error: directory not created in backing store"; exit 1; }

echo "Testing directory deletion..."
rmdir "$MOUNT_DIR/testdir"
[ ! -d "$BACKING_DIR/testdir" ] || { echo "Error: directory not removed from backing store"; exit 1; }

# 4. Test file creation, write, and read
echo "Testing file writing..."
PLAINTEXT="This is a top-secret message inside VaultFS!"
echo "$PLAINTEXT" > "$MOUNT_DIR/secret.txt"

echo "Testing file reading..."
READ_CONTENT=$(cat "$MOUNT_DIR/secret.txt")
if [ "$READ_CONTENT" != "$PLAINTEXT" ]; then
    echo "Error: Read content does not match plaintext!"
    echo "Read: $READ_CONTENT"
    exit 1
fi
echo "Plaintext round-trip: SUCCESS"

# 5. Check ciphertext in backing store
echo "Checking backing directory ciphertext..."
BACKING_CONTENT=$(cat "$BACKING_DIR/secret.txt")

if [ "$BACKING_CONTENT" = "$PLAINTEXT" ]; then
    echo "CRITICAL FAILURE: Backing file contains plaintext!"
    exit 1
fi

# The file should be non-empty and not contain the plaintext word
if grep -q "top-secret" "$BACKING_DIR/secret.txt"; then
    echo "CRITICAL FAILURE: Backing file contains plaintext substring!"
    exit 1
fi
echo "Ciphertext validation: SUCCESS (the content is encrypted)"

# 6. Test file permissions (chmod / chown)
echo "Testing file permissions..."
chmod 700 "$MOUNT_DIR/secret.txt"
BACKING_PERMS=$(stat -c "%a" "$BACKING_DIR/secret.txt")
MOUNT_PERMS=$(stat -c "%a" "$MOUNT_DIR/secret.txt")

if [ "$BACKING_PERMS" != "700" ] || [ "$MOUNT_PERMS" != "700" ]; then
    echo "Error: File permissions failed to propagate! Backing: $BACKING_PERMS, Mount: $MOUNT_PERMS"
    exit 1
fi
echo "Permissions propagation: SUCCESS"

# 7. Test file truncation
echo "Testing truncation..."
echo "Short" > "$MOUNT_DIR/secret.txt"
TRUNC_SIZE=$(stat -c "%s" "$MOUNT_DIR/secret.txt")
if [ "$TRUNC_SIZE" != "6" ]; then # "Short\n" = 6 bytes
    echo "Error: File truncation failed! Reported size: $TRUNC_SIZE"
    exit 1
fi
echo "Truncation test: SUCCESS"

# 8. Test large file writes (Multi-MB)
echo "Testing large file write (5MB)..."
dd if=/dev/urandom of=/tmp/large_source.bin bs=1M count=5 2>/dev/null
sha256sum /tmp/large_source.bin > /tmp/source.sha

cp /tmp/large_source.bin "$MOUNT_DIR/large.bin"
sha256sum "$MOUNT_DIR/large.bin" > /tmp/mount.sha

if ! diff -q /tmp/source.sha /tmp/mount.sha; then
    echo "Error: Large file SHA256 checksum mismatch!"
    exit 1
fi
echo "Large file round-trip: SUCCESS"
rm -f /tmp/large_source.bin /tmp/source.sha /tmp/mount.sha

echo "=== All Tests Passed Successfully ==="
