---
name: ship-changes
description: Commit, push and prepare a pull request for PuzzleMagic the way the designer wants it. Use when asked to commit, push, or open/prepare a PR.
---

# Ship changes

- Commit only when asked. Push only to the contributor's own fork remote (check `git remote -v`; in the original setup `fork` is the
  designer's fork and `origin` is the upstream `ValhalaStudio/PuzzleMagic`). Never push to the upstream remote, and never open a PR
  unless asked.
- Before committing: the code compiles and you have tested it (see `device-test` and `ui-overlap-check`). Commit only after the user
  has seen it work, unless they said otherwise. Check `git status`; leave untracked local folders (`Build/`, `Demo/`, `.vsconfig`) alone.
- Commit messages: a short subject, then what changed and why, with measured numbers where there are any. End with the
  `Co-Authored-By` line the current Claude Code session asks for.
- Regenerate `Architecture.html` (`python Tools\ArchitectureDoc\gen_architecture.py`) when types or scripts change; it fails if a
  top-level `Tools/*.py` or `*.ps1` script is not described in its SCRIPTS list. Keep `README.md` and `CLAUDE.md` current.
- Test builds must match the pushed commit: check that no source file is newer than the packaged build, or rebuild (`build-test-packages`).
- Pull request: compare the fork's branch with the upstream `main` (`git fetch <upstream>`, `git rev-list --left-right --count`, and
  `git merge-tree --write-tree HEAD <upstream>/main` for a conflict check that changes nothing). Write the description from the commits
  (what changed, how it was tested, what was not tested). `gh` may not be installed: open the prefilled GitHub compare URL
  (`.../compare/main...<user>:<repo>:<branch>?quick_pull=1&title=...&body=...`) for the user to click. List exactly which files it carries.
- Never commit the conversation log (`dev_chat.log`, git-ignored), memory files, tokens or personal settings.
