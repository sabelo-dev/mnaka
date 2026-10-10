# Foundation architecture

QML uses WorkspaceModel for display/input. The view model validates normalized
task requests and computes request hashes. EngineRegistry queues planning to a
dedicated worker thread. ConversationEngine uses MockModelProvider to produce a
deterministic one-step fixture plan. mnaka_core checks the plan's IDs, DAG, tools, argument schema,
resource scope, version and deadline before broker submission. Model-provided
approval flags, risk classes and shell fragments have no authority.

TaskOrchestrator owns task state on the execution host. It accepts a normalized
request, asks the registry for a plan, has the broker validate it, then dispatches
steps in dependency order and evaluates registered postconditions. State and
events are committed together through the TaskStore contract before any further
work; a refused commit stops the task. Only a session-only in-memory store exists
so far, used by tests and the opt-in mock session.

The broker defaults to disabled. The mock session explicitly uses fixture mode with one
trusted fixture resource and no filesystem/process/network operations. Approval
records bind the immutable fixture session, intent hash, resource version, expiry
and nonce. Cancellation removes pending records and blocks subsequent submissions
for the cancelled task. Completion requires the registered result.ok predicate.
There are no external effects to reconcile in this fixture.

Task state guards forbid transitions out of terminal states. Only verified
resumption can move interrupted to queued. View-model events have UUID IDs and
monotonic per-task sequences; replay is read-only and session-only. It does not
authorize broker actions. Production persistence must atomically record states
and events before any execution is enabled.

mnaka_storage_pg is a separate desktop-only library. Connections belong to the
thread that creates and destroys them. It reads Windows Credential Manager via
a target reference; QML receives no database configuration or password. Managed
connections require TLS hostname/certificate validation. Runtime role checks
reject elevated privileges and table-owner membership. Trusted repository code
must call beginActorRequest with identity from a verified session. No public SQL
execution method exists. Storage pooling and repositories are not yet implemented.

mnaka_migrate is a separate privileged deployment utility. It acquires a
deployment advisory lock and verifies ordered migration checksums. Each pending
migration and its ledger record commit together. It does not execute model output.
Desktop and relay use separate schemas in separate databases.

OpenAPI and schema artifacts describe contracts; they do not constitute a running
HTTP API, authentication service, qualified sandbox or production engine registry.

The implemented registry owns each engine once and rejects duplicate IDs. Six
required engines advertise disabled integration status. Cancellation removes the
active request before queued worker results can reach the broker. The mock provider
emits at most one terminal event for an accepted request. QML sees capability
metadata, streaming text and results, never engine or provider execution handles.
