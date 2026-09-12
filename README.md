# EncFS

A user-space encrypting filesystem built with FUSE3 in C. Files written
through the mount are transparently encrypted with authenticated encryption
(libsodium) before touching disk.

## Dependencies (Ubuntu/Debian, incl. WSL2)

```bash
sudo apt install build-essential gcc make git libfuse3-dev fuse3 libsodium-dev pkg-config valgrind
```

## Build

```bash
make
```

## Run

```bash
mkdir -p /tmp/encfs_backing /tmp/encfs_mount
export VAULT_KEY="choose a real passphrase here"
./encfs /tmp/encfs_backing /tmp/encfs_mount -f
```

In another terminal:

```bash
echo "hello" > /tmp/encfs_mount/test.txt
cat /tmp/encfs_mount/test.txt
xxd /tmp/encfs_backing/test.txt   # should NOT show "hello"
```

Unmount:

```bash
fusermount3 -u /tmp/encfs_mount
```

## Tests

```bash
make test          # runs both unit and integration tests
make test-crypto    # crypto round-trip / tamper-detection unit tests only
make test-fs        # mount + file-operation integration tests only
```

## Project layout

```
FUSE/
├── Makefile
├── README.md
├── CONTRIBUTING.md
├── assignments.md
├── include/
│   ├── encfs.h
│   ├── crypto.h
│   └── path_utils.h
├── src/
│   ├── main.c
│   ├── fuse_ops.c
│   ├── crypto.c
│   └── path_utils.c
├── tests/
│   ├── test_crypto.c
│   └── test_fs.sh
└── docs/
    ├── design.md
    └── tasks.md
```

## Working on this project

- Read [docs/design.md](docs/design.md) first — architecture, on-disk
  format, key management, and threat model.
- Implementation work is tracked as [GitHub
  Issues](https://github.com/jpsiddharth2008/cryptovault-fs/issues), broken
  into small, dependency-ordered tasks. [docs/tasks.md](docs/tasks.md) has
  the full dependency graph; [assignments.md](assignments.md) has a
  suggested split across a 4-person team.
- Before writing any code, read [CONTRIBUTING.md](CONTRIBUTING.md) — it
  covers branch naming, commit style, and the pull request / review rules
  everyone on the team follows.
