// ============================================================
// Translator factory — creates translator instances by provider
// ============================================================

import type { ITranslator, LLMConfig } from '../types.js';
import { OpenAITranslator } from './openai.js';

/**
 * Create a translator instance for the configured provider.
 */
export function createTranslator(config: LLMConfig): ITranslator {
  switch (config.provider) {
    case 'openai':
      return new OpenAITranslator(config);
    default:
      throw new Error(
        `Unknown LLM provider: "${config.provider}". Supported: openai`,
      );
  }
}

export { OpenAITranslator } from './openai.js';
export { buildSystemPrompt, buildUserMessage, parseTranslationResponse } from './prompt-builder.js';
