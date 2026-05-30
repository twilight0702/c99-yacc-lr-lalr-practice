<template>
  <div class="case-picker-wrapper">
    <span class="case-picker-caption">当前数据集</span>
    <details ref="detailsEl" class="case-picker">
      <summary class="case-picker-trigger">
        <strong class="case-picker-title">{{ currentCase?.case_id || modelValue || "未选择" }}</strong>
        <span class="case-picker-meta">
          {{ currentCase ? `${fmtTime(currentCase.generated_at)} · ${basename(currentCase.source)}` : "选择要展示的 case" }}
        </span>
      </summary>

      <div class="case-picker-panel">
        <div class="case-picker-toolbar">
          <input v-model.trim="keyword" class="case-picker-search" type="search" placeholder="搜索 case / source" />
          <button class="btn" type="button" @click.stop="$emit('refresh')">刷新列表</button>
        </div>

        <div class="case-picker-stats">
          <span>共 {{ cases.length }} 个 case</span>
          <button
            v-if="latestCaseId && latestCaseId !== modelValue"
            class="btn"
            type="button"
            @click.stop="selectCase(latestCaseId)"
          >
            切到最新
          </button>
        </div>

        <div v-if="filteredCases.length" class="case-picker-list">
          <button
            v-for="item in filteredCases"
            :key="item.case_id"
            class="case-option"
            :class="{ active: item.case_id === modelValue }"
            type="button"
            @click="selectCase(item.case_id)"
          >
            <div class="case-option-head">
              <strong>{{ item.case_id }}</strong>
              <span v-if="item.case_id === latestCaseId" class="case-badge">最新</span>
            </div>
            <p class="case-option-meta">{{ fmtTime(item.generated_at) }} · {{ basename(item.source) }} · {{ item.step_count }} steps</p>
          </button>
        </div>

        <p v-else class="case-picker-empty">没有匹配的 case。</p>
      </div>
    </details>
  </div>
</template>

<script setup lang="ts">
import { computed, ref } from "vue";
import type { CaseIndexItem } from "../types";

const props = defineProps<{
  modelValue: string;
  latestCaseId: string;
  cases: CaseIndexItem[];
}>();

const emit = defineEmits<{
  "update:modelValue": [value: string];
  refresh: [];
}>();

const detailsEl = ref<HTMLDetailsElement | null>(null);
const keyword = ref("");

const filteredCases = computed(() => {
  const needle = keyword.value.trim().toLowerCase();
  if (!needle) {
    return props.cases;
  }
  return props.cases.filter((item) => item.case_id.toLowerCase().includes(needle) || item.source.toLowerCase().includes(needle));
});

const currentCase = computed(() => props.cases.find((item) => item.case_id === props.modelValue) ?? null);

function basename(value: string): string {
  if (!value) {
    return "-";
  }
  const normalized = value.replace(/\\/g, "/");
  return normalized.split("/").pop() || normalized;
}

function fmtTime(iso: string): string {
  if (!iso) {
    return "-";
  }
  const date = new Date(iso);
  if (Number.isNaN(date.getTime())) {
    return iso;
  }
  return date.toLocaleString("zh-CN");
}

function selectCase(caseId: string): void {
  emit("update:modelValue", caseId);
  if (detailsEl.value) {
    detailsEl.value.open = false;
  }
}
</script>
