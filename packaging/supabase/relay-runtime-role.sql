-- Supabase project hosting the relay database. Run once as the project's `postgres` role,
-- after mnaka_migrate has applied migrations/relay-postgres. Not a migration: it is not checksummed.
-- No password belongs in this file. Set one out of band, e.g. in psql: \password mnaka_relay_runtime
CREATE ROLE mnaka_relay_runtime LOGIN NOSUPERUSER NOBYPASSRLS NOCREATEDB NOCREATEROLE NOINHERIT;
GRANT USAGE ON SCHEMA relay TO mnaka_relay_runtime;
GRANT SELECT, INSERT, UPDATE, DELETE ON relay.accounts, relay.registered_devices, relay.relay_commands,
    relay.pairing_challenges, relay.device_sessions, relay.synced_entities TO mnaka_relay_runtime;
-- Supabase's browser-facing Data API roles must never reach relay tables. Also keep `relay`
-- out of the project's exposed schemas (Project Settings > Data API).
REVOKE ALL ON SCHEMA relay FROM anon, authenticated;
REVOKE ALL ON ALL TABLES IN SCHEMA relay FROM anon, authenticated;
