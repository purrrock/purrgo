// ============================================================
// Translation pipeline — orchestrates the full translation flow
// ============================================================

import {
  readFileSync,
  writeFileSync,
  existsSync,
  mkdirSync,
  readdirSync,
} from 'node:fs';
import { resolve, relative, dirname, join } from 'node:path';
import type {
  TranslationConfig,
  Segment,
  GlossaryEntry,
  ITranslator,
  TranslationContext,
  FileTranslationResult,
  PipelineResult,
  ReviewItem,
} from './types.js';
import { loadConfig, loadGlossary, loadPromptTemplate, resolveProjectPath } from './config.js';
import { parseMarkdown, computeHash } from './markdown/parser.js';
import { reassemble } from './markdown/reassembler.js';
import { CacheManager } from './cache/index.js';
import { createTranslator } from './translator/index.js';
import { checkTranslationQuality } from './review/reporter.js';
import { generateReviewReport } from './review/reporter.js';
import { logger } from './utils/logger.js';

// ---------------------------------------------------------------------------
// Graceful interrupt support
// ---------------------------------------------------------------------------

/** Set to true when the user presses Ctrl+C during a pipeline run. */
let _abortRequested = false;

export interface PipelineOptions {
  /** Only translate these specific files (relative paths) */
  files?: string[];
  /** Force re-translate everything */
  force?: boolean;
  /** Dry run — don't write any files */
  dryRun?: boolean;
  /** Only generate review report, don't translate */
  reviewOnly?: boolean;
  /** Config overrides */
  configOverrides?: Partial<TranslationConfig>;
}

/**
 * Run the full translation pipeline.
 */
export async function runPipeline(
  options: PipelineOptions = {},
): Promise<PipelineResult> {
  const startTime = Date.now();

  // ---- Graceful Ctrl+C handling ----
  _abortRequested = false;
  // Preserve any previously registered SIGINT handlers (e.g. tsx / ts-node)
  const prevSigintHandlers = process.rawListeners('SIGINT').slice() as NodeJS.SignalsListener[];
  for (const h of prevSigintHandlers) process.removeListener('SIGINT', h);

  const sigintHandler = () => {
    // Stop spinner immediately so the terminal line is clean
    logger.stopProgress();
    process.stdout.write('\n');
    logger.warn('Interrupted — saving progress and generating partial report…');
    _abortRequested = true;
  };
  process.once('SIGINT', sigintHandler);

  // Load configuration
  const config = loadConfig(options.configOverrides);
  const glossary = loadGlossary(config);
  const promptTemplate = loadPromptTemplate(config);

  logger.info(
    `Translation: ${config.sourceLanguage} → ${config.targetLanguage}`,
  );
  logger.info(
    `Source: ${config.sourceDir} → Output: ${config.outputDir}`,
  );

  // Initialize cache
  const cacheDir = resolveProjectPath(config.cacheDir);
  const cache = new CacheManager(
    cacheDir,
    config.sourceLanguage,
    config.targetLanguage,
  );
  const stats = cache.getStats();
  logger.info(
    `Cache: ${stats.segmentEntries} segments, ${stats.trackedFiles} files tracked`,
  );

  // Initialize translator
  let translator: ITranslator | null = null;
  if (!options.reviewOnly) {
    translator = createTranslator(config.llm);
  }

  // Discover source files
  const sourceDir = resolveProjectPath(config.sourceDir);
  const outputDir = resolveProjectPath(config.outputDir);
  const allFiles = discoverFiles(sourceDir, config);

  // Filter to specific files if requested
  let filesToProcess: string[];
  if (options.files?.length) {
    filesToProcess = allFiles.filter((f) =>
      options.files!.some((pattern) =>
        matchesUserPattern(f, pattern, config.sourceDir),
      ),
    );
    if (filesToProcess.length === 0) {
      logger.warn('No matching files found for the specified patterns.');
      logger.info(
        `Patterns tried: ${options.files.join(', ')}`,
      );
      logger.info(
        `Available files (first 10): ${allFiles.slice(0, 10).join(', ')}${
          allFiles.length > 10 ? ` … (+${allFiles.length - 10} more)` : ''
        }`,
      );
    } else {
      logger.info(
        `Matched ${filesToProcess.length} file(s) from ${options.files.length} pattern(s)`,
      );
    }
  } else {
    filesToProcess = allFiles;
  }

  logger.info(`Found ${filesToProcess.length} source files to check`);

  // Process each file
  const fileResults: FileTranslationResult[] = [];
  const allReviewItems: ReviewItem[] = [];

  for (let fileIdx = 0; fileIdx < filesToProcess.length; fileIdx++) {
    if (_abortRequested) break;

    const file = filesToProcess[fileIdx];
    const result = await processFile(
      file,
      config,
      cache,
      translator,
      glossary,
      promptTemplate,
      sourceDir,
      outputDir,
      options,
      fileIdx,
      filesToProcess.length,
    );
    fileResults.push(result);
    allReviewItems.push(...result.reviewItems);

    // Save cache periodically
    cache.save();
  }

  // Final cache save (runs whether we finished normally or were interrupted)
  cache.save();

  // Aggregate results
  const pipelineResult: PipelineResult = {
    totalFiles: filesToProcess.length,
    fileResults,
    translated: fileResults.reduce((sum, r) => sum + r.translated, 0),
    cached: fileResults.reduce((sum, r) => sum + r.cached, 0),
    skipped: fileResults.reduce((sum, r) => sum + r.skipped, 0),
    errors: fileResults.reduce((sum, r) => sum + r.errors, 0),
    reviewItems: allReviewItems,
    duration: Date.now() - startTime,
  };

  // Generate review report (always, even on interrupt)
  const reviewDir = resolveProjectPath(config.reviewDir);
  generateReviewReport(pipelineResult, reviewDir);

  // Print summary
  printSummary(pipelineResult, _abortRequested);

  // Restore previously registered SIGINT handlers
  process.removeListener('SIGINT', sigintHandler);
  for (const h of prevSigintHandlers) process.addListener('SIGINT', h);

  if (_abortRequested) {
    // Exit with 130 (the conventional code for SIGINT-terminated processes)
    process.exit(130);
  }

  return pipelineResult;
}

/**
 * Process a single file: parse, translate, write output.
 */
async function processFile(
  relativePath: string,
  config: TranslationConfig,
  cache: CacheManager,
  translator: ITranslator | null,
  glossary: GlossaryEntry[],
  promptTemplate: string,
  sourceDir: string,
  outputDir: string,
  options: PipelineOptions,
  fileIdx = 0,
  totalFiles = 1,
): Promise<FileTranslationResult> {
  const result: FileTranslationResult = {
    file: relativePath,
    translated: 0,
    cached: 0,
    skipped: 0,
    errors: 0,
    reviewItems: [],
  };

  try {
    const sourcePath = resolve(sourceDir, relativePath);
    const outputPath = resolve(outputDir, relativePath);
    const sourceContent = readFileSync(sourcePath, 'utf-8');
    const sourceHash = computeHash(sourceContent);

    // Check if file has changed
    if (!options.force && !cache.isFileChanged(relativePath, sourceHash)) {
      logger.debug(`Unchanged: ${relativePath}`);
      result.skipped = 1; // Count as one skipped file
      return result;
    }

    logger.file(`Processing \x1b[36m[${fileIdx + 1}/${totalFiles}]\x1b[0m`, relativePath);

    // Check for human edits on existing output
    if (config.preserveHumanEdits && existsSync(outputPath)) {
      const existingOutput = readFileSync(outputPath, 'utf-8');
      if (cache.isOutputEdited(relativePath, existingOutput)) {
        logger.warn(`Human edits detected: ${relativePath}`);
        result.reviewItems.push({
          file: relativePath,
          type: 'human_edited',
          message:
            'Output file has been modified since last translation. Manual review recommended.',
        });

        if (!options.force) {
          // Skip this file to preserve human edits
          logger.info(`Skipping ${relativePath} to preserve human edits (use --force to override)`);
          result.skipped = 1;
          return result;
        }
      }
    }

    // Parse markdown into segments
    const segments = parseMarkdown(sourceContent, {
      blockComponents: config.blockComponents,
      translatableFrontmatterFields: config.translatableFrontmatterFields,
    });

    // Determine which segments need translation
    const segmentsToTranslate: Array<{ index: number; segment: Segment }> = [];

    for (let i = 0; i < segments.length; i++) {
      const segment = segments[i];

      if (!segment.translatable) {
        result.skipped++;
        continue;
      }

      // Check segment cache
      const cacheEntry = cache.getSegmentTranslation(segment.hash!);
      if (cacheEntry && !options.force) {
        segment.translation = cacheEntry.translation;
        result.cached++;
      } else {
        segmentsToTranslate.push({ index: i, segment });
      }
    }

    // Translate uncached segments
    if (segmentsToTranslate.length > 0 && translator && !options.reviewOnly) {
      const isNewFile = !cache.getFileManifest(relativePath);

      await translateSegments(
        segmentsToTranslate,
        config,
        cache,
        translator,
        glossary,
        promptTemplate,
        result,
        relativePath,
      );

      // Add review item for new/changed file
      if (isNewFile) {
        result.reviewItems.push({
          file: relativePath,
          type: 'new_translation',
          message: `New file: ${segmentsToTranslate.length} segments translated.`,
        });
      } else {
        result.reviewItems.push({
          file: relativePath,
          type: 'updated_translation',
          message: `Updated: ${segmentsToTranslate.length} segments re-translated.`,
        });
      }
    } else if (segmentsToTranslate.length > 0 && options.reviewOnly) {
      // In review-only mode, mark segments needing translation
      for (const { segment } of segmentsToTranslate) {
        result.reviewItems.push({
          file: relativePath,
          type: 'new_translation',
          message: 'Segment needs translation (review-only mode).',
          sourceSegment: segment.content,
        });
      }
    }

    // Reassemble and write output
    if (!options.dryRun && !options.reviewOnly) {
      const outputContent = reassemble(segments);

      mkdirSync(dirname(outputPath), { recursive: true });
      writeFileSync(outputPath, outputContent, 'utf-8');

      // Update file manifest
      const outputHash = computeHash(outputContent);
      const segmentHashes = segments.map((s) => s.hash!);
      cache.updateFileManifest(
        relativePath,
        sourceHash,
        outputHash,
        segmentHashes,
      );

      logger.success(
        `${relativePath} (${result.translated} new, ${result.cached} cached, ${result.skipped} skipped)`,
      );
    }
  } catch (error) {
    const msg = error instanceof Error ? error.message : String(error);
    logger.error(`Failed to process ${relativePath}: ${msg}`);
    result.errors++;
    result.reviewItems.push({
      file: relativePath,
      type: 'translation_error',
      message: `Processing failed: ${msg}`,
    });
  }

  return result;
}

/**
 * Translate segments using the LLM, batching by maxBatchSize (character count).
 *
 * Instead of sending a fixed number of segments per request, we pack as many
 * segments as possible into each batch (up to maxBatchSize characters and
 * segmentsPerBatch count limit). This gives the LLM much more context —
 * surrounding headings, paragraphs, etc. — and amortises the system prompt
 * cost across more translatable content.
 */
async function translateSegments(
  toTranslate: Array<{ index: number; segment: Segment }>,
  config: TranslationConfig,
  cache: CacheManager,
  translator: ITranslator,
  glossary: GlossaryEntry[],
  promptTemplate: string,
  result: FileTranslationResult,
  relativePath: string,
): Promise<void> {
  if (_abortRequested) return;
  const maxChars = config.maxBatchSize ?? 4000;
  const maxSegsPerBatch = config.segmentsPerBatch ?? 20;

  // Build batches based on character size
  const batches: Array<Array<{ index: number; segment: Segment }>> = [];
  let currentBatch: Array<{ index: number; segment: Segment }> = [];
  let currentSize = 0;

  for (const item of toTranslate) {
    const segSize = item.segment.content.length;
    // Start a new batch if adding this segment would exceed limits
    // (but always allow at least one segment per batch)
    if (
      currentBatch.length > 0 &&
      (currentSize + segSize > maxChars || currentBatch.length >= maxSegsPerBatch)
    ) {
      batches.push(currentBatch);
      currentBatch = [];
      currentSize = 0;
    }
    currentBatch.push(item);
    currentSize += segSize;
  }
  if (currentBatch.length > 0) {
    batches.push(currentBatch);
  }

  logger.debug(
    `${relativePath}: ${toTranslate.length} segments to translate (${batches.length} batch(es), max ${maxChars} chars)`,
  );

  const totalSegments = toTranslate.length;
  let doneSegments = 0;

  const context: TranslationContext = {
    sourceLanguage: config.sourceLanguage,
    targetLanguage: config.targetLanguage,
    glossary,
    promptTemplate,
  };

  // Translate each batch
  for (const batch of batches) {
    if (_abortRequested) {
      logger.warn(`Aborting remaining batches for ${relativePath}`);
      break;
    }

    const batchContents = batch.map(({ segment }) => segment.content);

    const segStart = Date.now();
    logger.startProgress(() => {
      const elapsed = ((Date.now() - segStart) / 1000).toFixed(1);
      const file    = `\x1b[33m${relativePath}\x1b[0m`;
      const segs    = `\x1b[2mseg\x1b[0m \x1b[32m${doneSegments}/${totalSegments}\x1b[0m`;
      const time    = `\x1b[90m${elapsed}s\x1b[0m`;
      // Show a short preview of the first segment in the batch
      const preview = batchContents[0].slice(0, 40).replace(/\n/g, ' ');
      const previewStr = `\x1b[2m${preview}${batchContents[0].length > 40 ? '…' : ''}\x1b[0m`;
      return `${file}  ${segs}  ${time}  ${previewStr}`;
    });

    try {
      const translations = await translator.translateBatch(batchContents, context);
      logger.stopProgress();

      for (let i = 0; i < batch.length; i++) {
        const { segment } = batch[i];
        const translation = translations[i];

        if (translation) {
          segment.translation = translation;

          cache.setSegmentTranslation(
            segment.hash!,
            segment.content,
            translation,
            config.llm.model,
          );

          const qualityItems = checkTranslationQuality(
            segment.content,
            translation,
            relativePath,
          );
          result.reviewItems.push(...qualityItems);

          result.translated++;
          doneSegments++;
        } else {
          logger.warn(
            `Empty translation for segment in ${relativePath}, using source as fallback`,
          );
          segment.translation = segment.content;
          result.errors++;
          result.reviewItems.push({
            file: relativePath,
            type: 'translation_error',
            message: `Empty translation received, source used as fallback.`,
            sourceSegment: segment.content,
          });
        }
      }
    } catch (error) {
      logger.stopProgress();
      const msg = error instanceof Error ? error.message : String(error);
      logger.error(
        `Translation failed for batch in ${relativePath}: ${msg}`,
      );

      // Fallback: use source content for all segments in the failed batch
      for (const { segment } of batch) {
        segment.translation = segment.content;
        result.errors++;
        result.reviewItems.push({
          file: relativePath,
          type: 'translation_error',
          message: `Segment translation failed: ${msg}. Source content used as fallback.`,
          sourceSegment: segment.content,
        });
      }
    }
  }
}

/**
 * Discover all markdown files in the source directory that match patterns.
 */
function discoverFiles(
  sourceDir: string,
  config: TranslationConfig,
): string[] {
  const files: string[] = [];

  function walk(dir: string): void {
    if (!existsSync(dir)) return;

    const entries = readdirSync(dir, { withFileTypes: true });
    for (const entry of entries) {
      const fullPath = join(dir, entry.name);
      const relPath = relative(sourceDir, fullPath);

      if (entry.isDirectory()) {
        // Check excludes
        if (shouldExclude(relPath + '/', config.exclude)) continue;
        walk(fullPath);
      } else if (entry.isFile()) {
        if (!matchesInclude(relPath, config.include)) continue;
        if (shouldExclude(relPath, config.exclude)) continue;
        files.push(relPath);
      }
    }
  }

  walk(sourceDir);
  return files.sort();
}

/**
 * Check if a path matches any of the include glob patterns.
 * Simplified glob matching (handles **.ext patterns).
 */
function matchesInclude(path: string, patterns: string[]): boolean {
  return patterns.some((pattern) => simpleGlobMatch(path, pattern));
}

/**
 * Check if a path should be excluded.
 */
function shouldExclude(path: string, patterns: string[]): boolean {
  return patterns.some((pattern) => simpleGlobMatch(path, pattern));
}

/**
 * Match a relative file path against a user-supplied pattern.
 *
 * Supported pattern forms:
 *   - Exact file path:   `tutorials/getting-started.md`
 *   - Directory path:    `tutorials` or `tutorials/`  → all files beneath
 *   - Glob pattern:      `tutorials/*.md`, `**\/api/*.md`
 *   - With sourceDir prefix:  `src/tutorials/getting-started.md`
 */
function matchesUserPattern(
  relPath: string,
  pattern: string,
  sourceDir: string,
): boolean {
  // Normalise: strip leading slashes
  let p = pattern.replace(/^\/+/, '');

  // Strip optional sourceDir prefix (e.g. 'src/')
  const sourceDirPrefix = sourceDir.replace(/\/*$/, '') + '/';
  if (p.startsWith(sourceDirPrefix)) {
    p = p.slice(sourceDirPrefix.length);
  }

  // Exact match
  if (p === relPath) return true;

  // Glob pattern (contains wildcard characters)
  if (p.includes('*') || p.includes('?') || p.includes('[')) {
    return simpleGlobMatch(relPath, p);
  }

  // Directory pattern: treat as prefix match
  const dirPrefix = p.replace(/\/*$/, '');
  return relPath.startsWith(dirPrefix + '/');
}

/**
 * Simple glob matching supporting:
 * - * (any chars except /)
 * - ** (any chars including /)
 * - ? (any single char)
 */
function simpleGlobMatch(path: string, pattern: string): boolean {
  // Convert glob to regex
  let regex = '';
  let i = 0;
  while (i < pattern.length) {
    const ch = pattern[i];
    if (ch === '*') {
      if (pattern[i + 1] === '*') {
        // ** matches any path
        regex += '.*';
        i += 2;
        if (pattern[i] === '/') i++; // Skip trailing /
      } else {
        regex += '[^/]*';
        i++;
      }
    } else if (ch === '?') {
      regex += '[^/]';
      i++;
    } else if ('.+^${}()|[]\\'.includes(ch)) {
      regex += '\\' + ch;
      i++;
    } else {
      regex += ch;
      i++;
    }
  }

  try {
    return new RegExp(`^${regex}$`).test(path);
  } catch {
    return false;
  }
}

/**
 * Print a summary of the pipeline results.
 */
function printSummary(result: PipelineResult, interrupted = false): void {
  const duration = (result.duration / 1000).toFixed(1);
  console.log('');
  const title = interrupted ? 'Translation Interrupted' : 'Translation Summary    ';
  console.log('╭────────────────────────────────────────╮');
  console.log(`│          \x1b[33m${title}\x1b[0m       │`);
  console.log('├────────────────────────────────────────┤');
  console.log(`│  Files processed:     ${padEnd(String(result.totalFiles), 17)}│`);
  console.log(`│  Segments translated: ${padEnd(String(result.translated), 17)}│`);
  console.log(`│  Segments cached:     ${padEnd(String(result.cached), 17)}│`);
  console.log(`│  Segments skipped:    ${padEnd(String(result.skipped), 17)}│`);
  console.log(`│  Errors:              ${padEnd(String(result.errors), 17)}│`);
  console.log(`│  Review items:        ${padEnd(String(result.reviewItems.length), 17)}│`);
  console.log(`│  Duration:            ${padEnd(duration + 's', 17)}│`);
  console.log('╰────────────────────────────────────────╯');
  console.log('');
}

function padEnd(str: string, len: number): string {
  return str + ' '.repeat(Math.max(0, len - str.length));
}
