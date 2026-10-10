# Threat model (foundation)

This covers the code that exists today. It is not the specification's full
threat model: the host API, authentication, relay, sandbox and provider
adapters are not built, so their threats are listed as open rather than mitigated.

## Assets

- Authority to cause effects (tool execution, approvals).
- Task state and the event log.
- Database credentials and owner-scoped rows.
- User content: drafts, selected images, pasted skill manifests.

## Trust boundaries

| Boundary | Trusted side | Untrusted side |
| --- | --- | --- |
| QML ↔ view model | `WorkspaceModel` (C++) | QML and anything it displays |
| Engine/provider ↔ core | `EngineRegistry`, `ToolBroker` | Model output, plans, streamed text |
| Skill manifest ↔ validator | `SkillValidator` | User-supplied manifest JSON |
| Local file ↔ image loader | Worker-thread decoder | File contents |
| Application ↔ PostgreSQL | `mnaka_storage_pg` | Network path, database role |
| Deployment ↔ migrations | `mnaka_migrate` | Migration files on disk |
| Browser page ↔ client | WebAssembly client | Hosting origin, other page content |

## Threats and current mitigations

**Model output gains authority.** Plans are schema-checked; IDs, DAG shape,
tool, argument schema, resource scope, version and deadline are checked before
broker submission. Approval flags, risk classes and shell fragments supplied by
a model are ignored. A plan may only name tools its engine registered. The
broker is disabled by default and only runs two fixture tools.

**Approval replay or substitution.** An approval binds the session, intent
hash, resource version, expiry and a nonce consumed on use. The nonce stays in
C++ and never reaches QML. Changed hash, reused nonce, expiry and duplicate
execution are covered by tests. Cancellation revokes pending approvals.

**Late or duplicated results.** A cancelled or completed request is removed
before queued worker results arrive; the registry drops output for inactive
tasks. Task state guards forbid transitions out of terminal states.

**State and events diverge.** Each state change commits together with its
events before any further work; a refused commit stops the task. Only the
in-memory store exists, so this holds per session only.

**Malicious skill manifest.** Unknown fields, unknown tools, grant escalation,
expression injection, non-UUID references and payload-hash mismatch are
rejected. Scripts are never evaluated. Inspection grants nothing.

**Malicious image file.** Size, detected format and dimensions are bounded
before decoding, on a worker thread. Remote URLs are rejected. Only a
re-encoded in-memory PNG preview reaches the UI. Decoder vulnerabilities in Qt's
image plugins remain a residual risk.

**Content rendered as markup.** Messages are rendered as plain text.

**Database credential exposure or cross-owner access.** Credentials are read
from Windows Credential Manager by target reference; QML receives no
configuration. Remote connections require TLS `verify-full`. Role checks reject
elevated privileges and table ownership. Requests set a transaction-local actor
identity; there is no public SQL execution method. None of this has run against
a real server, so it is unverified.

**Tampered migrations.** The runner takes an advisory lock and verifies ordered
checksums; each migration commits with its ledger record. `.gitattributes`
keeps the SQL byte-exact.

**Account sign-in.** With a configured Supabase project the client sends the
user's email and password to its Auth endpoint over HTTPS and nowhere else; the
URL must be an https origin, redirects are not followed, and secret or
service-role keys are refused. Tokens stay in C++ memory and never reach QML or
storage. Server error text is bounded and shown as plain text. The publishable
key and project URL are public; what they permit is decided by the Supabase
project's own settings and policies.

**Account session and own model.** A signed-in session sends the user's messages,
and an image only when the user chooses Interpret, to the project's `assistant`
function, which checks the token, then calls the owner's model with a server-held
endpoint and key. Model output is untrusted: it is displayed as plain text and maps
to one effect-free step; it cannot name tools or grant itself authority. Saved
conversations, memories, skill manifests and rules are plain text in the project
database, readable by the owning user under row-level security and by anyone with
the project's service role or database access. Sign-up is open, so anyone who
creates an account can spend the owner's model capacity: there is no rate limit or
budget yet. A saved skill or rule is inert data. The model endpoint sees message
content; a remembered fact is sent with every message.

**Hosted browser client.** The WebAssembly build is static files. The only
configuration it carries is the public Supabase URL and publishable key, and its
only network calls are account sign-in. The hosting template serves it over HTTPS
only, from a private bucket, with a CSP that allows same-origin scripts and
WebAssembly compilation but not `unsafe-eval`, and forbids framing. Inline styles
are allowed because Qt injects one. A picked image stays in page memory. Whoever
can write to the bucket controls what users run; the files are unsigned and there
is no subresource integrity. None of this has been deployed.

**Resource exhaustion.** 16 active tasks, 1000 seen tasks per registry, 10-minute
deadline, 200 progress events per task, 64 KiB text and manifest limits, bounded
session history.

## Open

- No rate limit or budget on the assistant function; no encryption of saved content.
- The broker identity is local to a run; account identity is enforced only server-side by the user's token.
- No persistence, so interrupted-task recovery and replay are session-only.
- Transactional approval/tool-effect ordering (section 23) is not built.
- No sandbox, process isolation or control adapters.
- No relay, pairing or HTTP surface; the OpenAPI document is a contract only.
- No encrypted content store or vault provisioning.
- Dependency and licence locks are incomplete; binaries are unsigned.
- Row-level security and owner isolation are untested against PostgreSQL.
