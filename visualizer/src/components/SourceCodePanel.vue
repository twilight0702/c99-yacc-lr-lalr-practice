<template>
  <section class="panel">
    <header class="panel-header">
      <div>
        <h3>{{ title }}</h3>
        <p v-if="meta" class="panel-meta">{{ meta }}</p>
      </div>
    </header>
    <div class="source-panel">
      <div v-if="!lines.length" class="source-empty">暂无可展示内容</div>
      <div v-else class="source-code">
        <div v-for="(line, index) in lines" :key="`${title}-${index}`" class="source-line">
          <span class="source-line-no">{{ index + 1 }}</span>
          <code class="source-line-text">{{ line || " " }}</code>
        </div>
      </div>
    </div>
  </section>
</template>

<script setup lang="ts">
import { computed } from "vue";

const props = defineProps<{
  title: string;
  code: string;
  meta?: string;
}>();

const lines = computed(() => props.code.split("\n"));
</script>
