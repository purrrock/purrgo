// ============================================================
// Prompt builder — builds translation prompts from templates
// ============================================================

import type { GlossaryEntry, TranslationContext } from '../types.js';

/**
 * Build the system prompt for translation.
 */
export function buildSystemPrompt(context: TranslationContext): string {
  const template = context.promptTemplate || getDefaultTemplate();

  const glossaryText = formatGlossary(context.glossary);

  return template
    .replace(/\{\{sourceLanguage\}\}/g, getLanguageName(context.sourceLanguage))
    .replace(/\{\{targetLanguage\}\}/g, getLanguageName(context.targetLanguage))
    .replace(/\{\{sourceLanguageCode\}\}/g, context.sourceLanguage)
    .replace(/\{\{targetLanguageCode\}\}/g, context.targetLanguage)
    .replace(/\{\{glossary\}\}/g, glossaryText);
}

/**
 * Build the user message for a batch of segments.
 * Uses numbered markers to delimit segments within a batch.
 */
export function buildUserMessage(segments: string[]): string {
  if (segments.length === 1) {
    return segments[0];
  }

  return segments
    .map((seg, i) => `⟨SEG:${i}⟩\n${seg}`)
    .join('\n\n');
}

/**
 * Parse the LLM response back into individual translated segments.
 */
export function parseTranslationResponse(
  response: string,
  expectedCount: number,
): string[] {
  if (expectedCount === 1) {
    return [response.trim()];
  }

  // Try to split by segment markers
  const markerPattern = /⟨SEG:(\d+)⟩\n?/g;
  const parts: Array<{ index: number; content: string }> = [];
  let lastIndex = 0;
  let lastSegIndex = -1;
  let match: RegExpExecArray | null;

  while ((match = markerPattern.exec(response)) !== null) {
    if (lastSegIndex >= 0) {
      parts.push({
        index: lastSegIndex,
        content: response.slice(lastIndex, match.index).trim(),
      });
    }
    lastSegIndex = parseInt(match[1], 10);
    lastIndex = match.index + match[0].length;
  }

  // Last segment
  if (lastSegIndex >= 0) {
    parts.push({
      index: lastSegIndex,
      content: response.slice(lastIndex).trim(),
    });
  }

  // If we got the expected number of segments, return them in order
  if (parts.length === expectedCount) {
    const result = new Array<string>(expectedCount);
    for (const part of parts) {
      if (part.index < expectedCount) {
        result[part.index] = part.content;
      }
    }
    // Fill any gaps with empty strings
    for (let i = 0; i < expectedCount; i++) {
      if (!result[i]) result[i] = '';
    }
    return result;
  }

  // Fallback: if markers weren't preserved, try splitting by double newlines
  // and hope for the best (or return the whole response as segment 0)
  if (parts.length === 0) {
    // Try splitting by blank line groups
    const blocks = response.split(/\n\n+/);
    if (blocks.length === expectedCount) {
      return blocks.map((b) => b.trim());
    }
  }

  // Last resort: return whole response for segment 0, empty for the rest
  const fallback = new Array<string>(expectedCount).fill('');
  fallback[0] = response.trim();
  return fallback;
}

/**
 * Format glossary entries for inclusion in the prompt.
 */
function formatGlossary(glossary: GlossaryEntry[]): string {
  if (glossary.length === 0) {
    return '(No glossary entries defined)';
  }

  const lines = glossary.map((entry) => {
    let line = `- "${entry.source}" → "${entry.target}"`;
    if (entry.context) {
      line += ` (${entry.context})`;
    }
    return line;
  });

  return lines.join('\n');
}

/**
 * Map language codes to human-readable names.
 */
function getLanguageName(code: string): string {
  const names: Record<string, string> = {
    'zh-CN': 'Chinese (Simplified)',
    'zh-TW': 'Chinese (Traditional)',
    en: 'English',
    ja: 'Japanese',
    ko: 'Korean',
    fr: 'French',
    de: 'German',
    es: 'Spanish',
    pt: 'Portuguese',
    ru: 'Russian',
    ar: 'Arabic',
  };
  return names[code] || code;
}

/**
 * Default system prompt template.
 */
function getDefaultTemplate(): string {
  return `You are a professional technical documentation translator.
Translate the following content from {{sourceLanguage}} to {{targetLanguage}}.

## Rules
1. Preserve ALL Markdown formatting exactly: headings (#), lists (- / 1.), tables (|), links ([text](url)), bold (**), italic (*), etc.
2. DO NOT translate:
   - Inline code (\`...\`)
   - Code blocks (\`\`\`...\`\`\`)
   - URLs, file paths, and anchors
   - HTML/Vue component tags and all their attributes (e.g. \`<Decl>\`, \`<Glyphix>\`, \`<Experimental>\`)
   - Variable names, function names, class names, API identifiers
   - CLI commands and shell examples
3. Translate:
   - Prose text, descriptions, and explanations
   - Link display text (but keep the link URL unchanged)
   - Heading text (but preserve code identifiers within headings)
   - VuePress container titles (the text after ::: tip, ::: warning, etc.)
4. Preserve VuePress syntax exactly:
   - Container directives (::: tip, ::: warning, ::: danger, ::: details)
   - Custom component tags and their attributes
   - Frontmatter YAML keys (only translate values if instructed)
5. Keep the same line breaks, blank lines, and whitespace as the original.
6. Use the glossary below for terminology consistency.
7. Output ONLY the translated content. No explanations, notes, comments, or markdown code fences around the output.
8. When multiple segments are provided (marked with ⟨SEG:N⟩), translate each segment independently and preserve the segment markers in your output.

## Glossary
{{glossary}}`;
}
