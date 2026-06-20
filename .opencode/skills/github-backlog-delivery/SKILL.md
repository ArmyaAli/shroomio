---
name: github-backlog-delivery
description: Use when the user asks to check GitHub backlog/issues and deliver a repo change end-to-end through devcontainer gh, feature branch, tests/e2e gates, PR, CI, and merge.
---

# GitHub Backlog Delivery

Use this skill when the user asks for backlog-driven implementation, issue delivery, PR creation, CI watching, or merging through GitHub CLI from this repo's devcontainer.

## Core Rules

- Use host tools for local inspection, but run GitHub CLI through the devcontainer Make targets.
- Prefer `make devcontainer-gh ARGS='...'` for `gh` operations.
- Prefer `make devcontainer-exec CMD='...'` for commands that must run inside the devcontainer.
- Never expose tokens or `gh auth` secrets in output.
- Preserve unrelated worktree changes. Do not revert user changes.
- Do not use destructive commands such as `git reset --hard` or `git checkout --`.
- Use non-interactive `git` and `gh` commands.
- Commit, push, open PRs, and merge only when the user explicitly authorizes them.

## Workflow

1. Verify devcontainer support.

   Check `Makefile` for `devcontainer-up`, `devcontainer-exec`, `devcontainer-gh`, and `devcontainer-github-status`.

2. Start or refresh the devcontainer.

   Run `make devcontainer-up` if the container is not already running.

3. Verify GitHub auth inside the devcontainer.

   Run `make devcontainer-github-status`.

4. Inspect backlog.

   Use `make devcontainer-gh ARGS='issue list --limit 30 --state open ...'` and `make devcontainer-gh ARGS='issue view <number> --json number,title,body,labels,url'`.

5. Choose an issue.

   Prefer issues that are feasible to complete safely and can be protected with meaningful automated tests. If multiple issues are feasible, choose the highest-value or highest-priority issue.

6. Prepare the branch.

   Inspect status first. Sync `main` with `origin/main` using fast-forward only, then create a feature branch from fresh `main`.

7. Implement minimal changes.

   Keep the patch small and focused on the selected issue. Avoid speculative abstractions and broad rewrites.

8. Add tests and e2e coverage.

   Add unit tests plus any existing e2e/ImGui tests relevant to the issue. Treat these as merge gates, not optional checks.

9. Run local gates inside the devcontainer.

   Use existing Makefile targets. Typical gates are `make test`, `make imgui-test`, `make linux`, `make server`, `make format-check`, or `make lint` when present and relevant.

10. Commit and push.

   Inspect `git status`, `git diff`, and recent commits first. Stage only intended files. Use a concise commit message matching repo style.

11. Open the PR.

   Use `make devcontainer-gh ARGS='pr create ...'`. Include the issue number, summary, and exact local gates run.

12. Watch CI.

   Use `make devcontainer-gh ARGS='pr checks <pr> --watch'` or equivalent. If CI fails, inspect logs, fix, push, and watch again.

13. Merge when green.

   Use the repository's normal merge method through `gh pr merge`. Do not force push. Do not amend unless explicitly requested or clearly safe.

## Final Report

Return:

- Chosen issue number and title.
- Branch name.
- PR URL.
- Summary of changes.
- Exact local gates run.
- CI status.
- Merge result.
- Follow-up notes or blockers.

## Common Commands

```bash
make devcontainer-up
make devcontainer-github-status
make devcontainer-gh ARGS='issue list --limit 30 --state open'
make devcontainer-gh ARGS='issue view 119 --json number,title,body,labels,url'
make devcontainer-exec CMD='git fetch origin && git switch main && git pull --ff-only origin main && git switch -c feature/example'
make devcontainer-exec CMD='make test'
make devcontainer-gh ARGS='pr create --base main --head feature/example --title "..." --body "..."'
make devcontainer-gh ARGS='pr checks <number> --watch'
make devcontainer-gh ARGS='pr merge <number> --squash --delete-branch'
```
