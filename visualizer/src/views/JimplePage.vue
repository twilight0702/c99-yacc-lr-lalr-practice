<template>
  <section class="page">
    <div class="page-toolbar">
      <h2>中间代码生成结果</h2>
    </div>

    <div v-if="store.error" class="error-box">{{ store.error }}</div>
    <div v-else-if="pageError" class="error-box">{{ pageError }}</div>
    <template v-else>
      <section class="metrics-grid">
        <MetricCard title="当前 Case" :value="caseId" hint="与顶部 URL 参数保持一致" />
        <MetricCard title="Jimple 文件" :value="jimple?.label ?? '-'" :hint="jimple?.note" />
        <MetricCard title="Jimple 行数" :value="countSourceLines(jimple?.code ?? '')" />
        <MetricCard title="非空行数" :value="countNonEmptyLines(jimple?.code ?? '')" />
      </section>

      <SourceCodePanel title="最终 Jimple 输出" :code="jimple?.code ?? ''" :meta="jimple?.label" />
    </template>
  </section>
</template>

<script setup lang="ts">
import { onMounted, ref } from "vue";
import MetricCard from "../components/MetricCard.vue";
import SourceCodePanel from "../components/SourceCodePanel.vue";
import { useYaccStore } from "../stores/yacc";
import { getCaseId } from "../utils/data";
import { countNonEmptyLines, countSourceLines, loadJimpleArtifact, type ResolvedArtifact } from "../utils/pipelineArtifacts";

const store = useYaccStore();
const caseId = getCaseId();
const pageError = ref("");
const jimple = ref<ResolvedArtifact | null>(null);

onMounted(async () => {
  await store.ensureManifest();
  if (!store.manifest) {
    return;
  }
  try {
    jimple.value = await loadJimpleArtifact(store.manifest);
  } catch (error) {
    pageError.value = error instanceof Error ? error.message : String(error);
  }
});
</script>
