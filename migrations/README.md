# PostgreSQL migrations

Desktop and relay use separate PostgreSQL 17 databases. Run each directory through
`mnaka_migrate`; do not blindly rerun the bootstrap SQL. The runner creates its
ledger, acquires a session advisory lock, verifies every applied version/checksum,
and commits each pending migration together with its ledger entry. Migration SQL
is trusted deployment input, never model output. Do not edit applied migrations.

Migration 001 implements the specification's bootstrap. Migration 002 adds FORCE
RLS to all owner/child tables and desktop composite owner/resource foreign keys.
Desktop child tables carry an actor_id so ownership policies avoid recursive
parent lookups. The ledger is not an owner table and receives no runtime privileges.

Provision a separate runtime login (NOSUPERUSER, NOBYPASSRLS, NOCREATEDB,
NOCREATEROLE), migration owner and backup role outside these migrations. Do not
grant the runtime role membership in the migration owner. Grant schema USAGE and
only required table operations; never grant privileges on schema_migrations,
TRUNCATE, CREATE or role administration. RLS alone does not secure authentication.

The Windows runner reads a UTF-8 generic credential blob from Windows Credential
Manager using MNAKA_DB_VAULT_REF. Provision that credential outside the repository.
The credential target must refer to a migration-owner credential for migrations.
MNAKA_DB_HOST (default 127.0.0.1), MNAKA_DB_PORT, MNAKA_DB_NAME and MNAKA_DB_USER
contain metadata only. Managed hosts additionally require MNAKA_DB_TLS_CA; the
connection uses verify-full. No passwords belong in presets or mobile builds.

```powershell
& build/foundation/mnaka_migrate.exe desktop migrations/desktop-postgres
# Use a separately configured relay database and migration credential:
& build/foundation/mnaka_migrate.exe relay migrations/relay-postgres
```

Only the Windows vault adapter is implemented; other desktop keyring adapters
fail closed. Android/iOS do not compile or link this storage library.

## Qualification still required

No PostgreSQL server is installed in this development environment. These scripts
and the runner are compiled source, not verified database behavior. Run fresh
migration, repeated checksum verification, altered checksum rejection, rollback,
backup/restore and owner-denial tests against real PostgreSQL 17 before use.
`tests/integration/desktop_owner_isolation.sql` is a disposable-database negative
test to run with psql and ON_ERROR_STOP. Relay ownership/revocation tests and
connection pool/thread/TLS/identity-reset integration tests remain pending.

The local schema contains storage references, not an encryption implementation.
Do not persist private content until the encrypted content repository is added.
