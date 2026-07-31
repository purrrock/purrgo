// ============================================================
// OpenAI-compatible translator implementation
// ============================================================

import type {
  ITranslator,
  TranslationContext,
  LLMConfig,
} from '../types.js';
import {
  buildSystemPrompt,
  buildUserMessage,
  parseTranslationResponse,
} from './prompt-builder.js';
import { logger } from '../utils/logger.js';

/**
 * Translator using the OpenAI-compatible chat completions API.
 * Works with OpenAI, Azure OpenAI, Anthropic (via proxy), local models, etc.
 */
export class OpenAITranslator implements ITranslator {
  private config: LLMConfig;

  constructor(config: LLMConfig) {
    this.config = config;

    if (!config.apiKey) {
      throw new Error(
        'API key is required. Set TRANSLATE_API_KEY env var or configure llm.apiKey in translate.config.json',
      );
    }
  }

  async translateBatch(
    segments: string[],
    context: TranslationContext,
  ): Promise<string[]> {
    const systemPrompt = buildSystemPrompt(context);
    const userMessage = buildUserMessage(segments);

    logger.debug(
      `Translating batch of ${segments.length} segments (${userMessage.length} chars)`,
    );

    const response = await this.callAPI(systemPrompt, userMessage);
    const results = parseTranslationResponse(response, segments.length);

    // Validate we got the right number of results
    if (results.length !== segments.length) {
      logger.warn(
        `Expected ${segments.length} translations, got ${results.length}`,
      );
    }

    return results;
  }

  private async callAPI(
    systemPrompt: string,
    userMessage: string,
  ): Promise<string> {
    const baseUrl = (this.config.baseUrl || 'https://api.openai.com/v1').replace(
      /\/+$/,
      '',
    );
    const url = `${baseUrl}/chat/completions`;

    const body = {
      model: this.config.model,
      messages: [
        { role: 'system' as const, content: systemPrompt },
        { role: 'user' as const, content: userMessage },
      ],
      temperature: this.config.temperature ?? 0.1,
      max_tokens: this.config.maxTokens ?? 8192,
    };

    const maxRetries = 3;
    let lastError: Error | undefined;

    for (let attempt = 1; attempt <= maxRetries; attempt++) {
      try {
        const response = await fetch(url, {
          method: 'POST',
          headers: {
            'Content-Type': 'application/json',
            Authorization: `Bearer ${this.config.apiKey}`,
          },
          body: JSON.stringify(body),
        });

        if (!response.ok) {
          const errorText = await response.text().catch(() => 'Unknown error');
          const error = new Error(
            `API request failed (${response.status}): ${errorText}`,
          );

          // Retry on rate limit or server errors
          if (response.status === 429 || response.status >= 500) {
            lastError = error;
            const delay = Math.min(1000 * Math.pow(2, attempt), 30000);
            logger.warn(
              `API error ${response.status}, retrying in ${delay}ms (attempt ${attempt}/${maxRetries})`,
            );
            await sleep(delay);
            continue;
          }

          throw error;
        }

        const data = (await response.json()) as {
          choices: Array<{
            message: { content: string };
            finish_reason: string;
          }>;
          usage?: { prompt_tokens: number; completion_tokens: number };
        };

        if (!data.choices?.[0]?.message?.content) {
          throw new Error('Empty response from API');
        }

        if (data.usage) {
          logger.debug(
            `API usage: ${data.usage.prompt_tokens} prompt + ${data.usage.completion_tokens} completion tokens`,
          );
        }

        return data.choices[0].message.content;
      } catch (error) {
        lastError = error instanceof Error ? error : new Error(String(error));

        if (attempt < maxRetries) {
          const delay = Math.min(1000 * Math.pow(2, attempt), 30000);
          logger.warn(
            `Request failed: ${lastError.message}, retrying in ${delay}ms (attempt ${attempt}/${maxRetries})`,
          );
          await sleep(delay);
        }
      }
    }

    throw lastError || new Error('Translation request failed after retries');
  }
}

function sleep(ms: number): Promise<void> {
  return new Promise((resolve) => setTimeout(resolve, ms));
}
