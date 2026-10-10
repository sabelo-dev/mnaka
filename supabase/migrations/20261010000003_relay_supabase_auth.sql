-- Supabase only; mnaka_migrate does not apply this file. It ties relay accounts to Supabase
-- Auth users and provisions the relay's runtime role. The relay schema stays out of the Data API.

-- Runtime login for the relay service. No password here: set one out of band before first use.
DO $$ BEGIN
    IF NOT EXISTS (SELECT 1 FROM pg_roles WHERE rolname = 'mnaka_relay_runtime') THEN
        CREATE ROLE mnaka_relay_runtime LOGIN NOSUPERUSER NOBYPASSRLS NOCREATEDB NOCREATEROLE NOINHERIT;
    END IF;
END $$;
GRANT USAGE ON SCHEMA relay TO mnaka_relay_runtime;
GRANT SELECT, INSERT, UPDATE, DELETE ON relay.accounts, relay.registered_devices, relay.relay_commands,
    relay.pairing_challenges, relay.device_sessions, relay.synced_entities TO mnaka_relay_runtime;
-- Browser-facing Data API roles never reach relay tables.
REVOKE ALL ON SCHEMA relay FROM anon, authenticated;
REVOKE ALL ON ALL TABLES IN SCHEMA relay FROM anon, authenticated;

-- The relay knows the verified token subject before it knows the account. This lets it read
-- that one account row, and nothing else, by setting relay.auth_subject for the transaction.
CREATE POLICY accounts_subject_lookup ON relay.accounts FOR SELECT
    USING (auth_subject = NULLIF(current_setting('relay.auth_subject', true), ''));

-- One relay account per Supabase Auth user, created in the same transaction as the user.
CREATE FUNCTION relay.create_account_for_auth_user() RETURNS trigger
LANGUAGE plpgsql SECURITY DEFINER SET search_path = '' AS $$
DECLARE
    new_account uuid := gen_random_uuid();
BEGIN
    -- accounts has FORCE ROW LEVEL SECURITY: its owner policy applies to this function too.
    PERFORM set_config('relay.account_id', new_account::text, true);
    INSERT INTO relay.accounts(id, auth_subject, created_at) VALUES (new_account, NEW.id::text, now())
        ON CONFLICT (auth_subject) DO NOTHING;
    PERFORM set_config('relay.account_id', '', true);
    RETURN NEW;
END $$;
REVOKE ALL ON FUNCTION relay.create_account_for_auth_user() FROM PUBLIC, anon, authenticated;
CREATE TRIGGER mnaka_relay_account AFTER INSERT ON auth.users
    FOR EACH ROW EXECUTE FUNCTION relay.create_account_for_auth_user();

-- Users who signed up before this migration.
DO $$
DECLARE
    existing record;
    new_account uuid;
BEGIN
    FOR existing IN SELECT id FROM auth.users LOOP
        new_account := gen_random_uuid();
        PERFORM set_config('relay.account_id', new_account::text, true);
        INSERT INTO relay.accounts(id, auth_subject, created_at) VALUES (new_account, existing.id::text, now())
            ON CONFLICT (auth_subject) DO NOTHING;
    END LOOP;
    PERFORM set_config('relay.account_id', '', true);
END $$;
