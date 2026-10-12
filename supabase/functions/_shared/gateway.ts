// Shared by the account functions: CORS, replies, caller verification, and the call to whichever
// model provider the project's secrets select (see providers.ts).
import { createClient, type SupabaseClient } from "npm:@supabase/supabase-js@2";
import { type ChatRequest, type ChatResult, ProviderError, selectProvider } from "./providers.ts";

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

// What the client is told about the model: whether one is usable, and which. No endpoint or key.
export function modelStatus(): { configured: boolean; provider: string; model: string } {
  const { provider } = selectProvider();
  return { configured: provider !== null, provider: provider?.id ?? "", model: provider?.model() ?? "" };
}

// Returns the model's reply, or a ready error response.
export async function completeChat(request: ChatRequest): Promise<ChatResult | Response> {
  const { provider, reason } = selectProvider();
  if (!provider) {
    console.error("no model provider:", reason);
    return fail(503, "PROVIDER_UNAVAILABLE", "No model is set up for this workspace.");
  }
  try {
    return await provider.complete(request);
  } catch (error) {
    if (error instanceof ProviderError) return fail(error.status === 200 ? 422 : error.status, error.code, error.message);
    console.error("model provider failed", error instanceof Error ? error.name : "error");
    return fail(502, "PROVIDER_UNAVAILABLE", "The model provider failed.");
  }
}
