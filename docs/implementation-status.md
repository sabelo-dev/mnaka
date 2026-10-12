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
template (private S3, CloudFront, security headers) and an upload script. See
`web-hosting.md`.

Checked in headless Chrome against a local server sending the production headers
and Brotli encoding: startup, navigation, a mock-session task through to
`succeeded`, and an image preview through the file-input fallback. Not checked:
Chrome's File System Access picker beyond opening it, other browsers, mobile, and
any real AWS deployment. The template passes cfn-lint but has never been deployed. The browser build has no automated
tests, and `--smoke-test` does not work there.

## Account sign-in (Supabase Auth)

`adapters/supabase/SupabaseAuth` signs in, creates accounts and signs out with
email and password against a Supabase project's Auth endpoints, and refreshes the
session before it expires. The project URL and publishable key come from
`MNAKA_SUPABASE_URL` and `MNAKA_SUPABASE_PUBLISHABLE_KEY`; the browser page sets
them from `mnaka-config.json`, written by the staging script. Secret and
service-role keys and non-https URLs are rejected before any request. Tokens stay
in C++ memory and are not persisted; QML sees the email only. Settings has an
Account card. Without configuration it says so and nothing is sent.

This is identity only. A session enables no engine, tool or storage, and no relay
or host API exists to verify it. Password reset and session persistence are not built.

A ninth CTest target, supabase_auth_contract, runs against a loopback stand-in:
key and URL validation, request shape, sign-in and sign-out, rejected credentials,
malformed sessions, sign-up awaiting confirmation, an unreachable service and a
rejected refresh. All nine targets pass on the MinGW kit. Against the live project,
only a rejected sign-in was exercised, from the browser build under the production
CSP. A successful sign-in, account creation and token refresh have not run against
real Supabase, and desktop HTTPS was not exercised.

## Supabase CLI project for the relay database

`supabase/` holds a minimal `config.toml` (PostgreSQL 17, `relay` not exposed to the
Data API) and three migrations: byte-identical copies of the two relay migrations,
and a Supabase-only one that provisions the `mnaka_relay_runtime` role, creates a
`relay.accounts` row for each Supabase Auth user by trigger (with a backfill), and
lets the relay look up an account by verified token subject. Two CTest targets
compare the copies with `migrations/relay-postgres`; eleven targets pass.

The migrations were applied to an in-process PostgreSQL (PGlite, version 18) with a
stand-in `auth` schema, as a role without `BYPASSRLS`, and owner isolation, the
subject lookup and denial of `anon`/`authenticated` were checked there. They have
not run on PostgreSQL 17, on Supabase, or against the hosted project: `db push`
needs the owner's Supabase login and database password. For any one database use
the Supabase CLI or `mnaka_migrate`, not both.

## Command Centre interface

`Main.qml` was restyled after the Command Centre concept: logo and icon sidebar,
titled header, and a three-column Overview (conversation, state orb with the six
engine cards, and Active tasks / Approval required / Upcoming). It narrows to two
columns below 1320 px and one below 850 px. Icons and the orb are drawn with
`QtQuick.Shapes`; the logo is `apps/client/assets/logo.png`, also used by the web
loading page. The nine screens, their order and all view-model bindings are
unchanged ("Home" and "Task centre" are now labelled "Overview" and "Tasks").

Unlike the concept image, the screen shows live state only: engines report their
registry status rather than "Ready", task and approval panels are empty until the
mock session produces something, Upcoming states that scheduling is unavailable,
and there are no voice, search, notification or attachment controls because
nothing backs them. The orb is decorative and turns only while a task is active.

All eleven CTest targets pass, including both headless QML runs with zero
warnings. The layouts at 1680, 1100 and 420 px wide, a typed mock task and the
approval panel were checked by screenshot in headless Chrome only.

## Developer Workspace screen

The Developer screen now has the concept's layout: a project tree, a tabbed panel
(Diff, Build output, Tests), a task checklist, a review panel, a summary and a
request bar. No project engine exists, so by default every panel is an empty state
that says what is missing. The request bar is real: it submits through the same
`WorkspaceModel.submit` path in Developer mode and shows the reply or notice.

In the mock session only, the tree, diff, checklist, review and summary show a
static sample defined in `Main.qml`, each panel tagged "SAMPLE DATA" with a notice
that nothing was read from or written to any file. It is presentation content, not
output of the orchestrator, broker or any tool. "Apply changes" is disabled, and the
Build output and Tests tabs always state that nothing has run.

## Account session: own model, saved data, no mock switch

Signing in now starts a real session. This supersedes the statements above that
sign-in is identity only, that the sample on the Developer screen needs the mock
session, and that the mock session has a Settings switch.

- **Assistant.** `adapters/supabase/GatewayModelProvider` is a `ModelProvider` that posts
  the message to the project's `assistant` Edge Function with the user's token. The
  function (`supabase/functions/assistant`) verifies the caller, adds the last 20
  messages and the user's memories, calls the owner's own model over an
  OpenAI-compatible `/chat/completions` endpoint, and stores the exchange. The endpoint,
  model name and key are function secrets (`MNAKA_LLM_BASE_URL`, `MNAKA_LLM_MODEL`,
  `MNAKA_LLM_API_KEY`) and never reach the client. The reply still goes through
  `ConversationEngine`, plan validation, the orchestrator and the broker as a single
  `demo.echo` step, which has no effects. Replies are not streamed.
- **Saved data.** `supabase/migrations/20261011000001_account_data.sql` adds
  `mnaka_messages`, `mnaka_memories`, `mnaka_skills` and `mnaka_rules` in `public`, each
  row owned by one Auth user under row-level security. `adapters/supabase/SupabaseData`
  reads and writes them through the Data API. Content is stored as plain text: this
  deliberately sets aside the earlier rule against persisting content before the
  encrypted content repository exists. Tasks and events are still session-only.
- **Memory** (Projects screen): add and forget facts; they are sent with every message.
- **Skills**: a manifest that passes local inspection can be saved and deleted. Saving
  grants nothing and nothing is executed.
- **Proactive**: rules can be saved, switched on or off and deleted. Nothing schedules
  or runs them; the switch records a choice only.
- **Vision**: "Interpret image" sends a JPEG copy, at most 1024 px, with a question to
  the same function. The image is not stored. This needs a model that accepts images.
- **Mock session**: removed from the application's screens. `--mock-session` and
  `WorkspaceModel::setMockSession` remain for the automated tests.
- **Developer screen**: the sample is now an explicit "Show sample" toggle.
- Software Engineering and Computer Control are unchanged: they need the desktop
  client, relay and sandbox, none of which exist.

`Main.qml` is compiled to bytecode only (`QT_QMLCACHEGEN_ARGUMENTS --only-bytecode`):
full C++ generation overflowed the MinGW object format and took over ten minutes for
WebAssembly.

Verified: twelve CTest targets pass, including `account_session_contract` against a
loopback stand-in (sign-in, history load and order, a model task to `succeeded`, a 503
failing the task, memory and rule round trips, history deletion, sign-out, and the
bounded JPEG). The migration's ownership rules were checked on an in-process
PostgreSQL. The function was run under Deno against a stand-in model. The browser
build was driven through sign-in, chat and the new screens against the same stand-in.

Deployed to the Supabase project on 2026-10-11: the account-data migration and the
`assistant` function. Checked there: signed-out reads of the four tables are refused,
and the function refuses a call without a session. Not checked anywhere real: a
signed-in call, because no model secrets are set and no test account was created, and
any actual model, including image input.

## Terminal theme and scrolling

The interface now uses a terminal look: near-black background, green accents with
amber for warnings, one monospace font, square corners, bracketed section titles,
lower-case navigation, a blinking caret beside the greeting and static scanlines.
Layout, screens and bindings are unchanged.

Scrolling: mouse-wheel and touch scrolling already worked in headless Chrome. Fixed
around it: the browser build no longer enforces a minimum window size (a viewport
under 380 x 600 used to cut content off with no way to reach it); the page scrollbar
is always shown, and wider, when the page is taller than the view; Page Up, Page
Down, Ctrl+Home and Ctrl+End scroll the page; changing screen returns to the top; and
the Overview follows the composer as the conversation grows. Checked by wheel and
touch input in headless Chrome only, not on a physical phone or trackpad.

## Developer Workspace: working change flow

The Developer screen no longer shows sample data. `apps/client/DeveloperSession`
holds text files the user adds (up to 24, 64 KiB each, 256 KiB together, UTF-8 only,
file names only), sends them with a change request to the `developer` Edge Function,
and shows the model's proposal as a real line diff (`engines/software/LineDiff`).
The user applies or discards it; applying changes the in-session copies only, and a
file reaches a disk only through an explicit save (a browser download, or a save
dialog on desktop). This supersedes the earlier "Developer Workspace screen" section.

The function (`supabase/functions/developer`) asks the owner's model for complete new
file contents as JSON, extracts the object, and drops anything that is not a plain,
bounded text file with a safe name. The client validates the reply again and refuses
the whole proposal if any entry is unsafe. Model output is never executed. History
stores the request and the model's summary, not file contents. `assistant` and
`developer` now share `supabase/functions/_shared/gateway.ts`.

Not built: builds, tests, folders, binary files, project indexing, partial-file edits,
conflict handling, and routing this through the orchestrator and broker (it is a
direct call, like image interpretation). The checklist marks "Run tests" as not run,
and the summary is labelled as the model's own, unverified.

Verified: `account_session_contract` gained diff, validation and end-to-end cases
(request, proposal, apply, save to a chosen path, refused and malformed replies,
discard, sign-out); twelve targets pass. The function was run under Deno against a
stand-in model that wraps its JSON in a code fence and names an unsafe path. The
browser build added a file through the file chooser fallback, sent a request and
rendered the diff against the same stand-in. Both functions were redeployed on
2026-10-11 and refuse signed-out calls there. Not verified: any real model, the
browser download path, and the File System Access picker with more than one file.

## Voice input (browser)

`apps/client/VoiceInput` adds push-to-talk speech input in the browser build through
the page's Web Speech API. There is a microphone button beside the composer and under
the orb; the orb card shows the listening state and the live transcript. One finished
utterance is handled like typed text: "open/go to/show <screen>" and "cancel" act
locally, and anything else is submitted as a message (or left in the composer when it
cannot be sent). Nothing spoken is executed.

The audio never reaches MNAKA or the owner's model. The browser captures it, and in
Chrome and Edge sends it to the browser vendor's speech service; the screens say so.
The hosting headers previously sent `Permissions-Policy: microphone=()`, which blocks
the microphone outright; they now send `microphone=(self)`.

Not built: a wake word, continuous listening, spoken replies, and any desktop or
Android recogniser (`available` is false there, and Firefox has no Web Speech
recognition). Verified in headless Chrome with a simulated recogniser: listening state,
interim transcript, a spoken navigation command and a spoken message. Not verified: a
real microphone, the permission prompt, or real speech recognition accuracy.

## Installable on Android (web app)

The hosted client can be installed from Chrome on Android: `packaging/web/manifest.json`
(standalone display, theme colours) and three icons generated from the logo by
`packaging/web/make-icons.py`, linked from `index.html` and staged with the bundle.
The CSP gained `manifest-src 'self'`, which `default-src 'none'` otherwise blocks.
There is no service worker, so the app needs a connection to start and nothing works
offline. Chrome reported a valid manifest and no installability errors under mobile
emulation; the phone-width layout was checked by screenshot there. Not checked on a
physical phone: installing, the on-screen keyboard, the microphone prompt and
performance of the 30 MB download on mobile data.

A native Android package is not built. The `android-debug` preset and the Qt Android
kits exist, and the Android SDK and NDK are installed, but no JDK is, and the client
has never been built, packaged or run for Android. A native build would also have no
voice input and no browser file chooser.

## Native Android package

`packaging/android/build-apk.ps1` builds a debug-signed APK with the `android-debug`
preset: Qt 6.12.0 `android_arm64_v8a`, NDK 27.2, JDK 21, Gradle 9.5.1. This supersedes
"A native Android package is not built" above. Package `org.mnaka.client`, label
"MNAKA", launcher icons generated from the logo, arm64-v8a only, about 82 MB.

Android-specific changes: the Supabase URL and publishable key can be compiled in
(`MNAKA_SUPABASE_URL`, `MNAKA_SUPABASE_PUBLISHABLE_KEY` cache variables; a key that is
not a publishable key fails configure) because the app has no environment to read;
files from the system picker arrive as `content://` URIs and are opened as such; the
window has no minimum size; the code font is the system monospace; and OpenSSL 3 is
packaged for HTTPS, fetched at configure time from KDAB's `android_openssl` pinned to
commit b71f1470 with a SHA-256 check. That is a prebuilt binary dependency taken on
trust from its publisher.

Verified: the package builds, and its manifest, permissions (INTERNET,
ACCESS_NETWORK_STATE, and Qt's default storage permissions), native libraries
(including `libssl_3`, `libcrypto_3` and Qt's OpenSSL backend) and debug signature
were inspected. Not verified: the app has never been installed or started on a device
or emulator, so startup, rendering, sign-in over HTTPS, the keyboard, file picking and
saving on Android are all unproven. Voice input is not available in this build; it
exists only in the browser. The package is debug-signed and unoptimised: it can be
side-loaded, not published to a store, and an update must be signed with the same
debug key from this machine.

## Settings in plain language, model check, greeting overflow

Settings now has a Services list (one row per feature with ON, NEEDS MODEL, SIGN IN or
UNAVAILABLE and a sentence saying what it does) and a Privacy list, replacing the
engine registry dump and the storage/broker wording. The registry itself is unchanged.

The client used to say "Connected" as soon as sign-in succeeded, even with no model
configured. The `assistant` function now answers `{"probe": true}` with whether its
model secrets are set, without calling the model; the client asks once after sign-in
and shows "Model not set up" in the header, banner, orb card and Settings. A probe
that fails leaves the status unknown and the old wording in place.

A long account name made the Overview greeting wider than its column, pushing "Clear
session" and the composer over the centre column. The greeting now shrinks and elides,
and its caret is part of the text. Checked in headless Chrome signed in as a long
address against a stand-in backend with no model configured; twelve targets pass.

## Release versioning, start-up recovery, sign-in flows, capability states

**Releases.** `stage-web.ps1` names a release after the hash of its files and puts them
under `r/<release>/`; `index.html` is generated to reference that folder and
`release.json` records the id. Versioned files are served immutable, the page and the
small files beside it are revalidated, and a running page reloads once if
`release.json` names a different release. `amplify.yml` refuses to publish a page whose
release folder is missing. `deploy-web.ps1` (S3) uploads the same layout.

**Start-up.** `mnaka-boot.js` downloads the application itself, so it can show bytes
and a progress bar, abort a stalled transfer, retry four times with a pause, wait for
the connection when offline, and end on a message with a "Try again" button. The
length is checked against the build's.

**Sign-in.** The page takes the result of an email link out of the address bar and
passes it to the client: an error becomes an explanation on Settings with a "Resend
confirmation email" action; a confirmation link signs the user in. Sign-up and resend
ask Supabase to return to this site. The refresh token is remembered in the browser's
storage for this site and in the Android app's private storage (not on desktop, which
has no vault adapter for it), restored at start, rotated as it changes, kept when the
service is unreachable, and forgotten on sign-out or when the server refuses it.

**Capability states.** One list drives Settings and the Overview cards: AVAILABLE,
EXPERIMENTAL, NOT CONFIGURED, DESKTOP REQUIRED, SIGN IN (and UNAVAILABLE for voice
where the browser lacks it). The cards now say what a feature does today.

**Keyboard.** Enter and Return now press the focused button; Qt alone answers Space.

Verified: twelve CTest targets, with new cases for restore, refusal, offline restore,
resend, the unconfirmed-account hint, remembered sessions and email-link results. In
headless Chrome against a stand-in backend: a throttled download showing progress, a
blocked download ending in the retry state, a stale page reloading once, an expired
link, a confirmation link, staying signed in across a reload, Tab and Enter, 200%
zoom, a refused microphone (simulated) and sign-in while offline. Not verified:
Amplify's handling of the new cache headers, real email links from Supabase (their
exact fragment and the Site URL allow-list), a real microphone permission prompt, real
phones, screen readers, and other browsers. The Android package predates these changes.

## Model provider adapter and Claude as the interim model

`supabase/functions/_shared/providers.ts` is a provider adapter. The functions describe
a conversation in neutral terms (system text, turns, an optional image, an effort
level); a provider turns that into one backend's API. Two providers exist:
`anthropic`, which calls the Messages API through the official SDK, and
`openai-compatible`, which is the slot for MNAKA's own model (or Ollama, vLLM,
llama.cpp). Project secrets choose between them on every call
(`MNAKA_LLM_PROVIDER`, `MNAKA_LLM_MODEL`, `ANTHROPIC_API_KEY`, `MNAKA_LLM_BASE_URL`,
`MNAKA_LLM_API_KEY`), so replacing the model needs no rebuild and no redeploy of the app
or the functions. The client only sees which model is active, through the probe.

The interim model is Claude Sonnet 5.5 (`claude-sonnet-5-5`), with server-side refusal
fallback enabled, `low` effort for conversation and `medium` for the Developer
workspace. A refusal is reported to the client as such rather than as an outage. The
wording in the app no longer says "your own model".

Verified under Deno against stand-ins: the Anthropic path (request shape, image input,
a refusal, a rejected key), switching to the OpenAI-compatible provider by environment
alone with the Anthropic key still present, and the unconfigured and unknown-provider
cases. Deployed 2026-10-12. Not verified: a call to the real Anthropic API, because no
key is set on the project.

## Wake word

Voice input has a hands-free mode: the browser's recogniser stays on and only speech
that begins with "Mnaka" (optionally after "hey", "ok" or "hi") is acted on; the request
is what follows. The name on its own opens an eight-second window for the request.
Recognisers have never heard the name, so a short list of mishearings and near
spellings also counts. Everything else heard is dropped in `VoiceInput` and is never
shown, stored or sent to the model. The choice is remembered for the site; a refused
microphone turns it off.

This is not an on-device wake word. While hands-free is on, the browser streams what
the microphone hears to its speech service (Google's in Chrome, Microsoft's in Edge);
the app says so beside the switch. It works only while the page is open and visible,
and the browser may ask for the microphone again after a reload.

Verified: unit cases for the matcher and for hands-free filtering; twelve targets pass.
Not verified: real speech. How reliably a recogniser transcribes "Mnaka" is unknown and
is the main risk; the list of accepted spellings will need tuning from real use.
