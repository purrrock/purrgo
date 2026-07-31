import { withBase } from '@vuepress/client'

async function fetchFile(path: string): Promise<Uint8Array> {
  let res = await fetch(path, { method: 'GET', credentials: 'same-origin' })
  return new Uint8Array(await res.arrayBuffer())
}

/**
 * Returns a lazy, cached fetcher for a binary resource.
 */
function fetchCachedFile(url: string): () => Promise<Uint8Array> {
  let promise: Promise<Uint8Array> | null = null;
  return () => {
    if (!promise) {
      promise = fetch(url, { method: 'GET', credentials: 'same-origin' })
        .then((res) => {
          if (!res.ok) throw new Error(`Failed to fetch file: ${res.status} ${res.statusText}`);
          return res.arrayBuffer();
        })
        .then((buf) => new Uint8Array(buf));
    }
    return promise;
  }
}

// new URL(..., import.meta.url) tells Vite/Rollup to emit both files with
// stable names (configured via assetFileNames / chunkFileNames in config.ts)
// and rewrites the URLs in the built output automatically.
const wasmBinaryUrl = new URL('./emulator/glyphix-wasm.wasm', import.meta.url);
const wasmScriptUrl = new URL('./emulator/glyphix-wasm.js', import.meta.url);

// Lazily load glyphix-wasm.js as an external ES module so Rollup does not
// bundle its contents.  /* @vite-ignore */ suppresses Vite's static-analysis
// of the dynamic import target; the actual URL is resolved at runtime.
let _createModule: ((options: any) => Promise<any>) | null = null;
async function loadCreateModule() {
  if (!_createModule) {
    const mod = await import(/* @vite-ignore */ wasmScriptUrl.href);
    _createModule = mod.default ?? mod;
  }
  return _createModule!;
}

// When running in dev mode and the local files are absent, fall back to the
// hosted build so the emulator still works without a local copy of the binaries.
const fetchWasmBinary = fetchCachedFile(wasmBinaryUrl.href);
const fetchGlobalPackage = fetchCachedFile(withBase('res/global.pkg'));

function devicePixelRatio() {
  return window.devicePixelRatio || 1
}

function setupCanvas(canvas: HTMLElement) {
  const dpr = Math.max(devicePixelRatio(), 2)
  let rect = canvas.getBoundingClientRect();
  canvas.style.width = Math.round(rect.width / dpr) + 'px';
  canvas.style.height = Math.round(rect.height / dpr) + 'px';
  return canvas
}

function decodeString(str: string): Uint8Array {
  return new TextEncoder().encode(str);
}

export async function createEmulator(canvas: HTMLElement, props: { id: string, mouseWheel?: boolean }) {
  // Ensure canvas is properly sized before loading the emulator to avoid incorrect display if fetching the WASM binary takes a while.
  canvas = setupCanvas(canvas);
  const createModule = await loadCreateModule();
  await createModule({
    wasmBinary: await fetchWasmBinary(), // Used to load the WebAssembly binary
    // The emulator must be mounted on a canvas DOM element.
    canvas: canvas,
    pixelRatio: devicePixelRatio(),
    // The ID of the running applet.
    targetApplet: 'com.example.app',
    // The system default font size in px.
    fontSize: 28,
    textColor: '#3c3c44',
    mouseWheel: props.mouseWheel ?? false,
    // The file system, the key is the file path and the value is the file content (type is Uint8Array).
    files: {
      '/global.pkg': await fetchGlobalPackage(),
      '/pkgs.db': decodeString('{"apps":{"com.example.app":{"uri":"pkg://com.example.app"}},"dials":{}}'),
      '/apps/com.example.app.pkg': await fetchFile(withBase(`res/demo/${props.id}.pkg`)),
    }
  })
}
