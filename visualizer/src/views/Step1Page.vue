<template>
  <section class="page">
    <div class="page-toolbar">
      <h2>Step 1 · 输入文件与词法符号基线</h2>
      <button class="btn primary" type="button" @click="exportJson" :disabled="!data">
        导出本页 JSON
      </button>
    </div>

    <div v-if="store.error" class="error-box">{{ store.error }}</div>
    <template v-else-if="data">
      <section class="metrics-grid">
        <MetricCard title="源文件存在" :value="sourceExistsText" />
        <MetricCard title="总行数" :value="overview.total_lines ?? 0" />
        <MetricCard title="非空行数" :value="overview.nonempty_lines ?? 0" />
        <MetricCard title="符号总数" :value="baseline.symbols" />
        <MetricCard title="终结符" :value="baseline.terminals" />
        <MetricCard title="非终结符" :value="baseline.nonterminals" />
        <MetricCard title="产生式总数" :value="baseline.productions_with_augmented" />
      </section>

      <section class="panel">
        <header class="panel-header">
          <h3>源语法文件预览</h3>
        </header>
        <div class="code-list">
          <p v-for="line in overview.preview_lines ?? []" :key="line.line" class="mono-line">
            {{ line.line }} | {{ line.text }}
          </p>
        </div>
      </section>

      <section class="panel">
        <header class="panel-header">
          <h3>输入规范校验</h3>
        </header>
        <div class="table-wrap">
          <table class="table">
            <thead>
              <tr>
                <th>检查项</th>
                <th>状态</th>
                <th>明细</th>
              </tr>
            </thead>
            <tbody>
              <tr v-for="row in specChecks" :key="row.id">
                <td>{{ row.label }}</td>
                <td>{{ row.status }}</td>
                <td>{{ row.detail }}</td>
              </tr>
            </tbody>
          </table>
        </div>
      </section>

    </template>
  </section>
</template>

<script setup lang="ts">
import { computed, onMounted } from "vue";
import MetricCard from "../components/MetricCard.vue";
import { useYaccStore } from "../stores/yacc";
import { downloadJsonFile } from "../utils/data";

const store = useYaccStore();
const data = computed(() => store.stepData[1]);
const overview = computed(() => data.value?.step1_overview ?? {
  source_exists: false,
  total_lines: 0,
  nonempty_lines: 0,
  preview_lines: []
});
const baseline = computed(() => data.value?.step1_baseline ?? {
  symbols: 0,
  terminals: 0,
  nonterminals: 0,
  productions_with_augmented: 0
});

const sourceExistsText = computed(() => (overview.value.source_exists ? "yes" : "no"));
const specChecks = computed(() => data.value?.input_spec_checks ?? []);

function exportJson() {
  if (!data.value) {
    return;
  }
  downloadJsonFile("step1_data.json", data.value);
}

onMounted(async () => {
  await store.ensureStep(1);
});
</script>
