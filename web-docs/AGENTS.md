# AGENTS.md — web-docs

This is the documentation repository for Glyphix. Content is authored in the `src/` directory using VuePress and deployed as a static site.

> Scope: `web-docs/` only. `spec-docs/` is manually maintained — do not touch it.

## Stack

- VuePress 2 (`2.0.0-rc.19`) + `vuepress-theme-hope`
- Package manager: **pnpm** (do not use npm/yarn for local dev)
- Node ESM (`"type": "module"` in package.json)

## Commands

```bash
pnpm install          # install deps
pnpm docs:dev         # local dev server (hot-reload)
pnpm docs:build       # production build → src/.vuepress/dist/
```

There are **no test or lint scripts** defined.

## Directory layout

```
src/                  # Documentation content (Chinese = primary)
  .vuepress/
    config.ts         # VuePress config, Vite bundler settings
    theme.ts          # Theme + markdown plugin config
    components/       # Auto-registered Vue components
    plugins/          # Custom VuePress plugins (glyphix-demo, shiki langs)
    sidebar/          # Sidebar definitions
    navbar/           # Navbar definitions
  en/                 # English translations (auto-generated, DO NOT edit)
  api/  components/  cookbook/  cxxdev/  framework/  tutorials/
i18n/                 # Translation tooling (for human use)
```

## Key rules

- **Only edit Chinese (primary) content.** Files under `src/en/` are machine-translated by `pnpm translate` — never edit them directly.
- **Base URL** is `/glyphix/docs/` — all asset/link references must account for this.
- **Emulator WASM files are gitignored.** If absent, dev server auto-proxies to the hosted build — no manual download needed.
- **Custom shiki plugin override** via `pnpm.overrides` pointing to a GitHub fork. Do not upgrade `@vuepress/plugin-shiki` through normal means.
- **Custom Shiki languages** `ebnf` and `html_ux` are defined in `src/.vuepress/plugins/`.

## Content authoring conventions

- Write all documentation in **Chinese**. English is generated automatically.
- VuePress components (e.g. `<glyphix>`, `:::` containers) must be preserved verbatim.
- See `.github/instructions/cxxdev.instructions.md` for C++ doc writing style guidance.

## CI (GitLab)

Build pipeline: `wasm-emulator` → `build-docs` → `publish-pages`. The build job copies WASM artifacts and C++ API docs into `src/.vuepress/public/` before running `npm run docs:build`. GitHub sync happens on tags only.
