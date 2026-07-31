export const ebnf = {
  "displayName": "EBNF",
  "fileTypes": ["ebnf"],
  "name": "ebnf",
  "patterns": [
    { "include": "#comment" },
    { "include": "#rule" },
    { "include": "#indented-symbols" }
  ],
  "repository": {
    "comment": {
      "patterns": [
        {
          "name": "comment.block.ebnf",
          "begin": "\\(\\*",
          "end": "\\*\\)",
          "patterns": [
            {
              "include": "#comment-tags"
            }
          ]
        },
        {
          "name": "comment.line.ebnf",
          "begin": "#",
          "end": "$",
          "patterns": [
            {
              "include": "#comment-tags"
            }
          ]
        }
      ]
    },
    "comment-tags": {
      "patterns": [
        {
          "name": "keyword.codetag.ebnf",
          "match": "\\b(?:TODO|FIXME|BUG|NOTE|HACK)\\b"
        }
      ]
    },
    "rule": {
      "patterns": [
        {
          "match": "([\\w\\-<>]+)\\s*(:=|=)\\s*(.+)?",
          "captures": {
            "1": {
              "name": "entity.name.class.ebnf"
            },
            "2": {
              "name": "keyword.symbol.ebnf"
            },
            "3": {
              "patterns": [
                {
                  "include": "#symbols"
                }
              ]
            }
          }
        }
      ]
    },
    "symbols": {
      "patterns": [
        { "include": "#comment" },
        { "include": "#string" },
        { "include": "#symbol" },
        { "include": "#operators" }
      ]
    },
    "indented-symbols": {
      "patterns": [
        { "include": "#symbols" }
      ]
    },
    "symbol": {
      "patterns": [
        {
          "match": "\\b<[A-Z][_A-Z0-9]*>\\b",
          "name": "support.variable.ebnf"
        },
        {
          "match": "\\b[_a-zA-Z][_a-zA-Z0-9\\-]*\\b",
          "name": "entity.name.class.ebnf"
        }
      ]
    },
    "string": {
      "patterns": [
        {
          "name": "string.quoted.single.ebnf",
          "begin": "'",
          "end": "'",
          "patterns": [
            {
              "name": "constant.character.escape.ebnf",
              "match": "\\\\."
            }
          ]
        },
        {
          "name": "string.quoted.double.ebnf",
          "begin": "\"",
          "end": "\"",
          "patterns": [
            {
              "name": "constant.character.escape.ebnf",
              "match": "\\\\."
            }
          ]
        }
      ]
    },
    "operators": {
      "patterns": [
        {
          "match": "[?+*|()\\[\\]{},.\\-!]",
          "name": "keyword.control.ebnf"
        },
        {
          "match": ";",
          "name": "keyword.symbol.ebnf"
        }
      ]
    }
  },
  "scopeName": "source.ebnf"
}
