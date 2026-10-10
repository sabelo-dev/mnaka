-- Supabase only. Per-account data the signed-in client reads and writes through the Data API:
-- conversation history, remembered facts, saved skill manifests and proactive rules.
-- Every row belongs to one Supabase Auth user and is visible to that user alone.
-- Content is stored as plain text; there is no client-side encryption.

CREATE TABLE public.mnaka_messages(
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    user_id UUID NOT NULL DEFAULT auth.uid() REFERENCES auth.users(id) ON DELETE CASCADE,
    role TEXT NOT NULL CHECK (role IN ('user', 'assistant')),
    content TEXT NOT NULL CHECK (char_length(content) BETWEEN 1 AND 65536),
    created_at TIMESTAMPTZ NOT NULL DEFAULT now());
CREATE INDEX mnaka_messages_owner ON public.mnaka_messages(user_id, created_at);

CREATE TABLE public.mnaka_memories(
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    user_id UUID NOT NULL DEFAULT auth.uid() REFERENCES auth.users(id) ON DELETE CASCADE,
    content TEXT NOT NULL CHECK (char_length(content) BETWEEN 1 AND 2000),
    created_at TIMESTAMPTZ NOT NULL DEFAULT now());
CREATE INDEX mnaka_memories_owner ON public.mnaka_memories(user_id, created_at);

-- A saved manifest is a record only: saving grants no permission and runs nothing.
CREATE TABLE public.mnaka_skills(
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    user_id UUID NOT NULL DEFAULT auth.uid() REFERENCES auth.users(id) ON DELETE CASCADE,
    skill_id TEXT NOT NULL CHECK (char_length(skill_id) BETWEEN 1 AND 200),
    version TEXT NOT NULL CHECK (char_length(version) BETWEEN 1 AND 64),
    artifact_hash TEXT NOT NULL CHECK (char_length(artifact_hash) BETWEEN 1 AND 200),
    manifest JSONB NOT NULL CHECK (pg_column_size(manifest) <= 70000),
    created_at TIMESTAMPTZ NOT NULL DEFAULT now(),
    UNIQUE(user_id, artifact_hash));

-- A stored rule is a record only: nothing schedules or executes it yet.
CREATE TABLE public.mnaka_rules(
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    user_id UUID NOT NULL DEFAULT auth.uid() REFERENCES auth.users(id) ON DELETE CASCADE,
    title TEXT NOT NULL CHECK (char_length(title) BETWEEN 1 AND 200),
    schedule TEXT NOT NULL CHECK (char_length(schedule) BETWEEN 1 AND 200),
    enabled BOOLEAN NOT NULL DEFAULT false,
    created_at TIMESTAMPTZ NOT NULL DEFAULT now());
CREATE INDEX mnaka_rules_owner ON public.mnaka_rules(user_id, created_at);

ALTER TABLE public.mnaka_messages ENABLE ROW LEVEL SECURITY;
ALTER TABLE public.mnaka_memories ENABLE ROW LEVEL SECURITY;
ALTER TABLE public.mnaka_skills ENABLE ROW LEVEL SECURITY;
ALTER TABLE public.mnaka_rules ENABLE ROW LEVEL SECURITY;
CREATE POLICY mnaka_messages_owner ON public.mnaka_messages TO authenticated
    USING (user_id = (SELECT auth.uid())) WITH CHECK (user_id = (SELECT auth.uid()));
CREATE POLICY mnaka_memories_owner ON public.mnaka_memories TO authenticated
    USING (user_id = (SELECT auth.uid())) WITH CHECK (user_id = (SELECT auth.uid()));
CREATE POLICY mnaka_skills_owner ON public.mnaka_skills TO authenticated
    USING (user_id = (SELECT auth.uid())) WITH CHECK (user_id = (SELECT auth.uid()));
CREATE POLICY mnaka_rules_owner ON public.mnaka_rules TO authenticated
    USING (user_id = (SELECT auth.uid())) WITH CHECK (user_id = (SELECT auth.uid()));

-- Signed-out visitors get nothing; signed-in users get only the operations the client uses.
REVOKE ALL ON public.mnaka_messages, public.mnaka_memories, public.mnaka_skills, public.mnaka_rules FROM PUBLIC, anon, authenticated;
GRANT SELECT, INSERT, DELETE ON public.mnaka_messages, public.mnaka_memories, public.mnaka_skills TO authenticated;
GRANT SELECT, INSERT, DELETE ON public.mnaka_rules TO authenticated;
GRANT UPDATE (enabled) ON public.mnaka_rules TO authenticated;
