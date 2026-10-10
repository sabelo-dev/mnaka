# Capabilities

`capabilities.json` at the repository root is the machine-readable form of this
page. Both describe what the foundation build can do today, not the v1.1 target.
The file's shape is provisional: it mirrors `EngineRegistry::capabilities()` and
has not been checked against the specification's capability contract.

## Engines

| ID | Label | Default | Mock session |
| --- | --- | --- | --- |
| assistant | – | Service unavailable | Mock provider (fixture): `demo.echo`, `demo.review` |
| software | ENG-01 | Service unavailable | Integration pending |
| control | ENG-02 | Service unavailable | Integration pending |
| skills | ENG-03 | Service unavailable | Integration pending |
| memory | ENG-04 | Service unavailable | Integration pending |
| vision | ENG-05 | Service unavailable | Integration pending |
| proactive | ENG-06 | Service unavailable | Integration pending |

By default the application is disconnected: no registry exists and every engine
reports "Service unavailable". The mock session (Settings switch or
`--mock-session`) registers the assistant fixture and six disabled placeholders.
A disabled engine rejects submissions with `PROVIDER_UNAVAILABLE`.

## Tools

Only two fixture tools can run, both through the broker in fixture mode against
one trusted fixture resource. Neither touches the filesystem, processes or network.

- `demo.echo` runs without approval.
- `demo.review` requires an approval bound to the intent hash, resource version,
  expiry and a single-use nonce.

## Local features outside the engines

- Vision: local PNG/JPEG preview (20 MiB, 8192 px per axis, 16 megapixels).
  No upload, persistence, OCR or interpretation.
- Skills: manifest inspection (64 KiB). Grants no scope; nothing is installed or run.

## Not available

Provider adapters, PTT, project indexing and patches, sandboxing, control
adapters, skill installation, persistent memory, scheduling, relay/pairing and
all PostgreSQL-backed persistence. See `implementation-status.md`.
