// ============================================================
// Configuration loading and defaults
// ============================================================

import { readFileSync, existsSync } from 'node:fs';
import { resolve, dirname } from 'node:path';
import { fileURLToPath } from 'node:url';
import process from 'node:process';
import type { TranslationConfig, GlossaryEntry } from './types.js';

const __dirname = dirname(fileURLToPath(import.meta.url));

/** Default configuration values */
export const DEFAULT_CONFIG: TranslationConfig = {
  sourceLanguage: 'zh-CN',
  targetLanguage: 'en',

  sourceDir: 'src',
  outputDir: 'src/en',
  cacheDir: '.translation-cache',
  reviewDir: '.translation-review',
  glossaryPath: 'i18n/prompts/glossary.json',
  promptTemplatePath: 'i18n/prompts/translate.md',

  include: ['**/*.md'],
  exclude: [
    '.vuepress/**',
    '**/node_modules/**',
    '.demo/**',
    '**/.demo/**',
  ],

  llm: {
    provider: 'openai',
    model: 'gemini-3-flash',
    temperature: 0.2,
    maxTokens: 16384,
  },

  preserveHumanEdits: true,
  maxBatchSize: 2000,
  segmentsPerBatch: 20,
  concurrency: 5,

  translatableFrontmatterFields: ['description', 'title', 'summary'],

  blockComponents: ['Glyphix', 'glyphix', 'ArchDiagram', 'arch-diagram'],
};

/**
 * Load and merge configuration from the config file.
 * Config file path: web-docs/i18n/translate.config.json
 * (or pass a custom path)
 */
export function loadConfig(
  overrides: Partial<TranslationConfig> = {},
): TranslationConfig {
  const projectRoot = resolve(__dirname, '..');
  const configPath = resolve(projectRoot, 'i18n', 'translate.config.json');

  let fileConfig: Partial<TranslationConfig> = {};
  if (existsSync(configPath)) {
    try {
      const raw = readFileSync(configPath, 'utf-8');
      fileConfig = JSON.parse(raw);
    } catch {
      console.warn(`Warning: Failed to parse config file: ${configPath}`);
    }
  }

  // Merge: defaults < file config < overrides < env vars
  const config: TranslationConfig = {
    ...DEFAULT_CONFIG,
    ...fileConfig,
    ...overrides,
    llm: {
      ...DEFAULT_CONFIG.llm,
      ...(fileConfig.llm || {}),
      ...(overrides.llm || {}),
    },
  };

  // Environment variable overrides
  if (process.env.TRANSLATE_API_KEY) {
    config.llm.apiKey = process.env.TRANSLATE_API_KEY;
  }
  if (process.env.TRANSLATE_BASE_URL) {
    config.llm.baseUrl = process.env.TRANSLATE_BASE_URL;
  }
  if (process.env.TRANSLATE_MODEL) {
    config.llm.model = process.env.TRANSLATE_MODEL;
  }

  return config;
}

/**
 * Load glossary entries from the glossary JSON file.
 */
export function loadGlossary(config: TranslationConfig): GlossaryEntry[] {
  const projectRoot = resolve(__dirname, '..');
  const glossaryPath = resolve(projectRoot, config.glossaryPath);

  if (!existsSync(glossaryPath)) {
    return [];
  }

  try {
    const raw = readFileSync(glossaryPath, 'utf-8');
    const data = JSON.parse(raw);
    if (Array.isArray(data)) {
      return data as GlossaryEntry[];
    }
    if (data.entries && Array.isArray(data.entries)) {
      return data.entries as GlossaryEntry[];
    }
    return [];
  } catch {
    console.warn(`Warning: Failed to parse glossary file: ${glossaryPath}`);
    return [];
  }
}

/**
 * Load the prompt template content.
 */
export function loadPromptTemplate(config: TranslationConfig): string {
  const projectRoot = resolve(__dirname, '..');
  const templatePath = resolve(projectRoot, config.promptTemplatePath);

  if (!existsSync(templatePath)) {
    return getDefaultPromptTemplate();
  }

  try {
    return readFileSync(templatePath, 'utf-8');
  } catch {
    console.warn(`Warning: Failed to read prompt template: ${templatePath}`);
    return getDefaultPromptTemplate();
  }
}

/**
 * Fallback prompt template embedded in code.
 */
function getDefaultPromptTemplate(): string {
  return `You are a professional technical documentation translator.
Translate the following content from {{sourceLanguage}} to {{targetLanguage}}.

## Rules
1. Preserve ALL Markdown formatting exactly: headings, lists, tables, links, bold, italic, etc.
2. DO NOT translate: inline code, code blocks, URLs, file paths, HTML/Vue component tags and their attributes, variable/function/class names, CLI commands.
3. Translate: prose text, descriptions, explanations, link display text (preserve URLs), heading text (preserve code identifiers).
4. Preserve VuePress-specific syntax: container directives (::: tip/warning/danger/details), custom component tags, frontmatter YAML keys.
5. Keep the same line breaks and blank lines as the original.
6. Use the glossary below for consistent terminology.
7. Output ONLY the translated content — no explanations, notes, or wrapping.

## Glossary
{{glossary}}

## Content
{{content}}`;
}

/**
 * Resolve a path relative to the project root (web-docs/).
 */
export function resolveProjectPath(...segments: string[]): string {
  const projectRoot = resolve(__dirname, '..');
  return resolve(projectRoot, ...segments);
}
