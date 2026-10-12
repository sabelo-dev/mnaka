// Shared by the account functions: CORS, replies, caller verification and the call to the
// owner's own model over an OpenAI-compatible chat completions endpoint.
//
// Secrets: MNAKA_LLM_BASE_URL (e.g. https://llm.example.com/v1), MNAKA_LLM_MODEL, and
// MNAKA_LLM_API_KEY if the endpoint needs one. The endpoint must be reachable from Supabase.
import { createClient, type SupabaseClient } from "npm:@supabase/supabase-js@2";

export const cors = {
  "Access-Control-Allow-Origin": "*",
  "Access-Control-Allow-Headers": "authorization, apikey, content-type, x-client-info",
  "Access-Control-Allow-Methods": "POST, OPTIONS",
};
export const reply = (status: number, body: Record<string, unknown>) =>
  new Response(JSON.stringify(body), { status, headers: { ...cors, "Content-Type": "application/json" } });
export const fail = (status: number, code: string, message: string) => reply(status, { code, message });

// The caller's own token scopes every database call made with the returned client to that user's rows.
export async function authenticate(request: Request): Promise<SupabaseClient | null> {
  const supabase = createClient(Deno.env.get("SUPABASE_URL")!, Deno.env.get("SUPABASE_ANON_KEY")!, {
    global: { headers: { Authorization: request.headers.get("Authorization") ?? "" } },
    auth: { persistSession: false, autoRefreshToken: false },
  });
  const { data: { user } } = await supabase.auth.getUser();
  return user ? supabase : null;
}

export const modelConfigured = () => Boolean((Deno.env.get("MNAKA_LLM_BASE_URL") ?? "").trim() && (Deno.env.get("MNAKA_LLM_MODEL") ?? "").trim());

// Returns the model's text, or a ready error response.
export async function completeChat(messages: unknown[], maxTokens: number): Promise<{ text: string; model: string } | Response> {
  const base = (Deno.env.get("MNAKA_LLM_BASE_URL") ?? "").replace(/\/+$/, "");
  const model = Deno.env.get("MNAKA_LLM_MODEL") ?? "";
  if (!base || !model) return fail(503, "PROVIDER_UNAVAILABLE", "No model endpoint is configured for this workspace.");
  try {
    const key = Deno.env.get("MNAKA_LLM_API_KEY");
    const response = await fetch(base + "/chat/completions", {
      method: "POST",
      headers: { "Content-Type": "application/json", ...(key ? { Authorization: "Bearer " + key } : {}) },
      body: JSON.stringify({ model, messages, max_tokens: maxTokens, stream: false }),
      signal: AbortSignal.timeout(110_000),
    });
    if (!response.ok) {
      console.error("model endpoint status", response.status);
      return fail(502, "PROVIDER_UNAVAILABLE", "The model endpoint returned an error.");
    }
    const content = (await response.json())?.choices?.[0]?.message?.content;
    const text = typeof content === "string" ? content.trim() : "";
    if (!text) return fail(502, "PROVIDER_UNAVAILABLE", "The model returned an empty reply.");
    return { text, model };
  } catch (error) {
    console.error("model endpoint unreachable", error instanceof Error ? error.name : "error");
    return fail(502, "PROVIDER_UNAVAILABLE", "The model endpoint could not be reached.");
  }
}
