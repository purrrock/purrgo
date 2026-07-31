<script setup lang="ts">
import { ref, computed, onMounted, onUpdated, onBeforeUnmount } from 'vue'

// ── props ─────────────────────────────────────────────────────────────────── //

const props = withDefaults(defineProps<{
  /** Outer background frame (default true) */
  frame?: boolean
  /** Max-width of the layers column */
  maxWidth?: string
}>(), { frame: true, maxWidth: '460px' })

const frameClass = computed(() => props.frame ? 'framed' : '')

// ── palette ───────────────────────────────────────────────────────────────── //

interface Tint { bg: string; text: string; sub: string; shadow: string }

const LIGHT: Tint[] = [
  { bg: '#d6ebff', text: '#002d6b', sub: '#0d62e0', shadow: 'rgba(22,119,255,0.07)' },
  { bg: '#e2e9ff', text: '#162a8e', sub: '#4664e0', shadow: 'rgba(89,126,247,0.06)' },
  { bg: '#e3fdd4', text: '#0e4200', sub: '#3aab08', shadow: 'rgba(82,196,26,0.06)' },
  { bg: '#ffeccc', text: '#6d2c00', sub: '#e07a0a', shadow: 'rgba(250,140,22,0.06)' },
  { bg: '#ffdfed', text: '#800c54', sub: '#d62280', shadow: 'rgba(235,47,150,0.06)' },
  { bg: '#eaeaea', text: '#1a1a1a', sub: '#454545', shadow: 'rgba(0,0,0,0.05)' },
]
const DARK: Tint[] = [
  { bg: '#1c3a5e', text: '#60b8ff', sub: '#3da0ff', shadow: 'rgba(96,184,255,0.07)' },
  { bg: '#1e2260', text: '#9ab4ff', sub: '#7898ff', shadow: 'rgba(154,180,255,0.06)' },
  { bg: '#174030', text: '#7edc52', sub: '#62cc30', shadow: 'rgba(126,220,82,0.06)' },
  { bg: '#3e2a0e', text: '#ffb830', sub: '#ff9e00', shadow: 'rgba(255,184,48,0.06)' },
  { bg: '#3b1230', text: '#ff70b8', sub: '#f040a0', shadow: 'rgba(255,112,184,0.06)' },
  { bg: '#2c2c2c', text: '#ebebeb', sub: '#a0a0a0', shadow: 'rgba(255,255,255,0.05)' },
]

function dark() {
  return typeof document !== 'undefined' &&
    document.documentElement.getAttribute('data-theme') === 'dark'
}

// ── styling ───────────────────────────────────────────────────────────────── //

const layersEl = ref<HTMLElement>()
const SKIP_CLASSES = ['remark', 'label', 'accent', 'muted', 'sep']

function applyStyles() {
  const el = layersEl.value
  if (!el) return
  const isDark = dark()
  const pal = isDark ? DARK : LIGHT

  // 1. Palette vars on direct children
  ;(Array.from(el.children) as HTMLElement[]).forEach((ch, i) => {
    const t = pal[i % pal.length]
    ch.style.setProperty('--l-bg', t.bg)
    ch.style.setProperty('--l-text', t.text)
    ch.style.setProperty('--l-sub', t.sub)
    ch.style.setProperty('--l-shadow', t.shadow)
  })

  // 2. Nested-card bg var
  el.style.setProperty('--nested-bg',
    isDark ? 'rgba(255,255,255,0.11)' : 'rgba(255,255,255,0.62)')

  // 3. Mark card / nested via data-arch attribute
  walkMark(el, 0)
}

function walkMark(parent: HTMLElement, depth: number) {
  for (const ch of Array.from(parent.children) as HTMLElement[]) {
    if (SKIP_CLASSES.some(c => ch.classList.contains(c))) continue
    if (ch.classList.contains('group')) {
      // group is transparent — pass through depth
      walkMark(ch, depth)
    } else {
      ch.dataset.arch = depth === 0 ? 'card' : 'nested'
      walkMark(ch, depth + 1)
    }
  }
}

let obs: MutationObserver | null = null
onMounted(() => {
  applyStyles()
  updateScale()

  obs = new MutationObserver(applyStyles)
  obs.observe(document.documentElement, { attributes: true, attributeFilter: ['data-theme'] })

  scaleObs = new ResizeObserver(updateScale)
  if (scaleWrapperEl.value) scaleObs.observe(scaleWrapperEl.value)
})
onUpdated(() => { applyStyles(); updateScale() })
onBeforeUnmount(() => { obs?.disconnect(); scaleObs?.disconnect() })

// ── scale-to-fit ──────────────────────────────────────────────────────────── //

const scaleWrapperEl = ref<HTMLElement>()
const scaleVal = ref(1)
const layersNaturalHeight = ref(0)

/** Parse the numeric px value from the maxWidth prop (e.g. '460px' → 460). */
const naturalWidth = computed(() => parseFloat(props.maxWidth) || 460)

function updateScale() {
  const wrapper = scaleWrapperEl.value
  const layers  = layersEl.value
  if (!wrapper || !layers) return

  // `offsetHeight` is always the *layout* height — unaffected by CSS transform.
  // So we can read the true natural height even while a scale is applied.
  layersNaturalHeight.value = layers.offsetHeight

  const avail = wrapper.clientWidth
  scaleVal.value = avail < naturalWidth.value ? avail / naturalWidth.value : 1
}

let scaleObs: ResizeObserver | null = null

// ── copy as image ─────────────────────────────────────────────────────────── //

const copyState = ref<'idle' | 'ok' | 'err'>('idle')

async function copyAsImage() {
  const el = layersEl.value
  if (!el) return
  try {
    const svg = buildSvgFromDom(el)
    const blob = await renderPng(svg)
    await navigator.clipboard.write([new ClipboardItem({ 'image/png': blob })])
    copyState.value = 'ok'
  } catch (e) {
    console.warn('ArchDiagram copy:', e)
    copyState.value = 'err'
  } finally {
    setTimeout(() => (copyState.value = 'idle'), 2200)
  }
}

// ── DOM → native SVG (rect + text, no foreignObject) ──────────────────── //

const FNT = "-apple-system,BlinkMacSystemFont,'Segoe UI',Roboto,sans-serif"

function esc(s: string) {
  return s.replace(/&/g, '&amp;').replace(/</g, '&lt;')
          .replace(/>/g, '&gt;').replace(/"/g, '&quot;')
}

function buildSvgFromDom(container: HTMLElement): string {
  const cRect = container.getBoundingClientRect()
  const PAD = 20
  const W = Math.ceil(cRect.width) + PAD * 2
  const H = Math.ceil(cRect.height) + PAD * 2
  const bg = dark() ? '#1a1a1a' : '#fafafa'

  const parts: string[] = [`<rect width="${W}" height="${H}" rx="8" fill="${bg}"/>`]
  walkPaint(container, cRect, PAD, PAD, parts)
  return `<svg xmlns="http://www.w3.org/2000/svg" width="${W}" height="${H}">${parts.join('')}</svg>`
}

function walkPaint(el: Element, cRect: DOMRect, ox: number, oy: number, out: string[]) {
  const he = el as HTMLElement
  const rect = he.getBoundingClientRect()
  if (!rect.width || !rect.height) return

  const x = rect.left - cRect.left + ox
  const y = rect.top - cRect.top + oy
  const w = rect.width, h = rect.height
  const cs = getComputedStyle(he)

  // ── background ──
  let bg = cs.backgroundColor
  const isArchCard = he.hasAttribute('data-arch')
  if (isArchCard && (!bg || bg === 'rgba(0, 0, 0, 0)' || bg === 'transparent'))
    bg = cs.getPropertyValue('--l-bg').trim()

  if (bg && bg !== 'rgba(0, 0, 0, 0)' && bg !== 'transparent' && bg !== '') {
    const r = parseFloat(cs.borderRadius) || 0
    // soft shadow
    const sh = cs.boxShadow
    if (sh && sh !== 'none') {
      const mc = sh.match(/rgba?\([^)]+\)/)
      if (mc) out.push(`<rect x="${x}" y="${y + 2}" width="${w}" height="${h}" rx="${r}" fill="${mc[0]}"/>`)
    }
    out.push(`<rect x="${x}" y="${y}" width="${w}" height="${h}" rx="${r}" fill="${bg}"/>`)
  }

  // ── text nodes ──
  for (const node of el.childNodes) {
    if (node.nodeType !== Node.TEXT_NODE) continue
    const txt = node.textContent?.trim()
    if (!txt) continue
    const range = document.createRange()
    range.selectNodeContents(node)
    const rects = range.getClientRects()
    if (!rects.length) continue
    const tr = rects[0]
    const tx = tr.left - cRect.left + ox + tr.width / 2
    const ty = tr.top - cRect.top + oy + tr.height / 2
    out.push(
      `<text x="${tx}" y="${ty}" text-anchor="middle" dominant-baseline="central"` +
      ` font-family="${FNT}" font-size="${cs.fontSize}" font-weight="${cs.fontWeight}"` +
      ` fill="${cs.color}">${esc(txt)}</text>`)
  }

  // ── children ──
  for (const child of el.children) walkPaint(child, cRect, ox, oy, out)
}

function renderPng(svg: string): Promise<Blob> {
  const m = svg.match(/width="(\d+)".*?height="(\d+)"/)
  const w = m ? +m[1] : 500, h = m ? +m[2] : 300, S = 2
  const cvs = document.createElement('canvas')
  cvs.width = w * S; cvs.height = h * S
  const ctx = cvs.getContext('2d')!
  ctx.scale(S, S)
  const url = URL.createObjectURL(new Blob([svg], { type: 'image/svg+xml;charset=utf-8' }))
  return new Promise((resolve, reject) => {
    const img = new Image()
    img.onload = () => {
      ctx.drawImage(img, 0, 0, w, h)
      URL.revokeObjectURL(url)
      cvs.toBlob(b => b ? resolve(b) : reject(new Error('toBlob')), 'image/png')
    }
    img.onerror = () => { URL.revokeObjectURL(url); reject(new Error('svg render')) }
    img.src = url
  })
}
</script>

<template>
  <div class="arch-diagram" :class="frameClass">
    <div
      ref="scaleWrapperEl"
      class="arch-diagram__scale-wrapper"
      :style="scaleVal < 1
        ? { height: layersNaturalHeight * scaleVal + 'px' }
        : {}"
    >
      <div
        ref="layersEl"
        class="arch-diagram__layers"
        :style="scaleVal < 1
          ? { maxWidth, width: maxWidth, transform: `scale(${scaleVal})`, transformOrigin: 'top left' }
          : { maxWidth }"
      >
        <slot />
      </div>
    </div>
    <button
      class="arch-diagram__copy"
      :class="copyState"
      :data-tooltip="copyState === 'ok' ? 'Copied!' : 'Copy as image'"
      @click="copyAsImage"
    >
      <svg v-if="copyState !== 'ok'" xmlns="http://www.w3.org/2000/svg" width="15" height="15"
        viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"
        stroke-linecap="round" stroke-linejoin="round">
        <rect x="9" y="2" width="13" height="13" rx="2" ry="2"/>
        <path d="M5 15H4a2 2 0 0 1-2-2V4a2 2 0 0 1 2-2h9a2 2 0 0 1 2 2v1"/>
      </svg>
      <svg v-else xmlns="http://www.w3.org/2000/svg" width="15" height="15"
        viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2.5"
        stroke-linecap="round" stroke-linejoin="round">
        <polyline points="20 6 9 17 4 12"/>
      </svg>
    </button>
  </div>
</template>

<style scoped>
/* ================================================================
   CONTAINER
   ================================================================ */
.arch-diagram {
  position: relative;
  display: flex;
  align-items: flex-start;
  width: 100%;
  margin: 1.4rem 0;
}
.arch-diagram.framed {
  background: var(--vp-c-bg-soft);
  border-radius: 12px;
  padding: 10px 20px;
  transition: background 0.25s;
}

/* ================================================================
   SCALE WRAPPER — collapses to the scaled visual height
   ================================================================ */
.arch-diagram__scale-wrapper {
  width: 100%;
  overflow: hidden;   /* clip any sub-pixel bleed */
}

/* ================================================================
   LAYERS COLUMN
   ================================================================ */
.arch-diagram__layers {
  display: flex;
  flex-direction: column;
  gap: 8px;
  margin: 0 auto;
  width: 100%;
}

/* ================================================================
   CARDS  —  driven by data-arch attribute
   ================================================================ */
.arch-diagram__layers :deep([data-arch]) {
  display: flex;
  flex-direction: column;
  align-items: center;
  justify-content: center;
  gap: 3px;
  padding: 0 10px;
  min-height: 48px;
  border-radius: 10px;
  background-color: var(--l-bg, var(--vp-c-bg-soft));
  background-image: linear-gradient(135deg, rgba(255,255,255,0.06) 0%, transparent 100%);
  box-shadow: 0 1px 4px var(--l-shadow, rgba(0,0,0,0.05));
  color: var(--l-text, var(--vp-c-text));
  font-size: 0.88rem;
  font-weight: 600;
  text-align: center;
  line-height: 1.5;
  transition: background-color 0.25s, box-shadow 0.25s, color 0.25s;
}

/* Container card — has nested children → stretch layout */
.arch-diagram__layers :deep([data-arch]:has([data-arch])),
.arch-diagram__layers :deep([data-arch]:has(.group)) {
  align-items: stretch;
  gap: 8px;
  padding: 8px;
}

/* Nested cards */
.arch-diagram__layers :deep([data-arch="nested"]) {
  background-color: var(--nested-bg, rgba(255,255,255,0.62));
  background-image: none;
  box-shadow: 0 1px 2px var(--l-shadow, rgba(0,0,0,0.03));
  padding: 5px 10px;
  min-height: 38px;
  border-radius: 8px;
  font-size: 0.82rem;
}

/* ================================================================
   GROUP — transparent layout container
   ================================================================ */
.arch-diagram__layers :deep(.group) {
  display: flex;
  flex-direction: column;
  gap: 8px;
  width: 100%;
}

/* ================================================================
   ROW — horizontal flex modifier
   ================================================================ */
.arch-diagram__layers :deep(.row) {
  flex-direction: row;
  flex-wrap: wrap;
}

/* Items inside a row share space equally;
   min-width: fit-content lets wide-text items claim more space
   instead of wrapping. */
.arch-diagram__layers :deep(.row > [data-arch]) {
  flex: 1 1 0;
  min-width: fit-content;
}

/* ================================================================
   TEXT UTILITIES
   ================================================================ */
.arch-diagram__layers :deep(.remark) {
  font-size: 0.76rem;
  font-weight: 400;
  color: var(--l-sub, var(--vp-c-text-2));
  line-height: 1.4;
}

.arch-diagram__layers :deep(.subject) {
  padding: 0 9px;

  color: var(--l-text, var(--vp-c-brand-1));
  border: 1px solid color-mix(in srgb, var(--l-text, var(--vp-c-brand-1)) 28%, transparent);
  box-shadow:
    0 2px 8px color-mix(in srgb, var(--l-text, var(--vp-c-brand-1)) 14%, transparent);

  text-decoration-line: underline;
  text-decoration-thickness: 1px;
  text-decoration-color: color-mix(in srgb, var(--l-sub, var(--vp-c-brand-1)) 55%, transparent);
  text-underline-offset: 0.18em;
}

.arch-diagram__layers :deep(.accent) {
  color: var(--l-accent, var(--vp-c-brand-1));
  font-weight: 700;
}

.arch-diagram__layers :deep(.muted) {
  opacity: 0.6;
  font-weight: 400;
}

.arch-diagram__layers :deep(.label) {
  display: inline-flex;
  align-self: center;
  font-size: 0.68rem;
  font-weight: 600;
  padding: 1px 8px;
  border-radius: 4px;
  background: var(--l-text, var(--vp-c-brand-1));
  color: #fff;
  line-height: 1.6;
}

.arch-diagram__layers :deep(.sep) {
  display: flex;
  align-items: center;
  justify-content: center;
  font-size: 1rem;
  font-weight: 300;
  color: var(--vp-c-text-3, var(--vp-c-text-2));
  opacity: 0.45;
  min-height: 0;
  padding: 0;
}

/* ================================================================
   COPY BUTTON
   ================================================================ */
.arch-diagram__copy {
  position: absolute;
  top: 22px;
  right: 8px;
  transform: translateY(-50%);
  display: flex;
  align-items: center;
  justify-content: center;
  width: 32px;
  height: 32px;
  border: none;
  border-radius: 6px;
  background: transparent;
  color: var(--vp-c-text-3, var(--vp-c-text-2));
  cursor: pointer;
  opacity: 0;
  transition: opacity 0.2s, color 0.15s, background 0.15s;
}
.arch-diagram:hover .arch-diagram__copy { opacity: 1; }
.arch-diagram__copy:hover {
  color: var(--vp-c-text-1);
  background: var(--vp-c-bg-mute, var(--vp-c-bg-soft));
}
.arch-diagram__copy.ok  { opacity: 1; color: #52c41a; }
.arch-diagram__copy.err { opacity: 1; color: #ff4d4f; }

.arch-diagram__copy::after {
  content: attr(data-tooltip);
  position: absolute;
  right: calc(100% + 6px);
  top: 50%;
  transform: translateY(-50%);
  background: var(--vp-c-bg-elv, #333);
  color: var(--vp-c-text, #eee);
  font-size: 0.72rem;
  font-weight: 500;
  white-space: nowrap;
  padding: 3px 8px;
  border-radius: 5px;
  box-shadow: 0 2px 8px rgba(0,0,0,0.15);
  pointer-events: none;
  opacity: 0;
  transition: opacity 0.15s;
}
.arch-diagram__copy:hover::after { opacity: 1; }
.arch-diagram__copy.ok::after,
.arch-diagram__copy.err::after  { opacity: 1; }

.arch-diagram__copy.ok::after {
  color: var(--vp-c-accent, #eee);
}
</style>
