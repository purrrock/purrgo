// ============================================================
// Review report generator
// ============================================================

import { writeFileSync, mkdirSync } from 'node:fs';
import { resolve, dirname } from 'node:path';
import type { ReviewItem, PipelineResult } from '../types.js';
import { logger } from '../utils/logger.js';

/**
 * Generate a markdown review report from the pipeline results.
 */
export function generateReviewReport(
  result: PipelineResult,
  reviewDir: string,
): string | null {
  const { reviewItems } = result;

  if (reviewItems.length === 0) {
    logger.info('No review items — all translations look clean.');
    return null;
  }

  const timestamp = new Date().toLocaleString('sv-SE')
    .replace(/[:. ]/g, '') // Remove characters that are not good for filenames
    .replace(/-(\d\d)-(\d\d)/, '$1$2-'); // Remove dashes from date for better sorting
  const reportPath = resolve(reviewDir, `review-${timestamp}.md`);

  const content = buildReportContent(result, reviewItems);

  mkdirSync(dirname(reportPath), { recursive: true });
  writeFileSync(reportPath, content, 'utf-8');
  logger.info(`Review report written: ${reportPath}`);

  return reportPath;
}

/**
 * Build the markdown content for the review report.
 */
function buildReportContent(
  result: PipelineResult,
  items: ReviewItem[],
): string {
  const lines: string[] = [];
  const now = new Date().toISOString();

  lines.push('# Translation Review Report');
  lines.push('');
  lines.push(`**Generated:** ${now}  `);
  lines.push(`**Files processed:** ${result.totalFiles}  `);
  lines.push(
    `**Segments:** ${result.translated} translated, ${result.cached} cached, ${result.skipped} skipped, ${result.errors} errors  `,
  );
  lines.push(`**Duration:** ${(result.duration / 1000).toFixed(1)}s  `);
  lines.push(`**Review items:** ${items.length}  `);
  lines.push('');

  // Group by type
  const grouped = groupBy(items, (item) => item.type);

  // Human-edited files
  const humanEdited = grouped.get('human_edited');
  if (humanEdited?.length) {
    lines.push('## ✋ Human-Edited Files');
    lines.push('');
    lines.push(
      'These files appear to have been manually edited since last translation. Review carefully before overwriting.',
    );
    lines.push('');
    for (const item of humanEdited) {
      lines.push(`- **${item.file}**: ${item.message}`);
    }
    lines.push('');
  }

  // Translation errors
  const errors = grouped.get('translation_error');
  if (errors?.length) {
    lines.push('## ❌ Translation Errors');
    lines.push('');
    lines.push('These segments encountered errors during translation and need attention.');
    lines.push('');
    for (const item of errors) {
      lines.push(`### ${item.file}${item.line ? ` (line ~${item.line})` : ''}`);
      lines.push('');
      lines.push(item.message);
      if (item.sourceSegment) {
        lines.push('');
        lines.push('**Source:**');
        lines.push('```');
        lines.push(truncate(item.sourceSegment, 500));
        lines.push('```');
      }
      lines.push('');
    }
  }

  // Quality warnings
  const warnings = grouped.get('quality_warning');
  if (warnings?.length) {
    lines.push('## ⚠️ Quality Warnings');
    lines.push('');
    for (const item of warnings) {
      lines.push(`### ${item.file}${item.line ? ` (line ~${item.line})` : ''}`);
      lines.push('');
      lines.push(item.message);
      if (item.sourceSegment) {
        lines.push('');
        lines.push('**Source:**');
        lines.push('```');
        lines.push(truncate(item.sourceSegment, 300));
        lines.push('```');
      }
      if (item.translatedSegment) {
        lines.push('');
        lines.push('**Translation:**');
        lines.push('```');
        lines.push(truncate(item.translatedSegment, 300));
        lines.push('```');
      }
      lines.push('');
    }
  }

  // New translations
  const newTranslations = grouped.get('new_translation');
  if (newTranslations?.length) {
    lines.push('## 🆕 New Translations');
    lines.push('');
    lines.push(
      'These are newly translated files/segments. Consider reviewing for accuracy.',
    );
    lines.push('');
    for (const item of newTranslations) {
      lines.push(`- **${item.file}**: ${item.message}`);
    }
    lines.push('');
  }

  // Updated translations
  const updated = grouped.get('updated_translation');
  if (updated?.length) {
    lines.push('## 🔄 Updated Translations');
    lines.push('');
    lines.push(
      'These segments were re-translated due to source changes.',
    );
    lines.push('');
    for (const item of updated) {
      lines.push(`- **${item.file}**: ${item.message}`);
    }
    lines.push('');
  }

  return lines.join('\n');
}

/**
 * Run basic quality checks on translated content.
 */
export function checkTranslationQuality(
  source: string,
  translation: string,
  file: string,
): ReviewItem[] {
  const items: ReviewItem[] = [];

  // Check if translation is suspiciously short compared to source
  if (source.length > 20 && translation.length < source.length * 0.2) {
    items.push({
      file,
      type: 'quality_warning',
      message: `Translation seems too short (${translation.length} chars) compared to source (${source.length} chars).`,
      sourceSegment: source,
      translatedSegment: translation,
    });
  }

  // Check if markdown structure is preserved (heading levels)
  const sourceHeadings = source.match(/^#{1,6}\s/gm) || [];
  const translationHeadings = translation.match(/^#{1,6}\s/gm) || [];
  if (sourceHeadings.length !== translationHeadings.length) {
    items.push({
      file,
      type: 'quality_warning',
      message: `Heading count mismatch: source has ${sourceHeadings.length}, translation has ${translationHeadings.length}.`,
      sourceSegment: source,
      translatedSegment: translation,
    });
  }

  // Check if inline code count is preserved
  const sourceCode = source.match(/`[^`]+`/g) || [];
  const translationCode = translation.match(/`[^`]+`/g) || [];
  if (sourceCode.length !== translationCode.length) {
    items.push({
      file,
      type: 'quality_warning',
      message: `Inline code count mismatch: source has ${sourceCode.length}, translation has ${translationCode.length}.`,
      sourceSegment: source,
      translatedSegment: translation,
    });
  }

  // Check if link count is preserved
  const sourceLinks = source.match(/\[[^\]]*\]\([^)]*\)/g) || [];
  const translationLinks = translation.match(/\[[^\]]*\]\([^)]*\)/g) || [];
  if (sourceLinks.length !== translationLinks.length) {
    items.push({
      file,
      type: 'quality_warning',
      message: `Link count mismatch: source has ${sourceLinks.length}, translation has ${translationLinks.length}.`,
      sourceSegment: source,
      translatedSegment: translation,
    });
  }

  // Check if VuePress component tags are preserved
  const sourceComponents = source.match(/<\/?[A-Z][a-zA-Z]*[\s>]/g) || [];
  const transComponents = translation.match(/<\/?[A-Z][a-zA-Z]*[\s>]/g) || [];
  if (sourceComponents.length !== transComponents.length) {
    items.push({
      file,
      type: 'quality_warning',
      message: `Vue component tag count mismatch: source has ${sourceComponents.length}, translation has ${transComponents.length}.`,
      sourceSegment: source,
      translatedSegment: translation,
    });
  }

  return items;
}

// ---- Helpers ----

function groupBy<T, K>(items: T[], keyFn: (item: T) => K): Map<K, T[]> {
  const map = new Map<K, T[]>();
  for (const item of items) {
    const key = keyFn(item);
    const group = map.get(key) || [];
    group.push(item);
    map.set(key, group);
  }
  return map;
}

function truncate(str: string, maxLen: number): string {
  if (str.length <= maxLen) return str;
  return str.slice(0, maxLen) + '... (truncated)';
}
