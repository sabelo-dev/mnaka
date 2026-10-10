// Assistant gateway. A signed-in client sends a message (and optionally one image); this function
// forwards it to the owner's own model over an OpenAI-compatible chat completions endpoint and
// stores the exchange in the caller's history. The model endpoint and its key never reach the browser.
//
// Secrets: MNAKA_LLM_BASE_URL (e.g. https://llm.example.com/v1), MNAKA_LLM_MODEL, and
// MNAKA_LLM_API_KEY if the endpoint needs one. The endpoint must be reachable from Supabase.
import { createClient } from "npm:@supabase/supabase-js@2";

const cors = {
  "Access-Control-Allow-Origin": "*",
  "Access-Control-Allow-Headers": "authorization, apikey, content-type, x-client-info",
  "Access-Control-Allow-Methods": "POST, OPTIONS",
};
const reply = (status: number, body: Record<string, unknown>) =>
  new Response(JSON.stringify(body), { status, headers: { ...cors, "Content-Type": "application/json" } });
const fail = (status: number, code: string, message: string) => reply(status, { code, message });

const MAX_TEXT = 4000;
const MAX_IMAGE = 4_000_000; // characters of data URL
const SYSTEM = "You are MNAKA, an assistant inside the user's workspace. Be direct and accurate. " +
  "You cannot run tools, read files, browse, or act on the user's computer; say so rather than pretending.";

Deno.serve(async (request) => {
  if (request.method === "OPTIONS") return new Response(null, { headers: cors });
  if (request.method !== "POST") return fail(405, "INVALID_ARGUMENT", "Use POST.");

  // The caller's own token scopes every database call below to that user's rows.
  const supabase = createClient(Deno.env.get("SUPABASE_URL")!, Deno.env.get("SUPABASE_ANON_KEY")!, {
    global: { headers: { Authorization: request.headers.get("Authorization") ?? "" } },
    auth: { persistSession: false, autoRefreshToken: false },
  });
  const { data: { user } } = await supabase.auth.getUser();
  if (!user) return fail(401, "UNAUTHENTICATED", "Sign in to use the assistant.");

  let input: { text?: unknown; image?: unknown };
  try { input = await request.json(); } catch { return fail(400, "INVALID_ARGUMENT", "Body must be JSON."); }
  const text = typeof input.text === "string" ? input.text.trim() : "";
  if (text.length < 1 || text.length > MAX_TEXT) return fail(400, "INVALID_ARGUMENT", "Message must be 1 to 4000 characters.");
  const image = input.image === undefined ? null : input.image;
  if (image !== null && (typeof image !== "string" || image.length > MAX_IMAGE || !/^data:image\/(png|jpeg);base64,[A-Za-z0-9+/=]+$/.test(image))) {
    return fail(400, "INVALID_ARGUMENT", "Image must be a PNG or JPEG data URL under the size limit.");
  }

  const base = (Deno.env.get("MNAKA_LLM_BASE_URL") ?? "").replace(/\/+$/, "");
  const model = Deno.env.get("MNAKA_LLM_MODEL") ?? "";
  if (!base || !model) return fail(503, "PROVIDER_UNAVAILABLE", "No model endpoint is configured for this workspace.");

  const [history, memories] = await Promise.all([
    supabase.from("mnaka_messages").select("role, content").order("created_at", { ascending: false }).limit(20),
    supabase.from("mnaka_memories").select("content").order("created_at", { ascending: true }).limit(50),
  ]);
  if (history.error || memories.error) return fail(503, "STORAGE_UNAVAILABLE", "Account storage is unavailable.");
  const remembered = memories.data.length
    ? "\n\nFacts the user asked you to remember:\n" + memories.data.map((row) => "- " + row.content).join("\n")
    : "";
  const messages = [
    { role: "system", content: SYSTEM + remembered },
    ...history.data.reverse().map((row) => ({ role: row.role, content: row.content })),
    { role: "user", content: image ? [{ type: "text", text }, { type: "image_url", image_url: { url: image } }] : text },
  ];

  let answer = "";
  try {
    const key = Deno.env.get("MNAKA_LLM_API_KEY");
    const response = await fetch(base + "/chat/completions", {
      method: "POST",
      headers: { "Content-Type": "application/json", ...(key ? { Authorization: "Bearer " + key } : {}) },
      body: JSON.stringify({ model, messages, max_tokens: 1024, stream: false }),
      signal: AbortSignal.timeout(90_000),
    });
    if (!response.ok) {
      console.error("model endpoint status", response.status);
      return fail(502, "PROVIDER_UNAVAILABLE", "The model endpoint returned an error.");
    }
    const content = (await response.json())?.choices?.[0]?.message?.content;
    answer = typeof content === "string" ? content.trim().slice(0, 60000) : "";
  } catch (error) {
    console.error("model endpoint unreachable", error instanceof Error ? error.name : "error");
    return fail(502, "PROVIDER_UNAVAILABLE", "The model endpoint could not be reached.");
  }
  if (!answer) return fail(502, "PROVIDER_UNAVAILABLE", "The model returned an empty reply.");

  // The image itself is never stored; history records only that one was attached.
  // Distinct timestamps keep the pair in order when history is read back.
  const now = Date.now();
  const stored = await supabase.from("mnaka_messages").insert([
    { role: "user", content: image ? text + "\n[image attached]" : text, created_at: new Date(now).toISOString() },
    { role: "assistant", content: answer, created_at: new Date(now + 1).toISOString() },
  ]);
  return reply(200, { text: answer, model, saved: !stored.error });
});
