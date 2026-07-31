#!/usr/bin/env node
// ============================================================
// CLI entry point for the translation system
// ============================================================

import { resolve } from 'node:path';
import { existsSync } from 'node:fs';
import { runPipeline, type PipelineOptions } from './pipeline.js';
import { setLogLevel, LogLevel } from './utils/logger.js';

// ---- Parse CLI arguments ----

interface CLIArgs {
  files: string[];
  force: boolean;
  dryRun: boolean;
  reviewOnly: boolean;
  status: boolean;
  seedCache: boolean;
  debugPrompt: boolean;
  fixPaths: boolean;
  verbose: boolean;
  quiet: boolean;
  help: boolean;
  model?: string;
  provider?: string;
  concurrency?: number;
  maxChars?: number;
  batchIndex?: number;
  apiFormat?: boolean;
}

function parseArgs(argv: string[]): CLIArgs {
  const args: CLIArgs = {
    files: [],
    force: false,
    dryRun: false,
    reviewOnly: false,
    status: false,
    seedCache: false,
    debugPrompt: false,
    fixPaths: false,
    verbose: false,
    quiet: false,
    help: false,
  };

  let i = 2; // skip node and script path
  while (i < argv.length) {
    const arg = argv[i];

    switch (arg) {
      case '--force':
      case '-f':
        args.force = true;
        break;
      case '--dry-run':
      case '-n':
        args.dryRun = true;
        break;
      case '--review-only':
      case '-r':
        args.reviewOnly = true;
        break;
      case '--status':
      case '-s':
        args.status = true;
        break;
      case '--seed-cache':
        args.seedCache = true;
        break;
      case '--debug-prompt':
        args.debugPrompt = true;
        break;
      case '--fix-links':
        args.fixPaths = true;
        break;
      case '--batch':
        args.batchIndex = parseInt(argv[++i], 10) - 1; // user-facing 1-based
        break;
      case '--api-format':
        args.apiFormat = true;
        break;
      case '--verbose':
      case '-v':
        args.verbose = true;
        break;
      case '--quiet':
      case '-q':
        args.quiet = true;
        break;
      case '--help':
      case '-h':
        args.help = true;
        break;
      case '--model':
        args.model = argv[++i];
        break;
      case '--provider':
        args.provider = argv[++i];
        break;
      case '--concurrency':
        args.concurrency = parseInt(argv[++i], 10);
        break;
      case '--max-chars':
        args.maxChars = parseInt(argv[++i], 10);
        break;
      default:
        if (!arg.startsWith('-')) {
          args.files.push(arg);
        } else {
          console.error(`Unknown option: ${arg}`);
          process.exit(1);
        }
    }
    i++;
  }

  return args;
}

function printUsage(): void {
  console.log(`
\x1b[1mGlyphix Documentation Translation System\x1b[0m

\x1b[33mUsage:\x1b[0m
  pnpm translate [options] [files...]

\x1b[33mArguments:\x1b[0m
  files                  Files, directories, or glob patterns to translate.
                         Paths are relative to src/ (or include the src/ prefix).
                         If omitted, all source files are processed.

                         Supported forms:
                           tutorials/getting-started.md   exact file
                           tutorials                      all files in a directory
                           tutorials/                     (same, trailing slash)
                           "tutorials/*.md"               glob — one level deep
                           "**/*.md"                      glob — recursive

\x1b[33mOptions:\x1b[0m
  -f, --force            Force re-translate all segments (ignore cache)
  -n, --dry-run          Parse and detect changes without writing output files
  -r, --review-only      Only generate review report, don't translate
  -s, --status           Show translation status and exit
  --seed-cache           Populate cache from existing translation files
  --debug-prompt         Show translation prompt for debugging
  --batch <n>            Show prompt for batch N (1-based, default: 1).
                         Each batch = one real LLM API call.
  --api-format           Output prompt in API JSON format
  --fix-links            Fix absolute/anchor links in translated output files
  -v, --verbose          Enable debug logging
  -q, --quiet            Only show errors
  --model <name>         Override LLM model (e.g., gpt-4o-mini)
  --provider <name>      Override LLM provider (e.g., openai)
  --concurrency <n>      Override max parallel API requests
  --max-chars <n>        Override max characters per batch (reduce when model output is unstable)
  -h, --help             Show this help message

\x1b[33mEnvironment Variables:\x1b[0m
  TRANSLATE_API_KEY      API key for the LLM provider (required)
  TRANSLATE_BASE_URL     Custom API base URL
  TRANSLATE_MODEL        Override model name

\x1b[33mExamples:\x1b[0m
  pnpm translate                               # Translate all changed files
  pnpm translate tutorials/getting-started.md  # Single file
  pnpm translate tutorials                     # All files in a directory
  pnpm translate tutorials api                 # Multiple directories
  pnpm translate "tutorials/*.md"              # Glob pattern
  pnpm translate --force                       # Re-translate everything
  pnpm translate --force tutorials             # Re-translate one directory
  pnpm translate --dry-run                     # Preview changes without writing
  pnpm translate --review-only                 # Check what needs translation
  pnpm translate --seed-cache                  # Populate cache from existing files
  pnpm translate --debug-prompt                # Show example prompt
  pnpm translate --debug-prompt intro.md       # Debug specific file
  pnpm translate --debug-prompt intro.md --batch 2  # Show second API call
  pnpm translate --model gpt-5-mini            # Use a different model
  pnpm translate --fix-links                   # Fix links in translated files
  pnpm translate --fix-links --dry-run         # Preview path fixes

\x1b[33mConfiguration:\x1b[0m
  Edit i18n/translate.config.json for persistent settings.
  Edit i18n/prompts/glossary.json for terminology consistency.
  Edit i18n/prompts/translate.md for custom prompt template.
`);
}

async function showStatus(): Promise<void> {
  const { loadConfig, resolveProjectPath } = await import('./config.js');
  const { CacheManager } = await import('./cache/index.js');
  const config = loadConfig();

  const cacheDir = resolveProjectPath(config.cacheDir);
  const cache = new CacheManager(
    cacheDir,
    config.sourceLanguage,
    config.targetLanguage,
  );
  const stats = cache.getStats();

  console.log(`
\x1b[1mTranslation Status\x1b[0m
  Source:     ${config.sourceLanguage} → ${config.targetLanguage}
  Source dir: ${config.sourceDir}
  Output dir: ${config.outputDir}
  Cache dir:  ${config.cacheDir}
  Model:      ${config.llm.model}
  Cached segments:  ${stats.segmentEntries}
  Tracked files:    ${stats.trackedFiles}
`);
}

// ---- Main ----

async function main(): Promise<void> {
  const args = parseArgs(process.argv);

  if (args.help) {
    printUsage();
    process.exit(0);
  }

  if (args.verbose) setLogLevel(LogLevel.DEBUG);
  if (args.quiet) setLogLevel(LogLevel.ERROR);

  if (args.status) {
    await showStatus();
    process.exit(0);
  }

  if (args.seedCache) {
    const { seedCache } = await import('./seed-cache.js');
    const result = await seedCache({
      force: args.force,
      dryRun: args.dryRun,
    });
    process.exit(result.errors > 0 ? 1 : 0);
  }

  if (args.debugPrompt) {
    const { debugPrompt } = await import('./debug-prompt.js');
    try {
      await debugPrompt({
        file: args.files[0], // Use first file if specified
        batchIndex: args.batchIndex,
        showApiFormat: args.apiFormat,
      });
      process.exit(0);
    } catch (error) {
      console.error('\x1b[31mDebug error:\x1b[0m', error instanceof Error ? error.message : error);
      process.exit(1);
    }
  }

  if (args.fixPaths) {
    const { fixPaths } = await import('./fix-links.js');
    const configOverrides: Record<string, unknown> = {};
    if (args.model || args.provider) {
      configOverrides.llm = {
        provider: args.provider ?? 'openai',
        model: args.model ?? 'gpt-4o',
      };
    }
    try {
      const result = await fixPaths({
        dryRun: args.dryRun,
        configOverrides,
        files: args.files.length > 0 ? args.files : undefined,
      });
      process.exit(result.errors > 0 ? 1 : 0);
    } catch (error) {
      console.error('\x1b[31mFix-links error:\x1b[0m', error instanceof Error ? error.message : error);
      process.exit(1);
    }
  }

  // Check for API key early
  if (!args.reviewOnly && !args.dryRun && !process.env.TRANSLATE_API_KEY) {
    console.error(
      '\x1b[31mError:\x1b[0m TRANSLATE_API_KEY environment variable is required.',
    );
    console.error(
      'Set it with: export TRANSLATE_API_KEY="your-api-key"',
    );
    console.error(
      'Or create a .env file in the web-docs/ directory.',
    );
    process.exit(1);
  }

  // Load .env file if present
  const envPath = resolve(process.cwd(), '.env');
  if (existsSync(envPath)) {
    const { readFileSync } = await import('node:fs');
    const envContent = readFileSync(envPath, 'utf-8');
    for (const line of envContent.split('\n')) {
      const trimmed = line.trim();
      if (!trimmed || trimmed.startsWith('#')) continue;
      const eqIndex = trimmed.indexOf('=');
      if (eqIndex === -1) continue;
      const key = trimmed.slice(0, eqIndex).trim();
      let value = trimmed.slice(eqIndex + 1).trim();
      // Remove surrounding quotes
      if (
        (value.startsWith('"') && value.endsWith('"')) ||
        (value.startsWith("'") && value.endsWith("'"))
      ) {
        value = value.slice(1, -1);
      }
      if (!process.env[key]) {
        process.env[key] = value;
      }
    }
  }

  const options: PipelineOptions = {
    files: args.files.length > 0 ? args.files : undefined,
    force: args.force,
    dryRun: args.dryRun,
    reviewOnly: args.reviewOnly,
    configOverrides: {},
  };

  if (args.model || args.provider || args.concurrency || args.maxChars) {
    options.configOverrides = {
      llm: {
        provider: args.provider || 'openai',
        model: args.model || 'gpt-4o',
      },
      ...(args.concurrency ? { concurrency: args.concurrency } : {}),
      ...(args.maxChars ? { maxBatchSize: args.maxChars } : {}),
    };
  }

  try {
    const result = await runPipeline(options);
    process.exit(result.errors > 0 ? 1 : 0);
  } catch (error) {
    console.error(
      '\x1b[31mFatal error:\x1b[0m',
      error instanceof Error ? error.message : error,
    );
    process.exit(2);
  }
}

main();
