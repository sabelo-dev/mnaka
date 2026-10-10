-- Run with psql -v ON_ERROR_STOP=1 in a disposable PostgreSQL 17 test database,
-- after both desktop migrations. Requires a test administrator with CREATEROLE.
-- Everything, including the test role and fixture rows, is rolled back.
BEGIN;
DO $$ BEGIN
    IF current_setting('server_version_num')::int / 10000 <> 17 THEN
        RAISE EXCEPTION 'PostgreSQL 17 required';
    END IF;
    IF EXISTS (SELECT 1 FROM pg_tables WHERE schemaname='mnaka'
        AND tablename <> 'schema_migrations' AND NOT rowsecurity) THEN
        RAISE EXCEPTION 'Owner table without RLS';
    END IF;
    IF EXISTS (SELECT 1 FROM pg_class c JOIN pg_namespace n ON n.oid=c.relnamespace
        WHERE n.nspname='mnaka' AND c.relkind='r' AND c.relname <> 'schema_migrations'
        AND NOT c.relforcerowsecurity) THEN RAISE EXCEPTION 'Missing FORCE RLS'; END IF;
END $$;
CREATE ROLE mnaka_security_fixture_runtime NOLOGIN NOSUPERUSER NOBYPASSRLS NOCREATEDB NOCREATEROLE;
GRANT USAGE ON SCHEMA mnaka TO mnaka_security_fixture_runtime;
GRANT SELECT,INSERT,UPDATE,DELETE ON ALL TABLES IN SCHEMA mnaka TO mnaka_security_fixture_runtime;
REVOKE ALL ON mnaka.schema_migrations FROM mnaka_security_fixture_runtime;

SELECT set_config('mnaka.actor_id','11111111-1111-4111-8111-111111111111',true);
INSERT INTO mnaka.actors(id,display_name,created_at) VALUES('11111111-1111-4111-8111-111111111111','Fixture A',now());
INSERT INTO mnaka.devices(id,actor_id,name,platform,created_at)
 VALUES('aaaaaaaa-aaaa-4aaa-8aaa-aaaaaaaaaaaa','11111111-1111-4111-8111-111111111111','A','fixture',now());
INSERT INTO mnaka.projects(id,actor_id,name,root_uri,created_at)
 VALUES('aaaaaaaa-bbbb-4aaa-8aaa-aaaaaaaaaaaa','11111111-1111-4111-8111-111111111111','A','fixture:a',now());
INSERT INTO mnaka.tasks(id,actor_id,device_id,execution_host_id,engine,state,request_json,idempotency_key,request_hash,created_at,updated_at)
 VALUES('aaaaaaaa-cccc-4aaa-8aaa-aaaaaaaaaaaa','11111111-1111-4111-8111-111111111111',
 'aaaaaaaa-aaaa-4aaa-8aaa-aaaaaaaaaaaa','aaaaaaaa-aaaa-4aaa-8aaa-aaaaaaaaaaaa','assistant','queued','{}','fixture-a','fixture-hash',now(),now());
INSERT INTO mnaka.task_steps(id,actor_id,task_id,ordinal,tool,args_json,state)
 VALUES('aaaaaaaa-dddd-4aaa-8aaa-aaaaaaaaaaaa','11111111-1111-4111-8111-111111111111',
 'aaaaaaaa-cccc-4aaa-8aaa-aaaaaaaaaaaa',0,'demo.echo','{}','queued');
INSERT INTO mnaka.content_objects(id,actor_id,storage_uri,sha256,size_bytes,encryption_key_ref,created_at)
 VALUES('aaaaaaaa-eeee-4aaa-8aaa-aaaaaaaaaaaa','11111111-1111-4111-8111-111111111111','fixture:content','fixture',0,'fixture:key',now());

SELECT set_config('mnaka.actor_id','22222222-2222-4222-8222-222222222222',true);
INSERT INTO mnaka.actors(id,display_name,created_at) VALUES('22222222-2222-4222-8222-222222222222','Fixture B',now());
INSERT INTO mnaka.devices(id,actor_id,name,platform,created_at)
 VALUES('bbbbbbbb-aaaa-4bbb-8bbb-bbbbbbbbbbbb','22222222-2222-4222-8222-222222222222','B','fixture',now());
SET LOCAL ROLE mnaka_security_fixture_runtime;
DO $$ DECLARE count_rows int; BEGIN
    SELECT count(*) INTO count_rows FROM mnaka.projects;
    IF count_rows <> 0 THEN RAISE EXCEPTION 'Cross-owner project read'; END IF;
    SELECT count(*) INTO count_rows FROM mnaka.tasks;
    IF count_rows <> 0 THEN RAISE EXCEPTION 'Cross-owner task read'; END IF;
    SELECT count(*) INTO count_rows FROM mnaka.task_steps;
    IF count_rows <> 0 THEN RAISE EXCEPTION 'Cross-owner child read'; END IF;
    SELECT count(*) INTO count_rows FROM mnaka.content_objects;
    IF count_rows <> 0 THEN RAISE EXCEPTION 'Cross-owner content read'; END IF;
    UPDATE mnaka.projects SET name='forbidden';
    GET DIAGNOSTICS count_rows = ROW_COUNT;
    IF count_rows <> 0 THEN RAISE EXCEPTION 'Cross-owner update'; END IF;
    BEGIN
        INSERT INTO mnaka.tasks(id,actor_id,device_id,execution_host_id,project_id,engine,state,request_json,idempotency_key,request_hash,created_at,updated_at)
        VALUES('bbbbbbbb-cccc-4bbb-8bbb-bbbbbbbbbbbb','22222222-2222-4222-8222-222222222222',
            'bbbbbbbb-aaaa-4bbb-8bbb-bbbbbbbbbbbb','bbbbbbbb-aaaa-4bbb-8bbb-bbbbbbbbbbbb',
            'aaaaaaaa-bbbb-4aaa-8aaa-aaaaaaaaaaaa','assistant','queued','{}','fixture-b','fixture',now(),now());
        RAISE EXCEPTION 'Cross-owner project reference accepted';
    EXCEPTION WHEN foreign_key_violation THEN NULL; END;
    BEGIN
        INSERT INTO mnaka.task_steps(id,actor_id,task_id,ordinal,tool,args_json,state)
        VALUES('bbbbbbbb-dddd-4bbb-8bbb-bbbbbbbbbbbb','22222222-2222-4222-8222-222222222222',
            'aaaaaaaa-cccc-4aaa-8aaa-aaaaaaaaaaaa',1,'demo.echo','{}','queued');
        RAISE EXCEPTION 'Cross-owner parent reference accepted';
    EXCEPTION WHEN foreign_key_violation THEN NULL; END;
    BEGIN
        PERFORM * FROM mnaka.schema_migrations;
        RAISE EXCEPTION 'Runtime accessed migration ledger';
    EXCEPTION WHEN insufficient_privilege THEN NULL; END;
    BEGIN
        ALTER TABLE mnaka.projects DISABLE ROW LEVEL SECURITY;
        RAISE EXCEPTION 'Runtime bypassed RLS';
    EXCEPTION WHEN insufficient_privilege THEN NULL; END;
END $$;
SELECT set_config('mnaka.actor_id','',true);
DO $$ BEGIN
    IF EXISTS(SELECT 1 FROM mnaka.devices) THEN RAISE EXCEPTION 'Missing identity did not deny'; END IF;
END $$;
ROLLBACK;
DO $$ BEGIN
    IF NULLIF(current_setting('mnaka.actor_id',true),'') IS NOT NULL THEN
        RAISE EXCEPTION 'Transaction identity leaked after rollback';
    END IF;
END $$;
