# MNAKA implementation status

This repository implements a runnable UI foundation from the v1.1 specification,
not a completed M0 milestone or production release.

## Implemented

- Responsive dark cyan/purple workspace with nine navigation screens and six engine entries.
- C++ QObject view model; QML receives presentation data only.
- Typed, asynchronous deterministic mock responses; workflow modes, cancellation,
  bounded temporary message/task history, task IDs, timestamps and mock evidence.
- N1 clarification and explicit unavailable live traffic response.
- Task centre and session deletion confirmation; keyboard input (Enter/Shift+Enter,
  Ctrl+L, Escape), accessible control names, plain-text message rendering.
- Built-in Qt Quick 3D sample with rotation, zoom and selection controls, plus a
  functional 2D fallback. No imported assets or image interpretation are enabled.
- Asynchronous engine registry with a dedicated planning thread, mock model
  streaming and cancellation. All six required engines are catalogued as disabled;
  the assistant fixture is the only enabled engine.
- C++20, Qt 6.8 API minimum, Qt Test behavior checks.
- mnaka_core: full intent value type/round-trip serialization, deterministic
  contract-subset hashing, strict bundled schema validation, task state guards
  and a deny-by-default ToolBroker. Only demo.echo/demo.review fixture tools run.
- Task centre approval fixture with intent hash, resource version, destination,
  effect, expiry, nonce consumption, denial and cancellation. This is session-only
  approval behavior; production transactional tool-effect ordering is pending.
- Monotonic session events and cursor replay, with bounded memory and no persistence.
- Nine JSON Schema contract files and OpenAPI 3.1 route documentation. The API
  document is JSON-form YAML; HTTP endpoints and auth are not implemented.
- Desktop/relay PostgreSQL bootstraps and ownership migrations; desktop child
  rows have actor_id and composite owner/resource foreign keys. SQL has not run
  against a real PostgreSQL server in this environment.
- mnaka_storage_pg and mnaka_migrate: thread-owned QPSQL connection lifecycle,
  Windows generic credential reading, TLS verify-full for remote DBs,
  transaction-local actor identity and checksum/advisory-lock migration runner.
  Mobile excludes these targets. No storage is connected to the UI yet.
- Five CMake presets, Windows deployment script and observed dependency hashes.
  The production dependency lock is explicitly incomplete; PostgreSQL is unresolved.
- Declarative skill validation for the initial build.status catalogue: payload
  hash, host version, granted permissions, required UUID references, consistent
  input constraints and step limits. Binding validates supplied inputs and never
  evaluates scripts. Dependencies, installation and tool dispatch are unsupported.

## Mandatory next work

M0 still requires real PostgreSQL 17/QPSQL qualification, a bounded connection pool,
task/event repositories, encrypted content/vault provisioning, live migration/RLS
tests, implemented host API/authentication, persisted event replay, transactional
approval/tool effects, production dependency/licence locks and
Windows/Android CI qualification. The temporary mock
history has no execution authority and is discarded on exit. There is no SQLite or
other persistence fallback. PostgreSQL absence disables all actual tool execution.

M1–M6 require provider adapters, PTT, project indexing/patches and qualified sandbox,
the three control adapters, skill validation/install/rollback, persistent memory,
OCR and hardware 3D qualification, deterministic scheduling, authenticated relay/pairing and the full
acceptance/security matrix. No disabled control represents a completed capability.

## Local build

The inspected kit is C:/Qt/6.12.0/mingw_64 with MinGW 13.1. This differs from the
specification's production MSVC Windows baseline and needs qualification. Use
separate build trees for each kit. No PostgreSQL patch/digest has been invented.

```powershell
$env:PATH = 'C:/Qt/Tools/mingw1310_64/bin;C:/Qt/6.12.0/mingw_64/bin;' + $env:PATH
& C:/Qt/Tools/CMake_64/bin/cmake.exe -S . -B build/foundation -G Ninja -DCMAKE_MAKE_PROGRAM=C:/Qt/Tools/Ninja/ninja.exe -DCMAKE_PREFIX_PATH=C:/Qt/6.12.0/mingw_64 -DBUILD_TESTING=ON
& C:/Qt/Tools/CMake_64/bin/cmake.exe --build build/foundation
powershell.exe -NoProfile -ExecutionPolicy Bypass -File packaging/windows/deploy-foundation.ps1 -QtKit C:/Qt/6.12.0/mingw_64 -IncludeTests
& C:/Qt/Tools/CMake_64/bin/ctest.exe --test-dir build/foundation --output-on-failure
& build/foundation/mnaka_client.exe
```

Android packaging, real PostgreSQL tests, hardware accessibility/performance and
clean-machine deployment are not validated by these desktop unit tests.

## Validation performed

Built with the installed Qt 6.12.0 MinGW desktop kit. QML lint completed without
warnings. Qt Test covers input validation, busy submission rejection, traffic
clarification, disabled project execution, cancellation with no late response,
and history deletion during planning. A headless QML smoke test visits all nine
screens and switches to compact layout; QML warnings cause a failing exit code.
These checks are foundation checks, not the specification's release acceptance suite.

Six CTest targets now cover workspace behavior/approval event replay, broker
security contracts, PostgreSQL fail-closed configuration and headless QML navigation
with an approval action. All pass. Broker tests include cyclic/oversized plans,
unknown authority fields, invalid UUID/time/version, scope denial, changed hash or
nonce, duplicate execution, approval expiry, cancellation and resource locking.
Engine tests verify worker-thread planning, disabled capabilities, malformed input,
duplicate requests, single terminal cancellation and suppression of late results.
Skill tests include payload tampering, grant escalation, unknown tools, expression
injection, undefined inputs, traversal-shaped non-UUID input and unknown fields.
The 3D component compiles and loads without QML warnings under the local OpenGL
probe. Headless picking has not passed, and renderer teardown hung during that
probe. Interactive Windows picking, Android rendering and frame rate remain
unqualified. The automated QML smoke test deliberately uses the 2D fallback.

The QPSQL driver-load probe currently fails: compatible libpq/deployment must be
provided. No PostgreSQL service/psql/Docker installation was found. Real migrations,
RLS, database writes/reads, backup/restore and TLS are unverified. The checked-in
desktop SQL negative fixture is not a passing integration-test claim.

SchemaValidator supports only the closed subset used by the bundled schemas and
rejects unsupported keywords. It is not a general JSON Schema implementation.
Skill/rule/sync schemas currently define a deliberately narrow initial contract;
interpreters, semantic checks and production installation/scheduling are pending.

## App content update

The application no longer connects to mock providers or fixture brokers. It starts
with empty conversation, task, approval and event lists. Unavailable submissions
return a notice without clearing the user's draft or generating task records.
The demo approval action and sample scene have been removed from the UI and QML
module. Mock providers and broker fixtures remain in isolated automated tests.
This update supersedes earlier descriptions of the application's demo behavior.

## Local image preview

Vision accepts user-selected local PNG/JPEG images. A worker thread reads at most
20 MiB, validates detected format and dimensions (maximum 8192 per axis and
16 megapixels), decodes and creates a bounded 1600-pixel preview. The UI receives
only an in-memory PNG preview. No image is uploaded or persisted. Clear invalidates
pending results. OCR, camera capture, imported 3D assets and model interpretation
remain unavailable. Tests cover valid preview, invalid content, remote URLs and
clearing during load; native picker and Android device qualification remain pending.

## Skill inspection UI

Skills now accepts user-provided manifest JSON (maximum 64 KiB) and displays
structural, pinned-hash, input-reference, step-limit and host-version checks.
Compatibility is checked against the current foundation host version 0.1.0.
Inspection is distinct from permission validation: a passing report grants no
scope and cannot install or execute a workflow. No manifest is prefilled or stored.
Changing the editor clears the old report. Runtime skill activation remains pending.

## Task orchestrator and opt-in mock session

`core/orchestrator/TaskOrchestrator` now drives a typed request end to end:
task-request schema check, execution-host match, request hash and idempotency
(same key and payload returns the original task; a changed payload is CONFLICT),
asynchronous planning through EngineRegistry, broker plan validation, dependency-
ordered step dispatch, approval, the `result.ok` postcondition and a terminal
state. Each state change is committed together with its schema-validated events
(`task.created`, `task.state_changed`, `step.started`, `step.progress`,
`step.completed`, `approval.requested`, `task.completed`) before anything else
happens; sequences are monotonic per task and `eventsAfter` replays from a
cursor. A refused commit stops the task and cancels its engine/broker work.
`recover()` marks tasks a previous run left active as `interrupted` (never
resumed or retried); a task that was only queued is failed as retryable.
Cancellation goes through `cancelling`, revokes pending approvals and reports how
many steps had already completed. Limits: 16 active tasks, 10-minute deadline
(deadline during an unconfirmed cancellation becomes `interrupted`), 200 progress
events per task. Steps the broker rate-limits are retried when capacity frees.

`TaskStore` is the persistence contract. The only implementation is
`InMemoryTaskStore` (bounded, session-only). It is not a persistence fallback:
the PostgreSQL implementation is still pending, and with it A23/A29-A31.
`artifact.created`, verified resumption of interrupted tasks, per-task budgets
and the transactional approval/tool_effect ordering of section 23 are not built.

The application is still disconnected by default. Settings has a "Mock session"
switch (or start with `--mock-session`) that connects the view model to the
orchestrator, MockModelProvider and the broker's two fixture tools. The banner,
sidebar, replies and task evidence are labelled as mock while it is active; the
approval nonce stays in C++. Turning it off or clearing history discards the
session. This supersedes "the application no longer connects to mock providers"
above only while that switch is on.

Eight CTest targets pass on the Qt 6.12.0 MinGW kit. New: task_orchestrator_contract
(event order and schema, replay cursor, idempotency/conflict, wrong host,
unavailable store, approval binding and single use, denial, cancellation during
planning and approval, dependency ordering beyond broker capacity, optional and
required step failures, deadline, store failure plus restart, 100 submissions
bounded) and workspace_qml_mock_task (headless UI run of a plain task and an
approved task with zero QML warnings). These use the in-memory store and
fixtures; they are not acceptance-matrix results.

## Known deviations from the specification's layout

The client now lives in `apps/client` (`main.cpp`, `WorkspaceModel.*`,
`qml/Main.qml`), the target is `mnaka_client` and the QML URI is `Mnaka.App`,
as sections 16/25 require. `WorkspaceModel` and `Main.qml` build once in the
static `mnaka_client_module` library, which the client and `workspace_tests` link. `docs/capabilities.md`, `docs/threat-model.md` and
`capabilities.json` exist; the JSON shape is provisional and both documents
describe the foundation only. Still missing: `mnaka_worker`,
`mnaka_skill_worker` and `mnaka_relay` targets and CI configuration. The local
kit is Qt 6.12.0 MinGW, not the MSVC baseline. `apps/client/qml/SceneViewer.qml`
is not part of the QML module. Build trees configured before the move still
contain a stale `appmnaka.exe`.

## PostgreSQL project repository

The desktop-only ProjectRepository lists owned project metadata using prepared
queries, transaction-local actor identity and UUID cursor pagination (maximum
100 rows). It excludes root paths and settings from returned metadata. Failed
queries roll back; results are returned only after commit. It rejects missing
identity, invalid cursors and out-of-range limits. The UI remains disconnected
until authenticated sessions and real PostgreSQL integration tests are qualified.
Current automated tests verify unavailable-storage and invalid-input behavior;
live pagination, owner isolation and identity-reset verification are still pending.

## View-model notifications, provider injection and Vision notices

`WorkspaceModel` now notifies lists separately (`messagesChanged`, `tasksChanged`,
`capabilitiesChanged`, `imageChanged`, `skillReportChanged`), so a streaming delta
no longer rebuilds message and task delegates. Image status has its own
`imageNotice`, shown on the Vision screen instead of the Home notice. Previews
bound large images to 1600 pixels and no longer upscale small ones.

`ConversationEngine` takes its `ModelProvider` from the caller and owns it; the
mock session and tests pass `MockModelProvider`. No other provider exists yet.

The application no longer requires or links Qt Quick 3D. `SceneViewer.qml`
remains in the tree outside the QML module and would need that dependency back.
`mnaka_migrate` is part of the install rule. The repository is under git;
`.gitattributes` keeps migration SQL byte-exact because the runner checksums it.
All eight CTest targets pass on the Qt 6.12.0 MinGW kit after these changes.

## Browser client (Qt for WebAssembly)

A sixth preset, `wasm-release`, builds `mnaka_client` for the browser with the
Qt 6.12.0 `wasm_singlethread` kit and Emscripten 5.0.5. `mnaka_storage_pg`,
`mnaka_migrate` and the tests are not part of it. That kit has no threads: an
enabled engine stays on the registry's thread (its calls remain queued), and image
previews decode on the event loop. Vision uses the browser's file chooser through
Qt's private `QWasmLocalFileAccess`, with the same size, format and dimension limits.
The client links with `-sDYNAMIC_EXECUTION=0` so it runs under a CSP without
`unsafe-eval`. The desktop build is unchanged apart from sharing the refactored
image loader; all eight CTest targets still pass on the MinGW kit.

`packaging/web` stages the static bundle, `packaging/aws` holds a CloudFormation
template (private S3, CloudFront, security headers) and an upload script, and
`packaging/supabase` holds the relay runtime-role grants. See `web-hosting.md`.

Checked in headless Chrome against a local server sending the production headers
and Brotli encoding: startup, navigation, a mock-session task through to
`succeeded`, and an image preview through the file-input fallback. Not checked:
Chrome's File System Access picker beyond opening it, other browsers, mobile, and
any real AWS or Supabase deployment. The template passes cfn-lint but has never
been deployed; the Supabase SQL has never run. The browser build has no automated
tests, and `--smoke-test` does not work there.
