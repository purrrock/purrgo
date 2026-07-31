// ============================================================
// Cache manager — stores and retrieves translation cache
// ============================================================
//
// Cache structure on disk:
//   .translation-cache/
//     manifest.json       — per-file metadata (source hash, output hash)
//     segments/            — segment-level translation cache
//       <lang-pair>.json  — e.g., zh-CN_en.json
//

import { readFileSync, writeFileSync, mkdirSync, existsSync } from 'node:fs';
import { resolve, dirname } from 'node:path';
import type {
  CacheEntry,
  TranslationCache,
  ManifestStore,
  FileManifest,
} from '../types.js';
import { computeHash } from '../markdown/parser.js';
import { logger } from '../utils/logger.js';

export class CacheManager {
  private cacheDir: string;
  private segmentCache: TranslationCache;
  private manifest: ManifestStore;
  private segmentCachePath: string;
  private manifestPath: string;
  private dirty = false;
  private manifestDirty = false;

  constructor(
    cacheDir: string,
    private sourceLanguage: string,
    private targetLanguage: string,
  ) {
    this.cacheDir = resolve(cacheDir);
    const segmentsDir = resolve(this.cacheDir, 'segments');
    this.segmentCachePath = resolve(
      segmentsDir,
      `${sourceLanguage}_${targetLanguage}.json`,
    );
    this.manifestPath = resolve(this.cacheDir, 'manifest.json');

    // Ensure directories exist
    mkdirSync(segmentsDir, { recursive: true });

    // Load existing caches
    this.segmentCache = this.loadJSON<TranslationCache>(
      this.segmentCachePath,
      { version: 1, entries: {} },
    );
    this.manifest = this.loadJSON<ManifestStore>(this.manifestPath, {
      version: 1,
      files: {},
    });
  }

  // ---- Segment cache operations ----

  /**
   * Look up a cached translation by segment content hash.
   */
  getSegmentTranslation(hash: string): CacheEntry | undefined {
    return this.segmentCache.entries[hash];
  }

  /**
   * Store a translated segment in cache.
   */
  setSegmentTranslation(
    hash: string,
    source: string,
    translation: string,
    model: string,
  ): void {
    this.segmentCache.entries[hash] = {
      source,
      translation,
      sourceLanguage: this.sourceLanguage,
      targetLanguage: this.targetLanguage,
      model,
      translatedAt: new Date().toISOString(),
    };
    this.dirty = true;
  }

  // ---- File manifest operations ----

  /**
   * Get the manifest for a specific file.
   */
  getFileManifest(relativePath: string): FileManifest | undefined {
    return this.manifest.files[relativePath];
  }

  /**
   * Check if a source file has changed since last translation.
   * Returns true if the file needs re-processing.
   */
  isFileChanged(relativePath: string, currentSourceHash: string): boolean {
    const manifest = this.manifest.files[relativePath];
    if (!manifest) return true;
    return manifest.sourceHash !== currentSourceHash;
  }

  /**
   * Check if the output file has been manually edited since last write.
   */
  isOutputEdited(relativePath: string, currentOutputContent: string): boolean {
    const manifest = this.manifest.files[relativePath];
    if (!manifest) return false;
    const currentHash = computeHash(currentOutputContent);
    return manifest.outputHash !== currentHash;
  }

  /**
   * Update the file manifest after successful translation.
   */
  updateFileManifest(
    relativePath: string,
    sourceHash: string,
    outputHash: string,
    segmentHashes: string[],
  ): void {
    this.manifest.files[relativePath] = {
      sourceHash,
      outputHash,
      lastWritten: new Date().toISOString(),
      segmentHashes,
    };
    this.manifestDirty = true;
  }

  // ---- Persistence ----

  /**
   * Save all dirty caches to disk.
   */
  save(): void {
    if (this.dirty) {
      this.writeJSON(this.segmentCachePath, this.segmentCache);
      logger.debug(`Cache saved: ${Object.keys(this.segmentCache.entries).length} entries`);
      this.dirty = false;
    }
    if (this.manifestDirty) {
      this.writeJSON(this.manifestPath, this.manifest);
      logger.debug(`Manifest saved: ${Object.keys(this.manifest.files).length} files`);
      this.manifestDirty = false;
    }
  }

  /**
   * Get cache statistics.
   */
  getStats(): { segmentEntries: number; trackedFiles: number } {
    return {
      segmentEntries: Object.keys(this.segmentCache.entries).length,
      trackedFiles: Object.keys(this.manifest.files).length,
    };
  }

  // ---- Private helpers ----

  private loadJSON<T>(filePath: string, defaultValue: T): T {
    if (!existsSync(filePath)) return defaultValue;
    try {
      const raw = readFileSync(filePath, 'utf-8');
      return JSON.parse(raw) as T;
    } catch {
      logger.warn(`Failed to parse cache file: ${filePath}, using defaults`);
      return defaultValue;
    }
  }

  private writeJSON(filePath: string, data: unknown): void {
    mkdirSync(dirname(filePath), { recursive: true });
    writeFileSync(filePath, JSON.stringify(data, null, 2), 'utf-8');
  }
}
