# Task List

> Master task tracker for the project. Auto-generated when Phase 1 starts.
> Dynamically append new tasks as they are discovered during the workflow.

---

## Rules

- `[x]` = completed
- `[-]` = in progress / blocked (add reason in parentheses)
- `[ ]` = not started
- Hierarchy indicated by indentation (tab)
- **New tasks discovered during phases → append under the relevant phase, indented appropriately**
- Task IDs (e.g. `T-01`) reference task spec files in `docs/design-scoper/specs/`

---

## Phase 1: Scope Definition

[x] **B1:** Technical Assessment
[x] **B2:** Q&A → POV
[x] **B3:** HMW Questions
[x] **B4:** Design Brief
[x] **B5:** Context Map
[x] **B6:** Sketching

## Phase 2: Task Decomposition

[-] **B1: Identify Natural Boundaries**
    [ ] Identify component boundaries
    [ ] Identify module boundaries
    [ ] Identify functional boundaries
    ## Phase 2: Task Decomposition

[x] **B1: Identify Natural Boundaries**
    [x] Identify component boundaries
    [x] Identify module boundaries
    [x] Identify functional boundaries
    [x] Output: 18 tasks identified (T-01→T-18)

[x] **B2: Decompose → Subtasks**
    [x] Check each task: small enough? large enough?
    [x] Split oversized tasks (T-08→T-07a/b, T-11→T-10a/b)
    [x] Merge undersized tasks
    [x] Output: hierarchical task list with 20 tasks

[x] **B3: Define Input, Process, Output, Verification**
    [x] Per task: define input data and format
    [x] Per task: define processing logic
    [x] Per task: define output data and format
    [x] Per task: define acceptance criteria
    [x] Output: detailed task specs in `tasks.md`

[x] **B4: Write TODO & tasks.md**
    [x] Write all identified tasks into `docs/design-scoper/tasks.md`
    [x] Append tasks to this TODO.md with hierarchy
    [ ] Update DECISION-LOG.md
    [ ] Present to user for approval

> ── Tasks discovered during decomposition will appear below ──

## Phase 3: Logic Flow

[x] **B1: Sort Tasks by Dependency**
    [x] Read dependencies from `tasks.md`
    [x] Build dependency graph
    [x] Identify parallelizable tasks (G1-G5)
    [x] Determine execution order (Phase A→G)

[x] **B2: Draw Data Flow**
    [x] Trace data across task chain
    [x] Define channel per connection
    [x] Document per-task: input → process → output → channel
    [x] Output: `flow-diagrams/data-flow.md`

[x] **B3: Design Directory Structure**
    [x] Define top-level directories by natural boundary
    [x] Map tasks to directories
    [x] Plan shared/common directory
    [x] Output: `directory-structure.md`

## Phase 4: Spec Writing

[x] **B1: Write Spec per Task**
    [x] Write spec file for each task in `tasks.md`
    [x] Each spec: 8 sections
    [x] Prioritize: independent tasks first

[x] **B2: Cross-Check**
    [x] Check 1: Design brief coverage (22/22 ✅)
    [x] Check 2: Input/output chain match (18 chains ✅)
    [x] Add missing tasks if gaps found (T-02b supplement)
    [x] Update TODO.md and DECISION-LOG.md
    [ ] Present to user for final approval
