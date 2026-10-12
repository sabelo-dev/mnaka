// Model provider adapter. The functions describe a conversation in neutral terms; a provider
// turns that into one backend's API. Which provider runs is decided by secrets on the Supabase
// project, read on every call, so the model can be replaced without rebuilding or redeploying
// the app or these functions:
//
//   MNAKA_LLM_PROVIDER   "anthropic" or "openai-compatible". Optional: when unset, an
//                        OpenAI-compatible endpoint is used if MNAKA_LLM_BASE_URL is set,
//                        otherwise Anthropic if ANTHROPIC_API_KEY is set.
//   MNAKA_LLM_MODEL      model name. Required for openai-compatible; defaults to
//                        claude-sonnet-5-5 for anthropic.
//   ANTHROPIC_API_KEY    key for the anthropic provider.
//   ANTHROPIC_WORKSPACE_ID  only for a key that is not scoped to a workspace: the workspace to use.
//   MNAKA_LLM_BASE_URL   e.g. https://llm.example.com/v1, for openai-compatible (Ollama, vLLM,
//                        llama.cpp server, or MNAKA's own model behind the same API).
//   MNAKA_LLM_API_KEY    key for the openai-compatible endpoint, if it needs one.
//
// To add another backend, implement ModelProvider and list it in `providers`.
import Anthropic from "npm:@anthropic-ai/sdk";

export type Effort = "low" | "medium" | "high";
export interface ChatImage { mediaType: "image/png" | "image/jpeg"; base64: string }
export interface ChatTurn { role: "user" | "assistant"; text: string; image?: ChatImage }
export interface ChatRequest { system: string; turns: ChatTurn[]; maxTokens: number; effort: Effort }
export interface ChatResult { text: string; model: string; provider: string }

// A failure the caller can show: `code` is one of the client's error codes.
export class ProviderError extends Error {
  constructor(readonly status: number, readonly code: string, message: string) { super(message); }
}

export interface ModelProvider {
  readonly id: string;
  // Empty when ready; otherwise what is missing, in words an operator can act on.
  missing(): string;
  model(): string;
  // A free call that proves the key, workspace and model name are accepted. Empty when they
  // are; otherwise a short reason. Never sends user content.
  verify(): Promise<string>;
  complete(request: ChatRequest): Promise<ChatResult>;
}

const env = (name: string) => (Deno.env.get(name) ?? "").trim();

// Reads ANTHROPIC_API_KEY. A key that is not tied to one workspace also needs to say which
// workspace to bill and apply limits to.
function anthropicClient(): Anthropic {
  const workspace = env("ANTHROPIC_WORKSPACE_ID");
  return new Anthropic(workspace ? { defaultHeaders: { "anthropic-workspace-id": workspace } } : {});
}

const anthropicProvider: ModelProvider = {
  id: "anthropic",
  missing: () => env("ANTHROPIC_API_KEY") ? "" : "ANTHROPIC_API_KEY is not set.",
  model: () => env("MNAKA_LLM_MODEL") || "claude-sonnet-5-5",
  async verify() {
    try {
      // Token counting is free and runs the same checks as a real request.
      await anthropicClient().messages.countTokens({ model: this.model(), messages: [{ role: "user", content: "ping" }] });
      return "";
    } catch (error) {
      if (error instanceof Anthropic.AuthenticationError || error instanceof Anthropic.PermissionDeniedError) return "The provider rejected the key.";
      if (error instanceof Anthropic.NotFoundError) return "The provider does not know this model name.";
      if (error instanceof Anthropic.BadRequestError) {
        console.error("anthropic verify:", error.message);
        return "The provider did not accept the request. A key that is not tied to a workspace also needs ANTHROPIC_WORKSPACE_ID.";
      }
      if (error instanceof Anthropic.RateLimitError) return ""; // the key works; it is only busy
      return "The provider could not be reached.";
    }
  },
  async complete(request) {
    const client = anthropicClient();
    const model = this.model();
    try {
      // Server-side fallback: if the model's safety classifiers decline a request that another
      // Claude model would answer, the API retries there inside the same call.
      const response = await client.beta.messages.create({
        model,
        max_tokens: request.maxTokens,
        betas: ["server-side-fallback-2026-07-01"],
        fallbacks: "default",
        output_config: { effort: request.effort },
        system: request.system,
        messages: request.turns.map((turn) => ({
          role: turn.role,
          content: turn.image
            ? [
              { type: "image" as const, source: { type: "base64" as const, media_type: turn.image.mediaType, data: turn.image.base64 } },
              { type: "text" as const, text: turn.text },
            ]
            : turn.text,
        })),
      });
      if (response.stop_reason === "refusal") {
        throw new ProviderError(200, "REFUSED", "The model declined this request.");
      }
      let text = "";
      for (const block of response.content) if (block.type === "text") text += block.text;
      text = text.trim();
      if (!text) {
        throw new ProviderError(502, "PROVIDER_UNAVAILABLE",
          response.stop_reason === "max_tokens" ? "The reply was cut off before it began. Try a shorter request." : "The model returned an empty reply.");
      }
      return { text, model: response.model, provider: this.id };
    } catch (error) {
      if (error instanceof ProviderError) throw error;
      if (error instanceof Anthropic.AuthenticationError || error instanceof Anthropic.PermissionDeniedError) {
        console.error("anthropic: key rejected");
        throw new ProviderError(503, "PROVIDER_UNAVAILABLE", "The model provider rejected the configured key.");
      }
      if (error instanceof Anthropic.RateLimitError) {
        throw new ProviderError(429, "RATE_LIMITED", "The model provider is rate limiting requests. Try again shortly.");
      }
      if (error instanceof Anthropic.BadRequestError) {
        console.error("anthropic: bad request", error.message);
        throw new ProviderError(502, "PROVIDER_UNAVAILABLE", "The model provider did not accept the request.");
      }
      if (error instanceof Anthropic.APIError) {
        console.error("anthropic: status", error.status);
        throw new ProviderError(502, "PROVIDER_UNAVAILABLE", "The model provider returned an error.");
      }
      console.error("anthropic: unreachable", error instanceof Error ? error.name : "error");
      throw new ProviderError(502, "PROVIDER_UNAVAILABLE", "The model provider could not be reached.");
    }
  },
};

// Any server that speaks the OpenAI chat completions API. This is the slot MNAKA's own model
// takes: serve it behind that API and point the secrets at it.
const openAiCompatibleProvider: ModelProvider = {
  id: "openai-compatible",
  missing: () => !env("MNAKA_LLM_BASE_URL") ? "MNAKA_LLM_BASE_URL is not set." : !env("MNAKA_LLM_MODEL") ? "MNAKA_LLM_MODEL is not set." : "",
  model: () => env("MNAKA_LLM_MODEL"),
  async verify() {
    // Most compatible servers list their models; one that does not is given the benefit of the doubt.
    try {
      const key = env("MNAKA_LLM_API_KEY");
      const response = await fetch(env("MNAKA_LLM_BASE_URL").replace(/\/+$/, "") + "/models", {
        headers: key ? { Authorization: "Bearer " + key } : {}, signal: AbortSignal.timeout(8000) });
      return response.status === 401 || response.status === 403 ? "The model endpoint rejected the key." : "";
    } catch {
      return "The model endpoint could not be reached.";
    }
  },
  async complete(request) {
    const base = env("MNAKA_LLM_BASE_URL").replace(/\/+$/, "");
    const key = env("MNAKA_LLM_API_KEY");
    const messages = [
      { role: "system", content: request.system },
      ...request.turns.map((turn) => ({
        role: turn.role,
        content: turn.image
          ? [{ type: "text", text: turn.text }, { type: "image_url", image_url: { url: `data:${turn.image.mediaType};base64,${turn.image.base64}` } }]
          : turn.text,
      })),
    ];
    let response: Response;
    try {
      response = await fetch(base + "/chat/completions", {
        method: "POST",
        headers: { "Content-Type": "application/json", ...(key ? { Authorization: "Bearer " + key } : {}) },
        body: JSON.stringify({ model: this.model(), messages, max_tokens: request.maxTokens, stream: false }),
        signal: AbortSignal.timeout(110_000),
      });
    } catch (error) {
      console.error("openai-compatible: unreachable", error instanceof Error ? error.name : "error");
      throw new ProviderError(502, "PROVIDER_UNAVAILABLE", "The model endpoint could not be reached.");
    }
    if (!response.ok) {
      console.error("openai-compatible: status", response.status);
      throw new ProviderError(502, "PROVIDER_UNAVAILABLE", "The model endpoint returned an error.");
    }
    const content = (await response.json())?.choices?.[0]?.message?.content;
    const text = typeof content === "string" ? content.trim() : "";
    if (!text) throw new ProviderError(502, "PROVIDER_UNAVAILABLE", "The model returned an empty reply.");
    return { text, model: this.model(), provider: this.id };
  },
};

const providers: ModelProvider[] = [anthropicProvider, openAiCompatibleProvider];

// The provider the secrets select, or null with the reason none is usable.
export function selectProvider(): { provider: ModelProvider | null; reason: string } {
  const named = env("MNAKA_LLM_PROVIDER").toLowerCase();
  if (named) {
    const provider = providers.find((candidate) => candidate.id === named) ?? null;
    if (!provider) return { provider: null, reason: `MNAKA_LLM_PROVIDER names an unknown provider. Use one of: ${providers.map((p) => p.id).join(", ")}.` };
    const missing = provider.missing();
    return missing ? { provider: null, reason: missing } : { provider, reason: "" };
  }
  if (env("MNAKA_LLM_BASE_URL")) {
    const missing = openAiCompatibleProvider.missing();
    return missing ? { provider: null, reason: missing } : { provider: openAiCompatibleProvider, reason: "" };
  }
  if (!anthropicProvider.missing()) return { provider: anthropicProvider, reason: "" };
  return { provider: null, reason: "No model provider is configured." };
}
