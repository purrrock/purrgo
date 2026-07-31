// ============================================================
// Debug prompt — output translation prompts for debugging
// ============================================================

import { readFileSync, existsSync } from 'node:fs';
import { resolve } from 'node:path';
import { parseMarkdown } from './markdown/parser.js';
import { loadConfig, loadGlossary, loadPromptTemplate, resolveProjectPath } from './config.js';
import { buildSystemPrompt, buildUserMessage } from './translator/prompt-builder.js';
import type { TranslationConfig, TranslationContext, Segment } from './types.js';
import { logger } from './utils/logger.js';

export interface DebugPromptOptions {
  /** File to debug (relative to sourceDir) */
  file?: string;
  /**
   * Which batch to display (0-based).
   * Each batch corresponds to exactly one LLM API call.
   * Defaults to 0 (first batch).
   */
  batchIndex?: number;
  /** Show system prompt */
  showSystem?: boolean;
  /** Show user message */
  showUser?: boolean;
  /** Show both in API format (JSON) */
  showApiFormat?: boolean;
}

/** A single batch — maps 1-to-1 with one LLM API call */
export interface DebugBatch {
  index: number;
  segments: Segment[];
  systemPrompt: string;
  userMessage: string;
}

export interface DebugPromptResult {
  file?: string;
  totalSegments: number;
  translatableSegments: number;
  batches: DebugBatch[];
  selectedBatch: DebugBatch;
  context: TranslationContext;
}

/** Build batches using the same logic as translateSegments() in pipeline.ts */
function buildBatches(
  segments: Segment[],
  config: TranslationConfig,
): Segment[][] {
  const maxChars = config.maxBatchSize ?? 4000;
  const maxSegsPerBatch = config.segmentsPerBatch ?? 40;

  const batches: Segment[][] = [];
  let current: Segment[] = [];
  let currentSize = 0;

  for (const seg of segments) {
    if (!seg.translatable) continue;
    const segSize = seg.content.length;
    if (
      current.length > 0 &&
      (currentSize + segSize > maxChars || current.length >= maxSegsPerBatch)
    ) {
      batches.push(current);
      current = [];
      currentSize = 0;
    }
    current.push(seg);
    currentSize += segSize;
  }
  if (current.length > 0) batches.push(current);

  return batches;
}

/**
 * Generate and display translation prompts for debugging.
 */
export async function debugPrompt(options: DebugPromptOptions = {}): Promise<DebugPromptResult> {
  const config = loadConfig();
  const glossary = loadGlossary(config);
  const promptTemplate = loadPromptTemplate(config);

  const context: TranslationContext = {
    sourceLanguage: config.sourceLanguage,
    targetLanguage: config.targetLanguage,
    glossary,
    promptTemplate,
  };

  let segments: Segment[] = [];

  if (options.file) {
    const sourceDir = resolveProjectPath(config.sourceDir);
    const filePath = resolve(sourceDir, options.file);

    if (!existsSync(filePath)) {
      throw new Error(`File not found: ${filePath}`);
    }

    const content = readFileSync(filePath, 'utf-8');
    segments = parseMarkdown(content, {
      blockComponents: config.blockComponents,
      translatableFrontmatterFields: config.translatableFrontmatterFields,
    });

    logger.info(`Loaded file: ${options.file}`);
  } else {
    segments = generateExampleSegments();
  }

  const translatableSegments = segments.filter(s => s.translatable);
  const systemPrompt = buildSystemPrompt(context);

  // Build batches with identical logic to the pipeline
  const rawBatches = buildBatches(segments, config);

  if (rawBatches.length === 0) {
    throw new Error('No translatable segments found');
  }

  const batches: DebugBatch[] = rawBatches.map((segs, i) => ({
    index: i,
    segments: segs,
    systemPrompt,
    userMessage: buildUserMessage(segs.map(s => s.content)),
  }));

  const batchIndex = options.batchIndex ?? 0;
  if (batchIndex < 0 || batchIndex >= batches.length) {
    throw new Error(
      `Batch index ${batchIndex} out of range (0–${batches.length - 1})`,
    );
  }

  logger.info(
    `${batches.length} batch(es) total — showing batch ${batchIndex + 1}/${batches.length}`,
  );

  const selectedBatch = batches[batchIndex];

  const result: DebugPromptResult = {
    file: options.file,
    totalSegments: segments.length,
    translatableSegments: translatableSegments.length,
    batches,
    selectedBatch,
    context,
  };

  if (options.showApiFormat) {
    outputApiFormat(result);
  } else {
    outputHumanFormat(result, options);
  }

  return result;
}

/**
 * Generate example segments for testing prompt templates.
 */
function generateExampleSegments(): Segment[] {
  return [
    {
      type: 'heading',
      content: '# 快速开始指南',
      translatable: true,
    },
    {
      type: 'paragraph',
      content: '本指南将帮助您快速上手 Glyphix 图形框架。我们提供了简单易懂的步骤，让您能够在几分钟内创建第一个应用。',
      translatable: true,
    },
    {
      type: 'code_block',
      content: '```cpp\n#include <glyphix/app.h>\nint main() {\n  return glx::runApp();\n}\n```',
      translatable: false,
    },
    {
      type: 'paragraph',
      content: '上面的代码展示了一个最基础的 `Glyphix` 应用结构。`runApp()` 函数会初始化框架并启动事件循环。',
      translatable: true,
    },
  ];
}

/**
 * Output prompts in API-compatible JSON format for the selected batch.
 */
function outputApiFormat(result: DebugPromptResult): void {
  const batch = result.selectedBatch;
  const apiRequest = {
    model: 'gpt-4o',
    messages: [
      { role: 'system', content: batch.systemPrompt },
      { role: 'user',   content: batch.userMessage  },
    ],
    temperature: 0.4,
    max_tokens: 8192,
  };

  console.log(`\x1b[1mAPI Request JSON (batch ${batch.index + 1}/${result.batches.length}):\x1b[0m`);
  console.log(JSON.stringify(apiRequest, null, 2));
}

/**
 * Output prompts in human-readable format.
 */
function outputHumanFormat(result: DebugPromptResult, options: DebugPromptOptions): void {
  const showSystem = options.showSystem !== false;
  const showUser   = options.showUser   !== false;
  const batch      = result.selectedBatch;

  // ── Header ──────────────────────────────────────────────
  console.log('');
  console.log('\x1b[1m🔍 Translation Prompt Debug\x1b[0m');
  console.log('\x1b[2m' + '═'.repeat(50) + '\x1b[0m');
  console.log(`File:        ${result.file || '(example)'}`);
  console.log(`Translation: ${result.context.sourceLanguage} → ${result.context.targetLanguage}`);
  console.log(`Segments:    ${result.translatableSegments} translatable / ${result.totalSegments} total`);
  console.log(`Glossary:    ${result.context.glossary.length} entries`);
  console.log('');

  // ── Batch overview ────────────────────────────────────────
  console.log('\x1b[1m📦 Batches (= LLM API calls):\x1b[0m');
  console.log('\x1b[2m' + '─'.repeat(50) + '\x1b[0m');

  const systemTokens = estimateTokens(batch.systemPrompt);

  for (const b of result.batches) {
    const chars    = b.segments.reduce((s, seg) => s + seg.content.length, 0);
    const userToks = estimateTokens(b.userMessage);
    const marker   = b.index === batch.index ? '\x1b[1m\x1b[32m▶\x1b[0m' : ' ';
    const label    = b.index === batch.index ? `\x1b[1mbatch ${b.index + 1}\x1b[0m` : `batch ${b.index + 1}`;
    const types    = summariseTypes(b.segments);
    console.log(
      `${marker} ${label}  ${padEnd(String(b.segments.length) + ' seg', 8)}` +
      `  ${padEnd(String(chars) + ' ch', 9)}` +
      `  ~${padEnd(String(systemTokens + userToks) + ' tok', 10)}` +
      `  [${types}]`,
    );
  }
  console.log('');

  if (result.batches.length > 1) {
    console.log(
      `\x1b[2mShowing batch ${batch.index + 1}. Use --batch <n> to inspect a different batch.\x1b[0m`,
    );
    console.log('');
  }

  // ── Segment list for selected batch ───────────────────────
  console.log(`\x1b[1m📝 Batch ${batch.index + 1} — ${batch.segments.length} segment(s):\x1b[0m`);
  console.log('\x1b[2m' + '─'.repeat(50) + '\x1b[0m');
  batch.segments.forEach((seg, i) => {
    const preview = seg.content.slice(0, 70).replace(/\n/g, ' ');
    console.log(
      `  ${padEnd(String(i), 3)} \x1b[36m${padEnd(seg.type, 16)}\x1b[0m  ${preview}${seg.content.length > 70 ? '…' : ''}`,
    );
  });
  console.log('');

  // ── System prompt ─────────────────────────────────────────
  if (showSystem) {
    console.log('\x1b[1m🤖 System Prompt:\x1b[0m');
    console.log('\x1b[2m' + '─'.repeat(50) + '\x1b[0m');
    console.log(batch.systemPrompt);
    console.log('');
  }

  // ── User message ─────────────────────────────────────────
  if (showUser) {
    console.log('\x1b[1m👤 User Message:\x1b[0m');
    console.log('\x1b[2m' + '─'.repeat(50) + '\x1b[0m');
    console.log(batch.userMessage);
    console.log('');
  }

  // ── Token estimates ───────────────────────────────────────
  const userTokens  = estimateTokens(batch.userMessage);
  const totalTokens = systemTokens + userTokens;
  const systemPct   = Math.round((systemTokens / totalTokens) * 100);
  const userPct     = 100 - systemPct;

  console.log('\x1b[1m📊 Token Estimates (this batch):\x1b[0m');
  console.log('\x1b[2m' + '─'.repeat(50) + '\x1b[0m');
  console.log(`  System: ~${systemTokens} tokens  (${systemPct}%)`);
  console.log(`  User:   ~${userTokens} tokens  (${userPct}%)`);
  console.log(`  Total:  ~${totalTokens} tokens`);
  if (result.batches.length > 1) {
    const totalAllBatches = result.batches.reduce(
      (sum, b) => sum + systemTokens + estimateTokens(b.userMessage),
      0,
    );
    console.log(`  All ${result.batches.length} batches: ~${totalAllBatches} tokens`);
  }
  console.log('');
}

/**
 * Summarise segment types for a batch into a compact string.
 * e.g. "fm×1 h×3 p×5 code×2"
 */
function summariseTypes(segments: Segment[]): string {
  const counts: Record<string, number> = {};
  const abbrev: Record<string, string> = {
    frontmatter:    'fm',
    heading:        'h',
    paragraph:      'p',
    code_block:     'code',
    list:           'list',
    table:          'tbl',
    container_open: 'ctr',
    component_block:'cmp',
    html_block:     'html',
  };
  for (const seg of segments) {
    const key = abbrev[seg.type] ?? seg.type;
    counts[key] = (counts[key] ?? 0) + 1;
  }
  return Object.entries(counts)
    .map(([k, v]) => (v > 1 ? `${k}×${v}` : k))
    .join(' ');
}

/**
 * Rough token count estimation (1 token ≈ 4 chars for English, ~2.5 for Chinese).
 */
function estimateTokens(text: string): number {
  return Math.ceil(text.length / 3.5);
}

function padEnd(str: string, len: number): string {
  return str + ' '.repeat(Math.max(0, len - str.length));
}

/**
 * Pretty-print a segment for debugging.
 */
export function formatSegmentForDebug(segment: Segment, index?: number): string {
  const prefix      = index !== undefined ? `[${index + 1}] ` : '';
  const type        = `\x1b[36m${segment.type}\x1b[0m`;
  const translatable = segment.translatable ? '\x1b[32m✓\x1b[0m' : '\x1b[31m✗\x1b[0m';

  return `${prefix}${type} (${translatable}) ${segment.content.slice(0, 100)}${segment.content.length > 100 ? '…' : ''}`;
}
