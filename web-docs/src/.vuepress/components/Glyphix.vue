<script setup lang="ts">
import { ref, shallowRef, onMounted, inject, computed, type ComputedRef, nextTick } from 'vue'
import '../styles/code-demo.scss'
import { createEmulator } from './glyphix-emulator.js'

const dict = inject<ComputedRef<any>>('i18nDict', computed(() => ({})))
const t = computed(() => dict.value.demo ?? {})

const isExpanded = ref(false)
const height = ref("0");
const codeContainer = shallowRef()
const canvasRef = shallowRef()
const isLoading = ref(true) // Loading state
const loadingError = ref('') // Error state
const restartKey = ref(0) // Key to force re-render of the canvas

const props = withDefaults(
  defineProps<{
    title?: string,
    id: string,
    inline?: boolean,
    wheel?: boolean,
    width?: number | string,
    height?: number | string
  }>(), {
  title: undefined,
  inline: false,
  wheel: false,
  width: 480,
  height: 100
})

onMounted(async () => {
  await launchApplet()

  // Use Pointer Capture so the canvas continues to receive mouse input
  // even after the pointer leaves its bounds during a drag.
  //
  // Although the application only listens for legacy mouse/touch events
  // (e.g. via Emscripten), browsers continue dispatching the corresponding
  // compatibility mouse events to the capturing element while pointer capture
  // is active. This allows drag operations to continue without modifying the
  // existing C++ event handling code.
  const canvas = canvasRef.value
  canvas.addEventListener("pointerdown", (e: PointerEvent) => {
    canvas.setPointerCapture(e.pointerId);
  });
  canvas.addEventListener("pointerup", (e: PointerEvent) => {
    if (canvas.hasPointerCapture(e.pointerId))
      canvas.releasePointerCapture(e.pointerId);
  });
  canvas.addEventListener("pointercancel", (e: PointerEvent) => {
    if (canvas.hasPointerCapture(e.pointerId))
      canvas.releasePointerCapture(e.pointerId);
  });
})

function onToggle() {
  height.value = isExpanded.value
    ? "0"
    : `${codeContainer.value.clientHeight + 13.8}px`;
  isExpanded.value = !isExpanded.value
}

//! Launch a Glyphix applet emulator.
async function launchApplet() {
  try {
    isLoading.value = true
    loadingError.value = ''
    await createEmulator(canvasRef.value, { id: props.id, mouseWheel: props.wheel })
    isLoading.value = false
  } catch (error) {
    isLoading.value = false
    loadingError.value = t.value.failedLoading || 'Failed to load the emulator'
    console.error('Emulator loading error:', error)
  }
}

// Restart the emulator by re-mounting the canvas and re-launching the applet
async function restart() {
  restartKey.value++
  await nextTick()
  launchApplet()
}
</script>

<template>
  <canvas v-if="props.inline" ref="canvasRef" :width="props.width" :height="props.height"
    v-on:contextmenu="$event.preventDefault()" />
  <div v-else class="vp-code-demo">
    <div class="vp-container-header">
      <button type="button" title="toggle" aria-hidden
        :class="['vp-code-demo-toggle-button', isExpanded ? 'down' : 'end']" v-on:click="onToggle()">
      </button>
      <span class="vp-container-title">
        {{ decodeURIComponent(title || t.title || 'Demo') }}
      </span>
      <button type="button" class="vp-code-demo-restart-button" @click="restart" title="Restart the example">
        <svg xmlns="http://www.w3.org/2000/svg" width="24" height="24" viewBox="0 0 24 24">
          <path fill="currentColor"
            d="M17.65 6.35A7.96 7.96 0 0 0 12 4a8 8 0 0 0-8 8a8 8 0 0 0 8 8c3.73 0 6.84-2.55 7.73-6h-2.08A5.99 5.99 0 0 1 12 18a6 6 0 0 1-6-6a6 6 0 0 1 6-6c1.66 0 3.14.69 4.22 1.78L13 11h7V4z" />
        </svg>
      </button>
      <span v-if="props.wheel" class="vp-code-demo-wheel-hint"
        :data-tooltip="t.wheelHint || 'This demo supports mouse wheel interaction'">
        <svg xmlns="http://www.w3.org/2000/svg" width="24" height="24" viewBox="0 0 24 24">
          <path fill="currentColor"
            d="M12 2a7 7 0 0 0-7 7v6a7 7 0 0 0 14 0V9a7 7 0 0 0-7-7m5 13a5 5 0 0 1-10 0V9a5 5 0 0 1 10 0zm-5-9a1 1 0 0 0-1 1v3a1 1 0 0 0 2 0V7a1 1 0 0 0-1-1" />
        </svg>
      </span>
      <span class="vp-container-description">
        {{ props.width }}×{{ props.height }}px
      </span>
    </div>
    <div :class="['vp-code-demo-display', isExpanded ? 'vp-code-demo-spliter' : null]">
      <!-- Loading overlay -->
      <div v-if="isLoading || loadingError" class="vp-loading-overlay">
        <div v-if="isLoading" class="vp-loading-spinner">
          <div class="vp-spinner"></div>
          <p class="vp-loading-text">{{ t.loading || 'Loading emulator...' }}</p>
        </div>
        <div v-else-if="loadingError" class="vp-error-message">
          {{ loadingError }}
        </div>
      </div>

      <canvas :key="restartKey" ref="canvasRef" :width="props.width" :height="props.height"
        v-on:contextmenu="$event.preventDefault()" />
    </div>
    <div class="vp-code-demo-code-wrapper" :style="`height: ${height}`">
      <div class="vp-code-demo-codes" ref="codeContainer">
        <slot></slot>
      </div>
    </div>
  </div>
</template>

<style scoped>
.vp-code-demo-toggle-button,
.vp-code-demo-title {
  font-size: 1.15rem;
  font-weight: 600;
}

.vp-code-demo-restart-button {
  margin-left: 12px;
  padding: 4px;
  width: 24px;
  height: 24px;
  display: flex;
  align-items: center;
  justify-content: center;
  background-color: transparent;
  border: 1px solid var(--vp-c-divider);
  border-radius: 4px;
  cursor: pointer;
  color: var(--vp-c-text-2);
  transition: all 0.2s;
}

.vp-code-demo-restart-button:hover {
  background-color: var(--vp-c-bg-mute);
  color: var(--vp-c-brand);
  border-color: var(--vp-c-brand);
}

.vp-code-demo-wheel-hint {
  position: relative;
  margin-left: 8px;
  width: 24px;
  height: 24px;
  display: flex;
  align-items: center;
  justify-content: center;
  color: var(--vp-c-grey-text);
  cursor: help;
  transition: color 0.2s;
}

.vp-code-demo-wheel-hint svg {
  width: 16px;
  height: 16px;
}

.vp-code-demo-wheel-hint:hover {
  color: var(--vp-c-brand);
}

.vp-code-demo-wheel-hint::after {
  content: attr(data-tooltip);
  position: absolute;
  left: 50%;
  top: calc(100% + 6px);
  transform: translateX(-50%);
  background: var(--vp-c-bg-elv, #333);
  color: var(--vp-c-text, #eee);
  font-size: 0.72rem;
  font-weight: 500;
  white-space: nowrap;
  padding: 3px 8px;
  border-radius: 5px;
  box-shadow: 0 2px 8px rgba(0, 0, 0, 0.15);
  pointer-events: none;
  opacity: 0;
  transition: opacity 0.15s;
  z-index: 20;
}

.vp-code-demo-wheel-hint:hover::after {
  opacity: 1;
}

.vp-code-demo-toggle-button {
  margin: 8px 8px 8px 12px;
}

.vp-code-demo-header {
  padding: 0;
}

.vp-container-description {
  font-family: var(--vp-font-mono);
  font-weight: normal;
  font-size: 0.8rem;
  color: var(--vp-c-grey-text);
}

/* Loading overlay styles */
.vp-loading-overlay {
  position: absolute;
  top: 0;
  left: 0;
  right: 0;
  bottom: 0;
  display: flex;
  align-items: center;
  justify-content: center;
  background: transparent;
  z-index: 10;
}

.vp-loading-spinner {
  display: flex;
  flex-direction: column;
  align-items: center;
  gap: 16px;
}

.vp-spinner {
  width: 40px;
  height: 40px;
  border: 4px solid #f3f3f3;
  border-top: 4px solid #3498db;
  border-radius: 50%;
  animation: spin 1s linear infinite;
}

@keyframes spin {
  0% {
    transform: rotate(0deg);
  }

  100% {
    transform: rotate(360deg);
  }
}

.vp-loading-text {
  margin: 0;
  font-size: 14px;
  color: var(--vp-c-text-2);
}

.vp-error-message {
  display: flex;
  flex-direction: column;
  align-items: center;
  gap: 12px;
  color: var(--vp-c-red-text);
}

.vp-code-demo-display {
  position: relative;
}
</style>
