// Assistant gateway. A signed-in client sends a message (and optionally one image); this function
// forwards it to the model provider the project's secrets select and stores the exchange in the
// caller's history. The provider's endpoint and key never reach the browser.
import { authenticate, completeChat, cors, fail, modelStatus, reply } from "../_shared/gateway.ts";
import type { ChatTurn } from "../_shared/providers.ts";

const MAX_TEXT = 4000;
const MAX_IMAGE = 4_000_000; // characters of data URL
const SYSTEM = "You are MNAKA, an assistant inside the user's workspace. Be direct and accurate. " +
  "You cannot run tools, read files, browse, or act on the user's computer; say so rather than pretending.";

Deno.serve(async (request) => {
  if (request.method === "OPTIONS") return new Response(null, { headers: cors });
  if (request.method !== "POST") return fail(405, "INVALID_ARGUMENT", "Use POST.");
  const supabase = await authenticate(request);
  if (!supabase) return fail(401, "UNAUTHENTICATED", "Sign in to use the assistant.");

  let input: { text?: unknown; image?: unknown; probe?: unknown };
  try { input = await request.json(); } catch { return fail(400, "INVALID_ARGUMENT", "Body must be JSON."); }
  // Lets the client show "model not set up" instead of failing on the first message. No model call is made.
  if (input.probe === true) return reply(200, modelStatus());
  const text = typeof input.text === "string" ? input.text.trim() : "";
  if (text.length < 1 || text.length > MAX_TEXT) return fail(400, "INVALID_ARGUMENT", "Message must be 1 to 4000 characters.");
  const image = input.image === undefined ? null : input.image;
  const imageParts = typeof image === "string" && image.length <= MAX_IMAGE
    ? image.match(/^data:(image\/png|image\/jpeg);base64,([A-Za-z0-9+/=]+)$/) : null;
  if (image !== null && !imageParts) {
    return fail(400, "INVALID_ARGUMENT", "Image must be a PNG or JPEG data URL under the size limit.");
  }

  const [history, memories] = await Promise.all([
    supabase.from("mnaka_messages").select("role, content").order("created_at", { ascending: false }).limit(20),
    supabase.from("mnaka_memories").select("content").order("created_at", { ascending: true }).limit(50),
  ]);
  if (history.error || memories.error) return fail(503, "STORAGE_UNAVAILABLE", "Account storage is unavailable.");
  const remembered = memories.data.length
    ? "\n\nFacts the user asked you to remember:\n" + memories.data.map((row) => "- " + row.content).join("\n")
    : "";
  const turns: ChatTurn[] = history.data.reverse().map((row) => ({ role: row.role === "user" ? "user" : "assistant", text: row.content }));
  turns.push({
    role: "user", text,
    ...(imageParts ? { image: { mediaType: imageParts[1] as "image/png" | "image/jpeg", base64: imageParts[2] } } : {}),
  });
  // Conversation is quick back-and-forth: low effort keeps replies fast and inexpensive.
  const answer = await completeChat({ system: SYSTEM + remembered, turns, maxTokens: 16000, effort: "low" });
  if (answer instanceof Response) return answer;
  const answerText = answer.text.slice(0, 60000);

  // The image itself is never stored; history records only that one was attached.
  // Distinct timestamps keep the pair in order when history is read back.
  const now = Date.now();
  const stored = await supabase.from("mnaka_messages").insert([
    { role: "user", content: imageParts ? text + "\n[image attached]" : text, created_at: new Date(now).toISOString() },
    { role: "assistant", content: answerText, created_at: new Date(now + 1).toISOString() },
  ]);
  return reply(200, { text: answerText, model: answer.model, provider: answer.provider, saved: !stored.error });
});
