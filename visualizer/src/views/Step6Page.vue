<template>
  <section class="page">
    <div class="page-toolbar">
      <h2>Step 6 · LR(1) I0 闭包与 Goto</h2>
      <button class="btn primary" type="button" @click="exportJson" :disabled="!data">
        导出本页 JSON
      </button>
    </div>

    <div v-if="store.error" class="error-box">{{ store.error }}</div>
    <template v-else-if="data">
      <section class="metrics-grid">
        <MetricCard title="I0 kernel 项数" :value="data.summary.i0_kernel_items ?? '-'" />
        <MetricCard title="I0 closure 项数" :value="data.summary.i0_closure_items ?? '-'" />
        <MetricCard title="goto 边数" :value="data.summary.i0_goto_edges ?? '-'" />
        <MetricCard title="校验通过" :value="data.summary.lr1_step6_validation_passed ?? '-'" />
      </section>

      <CytoPanel
        title="I0 出边图（可拖拽/缩放）"
        file-name="step6_i0_goto_graph.png"
        :elements="graphElements"
      />

      <section class="panel">
        <header class="panel-header">
          <h3>I0 项目列表</h3>
          <div class="toolbar">
            <button class="btn" @click="activeTab = 'kernel'" :class="{ active: activeTab === 'kernel' }">
              Kernel
            </button>
            <button class="btn" @click="activeTab = 'closure'" :class="{ active: activeTab === 'closure' }">
              Closure
            </button>
          </div>
        </header>
        <div class="code-list">
          <p v-for="item in activeItems" :key="item" class="mono-line">{{ item }}</p>
        </div>
      </section>

      <section class="panel">
        <header class="panel-header">
          <h3>goto(I0, X) 明细</h3>
          <select v-model="selectedSymbol" class="input select">
            <option v-for="edge in gotoEdges" :key="edge.symbol_name" :value="edge.symbol_name">
              {{ edge.symbol_name }} ({{ edge.item_count }})
            </option>
          </select>
        </header>
        <div class="code-list">
          <p v-for="item in selectedGotoItems" :key="item" class="mono-line">{{ item }}</p>
        </div>
      </section>
    </template>
  </section>
</template>

<script setup lang="ts">
import type { ElementDefinition } from "cytoscape";
import { computed, onMounted, ref, watch } from "vue";
import CytoPanel from "../components/CytoPanel.vue";
import MetricCard from "../components/MetricCard.vue";
import { useYaccStore } from "../stores/yacc";
import { downloadJsonFile } from "../utils/data";

const store = useYaccStore();
const data = computed(() => store.stepData[6]);
const activeTab = ref<"kernel" | "closure">("kernel");
const selectedSymbol = ref("");

const gotoEdges = computed(() => data.value?.lr1_i0?.goto_edges ?? []);

watch(
  gotoEdges,
  (edges) => {
    if (!selectedSymbol.value && edges.length > 0) {
      selectedSymbol.value = edges[0].symbol_name;
    }
  },
  { immediate: true }
);

const graphElements = computed<ElementDefinition[]>(() => {
  const elements: ElementDefinition[] = [{ data: { id: "I0", label: "I0" } }];
  for (const edge of gotoEdges.value) {
    const nodeId = `G-${edge.symbol_name}`;
    elements.push({
      data: {
        id: nodeId,
        label: `${edge.symbol_name}\nitems=${edge.item_count}`
      }
    });
    elements.push({
      data: {
        id: `I0->${nodeId}`,
        source: "I0",
        target: nodeId,
        label: edge.symbol_name
      }
    });
  }
  return elements;
});

const activeItems = computed(() => {
  const lr1 = data.value?.lr1_i0;
  if (!lr1) {
    return [];
  }
  return activeTab.value === "kernel" ? lr1.kernel_items : lr1.closure_items;
});

const selectedGotoItems = computed(() => {
  const lr1 = data.value?.lr1_i0;
  if (!lr1) {
    return [];
  }
  return lr1.goto_items[selectedSymbol.value] ?? [];
});

function exportJson() {
  if (!data.value) {
    return;
  }
  downloadJsonFile("step6_data.json", data.value);
}

onMounted(async () => {
  await store.ensureStep(6);
});
</script>
