// Assistant gateway. A signed-in client sends a message (and optionally one image); this function
// forwards it to the model provider the project's secrets select and stores the exchange in the
// caller's history. The provider's endpoint and key never reach the browser.
import { authenticate, completeChat, cors, fail, modelStatus, reply } from "../_shared/gateway.ts";
import type { ChatResult, ChatTurn } from "../_shared/providers.ts";

const MAX_TEXT = 4000;
const MAX_IMAGE = 4_000_000; // characters of data URL
const SYSTEM = "You are MNAKA, an assistant inside the user's workspace. Be direct and accurate. " +
  "You cannot run tools, read files, browse, or act on the user's computer; say so rather than pretending.";
// A mode selects a workflow. It grants nothing: no mode can run tools or touch the user's computer.
const MODES = ["assistant", "developer", "autonomous"];
const DEVELOPER = " The user chose Developer mode: answer as a senior software engineer. Give working code in fenced " +
  "blocks and name the file each belongs in, state the assumptions you made, and point out risks and missing tests. " +
  "You cannot see or change their files from here; to change loaded files they use the Developer screen.";
const PLANNER = " The user chose Autonomous mode: you will work through their latest request in steps, without " +
  "asking them anything in between. Reply now with only a numbered plan of 2 to 4 steps, one short line each, that " +
  "you can complete yourself in text. Leave out anything that would need tools, files, browsing or their computer. " +
  "If the request needs only a direct answer, reply with one line starting with \"1.\".";
const WORKER = " The user chose Autonomous mode: you are carrying out your own plan one step at a time. Do the step " +
  "you are given fully and do not ask the user questions; where something is unknown, choose a reasonable assumption and state it.";
const MAX_STEPS = 4;
const STEP_BUDGET_MS = 80_000; // no new step starts after this, so the reply returns before the client gives up

// Autonomous mode: one request to plan, then one per step, each seeing the results so far.
async function workThrough(system: string, turns: ChatTurn[]): Promise<ChatResult | Response> {
  const started = Date.now();
  const planned = await completeChat({ system: system + PLANNER, turns, maxTokens: 2000, effort: "low" });
  if (planned instanceof Response) return planned;
  const steps = planned.text.split("\n")
    .map((line) => line.match(/^\s*\d+[.)]\s+(.{3,300})$/)?.[1].trim() ?? "")
    .filter((step) => step.length > 0).slice(0, MAX_STEPS);
  // Nothing to break down: answer directly, with more care than a quick chat reply.
  if (steps.length < 2) return completeChat({ system, turns, maxTokens: 16000, effort: "medium" });

  const plan = steps.map((step, i) => `${i + 1}. ${step}`).join("\n");
  const results: string[] = [];
  let model = planned.model, stopped = "";
  for (let i = 0; i < steps.length; ++i) {
    if (Date.now() - started > STEP_BUDGET_MS) { stopped = "the time limit for one request was reached"; break; }
    const soFar = results.map((result, done) => `Result of step ${done + 1}:\n${result.slice(0, 6000)}`).join("\n\n");
    const brief = (soFar ? soFar + "\n\n" : "") + `Carry out step ${i + 1} of ${steps.length}: ${steps[i]}\nGive only this step's result.` +
      (i === steps.length - 1 ? " This is the last step, so finish with the complete answer to my request." : "");
    const result = await completeChat({
      system: system + WORKER, maxTokens: 8000, effort: "medium",
      turns: [...turns, { role: "assistant", text: plan }, { role: "user", text: brief }],
    });
    if (result instanceof Response) {
      if (results.length === 0) return result;
      stopped = "the model failed on the next step"; break;
    }
    results.push(result.text); model = result.model;
  }
  const text = "Plan\n" + plan + "\n\n" +
    results.map((result, i) => `Step ${i + 1}: ${steps[i]}\n${result}`).join("\n\n") +
    (stopped ? `\n\nStopped after step ${results.length} of ${steps.length}: ${stopped}. The remaining steps were not done.` : "");
  return { text, model, provider: planned.provider };
}

Deno.serve(async (request) => {
  if (request.method === "OPTIONS") return new Response(null, { headers: cors });
  if (request.method !== "POST") return fail(405, "INVALID_ARGUMENT", "Use POST.");
  const supabase = await authenticate(request);
  if (!supabase) return fail(401, "UNAUTHENTICATED", "Sign in to use the assistant.");

  let input: { text?: unknown; image?: unknown; probe?: unknown; mode?: unknown };
  try { input = await request.json(); } catch { return fail(400, "INVALID_ARGUMENT", "Body must be JSON."); }
  // Lets the client show "model not set up" or "not working" instead of failing on the first message.
  if (input.probe === true) return reply(200, await modelStatus());
  const text = typeof input.text === "string" ? input.text.trim() : "";
  if (text.length < 1 || text.length > MAX_TEXT) return fail(400, "INVALID_ARGUMENT", "Message must be 1 to 4000 characters.");
  const mode = input.mode === undefined ? "assistant" : input.mode;
  if (typeof mode !== "string" || !MODES.includes(mode)) return fail(400, "INVALID_ARGUMENT", "Mode must be assistant, developer or autonomous.");
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
  const answer = mode === "autonomous" ? await workThrough(SYSTEM + remembered, turns)
    : mode === "developer" ? await completeChat({ system: SYSTEM + DEVELOPER + remembered, turns, maxTokens: 16000, effort: "medium" })
    : await completeChat({ system: SYSTEM + remembered, turns, maxTokens: 16000, effort: "low" });
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
