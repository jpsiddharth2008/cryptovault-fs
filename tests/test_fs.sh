#!/usr/bin/env bash
# Integration test: mount -> write -> read -> diff -> verify backing is
# ciphertext -> unmount. Run from the project root: bash tests/test_fs.sh

set -e

BACKING=$(mktemp -d)
MOUNT=$(mktemp -d)
BINARY=./encfs
FAIL=0

export VAULT_KEY="test-key-do-not-use-in-real-life"

cleanup() {
    fusermount3 -u "$MOUNT" 2>/dev/null || true
    rm -rf "$BACKING" "$MOUNT"
}
trap cleanup EXIT

echo "Backing dir: $BACKING"
echo "Mount point: $MOUNT"

"$BINARY" "$BACKING" "$MOUNT" -f &
FS_PID=$!
sleep 1
mountpoint -q "$MOUNT" || { echo "[FAIL] filesystem did not mount"; exit 1; }

check() {
    if [ "$2" = "0" ]; then
        echo "[PASS] $1"
    else
        echo "[FAIL] $1"
        FAIL=1
    fi
}

# Test 1: basic write/read round trip
echo "hello encfs" > "$MOUNT/test1.txt"
diff <(echo "hello encfs") "$MOUNT/test1.txt" > /dev/null
check "write/read round trip" $?

# Test 2: backing file is NOT plaintext
if grep -q "hello encfs" "$BACKING/test1.txt" 2>/dev/null; then
    check "backing file is encrypted (not plaintext)" 1
else
    check "backing file is encrypted (not plaintext)" 0
fi

# Test 3: directories
mkdir -p "$MOUNT/subdir"
echo "nested file" > "$MOUNT/subdir/nested.txt"
diff <(echo "nested file") "$MOUNT/subdir/nested.txt" > /dev/null
check "nested directory read/write" $?

# Test 4: empty file
touch "$MOUNT/empty.txt"
[ -f "$MOUNT/empty.txt" ] && [ ! -s "$MOUNT/empty.txt" ]
check "empty file created correctly" $?

# Test 5: delete
rm "$MOUNT/test1.txt"
[ ! -f "$MOUNT/test1.txt" ]
check "file deletion" $?

# Test 6: large-ish file (1MB of random data)
head -c 1000000 /dev/urandom > /tmp/encfs_test_large_src
cp /tmp/encfs_test_large_src "$MOUNT/large.bin"
diff /tmp/encfs_test_large_src "$MOUNT/large.bin" > /dev/null
check "large file (1MB) round trip" $?
rm -f /tmp/encfs_test_large_src

echo ""
if [ "$FAIL" = "0" ]; then
    echo "ALL INTEGRATION TESTS PASSED"
else
    echo "SOME INTEGRATION TESTS FAILED"
fi

kill "$FS_PID" 2>/dev/null || true
exit $FAIL
