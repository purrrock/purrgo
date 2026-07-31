export const html_ux = {
  "name": "html-ux-injection",
  "scopeName": "text.html.ux",
  "injectTo": ["text.html", "text.html.basic"],
  "injectionSelector": "L:text.html - (meta.embedded, comment)",
  "patterns": [
    { "include": "#attr-binding" },
    { "include": "#attr-event" },
    { "include": "#attr-for" },
    { "include": "#attr-keywords" },
    { "include": "#interpolation" },
  ],
  "repository": {
    "attr-binding": {
      "name": "meta.attribute.binding.ux.html",
      "begin": "(?<=\\s)(::?)([A-Za-z_][\\w:-]*)\\s*=\\s*(\")",
      "beginCaptures": {
        "1": { "name": "punctuation.definition.binding.prefix.ux.html" },
        "2": { "name": "entity.other.attribute-name.binding.ux.html" },
        "3": { "name": "punctuation.definition.string.begin.html" }
      },
      "end": "\"",
      "endCaptures": {
        "0": { "name": "punctuation.definition.string.end.html" }
      },
      "contentName": "source.js.embedded.html",
      "patterns": [{ "include": "source.js" }]
    },
    "attr-event": {
      "name": "meta.attribute.event.ux.html",
      "begin": "(?<=\\s)(on|model)(:)([A-Za-z_][\\w:-]*)\\s*=\\s*(\")",
      "beginCaptures": {
        "1": { "name": "keyword.control.command.ux.html" },
        "2": { "name": "punctuation.definition.binding.prefix.ux.html" },
        "3": { "name": "entity.other.attribute-name.event.ux.html" },
        "4": { "name": "punctuation.definition.string.begin.html" }
      },
      "end": "\"",
      "endCaptures": {
        "0": { "name": "punctuation.definition.string.end.html" }
      },
      "contentName": "source.js.embedded.html",
      "patterns": [{ "include": "source.js" }]
    },
    "attr-for": {
      "name": "meta.attribute.for.ux.html",
      "begin": "(?<=\\s)(for|if|elif)\\s*=\\s*(\")",
      "beginCaptures": {
        "1": { "name": "keyword.control.command.ux.html" },
        "2": { "name": "punctuation.definition.string.begin.html" }
      },
      "end": "\"",
      "endCaptures": {
        "0": { "name": "punctuation.definition.string.end.html" }
      },
      "contentName": "source.js.embedded.html",
      "patterns": [{ "include": "source.js" }]
    },
    "attr-keywords": {
      "name": "meta.attribute.else.ux.html",
      "match": "(?<=\\s)\\b(else)\\b",
      "captures": { "1": { "name": "keyword.control.block.ux.html" }, }
    },
    "interpolation": {
      "name": "meta.interpolation.ux.html",
      "begin": "\\{\\{",
      "beginCaptures": { "0": { "name": "punctuation.section.embedded.begin.ux.html" } },
      "end": "\\}\\}",
      "endCaptures": { "0": { "name": "punctuation.section.embedded.end.ux.html" } },
      "applyEndPatternLast": true,
      "contentName": "source.js.embedded.html",
      "patterns": [
        { "include": "source.js" }
      ]
    }
  },
  "embeddedLanguages": {
    "source.js.embedded.html": "javascript",
  },
}
