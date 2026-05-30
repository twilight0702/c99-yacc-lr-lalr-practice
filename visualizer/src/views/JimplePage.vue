<template>
  <section class="page">
    <div class="page-toolbar">
      <h2>中间代码生成结果</h2>
    </div>

    <div v-if="store.error" class="error-box">{{ store.error }}</div>
    <template v-else>
      <section class="metrics-grid">
        <MetricCard title="Jimple 文件" :value="jimple?.label ?? '无jimple文件'" :hint="jimple?.note" />
        <MetricCard title="Jimple 行数" :value="jimple ? countSourceLines(jimple.code) : '无jimple文件'" />
        <MetricCard title="非空行数" :value="jimple ? countNonEmptyLines(jimple.code) : '无jimple文件'" />
      </section>

      <SourceCodePanel title="最终 Jimple 输出" :code="jimple?.code ?? ''" :meta="jimple?.label" />
    </template>
  </section>
</template>

<script setup lang="ts">
import { computed, onMounted, ref } from "vue";
import MetricCard from "../components/MetricCard.vue";
import SourceCodePanel from "../components/SourceCodePanel.vue";
import { useYaccStore } from "../stores/yacc";
import { countNonEmptyLines, countSourceLines, loadJimpleArtifact, type ResolvedArtifact } from "../utils/pipelineArtifacts";

const store = useYaccStore();
const caseId = computed(() => store.currentCaseId || store.manifest?.case_id || "-");
const jimple = ref<ResolvedArtifact | null>(null);

onMounted(async () => {
  await store.ensureManifest();
  if (!store.manifest) {
    return;
  }
  try {
    jimple.value = await loadJimpleArtifact(store.manifest);
  } catch {
    // jimple file not found — page renders with "无jimple文件" fallback
  }
});
</script>
