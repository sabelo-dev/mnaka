# Browser client hosting (AWS and Supabase)

The browser client is the same `mnaka_client` QML application compiled with Qt for
WebAssembly. It has the foundation's limits: no provider and no persistence. The
mock session is the only thing that runs tasks. Its only network calls are account
sign-in to Supabase Auth, and only when the bundle is staged with a project URL and
publishable key. Hosting it publishes static files; it does not add a backend.

| Part | Where | State |
| --- | --- | --- |
| Browser client (static files) | AWS S3 behind CloudFront | Built and run locally; never deployed from this repository |
| Relay database | Supabase PostgreSQL 17 | Supabase CLI migrations written and checked locally; not pushed to the project |
| Identity | Supabase Auth | Email sign-in in the client; nothing verifies or uses the session yet |
| Relay service | AWS (planned) | Not implemented; `MNAKA_BUILD_RELAY` still fails at configure |

The browser never connects to PostgreSQL. `mnaka_storage_pg` and `mnaka_migrate`
are not built for WebAssembly. The page is given at most a Supabase project URL and
publishable key, which are public by design; database passwords and secret keys
never belong in the bundle, and the client refuses to start sign-in with one.

## Build

Needs the Qt `wasm_singlethread` kit, the matching desktop kit as host tools, and
the Emscripten version that Qt release asks for (5.0.5 for Qt 6.12.0), plus
`cmake` and `ninja` on `PATH`.

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File packaging/web/stage-web.ps1 `
    -QtKit C:/Qt/6.12.0/mingw_64 -WasmQtKit C:/Qt/6.12.0/wasm_singlethread -Emsdk C:/Users/<you>/emsdk `
    -SupabaseUrl https://<project-ref>.supabase.co -SupabasePublishableKey sb_publishable_...
```

The two Supabase arguments are optional. With them the script writes
`mnaka-config.json`; the page passes its values to the client as
`MNAKA_SUPABASE_URL` and `MNAKA_SUPABASE_PUBLISHABLE_KEY`. Without them Settings
shows that no account service is configured. The desktop client reads the same two
environment variables.

This configures and builds the `wasm-release` preset and stages `build/web-dist`:
`index.html`, `mnaka-shell.css`, `mnaka-boot.js`, `qtloader.js`, `mnaka_client.js`,
`mnaka_client.wasm` and Brotli copies of the last two. The `.wasm` is about 30 MB,
7.8 MB compressed.

Differences from the desktop build:

- No threads. An enabled engine stays on the registry's thread instead of a worker;
  its calls are still queued, so planning stays asynchronous. Image decoding runs on
  the event loop and can pause the page for a large image.
- Vision's "Select image" opens the browser's file chooser through Qt's private
  `QWasmLocalFileAccess`. The same 20 MiB, format and dimension limits apply, and the
  bytes stay in the page. Because this is a private header, the client is tied to the
  exact Qt version it was built with.
- Linked with `-sDYNAMIC_EXECUTION=0` so the page works without `unsafe-eval`.
- `--smoke-test` is desktop-only: a browser page cannot exit with a status.
- No tests are built for this preset.

## AWS Amplify Hosting (in use)

The Amplify app builds from the GitHub repository. Its build image has no Qt or
Emscripten, so `amplify.yml` compiles nothing: it publishes the prebuilt client
committed in `dist/` and sets the security headers. To release:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File packaging/web/stage-web.ps1 `
    -QtKit C:/Qt/6.12.0/mingw_64 -WasmQtKit C:/Qt/6.12.0/wasm_singlethread -Emsdk C:/Users/<you>/emsdk `
    -OutputDirectory dist -SkipCompression `
    -SupabaseUrl https://<project-ref>.supabase.co -SupabasePublishableKey sb_publishable_...
git add dist
git commit -m "Release browser client"
git push
```

`dist/mnaka-config.json` carries the Supabase URL and publishable key; both are
public by design. The CSP in `amplify.yml` names the same Supabase origin and must
change with it.

Trade-offs: every release adds a 30 MB binary to git history, and the `.wasm` is
served uncompressed because Amplify does not compress files over 10 MB and cannot
serve pre-encoded ones without extra rewrite rules. The `amplify.yml` headers and
build have not been confirmed on Amplify from this repository. The root
`package.json` is not used by the client.

## AWS: static hosting with CloudFormation (alternative)

`packaging/aws/web-hosting.yaml` creates a private, versioned S3 bucket, a CloudFront
distribution that reads it through origin access control, and a response headers
policy (CSP, HSTS, `nosniff`, frame denial, no referrer). The bucket is retained when
the stack is deleted. Requires the AWS CLI and credentials, neither of which is set
up in this development environment.

```powershell
aws cloudformation deploy --stack-name mnaka-web --template-file packaging/aws/web-hosting.yaml `
    --parameter-overrides "ConnectSources='self' https://<project-ref>.supabase.co"
aws cloudformation describe-stacks --stack-name mnaka-web --query "Stacks[0].Outputs"
powershell.exe -NoProfile -ExecutionPolicy Bypass -File packaging/aws/deploy-web.ps1 `
    -Bucket <BucketName> -DistributionId <DistributionId>
```

For a custom domain, pass `--parameter-overrides DomainName=app.example.com
AcmCertificateArn=<arn>`. The certificate must be in `us-east-1`. Then point the
domain at the `DistributionDomainName` output.

The deploy script uploads the Brotli copies under the original names with
`Content-Encoding: br`, because CloudFront does not compress objects over 10 MB.
File names are not content-hashed, so every object is revalidated by browsers and
the script invalidates the distribution after each upload.

The CSP allows `'wasm-unsafe-eval'` (WebAssembly compilation only) and inline styles,
which Qt injects for its window elements. `connect-src` defaults to `'self'`; sign-in
is blocked by the browser unless `ConnectSources` also lists the Supabase project
origin, as in the command above.

## Supabase: relay database and identity

### Sign-in

Settings has an Account card: email and password sign-in, account creation and
sign-out against the project's Auth endpoints (`adapters/supabase/SupabaseAuth`).
Access and refresh tokens stay in C++ memory, are refreshed shortly before expiry
and are gone on reload or exit. Only the email reaches QML. Signing in is identity
only: it enables no engine, tool or storage, and no MNAKA service checks the token.

In the Supabase dashboard, set Authentication > URL Configuration > Site URL to the
deployed site so confirmation emails link back to it. The project currently has
email sign-in on, sign-up open and email confirmation required.

### Relay database

`supabase/` is a Supabase CLI project for the relay database, not the desktop one.
Its migrations are:

1. `..._relay_bootstrap.sql` and `..._relay_owner_policies.sql`: byte-identical copies
   of `migrations/relay-postgres`. Two CTest targets fail if they drift.
2. `..._relay_supabase_auth.sql`, Supabase only:
   - creates the `mnaka_relay_runtime` login (no password, no elevated attributes) with
     table privileges on `relay`, and revokes everything on `relay` from `anon` and
     `authenticated`;
   - adds a trigger on `auth.users` that creates one `relay.accounts` row per user, with
     `auth_subject` set to the user id (the JWT `sub`), and backfills existing users;
   - adds a read-only policy so the relay can find an account by setting
     `relay.auth_subject` to a verified token subject, before it knows the account id.

The existing owner policies still key on `relay.account_id`, set by the relay for
each transaction. They do not use Supabase's `auth.uid()`, because the browser never
reaches these tables: `relay` is not in the Data API's exposed schemas.

To apply them to the hosted project (needs your Supabase login and the database password):

```powershell
npx supabase login
npx supabase link --project-ref <project-ref>
npx supabase db push --dry-run
npx supabase db push
```

Afterwards, set a password for `mnaka_relay_runtime` out of band (SQL editor:
`ALTER ROLE mnaka_relay_runtime PASSWORD '...'`) before a relay uses it. Supabase's
`postgres` role bypasses row-level security, so the relay must never connect as it.

Use the Supabase CLI or `mnaka_migrate` for a given database, never both: each keeps
its own ledger and the second would try to create the same tables. `mnaka_migrate`
also cannot run on this machine yet (no compatible libpq).

Checked: the three files apply in order to an in-process PostgreSQL (PGlite, server
version 18) with a stand-in `auth` schema, run as a role without `BYPASSRLS`. There the
trigger and backfill create accounts, the runtime role sees only the account named by
its settings, cannot write for another account, and `anon`/`authenticated` are denied.
The Supabase CLI loads `supabase/config.toml`. Not checked: PostgreSQL 17 itself, the
real Supabase `auth.users` table and roles, and `db push` against the hosted project.
A failing trigger on `auth.users` would block sign-ups, so try a test sign-up right
after the first push.

## Open

- The relay that verifies Supabase tokens. Until it exists a session unlocks nothing.
- Password reset, and keeping a session across reloads.
- The account-per-user trigger and the `relay.auth_subject` lookup policy are a proposal
  made for Supabase; the relay design has not confirmed them. Deleting an Auth user
  leaves its relay account in place.
- CI for the WebAssembly build, and qualification on Firefox, Safari and mobile browsers.
  Only headless Chrome was exercised.
- Content-hashed file names, to allow long browser caching.
