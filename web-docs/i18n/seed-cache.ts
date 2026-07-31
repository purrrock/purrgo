// ============================================================
// Seed cache — populate cache from existing translation files
// ============================================================

import { readFileSync, existsSync } from 'node:fs';
import { resolve, relative } from 'node:path';
import { parseMarkdown, computeHash } from './markdown/parser.js';
import { CacheManager } from './cache/index.js';
import { loadConfig, resolveProjectPath } from './config.js';
import { logger } from './utils/logger.js';

export interface SeedCacheOptions {
  /** Force re-seed everything, overwriting existing cache */
  force?: boolean;
  /** Dry run — don't actually write cache */
  dryRun?: boolean;
}

export interface SeedCacheResult {
  totalFiles: number;
  seeded: number;
  skipped: number;
  errors: number;
  duration: number;
}

/**
 * Seed the translation cache from existing output files.
 * This is useful when starting fresh but you already have translated files.
 */
export async function seedCache(options: SeedCacheOptions = {}): Promise<SeedCacheResult> {
  const startTime = Date.now();
  const config = loadConfig();
  const sourceDir = resolveProjectPath(config.sourceDir);
  const outputDir = resolveProjectPath(config.outputDir);
  
  logger.info(`Seeding cache from existing translations: ${config.sourceDir} → ${config.outputDir}`);

  // Initialize cache
  const cacheDir = resolveProjectPath(config.cacheDir);
  const cache = new CacheManager(
    cacheDir,
    config.sourceLanguage,
    config.targetLanguage,
  );

  const result: SeedCacheResult = {
    totalFiles: 0,
    seeded: 0,
    skipped: 0,
    errors: 0,
    duration: 0,
  };

  // Discover existing output files
  const outputFiles = await discoverFiles(outputDir, config.include, config.exclude);
  result.totalFiles = outputFiles.length;
  
  logger.info(`Found ${outputFiles.length} output files to process`);

  for (const [index, outputFile] of outputFiles.entries()) {
    try {
      await seedFileCache(
        outputFile,
        sourceDir,
        outputDir,
        config,
        cache,
        options,
        result,
        index + 1,
        outputFiles.length
      );
    } catch (error) {
      const msg = error instanceof Error ? error.message : String(error);
      logger.error(`Failed to seed cache for ${outputFile}: ${msg}`);
      result.errors++;
    }
  }

  // Save cache
  if (!options.dryRun) {
    cache.save();
    logger.info('Cache saved');
  }

  result.duration = Date.now() - startTime;
  printSeedSummary(result);

  return result;
}

/**
 * Seed cache for a single output file.
 */
async function seedFileCache(
  outputRelativePath: string,
  sourceDir: string,
  outputDir: string,
  config: any,
  cache: CacheManager,
  options: SeedCacheOptions,
  result: SeedCacheResult,
  fileIndex: number,
  totalFiles: number,
): Promise<void> {
  // Find corresponding source file
  const sourceRelativePath = outputRelativePath;
  const sourcePath = resolve(sourceDir, sourceRelativePath);
  const outputPath = resolve(outputDir, outputRelativePath);

  if (!existsSync(sourcePath)) {
    logger.warn(`No corresponding source file for ${outputRelativePath}, skipping`);
    result.skipped++;
    return;
  }

  logger.file(`Seeding \x1b[36m[${fileIndex}/${totalFiles}]\x1b[0m`, outputRelativePath);

  // Read both files
  const sourceContent = readFileSync(sourcePath, 'utf-8');
  const outputContent = readFileSync(outputPath, 'utf-8');

  // Parse segments
  const sourceSegments = parseMarkdown(sourceContent, {
    blockComponents: config.blockComponents,
  });
  const outputSegments = parseMarkdown(outputContent, {
    blockComponents: config.blockComponents,
  });

  // Filter to translatable segments
  const sourceTranslatableSegments = sourceSegments.filter(s => s.translatable);
  const outputTranslatableSegments = outputSegments.filter(s => s.translatable);

  if (sourceTranslatableSegments.length !== outputTranslatableSegments.length) {
    logger.warn(
      `Segment count mismatch in ${outputRelativePath}: ` +
      `source=${sourceTranslatableSegments.length}, output=${outputTranslatableSegments.length}`
    );
    // Continue anyway, pair up what we can
  }

  // Pair up segments and seed cache
  let seededCount = 0;
  const minCount = Math.min(sourceTranslatableSegments.length, outputTranslatableSegments.length);

  for (let i = 0; i < minCount; i++) {
    const sourceSegment = sourceTranslatableSegments[i];
    const outputSegment = outputTranslatableSegments[i];
    
    const sourceHash = computeHash(sourceSegment.content);
    sourceSegment.hash = sourceHash;

    // Check if already cached (unless force mode)
    const existing = cache.getSegmentTranslation(sourceHash);
    if (existing && !options.force) {
      continue;
    }

    // Seed the cache
    if (!options.dryRun) {
      cache.setSegmentTranslation(
        sourceHash,
        sourceSegment.content,
        outputSegment.content,
        config.llm.model,
      );
    }
    
    seededCount++;
  }

  // Update file manifest
  if (!options.dryRun && seededCount > 0) {
    const sourceHash = computeHash(sourceContent);
    const outputHash = computeHash(outputContent);
    const segmentHashes = sourceTranslatableSegments.map(s => s.hash!);
    
    cache.updateFileManifest(
      sourceRelativePath,
      sourceHash,
      outputHash,
      segmentHashes,
    );
  }

  if (seededCount > 0) {
    result.seeded++;
    logger.success(`${outputRelativePath} (${seededCount} segments seeded)`);
  } else {
    result.skipped++;
    logger.debug(`${outputRelativePath} (already cached)`);
  }
}

/**
 * Discover files in a directory matching include/exclude patterns.
 */
async function discoverFiles(
  dir: string,
  includePatterns: string[],
  excludePatterns: string[],
): Promise<string[]> {
  const { readdirSync } = await import('node:fs');
  const { join } = await import('node:path');
  
  const files: string[] = [];
  
  function walk(currentDir: string): void {
    if (!existsSync(currentDir)) return;
    
    const entries = readdirSync(currentDir, { withFileTypes: true });
    for (const entry of entries) {
      const fullPath = join(currentDir, entry.name);
      const relPath = relative(dir, fullPath);
      
      if (entry.isDirectory()) {
        if (shouldExclude(relPath + '/', excludePatterns)) continue;
        walk(fullPath);
      } else if (entry.isFile()) {
        if (!matchesInclude(relPath, includePatterns)) continue;
        if (shouldExclude(relPath, excludePatterns)) continue;
        files.push(relPath);
      }
    }
  }
  
  walk(dir);
  return files.sort();
}

/**
 * Check if a path matches any include patterns.
 */
function matchesInclude(path: string, patterns: string[]): boolean {
  return patterns.some(pattern => simpleGlobMatch(path, pattern));
}

/**
 * Check if a path should be excluded.
 */
function shouldExclude(path: string, patterns: string[]): boolean {
  return patterns.some(pattern => simpleGlobMatch(path, pattern));
}

/**
 * Simple glob matching supporting *, **, and ?.
 */
function simpleGlobMatch(path: string, pattern: string): boolean {
  let regex = '';
  let i = 0;
  
  while (i < pattern.length) {
    const ch = pattern[i];
    if (ch === '*') {
      if (pattern[i + 1] === '*') {
        regex += '.*';
        i += 2;
        if (pattern[i] === '/') i++;
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
 * Print summary of seed results.
 */
function printSeedSummary(result: SeedCacheResult): void {
  const duration = (result.duration / 1000).toFixed(1);
  console.log('');
  console.log('╭────────────────────────────────────────╮');
  console.log('│           \x1b[1mCache Seed Summary\x1b[0m           │');
  console.log('├────────────────────────────────────────┤');
  console.log(`│  Files processed:     ${padEnd(String(result.totalFiles), 17)}│`);
  console.log(`│  Files seeded:        ${padEnd(String(result.seeded), 17)}│`);
  console.log(`│  Files skipped:       ${padEnd(String(result.skipped), 17)}│`);
  console.log(`│  Errors:              ${padEnd(String(result.errors), 17)}│`);
  console.log(`│  Duration:            ${padEnd(duration + 's', 17)}│`);
  console.log('╰────────────────────────────────────────╯');
  console.log('');
}

function padEnd(str: string, len: number): string {
  return str + ' '.repeat(Math.max(0, len - str.length));
}