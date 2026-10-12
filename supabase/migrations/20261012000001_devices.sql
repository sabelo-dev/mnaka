-- Devices the user has connected: each MNAKA install they chose to register with their account.
-- A row says what the device is and when it was last seen. It carries no credential and gives
-- no control over the device. Each row belongs to one Supabase Auth user.

CREATE TABLE public.mnaka_devices(
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    user_id UUID NOT NULL DEFAULT auth.uid() REFERENCES auth.users(id) ON DELETE CASCADE,
    -- Lets an install find its own row again. An identifier, not a secret.
    install_key TEXT NOT NULL CHECK (install_key ~ '^[0-9a-f]{32}$'),
    name TEXT NOT NULL CHECK (char_length(name) BETWEEN 1 AND 60),
    kind TEXT NOT NULL CHECK (kind IN ('desktop', 'web', 'android')),
    facts JSONB NOT NULL DEFAULT '{}' CHECK (jsonb_typeof(facts) = 'object' AND pg_column_size(facts) <= 4096),
    last_seen TIMESTAMPTZ NOT NULL DEFAULT now(),
    created_at TIMESTAMPTZ NOT NULL DEFAULT now(),
    UNIQUE (user_id, install_key));

-- The server's clock decides when a device was last seen, and an account holds at most 20.
CREATE FUNCTION public.mnaka_devices_touch() RETURNS trigger LANGUAGE plpgsql SET search_path = '' AS $$
BEGIN
    IF TG_OP = 'INSERT' AND (SELECT count(*) FROM public.mnaka_devices WHERE user_id = NEW.user_id) >= 20 THEN
        RAISE EXCEPTION 'device limit reached' USING ERRCODE = 'check_violation';
    END IF;
    NEW.last_seen := now();
    RETURN NEW;
END $$;
CREATE TRIGGER mnaka_devices_touch BEFORE INSERT OR UPDATE ON public.mnaka_devices
    FOR EACH ROW EXECUTE FUNCTION public.mnaka_devices_touch();

-- Selected as a column: a connected device reports in about once a minute while MNAKA is open.
CREATE FUNCTION public.online(device public.mnaka_devices) RETURNS boolean LANGUAGE sql STABLE SET search_path = '' AS $$
    SELECT now() - device.last_seen < interval '150 seconds'
$$;

ALTER TABLE public.mnaka_devices ENABLE ROW LEVEL SECURITY;
CREATE POLICY mnaka_devices_owner ON public.mnaka_devices TO authenticated
    USING (user_id = (SELECT auth.uid())) WITH CHECK (user_id = (SELECT auth.uid()));

REVOKE ALL ON public.mnaka_devices FROM PUBLIC, anon, authenticated;
GRANT SELECT, INSERT, DELETE ON public.mnaka_devices TO authenticated;
GRANT UPDATE (name, facts) ON public.mnaka_devices TO authenticated;
REVOKE ALL ON FUNCTION public.online(public.mnaka_devices), public.mnaka_devices_touch() FROM PUBLIC, anon;
GRANT EXECUTE ON FUNCTION public.online(public.mnaka_devices) TO authenticated;
