// ============================================================
// Markdown parser — splits a markdown document into segments
// ============================================================
//
// The parser uses a line-by-line state machine to identify:
// - Frontmatter (--- ... ---)
// - Fenced code blocks (``` or ~~~)
// - VuePress container directives (::: tip/warning/danger/details)
// - Block-level Vue/HTML components (<Glyphix>...</Glyphix>)
// - Regular markdown content (headings, paragraphs, lists, tables, etc.)
//
// Each segment is classified as translatable or non-translatable.

import { createHash } from 'node:crypto';
import type { Segment, SegmentType } from '../types.js';

/** Components whose entire block content should be preserved (not translated) */
const DEFAULT_BLOCK_COMPONENTS = ['Glyphix', 'glyphix'];

/** Attributes on block components whose values should be translated */
const TRANSLATABLE_COMPONENT_ATTRS = ['title', 'label', 'alt', 'description', 'tooltip'];

interface ParserOptions {
  blockComponents?: string[];
  /** Frontmatter YAML fields whose values should be translated */
  translatableFrontmatterFields?: string[];
}

// ---- Regex patterns ----

const FRONTMATTER_DELIMITER = /^---\s*$/;
const FENCED_CODE_OPEN = /^(`{3,}|~{3,})(.*)$/;
const CONTAINER_OPEN = /^(:{3,})\s*(\S+)?\s*(.*)$/;
const CONTAINER_CLOSE = /^:{3,}\s*$/;
const THEMATIC_BREAK = /^(\*{3,}|-{3,}|_{3,})\s*$/;
const HEADING = /^#{1,6}\s/;
const TABLE_ROW = /^\|.*\|\s*$/;
const TABLE_SEPARATOR = /^\|[\s:|-]+\|\s*$/;
const HTML_BLOCK_TAGS =
  /^<\/?(div|p|table|thead|tbody|tr|th|td|ul|ol|li|blockquote|pre|hr|br|details|summary)\b/i;
const BLANK_LINE = /^\s*$/;

/**
 * Parse a markdown document into an ordered list of segments.
 */
export function parseMarkdown(
  content: string,
  options: ParserOptions = {},
): Segment[] {
  const blockComponents = options.blockComponents || DEFAULT_BLOCK_COMPONENTS;
  const translatableFmFields = options.translatableFrontmatterFields || [];
  const lines = content.split('\n');
  const segments: Segment[] = [];

  let state: 'normal' | 'frontmatter' | 'code_block' | 'component' = 'normal';
  let buffer: string[] = [];
  let codeBlockFence = ''; // the fence string used to open the code block
  let componentTag = ''; // the component tag name we're inside
  let lineIndex = 0;
  let seenContent = false; // whether we've seen any non-blank content

  function flushBuffer(type: SegmentType, translatable: boolean): void {
    if (buffer.length === 0) return;
    const content = buffer.join('\n');
    segments.push({ type, content, translatable });
    buffer = [];
  }

  /**
   * Flush the frontmatter buffer, extracting translatable field values
   * (e.g. title, description) as separate translatable segments.
   * Non-translatable lines (delimiters, other fields) become individual
   * non-translatable segments so that reassembly via join('\n') works.
   */
  function flushFrontmatterBuffer(): void {
    if (buffer.length === 0) return;

    if (translatableFmFields.length === 0) {
      // No fields configured — whole block is non-translatable
      const content = buffer.join('\n');
      segments.push({ type: 'frontmatter', content, translatable: false });
      buffer = [];
      return;
    }

    // YAML simple key: value pattern (single-line values only)
    const yamlKV = /^(\s*([\w][\w-]*)\s*:\s*)(.+)$/;

    for (const line of buffer) {
      const match = line.match(yamlKV);
      if (match && translatableFmFields.includes(match[2])) {
        // Extract the value, stripping surrounding quotes if present
        let value = match[3].trim();
        if (
          (value.startsWith('"') && value.endsWith('"')) ||
          (value.startsWith("'") && value.endsWith("'"))
        ) {
          value = value.slice(1, -1);
        }
        segments.push({
          type: 'frontmatter',
          content: value,
          rawContent: line,
          translatable: true,
        });
      } else {
        segments.push({
          type: 'frontmatter',
          content: line,
          translatable: false,
        });
      }
    }

    buffer = [];
  }

  function flushTextBuffer(): void {
    if (buffer.length === 0) return;

    // Classify the text buffer content
    const text = buffer.join('\n');
    const trimmed = text.trim();

    if (!trimmed) {
      segments.push({ type: 'blank', content: text, translatable: false });
      buffer = [];
      return;
    }

    // Check if it's a heading, table, list, etc.
    const type = classifyTextBlock(trimmed);
    segments.push({ type, content: text, translatable: true });
    buffer = [];
  }

  /**
   * Flush the component buffer:
   * 1. Opening tag line → non-translatable segment, but translatable attrs
   *    (e.g. title="...") are extracted as a separate translatable segment.
   * 2. Inner content lines → recursively parsed as regular markdown so all
   *    translatable text inside the block is also translated.
   * 3. Closing tag line → non-translatable segment.
   */
  function flushComponentBuffer(): void {
    if (buffer.length === 0) return;

    const openingLine = buffer[0];
    const closingLine = buffer[buffer.length - 1];
    const innerLines = buffer.slice(1, buffer.length > 1 ? -1 : 1);

    // --- Opening tag ---
    // Try to extract a translatable attribute (e.g. title)
    let titleExtracted = false;
    for (const attr of TRANSLATABLE_COMPONENT_ATTRS) {
      const attrPattern = new RegExp(`\\b${escapeRegex(attr)}="([^"]+)"`, 'i');
      const match = openingLine.match(attrPattern);
      if (match) {
        segments.push({
          type: 'component_block',
          content: match[1],
          rawContent: openingLine,
          translatable: true,
        });
        titleExtracted = true;
        break;
      }
    }
    if (!titleExtracted) {
      segments.push({ type: 'component_block', content: openingLine, translatable: false });
    }

    // --- Inner content (only when there's more than just open+close) ---
    if (innerLines.length > 0) {
      const innerContent = innerLines.join('\n');
      // Recursively parse so headings, paragraphs, lists, etc. inside the
      // component are all treated as normal translatable markdown.
      const innerSegments = parseMarkdown(innerContent, { blockComponents });
      segments.push(...innerSegments);
    }

    // --- Closing tag (only present when buffer had more than one line) ---
    if (buffer.length > 1) {
      segments.push({ type: 'component_block', content: closingLine, translatable: false });
    }

    buffer = [];
  }

  while (lineIndex < lines.length) {
    const line = lines[lineIndex];

    switch (state) {
      // ---- NORMAL STATE ----
      case 'normal': {
        // Check for frontmatter at the very beginning
        if (!seenContent && FRONTMATTER_DELIMITER.test(line)) {
          flushTextBuffer();
          buffer.push(line);
          state = 'frontmatter';
          lineIndex++;
          continue;
        }

        if (line.trim()) seenContent = true;

        // Check for fenced code block opening
        const codeMatch = line.match(FENCED_CODE_OPEN);
        if (codeMatch) {
          flushTextBuffer();
          buffer.push(line);
          codeBlockFence = codeMatch[1]; // save the fence chars
          state = 'code_block';
          lineIndex++;
          continue;
        }

        // Check for VuePress container directive
        const containerMatch = line.match(CONTAINER_OPEN);
        if (containerMatch && !CONTAINER_CLOSE.test(line.trim())) {
          // Opening a container — only if it has a directive type or content
          if (containerMatch[2]) {
            flushTextBuffer();
            segments.push({
              type: 'container_open',
              content: line,
              translatable: hasTranslatableContainerTitle(containerMatch),
            });
            lineIndex++;
            continue;
          }
        }

        // Check for container close (standalone :::)
        if (CONTAINER_CLOSE.test(line.trim())) {
          flushTextBuffer();
          segments.push({
            type: 'container_close',
            content: line,
            translatable: false,
          });
          lineIndex++;
          continue;
        }

        // Check for block component opening
        const componentMatch = matchBlockComponentOpen(line, blockComponents);
        if (componentMatch) {
          flushTextBuffer();
          buffer.push(line);
          componentTag = componentMatch;
          state = 'component';
          lineIndex++;
          continue;
        }

        // Check for blank line — use as paragraph separator
        if (BLANK_LINE.test(line)) {
          flushTextBuffer();
          // Collect consecutive blank lines
          const blankLines: string[] = [];
          while (lineIndex < lines.length && BLANK_LINE.test(lines[lineIndex])) {
            blankLines.push(lines[lineIndex]);
            lineIndex++;
          }
          segments.push({
            type: 'blank',
            content: blankLines.join('\n'),
            translatable: false,
          });
          continue;
        }

        // Regular content line — add to buffer
        buffer.push(line);
        lineIndex++;
        break;
      }

      // ---- FRONTMATTER STATE ----
      case 'frontmatter': {
        buffer.push(line);
        if (FRONTMATTER_DELIMITER.test(line) && buffer.length > 1) {
          flushFrontmatterBuffer();
          state = 'normal';
          seenContent = true;
        }
        lineIndex++;
        break;
      }

      // ---- CODE BLOCK STATE ----
      case 'code_block': {
        buffer.push(line);
        // Check if this line closes the code block
        const trimmedLine = line.trim();
        if (
          trimmedLine.startsWith(codeBlockFence.charAt(0)) &&
          trimmedLine.length >= codeBlockFence.length &&
          /^(`{3,}|~{3,})\s*$/.test(trimmedLine)
        ) {
          // Only close if the fence char matches and length >= opening fence
          const closeFenceChar = trimmedLine.charAt(0);
          if (closeFenceChar === codeBlockFence.charAt(0)) {
            flushBuffer('code_block', true);
            state = 'normal';
          }
        }
        lineIndex++;
        break;
      }

      // ---- COMPONENT BLOCK STATE ----
      case 'component': {
        buffer.push(line);
        // Check for the closing tag
        const closePattern = new RegExp(
          `^\\s*</${escapeRegex(componentTag)}\\s*>`,
          'i',
        );
        if (closePattern.test(line)) {
          flushComponentBuffer();
          state = 'normal';
        }
        lineIndex++;
        break;
      }
    }
  }

  // Flush any remaining buffer
  if (state === 'normal') {
    flushTextBuffer();
  } else if (state === 'component') {
    // Unclosed component block — still try to extract translatable attrs
    flushComponentBuffer();
  } else {
    // Unclosed frontmatter or code block
    if (state === 'frontmatter') {
      flushFrontmatterBuffer();
    } else {
      flushBuffer('code_block', true);
    }
  }

  // Compute hashes for all segments
  for (const segment of segments) {
    segment.hash = computeHash(segment.content);
  }

  return segments;
}

/**
 * Classify a text block into a more specific segment type.
 */
function classifyTextBlock(text: string): SegmentType {
  const firstLine = text.split('\n')[0].trim();

  if (HEADING.test(firstLine)) return 'heading';
  if (TABLE_ROW.test(firstLine)) return 'table';
  if (THEMATIC_BREAK.test(firstLine)) return 'thematic_break';
  if (HTML_BLOCK_TAGS.test(firstLine)) return 'html_block';

  // Check if it's a list (starts with -, *, +, or numbered)
  if (/^[-*+]\s/.test(firstLine) || /^\d+[.)]\s/.test(firstLine)) {
    return 'list';
  }

  return 'paragraph';
}

/**
 * Check if a container directive has a translatable title.
 * e.g., "::: tip My Title" → title "My Title" is translatable
 * but "::: tip" alone is not (just a keyword)
 */
function hasTranslatableContainerTitle(
  match: RegExpMatchArray,
): boolean {
  // match[3] is the title text after the directive keyword
  return !!(match[3] && match[3].trim());
}

/**
 * Check if a line opens a block-level component.
 * Returns the tag name if matched, null otherwise.
 */
function matchBlockComponentOpen(
  line: string,
  blockComponents: string[],
): string | null {
  const trimmed = line.trim();
  for (const tag of blockComponents) {
    // Match <Tag or <Tag ... (not self-closing)
    const pattern = new RegExp(`^<${escapeRegex(tag)}(\\s|>)`, 'i');
    if (pattern.test(trimmed)) {
      // Check it's not self-closing
      if (!trimmed.endsWith('/>')) {
        return tag;
      }
    }
  }
  return null;
}

/**
 * Compute SHA-256 hash of content.
 */
export function computeHash(content: string): string {
  return createHash('sha256').update(content, 'utf-8').digest('hex').slice(0, 16);
}

/**
 * Escape special regex characters in a string.
 */
function escapeRegex(str: string): string {
  return str.replace(/[.*+?^${}()|[\]\\]/g, '\\$&');
}
