// ============================================================
// Reassembler — reconstruct markdown from translated segments
// ============================================================

import type { Segment } from '../types.js';

/**
 * Reassemble a list of segments back into a markdown string.
 *
 * For each segment:
 * - If it has a `translation`, use that.
 * - Otherwise, use the original `content`.
 */
export function reassemble(segments: Segment[]): string {
  const parts: string[] = [];

  for (const segment of segments) {
    if (segment.translatable && segment.translation) {
      if (segment.rawContent) {
        if (segment.type === 'component_block') {
          // Component block with a translated attribute: patch attr="value"
          const escaped = segment.content.replace(/[.*+?^${}()|[\]\\]/g, '\\$&');
          const patched = segment.rawContent.replace(
            new RegExp(`(\\b\\w+=)"${escaped}"`, 'i'),
            `$1"${segment.translation}"`,
          );
          parts.push(patched);
        } else {
          // Frontmatter or other: replace the value portion in the raw line
          const escaped = segment.content.replace(/[.*+?^${}()|[\]\\]/g, '\\$&');
          parts.push(segment.rawContent.replace(new RegExp(escaped), segment.translation));
        }
      } else {
        parts.push(segment.translation);
      }
    } else {
      parts.push(segment.content);
    }
  }

  let result = parts.join('\n');

  // Ensure file ends with a single newline
  if (!result.endsWith('\n')) {
    result += '\n';
  }

  return result;
}
