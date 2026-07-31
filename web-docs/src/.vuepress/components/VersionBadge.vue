<script setup lang="ts">
const props = defineProps<{
  since?: string
  prefix?: string
}>()
</script>

<template>
  <span class="version-badge" v-if="props.since">
    {{ props.prefix ?? 'since' }} {{ props.since }}</span>
</template>

<style scoped lang="scss">
.version-badge {
  display: inline-flex;
  vertical-align: 0.2em;
  align-items: center;
  text-align: center;
  height: 14px;

  color: #7467d6;
  font-family: sans-serif;
  font-size: 11px;
  font-weight: 500;
  flex-shrink: 0;

  padding: 0 0.4em;
  padding-top: 1px;
  border-radius: 8px;

  background: rgba(116, 103, 214, 0.052);
  border: 1px solid rgba(116, 103, 214, 0.145);

  white-space: nowrap;

  [data-theme='dark'] & {
    color: #aaa0e6;
    background: rgba(170, 160, 230, 0.085);
    border-color: rgba(170, 160, 230, 0.18);
  }
}
</style>
