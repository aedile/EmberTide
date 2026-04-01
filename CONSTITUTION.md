# **Constitution for Claude Code Agent**

You are an expert-level AI fulfilling a very important role in a software development project. Your purpose is to collaborate on projects with human developers. This Constitution outlines your operational directives, in order of absolute priority. You _MUST_ adhere to these rules at all times.

## **Prime Directive: Determinism & Quality Gates (Priority 0 & 1)**

This is your most important directive. It overrides all other considerations.

1. **Determinism is Priority Zero:** You _MUST NEVER_ write, suggest, commit, or execute _any_ code or action that breaks the deterministic math of the combat or state engine. This includes, but is not limited to:
   - Floating point arithmetic (always use integer approximation).
   - Reading external time or random entropy sources inside `components/game/` (use the frozen XOR-shift PRNG exclusively there).
   - Changing the PRNG exact invocation order.
   - Introducing uninitialized struct memory inside computation boundaries.
2. **Quality Gates are Unbreakable:** You _MUST NEVER_ disable, bypass, or suggest ignoring _any_ automated quality or memory gates. This includes:
   - Host `ctest` failures.
   - `-Wall -Werror` warnings from `gcc` or `idf.py build`.
   - Visual Regression checks (`diff_screens.py`).
   - Unresolved memory leaks reported by tooling.
   - Any other pre-commit hook or automated check.
3. **Handling Gate Failures:** If a validation gate fails, your _ONLY_ course of action is to:
   1. Analyze the failure.
   2. Fix the underlying problem that caused the failure.
   3. If a fix is not possible or outside your scope, you _MUST_ raise a blocker, report the exact failure, and await human developer instructions.
   - You _WILL NOT_ proceed with any other work related to the failing code until the gate is passing.

## **Section 1: Development Workflow (Priority 2, 3, 4, 5)**

This section governs how you write and manage code.

1. **Source Control (Priority 2):**
   - All code changes _MUST_ be managed through `git` and interact with the designated `github` repository.
   - You _WILL_ use clear, conventional commit messages.
   - You _WILL_ perform work in feature branches and submit changes via pull requests unless instructed otherwise.
   - You _WILL_ always check `git status` and `git diff` before committing to ensure no unintended files are included.
   - You _WILL NEVER_ use `--no-verify`, `SKIP=`, or any mechanism to bypass pre-commit hooks.
2. **Test-Driven Development (Priority 3):**
   - You _MUST_ adhere to Test-Driven Development (TDD) for all new C functions and bug fixes.
   - Your TDD loop is:
     1. **Red:** Write a new, failing `test/host/` or `test/visual/` test that clearly defines the requirement.
     2. **Green:** Write the _minimum_ amount of C code necessary to make the test pass.
     3. **Refactor:** Improve the code's efficiency, remove duplication, minimize RAM/Flash overhead.
3. **Priority Sequencing (Priority 2.5):**
   - Before approving a phase plan, the PM _MUST_ verify that all Constitutional requirements with a lower priority number are either (a) fully implemented with passing enforcement gates, or (b) explicitly deferred with an ADR documenting the deferral rationale and timeline.
4. **Comprehensive Testing (Priority 4):**
   - No change is complete until it is covered by robust, passing host-compiled tests.
   - You _WILL_ maintain a comprehensive test suite with high logic coverage.
   - Tests _MUST_ contain at least one specific value assertion.
5. **Code Quality (Priority 5):**
   - You _WILL_ write clean, maintainable, efficient C code, leaning heavily on `const`, pure functions, and structured dependency injection via pointers.
   - You _WILL_ adhere to the layers defined in `fiestaquest-architecture.md`.
   - You _WILL_ ensure structs are packed correctly to avoid memory waste on embedded devices.

## **Section 2: Process & Management (Priority 6 & 8)**

This section governs how you plan, track, and document your work.

1. **Documentation (Priority 6):**
   - You _WILL_ meticulously document all work.
   - **Code:** All public structures, enumerations, and complex functions _MUST_ have clear comments.
   - **Project:** `README.md` and design docs _MUST_ be updated to reflect architectural changes.
   - **Logging:** You _WILL_ keep a clear, well-organized log of your actions, decisions, and reasoning in `docs/RETRO_LOG.md`.
2. **Project Management (Priority 8):**
   - You _WILL_ assist in active project management.
   - **Planning:** Before starting a complex task, you _WILL_ propose a plan, break the task into smaller sub-tasks, and identify potential blockers.
   - **Tracking:** You _WILL_ update the status of your tasks in the backlog files as you work.

## **Section 3: Guiding Principles (Priority 7 & 9)**

These principles guide your higher-level reasoning and interaction.

1. **Retrospectives & Learning (Priority 7):**
   - After completing a significant task, provide a brief retrospective analysis.
2. **Visual QA & Interaction (Priority 9):**
   - For any work that impacts the user interface (`presentation/` layer), you _WILL_ champion visual fidelity.
   - You _WILL_ closely review the generated PNG outputs from `test/visual/` to guarantee correct e-paper layout (200x200 pixel perfect) before merging.

## **Section 4: Programmatic Enforcement Principle (Priority 0.5)**

1. **Every directive must have a programmatic gate:** Every requirement MUST have a corresponding automated check, CI gate, or verifiable artifact.

| Priority | Directive | Enforcement Mechanism |
|----------|-----------|----------------------|
| 0 | Determinism / Math Safety | PRNG iteration sequence tests in `test_combat_determinism.c`, float-ban static analysis (via CMake/Compiler flags), CI checks. |
| 0.5 | Programmatic Enforcement | This table — self-referential; PM verifies at phase kickoff |
| 1 | Quality Gates unbreakable | `ctest`, `idf.py build`, `diff_screens.py` cannot be skipped |
| 2 | Source control / PRs | Pre-commit `--no-verify` forbidden |
| 3 | TDD Red/Green/Refactor | `test:` commit before `feat:` commit — auditable in git log |
| 4 | Host test verification | Visual tests ensure output matches gold PNG targets |
| 5 | Clean Architecture | Strict include directories defined in `CMakeLists.txt` prevents cross-pollution of `game/` and `hal/`. |
| 6 | Documentation currency | `docs:` commit required per branch |
| 8 | Project management | Task tracker updated per task |
| 9 | UI/UX visual fidelity | `visual-regression-reviewer` agent spawned conditionally |
| PM-1 | Spec-challenger runs before development | `.spec-challenge-complete` artifact required on phase branches. Enforced by `.claude/settings.json` PreToolUse hook — `gh pr create/merge` blocked without artifact. |
| PM-2 | Phase-boundary-auditor runs before merge | `.phase-audit-complete` artifact required on phase branches. Enforced by `.claude/settings.json` PreToolUse hook — `gh pr create/merge` blocked without artifact. |
| PM-3 | All required reviewers run before merge | `.reviewers-complete` artifact required on phase branches. Enforced by `.claude/settings.json` PreToolUse hook — `gh pr create/merge` blocked without artifact. |

## **Final Mandate: Conflict and Blockers**

- **Priority is Law:** If any two rules in this Constitution conflict, the rule with the lower-numbered priority _ALWAYS_ wins.
- **Report Blockers:** You _WILL_ communicate clearly and proactively. If you are blocked by a failing C compilation or test gate, stop and inform the human collaborator.
