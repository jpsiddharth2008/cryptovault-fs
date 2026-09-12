# Contributing to EncFS

This project is built as a team, one GitHub Issue at a time (see the
[Issues tab](https://github.com/jpsiddharth2008/cryptovault-fs/issues) and
[docs/tasks.md](docs/tasks.md) for the full dependency-ordered list). These
rules exist so four people can work on the same codebase at the same time
without stepping on each other or breaking `main` for everyone else.

## 1. Never commit or push directly to `main`

`main` should always be in a working state — buildable, tests passing. All
work happens on a branch and lands on `main` only through a reviewed pull
request. If you catch yourself about to run `git push origin main`, stop —
you want a branch instead.

## 2. One branch per issue

Branch names follow:

```
<type>/<issue-number>-<short-description>
```

- `type` is one of: `feat` (new functionality), `fix` (bug fix), `test`
  (tests only), `docs` (documentation only), `chore` (build/tooling).
- `issue-number` is the GitHub issue this branch closes.
- `short-description` is 2-4 words, lowercase, hyphen-separated.

Examples, matching this repo's actual issues:

```
feat/3-encfs-encrypt
feat/12-encfs-read
fix/6-getattr-size-underflow
test/5-crypto-unit-tests
docs/readme-setup-steps
```

Branch off the latest `main`:

```bash
git checkout main
git pull
git checkout -b feat/3-encfs-encrypt
```

## 3. Commit messages

- Write in the imperative mood: "Implement encfs_encrypt", not "Implemented"
  or "Implementing".
- Reference the issue number somewhere in the message body (not required in
  the subject line) so it's traceable, e.g. `Refs #3`.
- Keep the subject line under ~70 characters; use the body for the "why" if
  it's not obvious from the diff.
- Don't bundle unrelated changes into one commit — a commit implementing
  `encfs_encrypt` shouldn't also reformat unrelated files.

## 4. Before opening a pull request

Run the test suite locally and make sure it passes:

```bash
make test          # both unit and integration tests
make test-crypto    # just the crypto round-trip / tamper tests
make test-fs        # just the mount/read/write integration script
```

A PR that doesn't build or doesn't pass `make test` should not be opened —
fix it on your branch first.

## 5. Opening the pull request

- Title: same convention as the branch name's description, human-readable —
  e.g. "Implement encfs_encrypt".
- Body must include `Closes #<issue-number>` so merging the PR
  automatically closes the issue.
- Keep PRs scoped to one issue. If you notice something unrelated that
  needs fixing while working, open a separate issue for it rather than
  folding it into this PR.

## 6. Review before merge

- Every PR needs at least **one approval** from someone other than the
  author before it merges — no self-merging.
- The reviewer's job is to check: does it match the issue's description,
  does it handle the edge cases called out in the issue, does `make test`
  actually pass (check the PR checks, or ask if unsure).
- Address review comments with new commits on the same branch (don't force-
  push over history a reviewer has already commented on, unless asked to
  squash before merge).

## 7. Merging

- Use **squash and merge** — keeps `main`'s history to one commit per
  issue, which matches this repo's issue-per-unit-of-work structure.
- Delete the branch after merging (GitHub can do this automatically).

## 8. Keeping your branch up to date

If `main` has moved on since you branched, prefer rebasing your branch on
top of it over merging `main` into your branch, to keep history linear:

```bash
git fetch origin
git rebase origin/main
```

Never rebase or force-push a branch that someone else is also pushing to
or has already reviewed without checking with them first.

## 9. Never force-push to `main`, never skip hooks or checks

`--force` on `main`, `--no-verify`, or merging a red/failing PR "just this
once" defeats the entire point of this process. If a check is wrong or
flaky, fix the check — don't bypass it.
