# Browser client hosting (AWS and Supabase)

The browser client is the same `mnaka_client` QML application compiled with Qt for
WebAssembly. It has the foundation's limits: no provider, no persistence and no
network calls. The mock session is the only thing that runs tasks. Hosting it
publishes static files; it does not add a backend.

| Part | Where | State |
| --- | --- | --- |
| Browser client (static files) | AWS S3 behind CloudFront | Built and run locally; never deployed from this repository |
| Relay database | Supabase PostgreSQL 17 | Scripts written; not run against a Supabase project |
| Identity | Supabase Auth | Not wired; the relay that would verify its tokens does not exist |
| Relay service | AWS (planned) | Not implemented; `MNAKA_BUILD_RELAY` still fails at configure |

The browser never connects to PostgreSQL. `mnaka_storage_pg` and `mnaka_migrate`
are not built for WebAssembly, and the page is given no Supabase URL or key.

## Build

Needs the Qt `wasm_singlethread` kit, the matching desktop kit as host tools, and
the Emscripten version that Qt release asks for (5.0.5 for Qt 6.12.0), plus
`cmake` and `ninja` on `PATH`.

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File packaging/web/stage-web.ps1 `
    -QtKit C:/Qt/6.12.0/mingw_64 -WasmQtKit C:/Qt/6.12.0/wasm_singlethread -Emsdk C:/Users/<you>/emsdk
```

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

## AWS: static hosting

`packaging/aws/web-hosting.yaml` creates a private, versioned S3 bucket, a CloudFront
distribution that reads it through origin access control, and a response headers
policy (CSP, HSTS, `nosniff`, frame denial, no referrer). The bucket is retained when
the stack is deleted. Requires the AWS CLI and credentials, neither of which is set
up in this development environment.

```powershell
aws cloudformation deploy --stack-name mnaka-web --template-file packaging/aws/web-hosting.yaml
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
which Qt injects for its window elements. `connect-src` is `'self'`. When the client
gains network code, widen it with the `ConnectSources` parameter.

## Supabase: relay database and identity

Supabase hosts the relay database (`migrations/relay-postgres`), not the desktop one.
`relay.accounts.auth_subject` is the place for a Supabase Auth user id (the JWT `sub`).
The relay, once built, verifies the token and sets `relay.account_id` for the
transaction; the existing row-level security policies depend on that setting, not on
Supabase's `auth.uid()`.

1. Create a project on PostgreSQL 17. The connection check rejects other majors.
2. Download the project's database CA certificate and set `MNAKA_DB_TLS_CA` to it.
   Connections use `verify-full`.
3. Store the `postgres` password as a Windows generic credential and point
   `MNAKA_DB_VAULT_REF` at it. Set `MNAKA_DB_HOST`, `MNAKA_DB_PORT`, `MNAKA_DB_NAME`
   and `MNAKA_DB_USER`. Use the direct connection or the session pooler (port 5432).
   The transaction pooler (6543) breaks the runner's session advisory lock.
4. `mnaka_migrate.exe relay migrations/relay-postgres`
5. Run `packaging/supabase/relay-runtime-role.sql` as `postgres`, then set the role's
   password out of band. Supabase's `postgres` role bypasses row-level security, so
   the relay must never connect as it.
6. Leave `relay` out of the Data API's exposed schemas. `anon` and `authenticated`
   get no privileges on it.

Blocked today: `mnaka_migrate --driver-check` fails on this machine because no
compatible libpq is deployed, so step 4 cannot run until that is resolved. Nothing
here has run against a Supabase project.

## Open

- Sign-in in the client, and the relay that verifies Supabase tokens.
- The relay's account lookup by `auth_subject` happens before `relay.account_id` is
  known, which the current owner policies do not allow. The relay design has to settle it.
- CI for the WebAssembly build, and qualification on Firefox, Safari and mobile browsers.
  Only headless Chrome was exercised.
- Content-hashed file names, to allow long browser caching.
