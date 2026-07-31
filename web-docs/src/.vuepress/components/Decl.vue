<script lang="ts">
import { createHighlighter } from 'shiki'

let highlighterPromise: Promise<any> | null = null

export function highlighter() {
  if (!highlighterPromise) {
    highlighterPromise = createHighlighter({
      themes: ['catppuccin-latte', 'catppuccin-mocha'],
      langs: ['typescript'],
    })
  }
  return highlighterPromise
}
</script>

<script setup lang="ts">
import { computed, inject, onMounted, ref, shallowRef, type ComputedRef } from "vue"
import VersionBadge from "./VersionBadge.vue"
import Experimental from "./Experimental.vue"
import "../styles/code-demo.scss"

const dict = inject<ComputedRef<any>>('i18nDict', computed(() => ({})))
const t = computed(() => dict.value.decl ?? {})

const props = defineProps<{
  type?: string,
  version?: string,
  experimental?: boolean,
  get?: true,
  set?: true,
  listen?: true,
  method?: true,
  function?: true
}>()

const slots = defineSlots<{ default(): any }>()
const el = shallowRef<HTMLElement>()
const typeCode = ref<string>('')

const typeText = () => {
  const text = props.type
    ? props.type
    : slots.default ? slots.default()[0].children as string : undefined
  if (!text)
    return ''
  if (!props.method && !props.function && !text.startsWith("?:") && !text.startsWith(":"))
    return ': ' + text
  return text
}

onMounted(async () => {
  const parent = el.value?.parentElement as HTMLElement
  const previous = el.value?.previousSibling as HTMLElement
  const tags = /H1|H2|H3|H4|H5|H6/
  if (parent && /SPAN/.test(parent.tagName)) {
    parent.style.display = "flex"
    parent.style.alignItems = "baseline"
  } else if (previous && tags.test(previous.tagName)) {
    previous.style.display = 'inline'
    if (parent) {
      function updateWidth() {
        const style = getComputedStyle(parent!)
        const parentWidth = parent!.offsetWidth
          - parseFloat(style.paddingLeft) - parseFloat(style.paddingRight)
        const titleWidth = previous.offsetWidth
        if (el.value?.style)
          el.value.style.width = `${parentWidth - titleWidth - 1}px`
      }
      updateWidth()
      new ResizeObserver(updateWidth).observe(parent)
    }
  }

  let code = typeText()
  if (/:|\?:/.test(code))
    code = `let _$$_${code}`
  typeCode.value = (await highlighter()).codeToHtml(code, {
    lang: 'typescript',
    themes: { light: 'catppuccin-latte', dark: 'catppuccin-mocha' },
    defaultColor: false,
  }).replace(/background-color:#\w+;/, '')
    .replace(/ _\$\$_(.+)/, '$1')
    .replace(/<span[^>]+>let<\/span>/, '')
    .replace(/<span[^>]+> _\$\$_<\/span>/, '')
})

const inlineCode = computed(() => {
  return Boolean(props.type || !slots.default || !slots.default()[0].children)
})

const access = computed(() => {
  let res = ""
  function f(name, text) {
    if (props[name]) {
      if (res)
        res += " • "
      res += text
    }
  }
  f("get", t.value.get ?? '读取')
  f("set", t.value.set ?? '设置')
  f("listen", t.value.listen ?? '监听')
  f("method", t.value.method ?? '方法')
  f("function", t.value.function ?? '函数')
  return res
})
</script>

<template>
  <div :class="inlineCode ? ['decl-base'] : ['code-block', 'decl-base']" ref="el">
    <div class="type" :inline="inlineCode" v-html="typeCode"></div>
    <Experimental v-if="experimental" style="font-size: 18px; margin-right: 4px;" />
    <VersionBadge v-if="version" :since="version" />
    <a v-if="access.length > 0" class="access" :href="$withBase(t.docLink ?? '/framework/component/#属性文档规范')">
      {{ access }}
    </a>
  </div>
</template>

<style scoped lang="scss">
.decl-base {
  font-size: 14px;
  line-height: 1.5;
  flex: 1;
  display: inline-flex;
  justify-content: space-between;
  align-items: baseline;
}

.code-block {
  margin-top: calc(0em - var(--navbar-height));
  margin-bottom: -0.5rem;
  padding-top: calc(0.5rem + var(--navbar-height));
}

.decl-base>.type {
  overflow-x: auto;
  width: 0;
  flex: 1;
}

.decl-base>.access {
  color: #696969;
  font-family: var(--font-family);
  font-weight: 600;
  flex-shrink: 0;
  margin-left: 1em;
  border-bottom: solid 1.5px #1976D2D0;

  [data-theme='dark'] & {
    color: #b0b0b0;
    border-bottom-color: #64B5F6D0;
  }
}

.decl-base>.access:hover {
  text-decoration: initial;
  border-bottom-color: #1976D280;
  cursor: pointer;

  [data-theme='dark'] & {
    border-bottom-color: #64B5F680;
  }
}

.shiki span {
  color: var(--shiki-light, inherit);

  [data-theme='dark'] & {
    color: var(--shiki-dark, inherit);
  }
}
</style>

<style lang="scss">
.shiki {
  -webkit-font-smoothing: antialiased;
  -moz-osx-font-smoothing: grayscale;
  text-rendering: geometricPrecision;
}
</style>
