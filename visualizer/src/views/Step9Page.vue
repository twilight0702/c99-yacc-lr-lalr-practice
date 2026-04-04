<template>
  <section class="page">
    <div class="page-toolbar">
      <h2>Step 9 · LR 总控程序执行与解析轨迹</h2>
      <button class="btn primary" type="button" @click="exportJson" :disabled="!data">
        导出本页 JSON
      </button>
    </div>

    <div v-if="store.error" class="error-box">{{ store.error }}</div>
    <template v-else-if="data">
      <section class="metrics-grid">
        <MetricCard title="输入 token 数" :value="data.summary.input_tokens ?? '-'" />
        <MetricCard title="解析结果" :value="data.summary.parse_accepted ?? '-'" />
        <MetricCard title="执行步数" :value="data.summary.parse_total_steps ?? '-'" />
        <MetricCard title="规约次数" :value="data.summary.parse_reductions ?? '-'" />
      </section>

      <section class="panel">
        <header class="panel-header">
          <h3>输入 token 流</h3>
          <div class="toolbar">
            <button class="btn" type="button" :disabled="tokenPage <= 1" @click="tokenPage -= 1">上一页</button>
            <span class="mono-line">第 {{ tokenPage }} / {{ tokenTotalPages }} 页</span>
            <button class="btn" type="button" :disabled="tokenPage >= tokenTotalPages" @click="tokenPage += 1">
              下一页
            </button>
          </div>
        </header>
        <div class="table-wrap">
          <table class="table">
            <thead>
              <tr>
                <th>index</th>
                <th>symbol</th>
                <th>lexeme</th>
                <th>line</th>
                <th>column</th>
              </tr>
            </thead>
            <tbody>
              <tr v-for="row in pagedTokens" :key="`tk-${row.index}`">
                <td>{{ row.index }}</td>
                <td>{{ row.symbol_name }} ({{ row.symbol_id }})</td>
                <td>{{ row.lexeme }}</td>
                <td>{{ row.line }}</td>
                <td>{{ row.column }}</td>
              </tr>
            </tbody>
          </table>
        </div>
      </section>

      <section class="panel">
        <header class="panel-header">
          <h3>解析执行轨迹</h3>
          <div class="toolbar">
            <button class="btn" type="button" :disabled="tracePage <= 1" @click="tracePage -= 1">上一页</button>
            <span class="mono-line">第 {{ tracePage }} / {{ traceTotalPages }} 页</span>
            <button class="btn" type="button" :disabled="tracePage >= traceTotalPages" @click="tracePage += 1">
              下一页
            </button>
            <span class="mono-line">每页 {{ tracePageSize }} 条</span>
          </div>
        </header>
        <div class="table-wrap transition-table-wrap">
          <table class="table table-compact">
            <thead>
              <tr>
                <th>step</th>
                <th>state</th>
                <th>lookahead</th>
                <th>action</th>
                <th>prod</th>
                <th>input_idx</th>
                <th>state_stack</th>
                <th>symbol_stack</th>
              </tr>
            </thead>
            <tbody>
              <tr v-for="row in pagedTraceRows" :key="`tr-${row.step}`">
                <td>{{ row.step }}</td>
                <td>I{{ row.state }}</td>
                <td>{{ row.lookahead }} ({{ row.lookahead_id }})</td>
                <td>{{ row.action }}</td>
                <td>{{ row.production_id >= 0 ? `#${row.production_id}` : "-" }}</td>
                <td>{{ row.input_index }}</td>
                <td class="truncate-cell" :title="row.state_stack">{{ row.state_stack }}</td>
                <td class="truncate-cell" :title="row.symbol_stack">{{ row.symbol_stack }}</td>
              </tr>
            </tbody>
          </table>
        </div>
      </section>

      <section class="panel">
        <header class="panel-header">
          <h3>规约序列</h3>
          <div class="toolbar">
            <button class="btn" type="button" :disabled="reductionPage <= 1" @click="reductionPage -= 1">上一页</button>
            <span class="mono-line">第 {{ reductionPage }} / {{ reductionTotalPages }} 页</span>
            <button
              class="btn"
              type="button"
              :disabled="reductionPage >= reductionTotalPages"
              @click="reductionPage += 1"
            >
              下一页
            </button>
          </div>
        </header>
        <div class="table-wrap">
          <table class="table">
            <thead>
              <tr>
                <th>#</th>
                <th>production_id</th>
                <th>text</th>
              </tr>
            </thead>
            <tbody>
              <tr v-for="row in pagedReductions" :key="`rd-${row.index}`">
                <td>{{ row.index }}</td>
                <td>{{ row.production_id }}</td>
                <td>{{ row.text }}</td>
              </tr>
            </tbody>
          </table>
        </div>
      </section>

      <section class="panel">
        <header class="panel-header">
          <h3>错误分析</h3>
        </header>
        <div class="panel-body">
          <p class="mono-line">status: {{ runtimeError.status ?? "-" }}</p>
          <p class="mono-line">message: {{ runtimeError.message ?? "无错误（accept）" }}</p>
          <p class="mono-line">state: {{ runtimeError.state ?? "-" }}</p>
          <p class="mono-line">lookahead: {{ runtimeError.lookahead ?? "-" }}</p>
          <p class="mono-line">expected: {{ runtimeError.expected ?? "-" }}</p>
        </div>
      </section>
    </template>
  </section>
</template>

<script setup lang="ts">
import { computed, onMounted, ref, watch } from "vue";
import MetricCard from "../components/MetricCard.vue";
import { useYaccStore } from "../stores/yacc";
import { downloadJsonFile } from "../utils/data";

const store = useYaccStore();
const data = computed(() => store.stepData[9]);

const tokenPageSize = 100;
const tracePageSize = 120;
const reductionPageSize = 80;

const tokenPage = ref(1);
const tracePage = ref(1);
const reductionPage = ref(1);

const runtime = computed(() => data.value?.parse_runtime);
const inputTokens = computed(() => runtime.value?.input_tokens ?? []);
const traceRows = computed(() => runtime.value?.trace_rows ?? []);
const reductions = computed(() => runtime.value?.reductions ?? []);
const runtimeError = computed(() => runtime.value?.error ?? {});

const tokenTotalPages = computed(() => Math.max(1, Math.ceil(inputTokens.value.length / tokenPageSize)));
const traceTotalPages = computed(() => Math.max(1, Math.ceil(traceRows.value.length / tracePageSize)));
const reductionTotalPages = computed(() => Math.max(1, Math.ceil(reductions.value.length / reductionPageSize)));

watch([inputTokens], () => {
  tokenPage.value = 1;
});
watch([traceRows], () => {
  tracePage.value = 1;
});
watch([reductions], () => {
  reductionPage.value = 1;
});

watch(tokenPage, (p) => {
  if (p < 1) {
    tokenPage.value = 1;
    return;
  }
  if (p > tokenTotalPages.value) {
    tokenPage.value = tokenTotalPages.value;
  }
});

watch(tracePage, (p) => {
  if (p < 1) {
    tracePage.value = 1;
    return;
  }
  if (p > traceTotalPages.value) {
    tracePage.value = traceTotalPages.value;
  }
});

watch(reductionPage, (p) => {
  if (p < 1) {
    reductionPage.value = 1;
    return;
  }
  if (p > reductionTotalPages.value) {
    reductionPage.value = reductionTotalPages.value;
  }
});

const pagedTokens = computed(() => {
  const begin = (tokenPage.value - 1) * tokenPageSize;
  return inputTokens.value.slice(begin, begin + tokenPageSize);
});

const pagedTraceRows = computed(() => {
  const begin = (tracePage.value - 1) * tracePageSize;
  return traceRows.value.slice(begin, begin + tracePageSize);
});

const pagedReductions = computed(() => {
  const begin = (reductionPage.value - 1) * reductionPageSize;
  return reductions.value.slice(begin, begin + reductionPageSize);
});

function exportJson() {
  if (!data.value) {
    return;
  }
  downloadJsonFile("step9_data.json", data.value);
}

onMounted(async () => {
  await store.ensureStep(9);
});
</script>

<style scoped>
.table-compact th,
.table-compact td {
  padding: 6px 8px;
  white-space: nowrap;
  font-size: 12px;
}
</style>

