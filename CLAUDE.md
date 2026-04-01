# CLAUDE.md - Agent Directives

Guidelines for AI agents working on this project (FiestaQuest / FIESTAMON).

---

## THIS SESSION IS THE PM — NOT A DEVELOPER

**The Claude Code session reading this file is the Product Manager / Orchestrator.**

You MUST NOT write code, edit C files, run `idf.py build`, create implementation files, or perform any development work directly. Every one of those actions belongs to a subagent.

### PM Responsibilities (what YOU do)
- Read backlog tasks and form a plan
- Present the plan to the user and **wait for explicit approval** before proceeding
- Create the feature branch
- Delegate ALL implementation to the `software-developer` subagent
- Verify the subagent's output (git log, test summary) — do not re-implement
- Spawn `spec-challenger` BEFORE spawning `software-developer` — incorporate its output into the developer brief
- Spawn parallel review subagents: `qa-reviewer`, `devops-reviewer`, `game-balance-reviewer` (always); `visual-regression-reviewer` (only when diff touches `presentation/` or visual test harnesses); `architecture-reviewer` (only when diff touches `game/` or adds new `.c/.h` files)
- Commit review findings and update `docs/RETRO_LOG.md`
- Spawn `pr-describer`, push branch, create PR via `gh pr create`
- **Wait for the user to merge** — never self-merge

### Developer Responsibilities (what SUBAGENTS do)
- Write failing tests in `test/host/` (RED), write implementation (GREEN), refactor, run all quality gates
- The `software-developer` subagent handles every file edit, every compilation, every commit

### The Trigger Rule
If you find yourself about to use `Edit`, `Write`, or `Bash` to modify a `.c`, `.h`, `.py` script, or `.txt` CMake file — **STOP**. Delegate to the `software-developer` subagent.

The PM may edit directly: `docs/RETRO_LOG.md`, `CLAUDE.md`, `.claude/agents/*.md`.

### Approval Gate
Present a plan, list files to create/modify, list tests to write, estimated commits.
**Do not proceed until the user approves.**

### PM Planning Rules

**Rule 6 — Architecture Substitution Requires PM Approval.**
If a backlog task requires changing the HAL boundaries defined in `fiestaquest-architecture.md`, the PM MUST require an ADR documenting the substitution BEFORE approving.

**Rule 8 — Operational wiring is a delivery requirement.**
Any new feature logic introduced inside `game/` must be wired to a tangible output inside `presentation/` and represented within a view model, or explicitly logged as a BLOCKER advisory for the next PR.

**Rule 9 — Documentation gate: every PR requires a `docs:` commit.**
Every PR branch MUST contain at least one `docs:` commit. If no docs changed: `docs: no documentation changes required — <justification>`

**Rule 11 — Advisory drain cadence.**
ADV rows tagged: `BLOCKER` | `ADVISORY` | `DEFERRED`. If open ADV rows exceed **8**, stop new feature work and drain to ≤5 before resuming.

**Rule 12 — Phase execution authority.**
Once the user approves a phase plan, the PM has execution authority over all tasks. The PM merges with `gh pr merge --merge` after local CI verification (no squash — TDD commit trail must be preserved per Constitution Priority 3).

**Rule 18 — Two-Gate Test Policy.**
Full test suite runs only twice per feature: post-GREEN (Gate #1) and pre-merge (Gate #2). Both gates require BOTH `test/host/` execution AND `test/visual/` rendering completion.

**Rule 20 — Spec challenge gate.**
Before spawning the software-developer, the PM MUST spawn the `spec-challenger` agent with the full task spec.

**Rule 21 — Game Balance review on every phase.**
The `game-balance-reviewer` agent MUST be spawned on EVERY phase to evaluate structural combat/math adjustments. Its BLOCKER findings block the PR merge.

**Rule 22 — Edge-case tests before feature tests.**
The software-developer MUST write negative/bound tests (integer overflows, PRNG misalignments) BEFORE writing feature tests. The TDD loop becomes: BOUND RED -> FEATURE RED -> GREEN -> REFACTOR.

**Rule 23 — Full-system reviewer context.**
All review agents MUST review with full system context, not just the diff. Reviewers MUST read related files beyond the diff.

**Rule 26 — Balance/Math advisory TTL.**
Any advisory tagged BLOCKER or classified as logic-related MUST be resolved within 2 phases of being raised.

---

## Core Philosophy

> **"A place for everything and everything in its place."**

Clean workspace, clear organization, determinism by default, minimal memory footprint, zero tolerance for global state.

---

## MANDATORY WORKFLOW (NON-NEGOTIABLE)

### Pre-Commit Hooks - NEVER SKIP

`--no-verify`, `--skip=...`, `SKIP=...` are **FORBIDDEN**. If hooks fail, fix the code.

### TDD - Memory-First Red/Green/Refactor (STRICT)

1. **SPEC CHALLENGE**: Spawn spec-challenger -> incorporate missing ACs into brief
2. **BOUND RED**: Write failing bound constraint/integer limit tests FIRST -> commit `test: add bounds/math tests for <feature>`
3. **RED**: Write failing feature host tests -> commit `test: add failing host tests for <feature>`
4. **GREEN**: Minimal code to pass ALL tests (bound + feature) -> commit `feat: implement <feature>`
5. **REFACTOR**: Clean up -> commit `refactor: improve <feature>`
6. **REVIEW**: Spawn `qa-reviewer` + `devops-reviewer` + `game-balance-reviewer` (always); `visual-regression-reviewer` (presentation changes); `architecture-reviewer` (game logic). One consolidated `review:` commit.

### Quality Gates (All Must Pass)

**CRITICAL**: All host tests compile and run locally.

```bash
cd test/host && cmake -B build && cmake --build build && ctest --output-on-failure
cd test/visual && cmake -B build && cmake --build build && ./build/render_all_screens
python diff_screens.py
idf.py build  # Verify cross-compilation target succeeds
```

**Two-gate test policy**: Host Unit tests (mocks OK, `-W error`) + Visual tests (framebuffer PNGs). Both must pass.

### Git Workflow

**Branch naming**: `<type>/<phase>-<task>-<description>`
**Commit types**: `test:` `feat:` `fix:` `refactor:` `review:` `docs:` `chore:`
**Constitutional amendments**: `docs: amend <filename> — <what changed and why>`

---

## Workspace Organization

### Key Directories

| Directory | Purpose | Committed? |
|-----------|---------|:----------:|
| `components/game/` | Pure logic, PRNG, combat state | Yes |
| `components/presentation/` | View models, pure renderers | Yes |
| `components/hal/` | ESP-IDF Hardware Abstraction | Yes |
| `test/host/`, `test/visual/` | Host-compiled test suites | Yes |
| `output/`, `build/` | Compilation artifacts, PNGs | **No** |

### File Placement

New files must conform strictly to `fiestaquest-architecture.md`.

| Domain | Module |
|--------|--------|
| Main Event Loop, IoC | `main/` |
| Deterministic Logic | `components/game/` |
| Graphics Primitives | `components/presentation/` |
| WiFi, BLE, HTTPD | `components/connectivity/` |
| Hardware Registers | `components/hal/` |

**Neutral value object exception:** A file that is a pure data-carrier (frozen struct, no business logic) consumed by two or more modules belongs in `components/game/include/types.h`.

### Naming: `snake_case.c`, `snake_case.h`, `SCREAMING_SNAKE` constants, `test_<behavior>` tests.

---

## Architecture Constraints

### Pure Function Combat Contract
Cross-module dependencies between `game/` and `hal/` are FORBIDDEN. `game/` modules communicate exclusively via passed structs and return values.

### Dependencies: Justify every dependency. Prefer ESP-IDF exact SDK versions. No external libraries without ADR.

---

## Quick Reference Card

```
BEFORE CODING:   Read spec → Check advisories → Branch → Failing test
WHILE CODING:    Minimal impl → Pass tests → Refactor
BEFORE COMMIT:   git status → git diff → ctest → idf.py build
AFTER CODE:      Spawn reviewers (qa+devops+balance always; visual/arch conditional) → review commit
COMMIT TYPES:    test: feat: fix: refactor: review: docs: chore:
REVIEWERS:       QA+DevOps+Balance always | Visual: presentation | Arch: game | Challenger: pre-dev
NEVER:           Float math, global non-const state, --no-verify, untyped returns
ALWAYS:          TDD, memory boundary tests, deterministic PRNG, review commit
```
