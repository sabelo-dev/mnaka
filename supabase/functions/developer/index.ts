// Developer gateway. A signed-in client sends a change request and the text files it concerns;
// the configured model returns complete new contents for the files it wants to change. Nothing is
// written anywhere by this function: the client shows the difference and the user decides.
import { authenticate, completeChat, cors, fail, reply } from "../_shared/gateway.ts";

const MAX_REQUEST = 4000;
const MAX_FILES = 24;
const MAX_FILE = 65536;   // characters per file
const MAX_TOTAL = 262144; // characters across the project
const SAFE_PATH = /^[A-Za-z0-9._-]{1,120}$/;
const SYSTEM = [
  "You change source files on request. You are given a request and the full text of some project files.",
  "Reply with ONE JSON object and nothing else, no markdown fences:",
  '{"summary": "<one or two sentences on what you changed and why>", "files": [{"path": "<file name>", "content": "<complete new file content>"}]}',
  "Rules: include only files you change or add. Give each file's complete content, not a fragment or a diff.",
  "Use the exact file names you were given; a new file needs a plain file name with no folders.",
  'If the request needs no change or cannot be done from these files, return an empty "files" array and say why in "summary".',
  "You cannot build, run or test anything. Do not claim that you did.",
].join("\n");

type ProposedFile = { path: string; content: string };

// Models often wrap JSON in prose or fences; take the outermost object.
function extractObject(text: string): Record<string, unknown> | null {
  const start = text.indexOf("{"), end = text.lastIndexOf("}");
  if (start < 0 || end <= start) return null;
  try {
    const value = JSON.parse(text.slice(start, end + 1));
    return value && typeof value === "object" && !Array.isArray(value) ? value : null;
  } catch { return null; }
}

Deno.serve(async (request) => {
  if (request.method === "OPTIONS") return new Response(null, { headers: cors });
  if (request.method !== "POST") return fail(405, "INVALID_ARGUMENT", "Use POST.");
  const supabase = await authenticate(request);
  if (!supabase) return fail(401, "UNAUTHENTICATED", "Sign in to use the developer workspace.");

  let input: { request?: unknown; files?: unknown };
  try { input = await request.json(); } catch { return fail(400, "INVALID_ARGUMENT", "Body must be JSON."); }
  const ask = typeof input.request === "string" ? input.request.trim() : "";
  if (ask.length < 1 || ask.length > MAX_REQUEST) return fail(400, "INVALID_ARGUMENT", "Request must be 1 to 4000 characters.");
  if (!Array.isArray(input.files) || input.files.length < 1 || input.files.length > MAX_FILES) {
    return fail(400, "INVALID_ARGUMENT", "Send 1 to 24 files.");
  }
  const files: ProposedFile[] = [];
  let total = 0;
  for (const entry of input.files) {
    const path = entry?.path, content = entry?.content;
    if (typeof path !== "string" || !SAFE_PATH.test(path) || typeof content !== "string" || content.length > MAX_FILE
      || files.some((file) => file.path === path)) {
      return fail(400, "INVALID_ARGUMENT", "Each file needs a unique plain file name and text content within the size limit.");
    }
    total += content.length;
    files.push({ path, content });
  }
  if (total > MAX_TOTAL) return fail(400, "INVALID_ARGUMENT", "The files are too large together.");

  const project = files.map((file) => `=== ${file.path} ===\n${file.content}`).join("\n\n");
  // Code changes repay more thought than conversation does.
  const answer = await completeChat({
    system: SYSTEM,
    turns: [{ role: "user", text: `Request:\n${ask}\n\nProject files:\n\n${project}` }],
    maxTokens: 16000,
    effort: "medium",
  });
  if (answer instanceof Response) return answer;

  const parsed = extractObject(answer.text);
  if (!parsed || !Array.isArray(parsed.files)) return fail(502, "INVALID_MODEL_OUTPUT", "The model did not return a usable change set.");
  const proposed: ProposedFile[] = [];
  for (const entry of parsed.files.slice(0, MAX_FILES)) {
    const path = entry?.path, content = entry?.content;
    // Model output is untrusted: anything that is not a plain, bounded text file is dropped.
    if (typeof path !== "string" || !SAFE_PATH.test(path) || typeof content !== "string" || content.length > MAX_FILE) continue;
    if (!proposed.some((file) => file.path === path)) proposed.push({ path, content });
  }
  const summary = (typeof parsed.summary === "string" ? parsed.summary : "").trim().slice(0, 2000);

  // History keeps the request and the summary, never the file contents.
  const now = Date.now();
  await supabase.from("mnaka_messages").insert([
    { role: "user", content: "[developer] " + ask, created_at: new Date(now).toISOString() },
    { role: "assistant", content: summary || "(no summary)", created_at: new Date(now + 1).toISOString() },
  ]);
  return reply(200, { summary, files: proposed, model: answer.model });
});
