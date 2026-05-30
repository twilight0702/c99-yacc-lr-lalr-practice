<template>
  <div class="app-shell">
    <AppSidebar />
    <main class="content">
      <header class="topbar">
        <CasePicker
          v-model="selectedCaseId"
          :cases="store.cases"
          :latest-case-id="store.latestCaseId"
          @refresh="reloadCases"
        />
      </header>
      <div v-if="store.casesError" class="error-box topbar-error">{{ store.casesError }}</div>
      <RouterView :key="viewKey" />
    </main>
  </div>
</template>

<script setup lang="ts">
import { computed, onMounted, watch } from "vue";
import { useRoute, useRouter } from "vue-router";
import AppSidebar from "./components/AppSidebar.vue";
import CasePicker from "./components/CasePicker.vue";
import { useYaccStore } from "./stores/yacc";
import { resolveCaseIdFromQuery } from "./utils/data";

const route = useRoute();
const router = useRouter();
const store = useYaccStore();

const selectedCaseId = computed({
  get: () => store.currentCaseId,
  set: async (value: string) => {
    if (!value || value === store.currentCaseId) {
      return;
    }
    await router.replace({
      path: route.path,
      query: {
        ...route.query,
        case: value,
      },
    });
  },
});

const viewKey = computed(() => `${store.currentCaseId || "default"}:${route.path}`);

async function syncCurrentCase(): Promise<void> {
  const queryCaseId = typeof route.query.case === "string" ? route.query.case.trim() : "";
  await store.ensureCurrentCase(queryCaseId || resolveCaseIdFromQuery() || undefined);
  if (store.currentCaseId && route.query.case !== store.currentCaseId) {
    await router.replace({
      path: route.path,
      query: {
        ...route.query,
        case: store.currentCaseId,
      },
    });
  }
}

async function reloadCases(): Promise<void> {
  store.invalidateCases();
  await syncCurrentCase();
}

onMounted(async () => {
  await syncCurrentCase();
});

watch(
  () => route.query.case,
  async () => {
    await syncCurrentCase();
  }
);
</script>
