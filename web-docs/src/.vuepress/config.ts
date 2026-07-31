import { defineUserConfig } from "vuepress"
import { viteBundler } from '@vuepress/bundler-vite'
import theme from "./theme.js"
import { registerComponentsPlugin } from '@vuepress/plugin-register-components'
import { glyphixDemoPlugin } from './plugins/glyphix-demo'
import { getDirname, path } from '@vuepress/utils'
import fs from 'node:fs'

const __dirname = getDirname(import.meta.url)

const BASE_URL = "/glyphix/docs/";

// If the local emulator files are absent, transparently proxy requests for
// them to the hosted build so `docs:dev` works without a local copy.
const DEV_FALLBACK_ORIGIN = 'https://docs.xfaith-tech.com' + BASE_URL;
const emulatorFilesExist =
  fs.existsSync(path.resolve(__dirname, './components/emulator/glyphix-wasm.wasm')) &&
  fs.existsSync(path.resolve(__dirname, './components/emulator/glyphix-wasm.js'));
if (!emulatorFilesExist)
  console.log('[glyphix] Emulator files not found, proxying to', DEV_FALLBACK_ORIGIN);

export default defineUserConfig({
  base: BASE_URL,

  title: "Glyphix",
  head: [
    ["link", { rel: "icon", href: "/glyphix/docs/favicon.ico" }],
  ],

  locales: {
    "/": {
      lang: "zh-CN",
      title: "Glyphix",
      description: "The documents for Glyphix project.",
    },
    "/en/": {
      lang: "en-US",
      title: "Glyphix",
      description: "The documents for Glyphix project.",
    },
  },

  bundler: viteBundler({
    viteOptions: {
      server: {
        host: false, // 默认只监听 localhost，设置为 true 以监听所有地址
        ...(!emulatorFilesExist && {
          proxy: {
            '^/.*?/components/emulator/.*': {
              target: DEV_FALLBACK_ORIGIN,
              changeOrigin: true,
              rewrite: (p) => p.replace(/^\/.*?\/components\/emulator/, '/assets'),
            },
          },
        }),
      },
      build: {
        rollupOptions: {
          output: {
            // Keep glyphix-wasm.* filenames stable (no content-hash suffix)
            // so they can be referenced reliably after `docs:build`.
            assetFileNames: (assetInfo) => {
              const names: readonly string[] = (assetInfo as any).names ??
                (assetInfo.name ? [assetInfo.name] : []);
              if (names.some((n) => /^glyphix-wasm\b/.test(n))) {
                return 'assets/[name][extname]';
              }
              return 'assets/[name]-[hash][extname]';
            },
            // Same stable-name rule for the JS chunk emitted via new URL().
            chunkFileNames: (chunkInfo) => {
              if (/^glyphix-wasm\b/.test(chunkInfo.name)) {
                return 'assets/[name].js';
              }
              return 'assets/[name]-[hash].js';
            },
          },
        },
      },
    },
    vuePluginOptions: {},
  }),

  plugins: [
    registerComponentsPlugin({
      componentsDir: path.resolve(__dirname, './components'),
    }),
    glyphixDemoPlugin,
  ],

  theme,

  // Enable it with pwa
  // shouldPrefetch: false,
});
