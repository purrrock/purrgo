// ============================================================
// Type definitions for the translation system
// ============================================================

/** Supported segment types from Markdown parsing */
export type SegmentType =
  | 'frontmatter'
  | 'heading'
  | 'paragraph'
  | 'code_block'
  | 'container_open'
  | 'container_content'
  | 'container_close'
  | 'component_block'
  | 'list'
  | 'table'
  | 'blank'
  | 'thematic_break'
  | 'html_block';

/**
 * A segment is a logical block of markdown content.
 * The document is split into an ordered list of segments.
 */
export interface Segment {
  /** Segment type */
  type: SegmentType;
  /** Raw content of this segment (including markdown syntax) */
  content: string;
  /** Whether this segment should be translated */
  translatable: boolean;
  /** SHA-256 hash of the content (set during processing) */
  hash?: string;
  /** Translated content (set after translation) */
  translation?: string;
  /**
   * For component_block segments with translatable attributes:
   * the full original block content used to reconstruct the output.
   * `content` in this case holds only the translatable attribute value.
   */
  rawContent?: string;
}

/** A single glossary entry mapping source term to target term */
export interface GlossaryEntry {
  /** Source language term */
  source: string;
  /** Target language term */
  target: string;
  /** Optional context hint for disambiguation */
  context?: string;
  /** Whether matching is case-sensitive (default: false) */
  caseSensitive?: boolean;
}

/** A cached translation entry */
export interface CacheEntry {
  /** Original source text */
  source: string;
  /** Translated text */
  translation: string;
  /** Source language code */
  sourceLanguage: string;
  /** Target language code */
  targetLanguage: string;
  /** Model used for translation */
  model: string;
  /** ISO timestamp when translated */
  translatedAt: string;
}

/** Translation cache file structure */
export interface TranslationCache {
  version: number;
  entries: Record<string, CacheEntry>;
}

/** Per-file manifest tracking translation state */
export interface FileManifest {
  /** Hash of the source file content */
  sourceHash: string;
  /** Hash of the generated output file */
  outputHash: string;
  /** ISO timestamp of last write */
  lastWritten: string;
  /** Segment hashes for incremental updates */
  segmentHashes: string[];
}

/** Manifest store for all files */
export interface ManifestStore {
  version: number;
  files: Record<string, FileManifest>;
}

/** Review item types */
export type ReviewType =
  | 'human_edited'
  | 'new_translation'
  | 'updated_translation'
  | 'translation_error'
  | 'quality_warning';

/** A single review item for the review report */
export interface ReviewItem {
  /** Relative file path */
  file: string;
  /** Review type */
  type: ReviewType;
  /** Human-readable description */
  message: string;
  /** Original segment (if applicable) */
  sourceSegment?: string;
  /** Translated segment (if applicable) */
  translatedSegment?: string;
  /** Line number in output file (approximate) */
  line?: number;
}

/** Result of the translation pipeline for a single file */
export interface FileTranslationResult {
  /** Relative file path */
  file: string;
  /** Number of segments translated (new) */
  translated: number;
  /** Number of segments reused from cache */
  cached: number;
  /** Number of segments skipped (non-translatable) */
  skipped: number;
  /** Number of errors */
  errors: number;
  /** Review items generated */
  reviewItems: ReviewItem[];
}

/** Overall translation pipeline result */
export interface PipelineResult {
  /** Total files processed */
  totalFiles: number;
  /** Per-file results */
  fileResults: FileTranslationResult[];
  /** Aggregate counts */
  translated: number;
  cached: number;
  skipped: number;
  errors: number;
  /** All review items */
  reviewItems: ReviewItem[];
  /** Duration in milliseconds */
  duration: number;
}

/** Translation system configuration */
export interface TranslationConfig {
  /** Source language code (e.g., 'zh-CN') */
  sourceLanguage: string;
  /** Target language code (e.g., 'en') */
  targetLanguage: string;

  /** Source directory relative to project root (e.g., 'src') */
  sourceDir: string;
  /** Output directory relative to project root (e.g., 'src/en') */
  outputDir: string;
  /** Cache directory relative to project root */
  cacheDir: string;
  /** Review report output directory */
  reviewDir: string;
  /** Path to glossary JSON file */
  glossaryPath: string;
  /** Path to prompt template file */
  promptTemplatePath: string;

  /** Glob patterns to include */
  include: string[];
  /** Glob patterns to exclude */
  exclude: string[];

  /** LLM provider configuration */
  llm: LLMConfig;

  /** Whether to preserve human-edited translations */
  preserveHumanEdits: boolean;
  /** Maximum characters per translation batch */
  maxBatchSize: number;
  /** Number of segments to send per translation batch (default: 2) */
  segmentsPerBatch: number;
  /** Maximum parallel API requests */
  concurrency: number;

  /** Frontmatter fields that should be translated */
  translatableFrontmatterFields: string[];

  /** Component tag names that are block-level (content preserved as-is) */
  blockComponents: string[];
}

/** LLM provider configuration */
export interface LLMConfig {
  /** Provider name (e.g., 'openai') */
  provider: string;
  /** Model name (e.g., 'gpt-4o') */
  model: string;
  /** API key (can also be set via TRANSLATE_API_KEY env var) */
  apiKey?: string;
  /** Base URL for the API */
  baseUrl?: string;
  /** Temperature for generation */
  temperature?: number;
  /** Max tokens per response */
  maxTokens?: number;
}

/** Translator interface — implemented by each LLM provider */
export interface ITranslator {
  /**
   * Translate a batch of text segments.
   * @param segments Array of source text segments
   * @param context Additional context for translation
   * @returns Array of translated text segments (same order)
   */
  translateBatch(
    segments: string[],
    context: TranslationContext,
  ): Promise<string[]>;
}

/** Context passed to the translator for each batch */
export interface TranslationContext {
  sourceLanguage: string;
  targetLanguage: string;
  glossary: GlossaryEntry[];
  /** Surrounding non-translatable content for context */
  fileContext?: string;
  /** Custom prompt template content */
  promptTemplate?: string;
}
