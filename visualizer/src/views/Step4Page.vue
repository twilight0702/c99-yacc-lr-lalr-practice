<template>
  <section class="page">
    <div class="page-toolbar">
      <h2>Step 4 · 文法预处理与增广</h2>
      <button class="btn primary" type="button" @click="exportJson" :disabled="!data">
        导出本页 JSON
      </button>
    </div>

    <div v-if="store.error" class="error-box">{{ store.error }}</div>
    <template v-else-if="data">
      <section class="metrics-grid">
        <MetricCard title="预处理通过" :value="data.summary.preprocess_passed ?? '-'" />
        <MetricCard title="增广开始符" :value="data.summary.augmented_start_symbol ?? '-'" />
        <MetricCard title="增广产生式 ID" :value="data.summary.augmented_production_id ?? '-'" />
        <MetricCard title="非终结符数量" :value="data.summary.nonterminals ?? '-'" />
      </section>

      <section class="panel">
        <header class="panel-header">
          <h3>增广文法</h3>
        </header>
        <p class="mono-line">{{ data.augmented?.production || "无" }}</p>
      </section>

      <CytoPanel
        title="LHS 到产生式索引图（可拖拽/缩放）"
        file-name="step4_lhs_prod_graph.png"
        :elements="graphElements"
      />
    </template>
  </section>
</template>

<script setup lang="ts">
import type { ElementDefinition } from "cytoscape";
import { computed, onMounted } from "vue";
import CytoPanel from "../components/CytoPanel.vue";
import MetricCard from "../components/MetricCard.vue";
import { useYaccStore } from "../stores/yacc";
import { downloadJsonFile } from "../utils/data";

const store = useYaccStore();
const data = computed(() => store.stepData[4]);

function shortProdText(text: string): string {
  const max = 42;
  return text.length > max ? `${text.slice(0, max)}...` : text;
}

const graphElements = computed<ElementDefinition[]>(() => {
  const elements: ElementDefinition[] = [];
  const indexRows = data.value?.prod_index_by_lhs ?? [];
  const prodMap = new Map((data.value?.productions ?? []).map((p) => [p.id, p]));
  for (const row of indexRows) {
    const lhsNodeId = `lhs-${row.lhs_id}`;
    elements.push({
      data: {
        id: lhsNodeId,
        label: `${row.lhs_name}\n(${row.production_ids.length})`
      }
    });
    for (const pid of row.production_ids) {
      const prodNodeId = `prod-${pid}`;
      const prod = prodMap.get(pid);
      const rhsText = prod ? (prod.rhs.length ? prod.rhs.join(" ") : "epsilon") : "?";
      const fullLabel = prod ? `${prod.lhs} -> ${rhsText}` : `#${pid}`;
      elements.push({
        data: {
          id: prodNodeId,
          label: `${shortProdText(fullLabel)}\n(#${pid})`
        }
      });
      elements.push({
        data: {
          id: `${lhsNodeId}->${prodNodeId}`,
          source: lhsNodeId,
          target: prodNodeId,
          label: "index"
        }
      });
    }
  }
  return elements;
});

function exportJson() {
  if (!data.value) {
    return;
  }
  downloadJsonFile("step4_data.json", data.value);
}

onMounted(async () => {
  await store.ensureStep(4);
});
</script>
