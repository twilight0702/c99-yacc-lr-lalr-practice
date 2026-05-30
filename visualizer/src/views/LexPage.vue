<template>
  <section class="page">
    <div class="page-toolbar">
      <h2>Lex 输入概览</h2>
    </div>

    <div v-if="store.error" class="error-box">{{ store.error }}</div>
    <div v-else-if="pageError" class="error-box">{{ pageError }}</div>
    <template v-else>
      <section class="metrics-grid">
        <MetricCard title="当前 Case" :value="caseId" hint="与顶部 URL 参数保持一致" />
        <MetricCard title="Lex 规则文件" :value="lexSpec?.label ?? '-'" :hint="lexSpec?.note" />
        <MetricCard title="C 输入来源" :value="lexInput?.label ?? '-'" :hint="lexInput?.note" />
        <MetricCard title="Lex 行数" :value="countSourceLines(lexSpec?.code ?? '')" />
        <MetricCard title="C 输入行数" :value="countSourceLines(lexInput?.code ?? '')" />
        <MetricCard title="C 非空行" :value="countNonEmptyLines(lexInput?.code ?? '')" />
      </section>

      <SourceCodePanel title="Lex 规则文件（.l）" :code="lexSpec?.code ?? ''" :meta="lexSpec?.label" />
      <SourceCodePanel title="词法分析输入代码（C）" :code="lexInput?.code ?? ''" :meta="lexInput?.label" />
    </template>
  </section>
</template>

<script setup lang="ts">
import { onMounted, ref } from "vue";
import MetricCard from "../components/MetricCard.vue";
import SourceCodePanel from "../components/SourceCodePanel.vue";
import { useYaccStore } from "../stores/yacc";
import { getCaseId } from "../utils/data";
import { countNonEmptyLines, countSourceLines, loadLexArtifacts, type ResolvedArtifact } from "../utils/pipelineArtifacts";

const store = useYaccStore();
const caseId = getCaseId();
const pageError = ref("");
const lexSpec = ref<ResolvedArtifact | null>(null);
const lexInput = ref<ResolvedArtifact | null>(null);

onMounted(async () => {
  await store.ensureManifest();
  if (!store.manifest) {
    return;
  }
  try {
    const artifacts = await loadLexArtifacts(store.manifest);
    lexSpec.value = artifacts.lexSpec;
    lexInput.value = artifacts.lexInput;
  } catch (error) {
    pageError.value = error instanceof Error ? error.message : String(error);
  }
});
</script>
