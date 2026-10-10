CREATE SCHEMA IF NOT EXISTS mnaka;
SET search_path TO mnaka, public;
CREATE TABLE IF NOT EXISTS schema_migrations(version BIGINT PRIMARY KEY, checksum TEXT NOT NULL, applied_at TIMESTAMPTZ NOT NULL);
CREATE TABLE actors(id UUID PRIMARY KEY, display_name TEXT NOT NULL, created_at TIMESTAMPTZ NOT NULL);
CREATE TABLE devices(id UUID PRIMARY KEY, actor_id UUID NOT NULL REFERENCES actors(id), name TEXT NOT NULL,
 platform TEXT NOT NULL, public_key_ref TEXT, revoked_at TIMESTAMPTZ, created_at TIMESTAMPTZ NOT NULL);
CREATE TABLE projects(id UUID PRIMARY KEY, actor_id UUID NOT NULL REFERENCES actors(id), name TEXT NOT NULL,
 root_uri TEXT NOT NULL, settings_json JSONB NOT NULL DEFAULT '{}', created_at TIMESTAMPTZ NOT NULL,
 UNIQUE(actor_id,root_uri));
CREATE TABLE conversations(id UUID PRIMARY KEY, actor_id UUID NOT NULL REFERENCES actors(id),
 project_id UUID REFERENCES projects(id), created_at TIMESTAMPTZ NOT NULL);
CREATE TABLE messages(id UUID PRIMARY KEY, conversation_id UUID NOT NULL REFERENCES conversations(id) ON DELETE CASCADE,
 role TEXT NOT NULL CHECK(role IN ('user','assistant','tool')), content_ref TEXT NOT NULL, created_at TIMESTAMPTZ NOT NULL);
CREATE TABLE tasks(id UUID PRIMARY KEY, actor_id UUID NOT NULL REFERENCES actors(id),
 device_id UUID NOT NULL REFERENCES devices(id), project_id UUID REFERENCES projects(id),
 conversation_id UUID REFERENCES conversations(id), engine TEXT NOT NULL, state TEXT NOT NULL,
 request_json JSONB NOT NULL, plan_json JSONB, idempotency_key TEXT NOT NULL,
 created_at TIMESTAMPTZ NOT NULL, updated_at TIMESTAMPTZ NOT NULL, UNIQUE(actor_id,idempotency_key));
CREATE TABLE task_steps(id UUID PRIMARY KEY, task_id UUID NOT NULL REFERENCES tasks(id) ON DELETE CASCADE,
 ordinal INTEGER NOT NULL, tool TEXT NOT NULL, args_json JSONB NOT NULL, dependencies_json JSONB NOT NULL DEFAULT '[]',
 state TEXT NOT NULL, result_json JSONB, started_at TIMESTAMPTZ, ended_at TIMESTAMPTZ, UNIQUE(task_id,ordinal));
CREATE TABLE events(id UUID PRIMARY KEY, task_id UUID NOT NULL REFERENCES tasks(id) ON DELETE CASCADE,
 sequence BIGINT NOT NULL, type TEXT NOT NULL, payload_json JSONB NOT NULL, occurred_at TIMESTAMPTZ NOT NULL,
 UNIQUE(task_id,sequence));
CREATE TABLE grants(id UUID PRIMARY KEY, actor_id UUID NOT NULL REFERENCES actors(id), device_id UUID REFERENCES devices(id),
 tool TEXT NOT NULL, resource TEXT NOT NULL, constraints_json JSONB NOT NULL, expires_at TIMESTAMPTZ, revoked_at TIMESTAMPTZ);
CREATE TABLE approvals(id UUID PRIMARY KEY, actor_id UUID NOT NULL REFERENCES actors(id),
 device_id UUID NOT NULL REFERENCES devices(id), step_id UUID NOT NULL REFERENCES task_steps(id),
 intent_hash TEXT NOT NULL, resource_version TEXT NOT NULL, nonce TEXT NOT NULL UNIQUE,
 decision TEXT NOT NULL CHECK(decision IN ('pending','allow','deny')), expires_at TIMESTAMPTZ NOT NULL,
 consumed_at TIMESTAMPTZ, created_at TIMESTAMPTZ NOT NULL);
CREATE TABLE artifacts(id UUID PRIMARY KEY, actor_id UUID NOT NULL REFERENCES actors(id),
 task_id UUID REFERENCES tasks(id), kind TEXT NOT NULL, storage_uri TEXT NOT NULL,
 sha256 TEXT NOT NULL, size_bytes BIGINT NOT NULL CHECK(size_bytes>=0), created_at TIMESTAMPTZ NOT NULL);
CREATE TABLE memories(id UUID PRIMARY KEY, actor_id UUID NOT NULL REFERENCES actors(id),
 project_id UUID REFERENCES projects(id), kind TEXT NOT NULL, content_ref TEXT,
 source_ref TEXT, confidence REAL NOT NULL CHECK(confidence BETWEEN 0 AND 1),
 status TEXT NOT NULL CHECK(status IN ('pending','confirmed','observation','superseded')),
 version BIGINT NOT NULL DEFAULT 1, created_at TIMESTAMPTZ NOT NULL, updated_at TIMESTAMPTZ NOT NULL, expires_at TIMESTAMPTZ, deleted_at TIMESTAMPTZ);
CREATE TABLE decisions(id UUID PRIMARY KEY, project_id UUID NOT NULL REFERENCES projects(id),
 memory_id UUID NOT NULL REFERENCES memories(id), supersedes_id UUID REFERENCES decisions(id), created_at TIMESTAMPTZ NOT NULL);
CREATE TABLE chunks(id UUID PRIMARY KEY, project_id UUID NOT NULL REFERENCES projects(id) ON DELETE CASCADE,
 path TEXT NOT NULL, file_hash TEXT NOT NULL, start_offset INTEGER NOT NULL, end_offset INTEGER NOT NULL,
 content_ref TEXT NOT NULL, embedding_ref TEXT, embedding_model TEXT);
CREATE TABLE skills(id TEXT NOT NULL, version TEXT NOT NULL, actor_id UUID NOT NULL REFERENCES actors(id),
 manifest_json JSONB NOT NULL, artifact_hash TEXT NOT NULL, status TEXT NOT NULL,
 installed_at TIMESTAMPTZ NOT NULL, PRIMARY KEY(id,version,actor_id));
CREATE TABLE rules(id UUID PRIMARY KEY, actor_id UUID NOT NULL REFERENCES actors(id),
 project_id UUID REFERENCES projects(id), enabled BOOLEAN NOT NULL,
 trigger_json JSONB NOT NULL, action_json JSONB NOT NULL, timezone TEXT NOT NULL,
 next_run_at TIMESTAMPTZ, last_run_at TIMESTAMPTZ, grant_id UUID REFERENCES grants(id), version BIGINT NOT NULL DEFAULT 1);
CREATE TABLE sync_records(id UUID PRIMARY KEY, actor_id UUID NOT NULL REFERENCES actors(id),
 entity_type TEXT NOT NULL, entity_id UUID NOT NULL, version BIGINT NOT NULL,
 deleted_at TIMESTAMPTZ, payload_json JSONB NOT NULL, UNIQUE(actor_id,entity_type,entity_id));
CREATE INDEX idx_tasks_owner_state ON tasks(actor_id,state,created_at);
CREATE INDEX idx_memories_scope ON memories(actor_id,project_id,status,deleted_at);
CREATE INDEX idx_chunks_project_path ON chunks(project_id,path);
CREATE INDEX idx_rules_next ON rules(enabled,next_run_at);

ALTER TABLE actors ADD COLUMN auth_subject TEXT UNIQUE;
ALTER TABLE tasks ADD COLUMN request_hash TEXT NOT NULL;
ALTER TABLE tasks ADD COLUMN execution_host_id UUID REFERENCES devices(id);
ALTER TABLE task_steps ADD COLUMN intent_hash TEXT;
ALTER TABLE task_steps ADD COLUMN resource_version TEXT;
ALTER TABLE task_steps ADD COLUMN attempt INTEGER NOT NULL DEFAULT 0 CHECK(attempt>=0);
ALTER TABLE tasks ADD CONSTRAINT task_state_valid CHECK(state IN
 ('queued','planning','awaiting_approval','running','cancelling','cancelled','succeeded','failed','interrupted'));
ALTER TABLE task_steps ADD CONSTRAINT step_state_valid CHECK(state IN
 ('queued','planning','awaiting_approval','running','cancelling','cancelled','succeeded','failed','interrupted','skipped'));
CREATE TABLE content_objects(id UUID PRIMARY KEY, actor_id UUID NOT NULL REFERENCES actors(id),
 storage_uri TEXT NOT NULL, sha256 TEXT NOT NULL, size_bytes BIGINT NOT NULL CHECK(size_bytes>=0),
 encryption_key_ref TEXT NOT NULL, created_at TIMESTAMPTZ NOT NULL, deleted_at TIMESTAMPTZ);
CREATE TABLE pairing_challenges(id UUID PRIMARY KEY, actor_id UUID NOT NULL REFERENCES actors(id),
 desktop_device_id UUID NOT NULL REFERENCES devices(id), challenge_hash TEXT NOT NULL,
 candidate_key TEXT, expires_at TIMESTAMPTZ NOT NULL, confirmed_at TIMESTAMPTZ, consumed_at TIMESTAMPTZ);
CREATE TABLE skill_activations(actor_id UUID NOT NULL REFERENCES actors(id), skill_id TEXT NOT NULL,
 version TEXT NOT NULL, generation BIGINT NOT NULL DEFAULT 1,
 PRIMARY KEY(actor_id,skill_id), FOREIGN KEY(skill_id,version,actor_id) REFERENCES skills(id,version,actor_id));
CREATE TABLE rule_runs(id UUID PRIMARY KEY, actor_id UUID NOT NULL REFERENCES actors(id),
 rule_id UUID NOT NULL REFERENCES rules(id), scheduled_for TIMESTAMPTZ NOT NULL,
 task_id UUID REFERENCES tasks(id), state TEXT NOT NULL, created_at TIMESTAMPTZ NOT NULL,
 UNIQUE(rule_id,scheduled_for));
CREATE TABLE tool_effects(id UUID PRIMARY KEY, actor_id UUID NOT NULL REFERENCES actors(id),
 step_id UUID NOT NULL REFERENCES task_steps(id), idempotency_key TEXT NOT NULL,
 intent_hash TEXT NOT NULL, state TEXT NOT NULL CHECK(state IN ('prepared','executing','committed','uncertain','aborted')),
 before_json JSONB, after_json JSONB, external_ref TEXT, updated_at TIMESTAMPTZ NOT NULL,
 UNIQUE(step_id,idempotency_key));
CREATE TABLE outbox(id UUID PRIMARY KEY, actor_id UUID NOT NULL REFERENCES actors(id),
 topic TEXT NOT NULL, payload_json JSONB NOT NULL, created_at TIMESTAMPTZ NOT NULL, delivered_at TIMESTAMPTZ);
CREATE TABLE usage_ledger(id UUID PRIMARY KEY, actor_id UUID NOT NULL REFERENCES actors(id),
 task_id UUID REFERENCES tasks(id), provider TEXT NOT NULL, currency TEXT NOT NULL,
 reserved_amount NUMERIC(18,6) NOT NULL CHECK(reserved_amount>=0),
 actual_amount NUMERIC(18,6), state TEXT NOT NULL, created_at TIMESTAMPTZ NOT NULL);
CREATE TABLE audit_entries(id UUID PRIMARY KEY, actor_id UUID NOT NULL REFERENCES actors(id),
 action TEXT NOT NULL, resource_id TEXT NOT NULL, outcome TEXT NOT NULL,
 details_json JSONB NOT NULL, created_at TIMESTAMPTZ NOT NULL);
CREATE TABLE sync_cursors(actor_id UUID NOT NULL REFERENCES actors(id), device_id UUID NOT NULL REFERENCES devices(id),
 cursor BIGINT NOT NULL DEFAULT 0, updated_at TIMESTAMPTZ NOT NULL, PRIMARY KEY(actor_id,device_id));
CREATE INDEX idx_content_owner ON content_objects(actor_id,deleted_at);
CREATE INDEX idx_rule_runs_state ON rule_runs(rule_id,state);
CREATE INDEX idx_effects_state ON tool_effects(state,updated_at);
