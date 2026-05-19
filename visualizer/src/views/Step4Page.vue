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
        <MetricCard title="预处理通过" :value="data.preprocess?.passed ?? '-'" />
        <MetricCard title="增广开始符" :value="data.summary.augmented_start_symbol ?? '-'" />
        <MetricCard title="增广产生式 ID" :value="data.summary.augmented_production_id ?? '-'" />
        <MetricCard title="预处理错误数" :value="data.preprocess?.errors_count ?? 0" />
        <MetricCard title="预处理告警数" :value="data.preprocess?.warnings_count ?? 0" />
      </section>

      <section class="panel">
        <header class="panel-header">
          <h3>增广文法</h3>
        </header>
        <div class="panel-body">
          <p class="hint">从 Step4 开始引入增广产生式（S' -> start_symbol）。</p>
          <p class="mono-line">{{ data.augmented?.production || "无" }}</p>
        </div>
      </section>

      <section class="panel">
        <header class="panel-header">
          <h3>LHS 到产生式索引明细</h3>
        </header>
        <div class="table-wrap">
          <table class="table">
            <thead>
              <tr>
                <th>lhs_id</th>
                <th>lhs_name</th>
                <th>production_ids</th>
              </tr>
            </thead>
            <tbody>
              <tr v-for="row in data.prod_index_by_lhs ?? []" :key="row.lhs_id">
                <td>{{ row.lhs_id }}</td>
                <td>{{ row.lhs_name }}</td>
                <td>{{ row.production_ids.join(", ") }}</td>
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
const data = computed(() => store.stepData[4]);

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
