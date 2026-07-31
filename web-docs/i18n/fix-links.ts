// ============================================================
// fix-links.ts — Fix absolute/anchor links in translated docs
//
// Replaces tools/fix-links.mjs with a TypeScript implementation
// that also handles anchor translation:
//   `./doc.md#某个标题`  →  `./doc.md#some-heading`
//
// Two-pass strategy:
//   1. Absolute path  → relative path    (original fix-links.mjs logic)
//   2. Non-ASCII anchor → translated anchor  (new: LLM-assisted matching)
// ============================================================

import { readFileSync, writeFileSync, existsSync, readdirSync } from 'node:fs';
import { resolve, relative, dirname, join, basename } from 'node:path';
import { loadConfig, resolveProjectPath } from './config.js';
import { createTranslator } from './translator/index.js';
import type { LLMConfig } from './types.js';
import { logger } from './utils/logger.js';

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

export interface FixPathsOptions {
  /** Dry run — show what would change without writing */
  dryRun?: boolean;
  /** Config overrides (e.g. model, provider) */
  configOverrides?: Record<string, unknown>;
  /**
   * Limit processing to these files/directories/globs.
   * Same syntax as the pipeline `files` option:
   *   - exact file:   `tutorials/getting-started.md`
   *   - directory:    `tutorials` or `tutorials/`
   *   - glob:         `tutorials/*.md`, `en/**\/api/*.md`
   *   - with src/ prefix:  `src/tutorials/getting-started.md`
   */
  files?: string[];
}

export interface FixPathsResult {
  fixedFiles: number;
  fixedLinks: number;
  fixedAnchors: number;
  errors: number;
}

// ---------------------------------------------------------------------------
// VuePress-compatible slugify (matches @mdit-vue/shared)
// ---------------------------------------------------------------------------

const rControl  = /[\u0000-\u001f]/g;
const rSpecial  = /[\s~`!@#$%^&*()\-_+=[\]{}|\\;:'",.<>/?]/g;
const rCombining = /[\u0300-\u036F]/g;

/**
 * Convert a heading text to a URL anchor fragment, matching how VuePress 2 /
 * vuepress-theme-hope generates heading IDs via @mdit-vue/shared.
 */
function slugify(str: string): string {
  return str
    .normalize('NFD')
    .replace(rCombining, '')
    .replace(rControl, '')
    .replace(rSpecial, '-')
    .replace(/-{2,}/g, '-')
    .replace(/^-+|-+$/g, '')
    .toLowerCase();
}

/**
 * Detect whether an anchor string likely originates from the source language
 * (i.e. contains non-ASCII characters that are NOT percent-encoded and NOT
 * plain ASCII slug chars).  Covers CJK, Cyrillic, Arabic, etc.
 */
function isNonAsciiAnchor(anchor: string): boolean {
  // anchor here is already decoded (no %-sequences expected)
  return /[^\x00-\x7F]/.test(anchor);
}

// ---------------------------------------------------------------------------
// Heading extraction
// ---------------------------------------------------------------------------

interface HeadingInfo {
  /** Heading level (1–6) */
  level: number;
  /** Raw heading text (inner content, no leading `#` or trailing whitespace) */
  text: string;
  /** Slugified anchor (for matching) */
  anchor: string;
}

/**
 * Extract all ATX-style headings from markdown content.
 * Returns them in document order.
 */
function extractHeadings(content: string): HeadingInfo[] {
  const results: HeadingInfo[] = [];
  // Skip fenced code blocks to avoid false positives
  const fenceRe = /^```[\s\S]*?^```/gm;
  const stripped = content.replace(fenceRe, (m) => '\n'.repeat(m.split('\n').length - 1));

  const headingRe = /^(#{1,6})\s+(.+?)(?:\s+#+)?\s*$/gm;
  let match: RegExpExecArray | null;
  while ((match = headingRe.exec(stripped)) !== null) {
    const text = match[2].trim();
    results.push({ level: match[1].length, text, anchor: slugify(text) });
  }
  return results;
}

// ---------------------------------------------------------------------------
// Path helpers (ported from fix-links.mjs)
// ---------------------------------------------------------------------------

interface ResolvedPath {
  file: string;
  /** Index into baseDirs that resolved the file */
  baseIndex: number;
}

/**
 * Given an absolute URL path such as `/framework/guide.md`, try to resolve it
 * to a real filesystem file by probing each directory in `baseDirs` in order.
 */
function resolveAbsPath(urlPath: string, baseDirs: string[]): ResolvedPath | null {
  const hashIdx = urlPath.indexOf('#');
  const rawPart = hashIdx === -1 ? urlPath : urlPath.slice(0, hashIdx);
  const pathPart = rawPart.startsWith('/') ? rawPart.slice(1) : rawPart;

  function candidatesFor(base: string): string[] {
    const joined = join(base, pathPart);
    if (pathPart.endsWith('.md')) return [joined];
    return [join(joined, 'README.md'), joined + '.md'];
  }

  for (let i = 0; i < baseDirs.length; i++) {
    for (const candidate of candidatesFor(baseDirs[i])) {
      if (existsSync(candidate)) return { file: candidate, baseIndex: i };
    }
  }
  return null;
}

function toRelativePath(fromFile: string, targetFile: string, anchor: string): string {
  let rel = relative(dirname(fromFile), targetFile).split(/[\\/]/).join('/');
  if (!rel.startsWith('.')) rel = './' + rel;
  return anchor ? rel + anchor : rel;
}

// ---------------------------------------------------------------------------
// LLM-assisted anchor matching
// ---------------------------------------------------------------------------

/**
 * Build a compact prompt asking the LLM to identity which translated heading
 * corresponds to the given source-language anchor/heading text.
 */
function buildAnchorMatchPrompt(
  sourceText: string,
  translatedHeadings: HeadingInfo[],
  sourceLanguage: string,
  targetLanguage: string,
): string {
  const headingList = translatedHeadings
    .map((h, i) => `${i + 1}. ${'#'.repeat(h.level)} ${h.text}`)
    .join('\n');

  return (
    `You are helping fix anchor links in translated documentation.\n\n` +
    `Source language: ${sourceLanguage}\n` +
    `Target language: ${targetLanguage}\n\n` +
    `The following anchor text appears in a ${sourceLanguage} document:\n` +
    `  "${sourceText}"\n\n` +
    `The translated (${targetLanguage}) document contains these headings:\n` +
    `${headingList}\n\n` +
    `Which heading number (1-based) corresponds to the source anchor text above?\n` +
    `Reply with ONLY the number. If none matches, reply with 0.`
  );
}

/**
 * Call the LLM to match a source-language anchor text to a translated heading.
 * Returns the matched HeadingInfo or null.
 */
async function llmMatchAnchor(
  sourceAnchorText: string,
  translatedHeadings: HeadingInfo[],
  llmConfig: LLMConfig,
  sourceLanguage: string,
  targetLanguage: string,
): Promise<HeadingInfo | null> {
  if (translatedHeadings.length === 0) return null;

  try {
    const translator = createTranslator(llmConfig);
    const prompt = buildAnchorMatchPrompt(
      sourceAnchorText,
      translatedHeadings,
      sourceLanguage,
      targetLanguage,
    );

    const results = await translator.translateBatch([prompt], {
      sourceLanguage,
      targetLanguage,
      glossary: [],
      // Use a neutral prompt template — we're sending a raw question
      promptTemplate:
        'Answer the user question directly. Output only what is asked, nothing else.\n\n## Question\n{{content}}',
    });

    const raw = (results[0] ?? '').trim();
    const idx = parseInt(raw, 10);
    if (!Number.isFinite(idx) || idx <= 0 || idx > translatedHeadings.length) return null;
    return translatedHeadings[idx - 1];
  } catch (err) {
    logger.debug(`LLM anchor match failed: ${err instanceof Error ? err.message : err}`);
    return null;
  }
}

// ---------------------------------------------------------------------------
// Anchor-translation cache & mapping
// ---------------------------------------------------------------------------

interface FileHeadingMap {
  source: HeadingInfo[];
  translated: HeadingInfo[];
}

/** Lazy-loaded heading maps keyed by source-file absolute path */
const headingCache = new Map<string, FileHeadingMap>();

/**
 * Load (and cache) heading info for a source/output file pair.
 * `srcFile`  = absolute path to the source (original-language) file.
 * `outFile`  = absolute path to the translated output file.
 */
function getHeadingMap(srcFile: string, outFile: string): FileHeadingMap {
  if (headingCache.has(srcFile)) return headingCache.get(srcFile)!;

  const source = existsSync(srcFile)
    ? extractHeadings(readFileSync(srcFile, 'utf-8'))
    : [];
  const translated = existsSync(outFile)
    ? extractHeadings(readFileSync(outFile, 'utf-8'))
    : [];

  const map: FileHeadingMap = { source, translated };
  headingCache.set(srcFile, map);
  return map;
}

/**
 * Given a non-ASCII anchor fragment (from the current translated file's link),
 * try to find the correct translated anchor.
 *
 * Strategy:
 *   1. Find source heading whose slug == anchor (positional matching)
 *   2. Return slug of translated heading at the same position
 *   3. If that fails, fall back to LLM
 *
 * Returns the new anchor string (with leading `#`), or the original if
 * no match is found.
 */
async function translateAnchor(
  anchor: string,   // e.g. "某个标题" (no leading #)
  srcFile: string,  // absolute path to source file
  outFile: string,  // absolute path to translated file
  llmConfig: LLMConfig | null,
  sourceLanguage: string,
  targetLanguage: string,
): Promise<{ newAnchor: string; changed: boolean }> {
  const map = getHeadingMap(srcFile, outFile);

  // --- Step 1: position-based matching ---
  // The anchor text from the link should match the slug of some source heading
  const answerSlug = anchor.toLowerCase(); // slugify is already applied on source headings

  const srcIdx = map.source.findIndex(
    (h) => h.anchor === answerSlug || slugify(h.text) === answerSlug,
  );

  if (srcIdx !== -1 && srcIdx < map.translated.length) {
    const translatedHeading = map.translated[srcIdx];
    const newAnchor = translatedHeading.anchor;
    if (newAnchor && newAnchor !== anchor) {
      logger.debug(
        `Anchor match (positional) "${anchor}" → "${newAnchor}" ` +
          `(heading: "${translatedHeading.text}")`,
      );
      return { newAnchor, changed: true };
    }
    if (newAnchor === anchor) return { newAnchor: anchor, changed: false };
  }

  // --- Step 2: LLM fallback ---
  if (llmConfig && map.translated.length > 0) {
    logger.debug(`Anchor "${anchor}" not found by position, trying LLM…`);
    const match = await llmMatchAnchor(
      anchor,
      map.translated,
      llmConfig,
      sourceLanguage,
      targetLanguage,
    );
    if (match) {
      const newAnchor = match.anchor;
      logger.debug(`Anchor match (LLM) "${anchor}" → "${newAnchor}" (heading: "${match.text}")`);
      return { newAnchor, changed: newAnchor !== anchor };
    }
  }

  logger.debug(`Could not resolve anchor "${anchor}" — keeping original`);
  return { newAnchor: anchor, changed: false };
}

// ---------------------------------------------------------------------------
// File processing
// ---------------------------------------------------------------------------

/**
 * Context for how to resolve paths for a specific file.
 */
interface FileContext {
  /** Ordered base directories for resolving absolute URL paths */
  baseDirs: string[];
  /**
   * Only base indices < preferredBaseCount count as "same-language" matches.
   * Matches beyond this keep their absolute URL for router fallback.
   */
  preferredBaseCount: number;
  /**
   * When non-null, source-language anchor translation is attempted.
   * sourceRoot: the root of the source-language tree (e.g. `src/`)
   * outputRoot: the root of the translated tree  (e.g. `src/en/`)
   */
  anchorContext: {
    sourceRoot: string;
    outputRoot: string;
    llmConfig: LLMConfig | null;
    sourceLanguage: string;
    targetLanguage: string;
  } | null;
}

interface LinkChange {
  /** Original URL fragment (path + anchor) inside the parens */
  from: string;
  /** Replacement fragment */
  to: string;
  /** What changed */
  type: 'path' | 'anchor' | 'both';
}

async function processFile(
  filePath: string,
  ctx: FileContext,
  dryRun: boolean,
): Promise<{ changed: boolean; fixedLinks: number; fixedAnchors: number; changes: LinkChange[] }> {
  const original = readFileSync(filePath, 'utf-8');

  // Matches inline/image link targets: (/<path>) or (/<path>#<anchor>)
  // Capture groups: (1) open-paren, (2) url-path, (3) anchor-with-#, (4) close-paren
  const LINK_RE = /(\()(\/[^()#\s][^()#\s]*?)(#[^()]*?)?(\))/g;

  let fixedLinks = 0;
  let fixedAnchors = 0;
  const changes: LinkChange[] = [];

  // We need async replacements, so collect all matches first
  interface LinkMatch {
    full: string;
    open: string;
    urlPath: string;
    anchor: string;
    close: string;
    index: number;
  }

  const matches: LinkMatch[] = [];
  let m: RegExpExecArray | null;
  while ((m = LINK_RE.exec(original)) !== null) {
    const [full, open, urlPath, anchor = '', close] = m as unknown as [string, string, string, string, string];
    if (urlPath.startsWith('//') || urlPath.includes('://')) continue;
    matches.push({ full, open, urlPath, anchor, close, index: m.index });
  }

  // Collect pure in-document anchor matches, e.g. (#some-heading)
  // Only needed when anchor translation context is available.
  if (ctx.anchorContext) {
    const ANCHOR_ONLY_RE = /\(#([^()#\s][^()]*?)\)/g;
    let a: RegExpExecArray | null;
    while ((a = ANCHOR_ONLY_RE.exec(original)) !== null) {
      const anchorText = a[1];
      if (!isNonAsciiAnchor(anchorText)) continue; // skip plain ASCII anchors
      // Avoid duplicates with LINK_RE matches at the same position
      if (matches.some((existing) => existing.index === a!.index)) continue;
      const full = a[0];           // e.g. "(#some-heading)"
      const anchor = '#' + anchorText;
      matches.push({ full, open: '(', urlPath: '', anchor, close: ')', index: a.index });
    }
  }

  if (matches.length === 0) return { changed: false, fixedLinks: 0, fixedAnchors: 0, changes: [] };

  // Build replacement map
  const replacements = new Map<number, string>();

  for (const match of matches) {
    const { full, open, urlPath, anchor, close, index } = match;

    // --- Part A: absolute path → relative ---
    const resolved = urlPath ? resolveAbsPath(urlPath, ctx.baseDirs) : null;
    let targetFile: string | null = null;
    let newUrlPart: string | null = null;

    if (urlPath === '') {
      // Pure anchor link — no path translation needed; target is the current file
      targetFile = filePath;
    } else if (resolved && resolved.baseIndex < ctx.preferredBaseCount) {
      const rel = toRelativePath(filePath, resolved.file, '');
      if (rel !== urlPath) {
        newUrlPart = rel;
        fixedLinks++;
      } else {
        newUrlPart = urlPath; // unchanged
      }
      targetFile = resolved.file;
    }
    // If resolved but outside preferred count → keep absolute (assets fallback)
    // If not resolved → keep original

    // --- Part B: non-ASCII anchor → translated anchor ---
    let newAnchor = anchor;
    if (
      anchor &&
      ctx.anchorContext &&
      isNonAsciiAnchor(anchor.slice(1)) // strip leading #
    ) {
      const anchorText = anchor.slice(1); // remove leading #

      // Determine which source/output files hold the headings
      // For pure anchor links the target is the current translated file itself.
      // For absolute links try to resolve against the output root.
      let translatedTarget: string | null = targetFile;
      if (!translatedTarget && urlPath) {
        // Try to resolve against output root
        const outResolved = resolveAbsPath(urlPath, [ctx.anchorContext.outputRoot]);
        if (outResolved) translatedTarget = outResolved.file;
      }

      if (translatedTarget) {
        // Derive source file path from translated file path
        const { sourceRoot, outputRoot } = ctx.anchorContext;
        const relToOutput = relative(outputRoot, translatedTarget);
        const sourceTarget = resolve(sourceRoot, relToOutput);

        if (existsSync(sourceTarget)) {
          const { newAnchor: resolved, changed } = await translateAnchor(
            anchorText,
            sourceTarget,
            translatedTarget,
            ctx.anchorContext.llmConfig,
            ctx.anchorContext.sourceLanguage,
            ctx.anchorContext.targetLanguage,
          );
          if (changed) {
            newAnchor = '#' + resolved;
            fixedAnchors++;
          }
        }
      }
    }

    // Build replacement string
    const finalUrlPart = newUrlPart ?? urlPath;
    const finalAnchor  = newAnchor;
    const replacement  = open + finalUrlPart + finalAnchor + close;

    if (replacement !== full) {
      replacements.set(index, replacement);
      const pathChanged   = newUrlPart !== null && newUrlPart !== urlPath;
      const anchorChanged = newAnchor !== anchor;
      changes.push({
        from: urlPath + anchor,
        to:   finalUrlPart + finalAnchor,
        type: pathChanged && anchorChanged ? 'both'
            : anchorChanged               ? 'anchor'
            :                               'path',
      });
    }
  }

  if (replacements.size === 0) return { changed: false, fixedLinks: 0, fixedAnchors: 0, changes: [] };

  // Apply replacements (iterate backwards by index to preserve positions)
  let result = original;
  const sortedIndices = [...replacements.keys()].sort((a, b) => b - a);
  for (const idx of sortedIndices) {
    const oldMatch = matches.find((m) => m.index === idx)!;
    const newStr   = replacements.get(idx)!;
    result = result.slice(0, idx) + newStr + result.slice(idx + oldMatch.full.length);
  }

  if (!dryRun) {
    writeFileSync(filePath, result, 'utf-8');
  }

  return { changed: true, fixedLinks, fixedAnchors, changes };
}

// ---------------------------------------------------------------------------
// File-pattern matching (mirrors pipeline.ts logic)
// ---------------------------------------------------------------------------

/**
 * Simple glob matching supporting * (no /), ** (any depth), ? (single char).
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
  return new RegExp('^' + regex + '$').test(path);
}

/**
 * Match a relative file path (relative to srcDir) against a user-supplied
 * pattern.  Supports exact file, directory prefix, and glob forms.
 */
function matchesUserPattern(
  relPath: string,
  pattern: string,
  sourceDir: string,
): boolean {
  let p = pattern.replace(/^\/+/, '');
  const sourceDirPrefix = sourceDir.replace(/\/*$/, '') + '/';
  if (p.startsWith(sourceDirPrefix)) p = p.slice(sourceDirPrefix.length);

  if (p === relPath) return true;
  if (p.includes('*') || p.includes('?') || p.includes('[')) {
    return simpleGlobMatch(relPath, p);
  }
  const dirPrefix = p.replace(/\/*$/, '');
  return relPath.startsWith(dirPrefix + '/');
}

// ---------------------------------------------------------------------------
// Directory scanning (ported from fix-links.mjs)
// ---------------------------------------------------------------------------

function collectMarkdownFiles(dir: string, skipDirs = new Set<string>()): string[] {
  const results: string[] = [];
  if (!existsSync(dir)) return results;

  for (const entry of readdirSync(dir, { withFileTypes: true })) {
    const fullPath = join(dir, entry.name);
    if (entry.isDirectory()) {
      if (!skipDirs.has(entry.name)) results.push(...collectMarkdownFiles(fullPath));
    } else if (entry.isFile() && entry.name.endsWith('.md')) {
      results.push(fullPath);
    }
  }
  return results;
}

// ---------------------------------------------------------------------------
// Main entry point
// ---------------------------------------------------------------------------

export async function fixPaths(options: FixPathsOptions = {}): Promise<FixPathsResult> {
  const dryRun = options.dryRun ?? false;

  // Load project config
  const config = loadConfig((options.configOverrides ?? {}) as never);
  const srcDir    = resolveProjectPath(config.sourceDir);
  const outputDir = resolveProjectPath(config.outputDir);

  // Determine language dirs (output dir name relative to srcDir)
  const outputRelToSrc = relative(srcDir, outputDir);
  // e.g. outputDir = src/en  → langDir = "en"
  const langDirNames = outputRelToSrc.includes('/')
    ? [outputRelToSrc.split('/')[0]]
    : [outputRelToSrc];

  logger.info(`Fix-links: source=${config.sourceDir}  output=${config.outputDir}`);
  if (dryRun) logger.info('Dry run — no files will be written.');

  // Build LLM config for anchor translation (null if no API key)
  let llmConfig: LLMConfig | null = null;
  const key = process.env.TRANSLATE_API_KEY ?? config.llm.apiKey ?? '';
  if (key) {
    llmConfig = { ...config.llm, apiKey: key };
  } else {
    logger.warn(
      'No TRANSLATE_API_KEY set — LLM-assisted anchor translation disabled. ' +
        'Only positional matching will be used.',
    );
  }

  const fixedFiles: string[] = [];
  let totalFixedLinks   = 0;
  let totalFixedAnchors = 0;
  let errors = 0;

  const cwd = process.cwd();

  /** Print per-file colored diff lines. */
  function logFileChanges(file: string, changes: LinkChange[]): void {
    const rel = relative(cwd, file);
    const tag = dryRun ? '\x1b[33m[dry-run]\x1b[0m' : '\x1b[32m[fixed]\x1b[0m';
    // Use logger.file-style timestamp
    process.stdout.write(
      `${tag} \x1b[1m${rel}\x1b[0m\n`,
    );
    for (const c of changes) {
      const typeTag =
        c.type === 'anchor' ? '\x1b[35m~anchor\x1b[0m' :
        c.type === 'both'   ? '\x1b[36m~both  \x1b[0m' :
                              '\x1b[33m~path  \x1b[0m';
      process.stdout.write(
        `  ${typeTag}  \x1b[90m${c.from}\x1b[0m  \x1b[90m→\x1b[0m  \x1b[32m${c.to}\x1b[0m\n`,
      );
    }
  }

  // ---- Pass 1: Primary language files (not inside any lang dir) ----
  const primarySkip = new Set(langDirNames);
  const allPrimaryFiles = collectMarkdownFiles(srcDir, primarySkip);
  const primaryFiles = options.files?.length
    ? allPrimaryFiles.filter((f) =>
        options.files!.some((pat) =>
          matchesUserPattern(relative(srcDir, f), pat, config.sourceDir),
        ),
      )
    : allPrimaryFiles;

  for (const file of primaryFiles) {
    try {
      const ctx: FileContext = {
        baseDirs: [srcDir],
        preferredBaseCount: 1,
        anchorContext: null, // primary files don't need anchor translation
      };
      const { changed, fixedLinks, fixedAnchors, changes } = await processFile(file, ctx, dryRun);
      if (changed) {
        fixedFiles.push(file);
        totalFixedLinks   += fixedLinks;
        totalFixedAnchors += fixedAnchors;
        logFileChanges(file, changes);
      }
    } catch (err) {
      logger.error(`Error processing ${file}: ${err instanceof Error ? err.message : err}`);
      errors++;
    }
  }

  // ---- Pass 2: Each translated language directory ----
  for (const langDir of langDirNames) {
    const langRoot = join(srcDir, langDir);
    if (!existsSync(langRoot)) {
      logger.warn(`Language directory not found, skipping: ${langRoot}`);
      continue;
    }

    const allLangFiles = collectMarkdownFiles(langRoot);
    const langFiles = options.files?.length
      ? allLangFiles.filter((f) =>
          options.files!.some((pat) =>
            matchesUserPattern(relative(srcDir, f), pat, config.sourceDir),
          ),
        )
      : allLangFiles;

    for (const file of langFiles) {
      try {
        const ctx: FileContext = {
          // Same-language root is preferred; primary srcDir is fallback for shared assets
          baseDirs: [langRoot, srcDir],
          preferredBaseCount: 1,
          anchorContext: {
            sourceRoot: srcDir,
            outputRoot: langRoot,
            llmConfig,
            sourceLanguage: config.sourceLanguage,
            targetLanguage: config.targetLanguage,
          },
        };
        const { changed, fixedLinks, fixedAnchors, changes } = await processFile(file, ctx, dryRun);
        if (changed) {
          fixedFiles.push(file);
          totalFixedLinks   += fixedLinks;
          totalFixedAnchors += fixedAnchors;
          logFileChanges(file, changes);
        }
      } catch (err) {
        logger.error(`Error processing ${file}: ${err instanceof Error ? err.message : err}`);
        errors++;
      }
    }
  }

  // ---- Report ----
  if (fixedFiles.length === 0) {
    logger.info('No files needed fixing.');
  } else {
    const parts: string[] = [
      `\x1b[1m${fixedFiles.length}\x1b[0m file(s)`,
    ];
    if (totalFixedLinks > 0)   parts.push(`\x1b[33m${totalFixedLinks} path(s)\x1b[0m`);
    if (totalFixedAnchors > 0) parts.push(`\x1b[35m${totalFixedAnchors} anchor(s)\x1b[0m`);
    const verb = dryRun ? '\x1b[33mWould fix\x1b[0m' : '\x1b[32mFixed\x1b[0m';
    logger.info(`${verb} ${parts.join(', ')}`);
  }
  if (errors > 0) {
    logger.warn(`${errors} file(s) failed — see errors above.`);
  }

  return {
    fixedFiles: fixedFiles.length,
    fixedLinks: totalFixedLinks,
    fixedAnchors: totalFixedAnchors,
    errors,
  };
}
