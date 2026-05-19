<template>
  <section class="page">
    <div class="page-toolbar">
      <h2>Step 2 · 文法模型初始化与合法性校验</h2>
      <button class="btn primary" type="button" @click="exportJson" :disabled="!data">
        导出本页 JSON
      </button>
    </div>

    <div v-if="store.error" class="error-box">{{ store.error }}</div>
    <template v-else-if="data">
      <section class="metrics-grid">
        <MetricCard title="起始符" :value="grammar.start_symbol || '-'" />
        <MetricCard title="终结符数量" :value="grammar.terminals.length" />
        <MetricCard title="非终结符数量" :value="grammar.nonterminals.length" />
        <MetricCard title="产生式数量" :value="grammar.productions_count || 0" />
        <MetricCard
          title="不可达非终结符"
          :value="data.analysis.unreachable_nonterminals_count ?? '-'"
        />
        <MetricCard title="未使用终结符" :value="data.analysis.unused_terminals_count ?? '-'" />
      </section>

      <section class="panel">
        <header class="panel-header">
          <h3>符号模型清单</h3>
          <div class="toolbar">
            <button class="btn" :class="{ active: tab === 'all' }" type="button" @click="setTab('all')">
              全部
            </button>
            <button
              class="btn"
              :class="{ active: tab === 'terminal' }"
              type="button"
              @click="setTab('terminal')"
            >
              终结符
            </button>
            <button
              class="btn"
              :class="{ active: tab === 'nonterminal' }"
              type="button"
              @click="setTab('nonterminal')"
            >
              非终结符
            </button>
          </div>
        </header>
        <div class="table-wrap">
          <table class="table">
            <thead>
              <tr>
                <th>id</th>
                <th>name</th>
                <th>kind</th>
              </tr>
            </thead>
            <tbody>
              <tr v-for="row in pagedSymbols" :key="row.id">
                <td>{{ row.id }}</td>
                <td>{{ row.name }}</td>
                <td>{{ row.kind }}</td>
              </tr>
            </tbody>
          </table>
        </div>
        <div class="panel-body toolbar">
          <button class="btn" type="button" @click="prevSymPage" :disabled="symPage <= 1">上一页</button>
          <span>第 {{ symPage }} / {{ symTotalPages }} 页</span>
          <button class="btn" type="button" @click="nextSymPage" :disabled="symPage >= symTotalPages">
            下一页
          </button>
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

type SymbolTab = "all" | "terminal" | "nonterminal";

const SYM_PAGE_SIZE = 24;
const store = useYaccStore();
const data = computed(() => store.stepData[2]);
const grammar = computed(() => data.value?.grammar_model ?? {
  start_symbol: "",
  terminals: [],
  nonterminals: [],
  productions_count: 0
});
const tab = ref<SymbolTab>("all");
const symPage = ref(1);

const symbolsByTab = computed(() => {
  const all = [...grammar.value.terminals, ...grammar.value.nonterminals];
  if (tab.value === "terminal") {
    return grammar.value.terminals;
  }
  if (tab.value === "nonterminal") {
    return grammar.value.nonterminals;
  }
  return all;
});

const symTotalPages = computed(() => Math.max(1, Math.ceil(symbolsByTab.value.length / SYM_PAGE_SIZE)));
const pagedSymbols = computed(() => {
  const start = (symPage.value - 1) * SYM_PAGE_SIZE;
  return symbolsByTab.value.slice(start, start + SYM_PAGE_SIZE);
});

watch([tab, symTotalPages], () => {
  if (symPage.value > symTotalPages.value) {
    symPage.value = symTotalPages.value;
  }
  if (symPage.value < 1) {
    symPage.value = 1;
  }
});

function setTab(next: SymbolTab) {
  tab.value = next;
  symPage.value = 1;
}

function prevSymPage() {
  symPage.value = Math.max(1, symPage.value - 1);
}

function nextSymPage() {
  symPage.value = Math.min(symTotalPages.value, symPage.value + 1);
}

function exportJson() {
  if (!data.value) {
    return;
  }
  downloadJsonFile("step2_data.json", data.value);
}

onMounted(async () => {
  await store.ensureStep(2);
});
</script>
