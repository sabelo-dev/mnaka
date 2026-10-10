CREATE SCHEMA IF NOT EXISTS relay;
SET search_path TO relay, public;
CREATE TABLE accounts(id UUID PRIMARY KEY, auth_subject TEXT NOT NULL UNIQUE, created_at TIMESTAMPTZ NOT NULL);
CREATE TABLE registered_devices(id UUID PRIMARY KEY, account_id UUID NOT NULL REFERENCES accounts(id),
 public_key TEXT NOT NULL, name TEXT NOT NULL, revoked_at TIMESTAMPTZ, created_at TIMESTAMPTZ NOT NULL,
 UNIQUE(account_id,id));
CREATE TABLE relay_commands(id UUID PRIMARY KEY, account_id UUID NOT NULL REFERENCES accounts(id),
 source_device_id UUID NOT NULL, target_device_id UUID NOT NULL, idempotency_key TEXT NOT NULL,
 request_hash TEXT NOT NULL, payload JSONB NOT NULL, state TEXT NOT NULL,
 expires_at TIMESTAMPTZ NOT NULL, created_at TIMESTAMPTZ NOT NULL, delivered_at TIMESTAMPTZ,
 UNIQUE(account_id,idempotency_key),
 FOREIGN KEY(account_id,source_device_id) REFERENCES registered_devices(account_id,id),
 FOREIGN KEY(account_id,target_device_id) REFERENCES registered_devices(account_id,id));
CREATE TABLE pairing_challenges(id UUID PRIMARY KEY, account_id UUID NOT NULL REFERENCES accounts(id),
 desktop_device_id UUID NOT NULL, challenge_hash TEXT NOT NULL, candidate_public_key TEXT,
 expires_at TIMESTAMPTZ NOT NULL, confirmed_at TIMESTAMPTZ, consumed_at TIMESTAMPTZ,
 FOREIGN KEY(account_id,desktop_device_id) REFERENCES registered_devices(account_id,id));
CREATE TABLE device_sessions(id UUID PRIMARY KEY, account_id UUID NOT NULL REFERENCES accounts(id),
 device_id UUID NOT NULL, token_hash TEXT NOT NULL UNIQUE, expires_at TIMESTAMPTZ NOT NULL, revoked_at TIMESTAMPTZ,
 FOREIGN KEY(account_id,device_id) REFERENCES registered_devices(account_id,id));
CREATE TABLE synced_entities(account_id UUID NOT NULL REFERENCES accounts(id), entity_type TEXT NOT NULL,
 entity_id UUID NOT NULL, version BIGINT NOT NULL CHECK(version>0), payload JSONB NOT NULL,
 deleted_at TIMESTAMPTZ, updated_at TIMESTAMPTZ NOT NULL, PRIMARY KEY(account_id,entity_type,entity_id));
CREATE INDEX relay_delivery ON relay_commands(target_device_id,state,expires_at);
CREATE INDEX sessions_expiry ON device_sessions(expires_at);
