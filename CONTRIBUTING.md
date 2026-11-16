# Claude AI Guide

This project has multiple contributors who may each be working with their own Claude instance. Follow these rules to avoid conflicts.

## Before Making Any Changes
1. Always run `git pull origin main` first
2. Check `git status` for uncommitted changes
3. Read recent commits with `git log --oneline -10` to understand what's been done

## Rules for Claude
- **Never assume** you know the current state of the codebase. Always read files before editing.
- **Pull before pushing.** Always pull latest changes before committing.
- **Use descriptive commits.** Include what was changed and why.
- **Don't refactor unprompted.** Only change what the user explicitly asks for.
- **Check for TODOs.** Look for `// TODO` or `# TODO` comments that indicate work in progress by others.

## Project Structure
```
Playhouse/
├── backend/    # Backend code
├── frontend/   # Frontend code
├── README.md   # Project documentation
└── start.sh    # Startup script
```

## Current Contributors
- Cole (tolsd) - working from his location
- Friend - working from separate location

## Active Work
Document any in-progress features here to prevent overlap:

| Feature | Who | Status |
|---------|-----|--------|
| -       | -   | -      |

## Commit Message Format
```
[area] Short description

- Detail 1
- Detail 2

```

Example: `[frontend] Add login button`
