<template>
  <aside class="sidebar">
    <div class="brand">
      <p class="brand-title">编译流程可视化工作台</p>
      <p class="brand-sub">Lex / YACC / Jimple · Sidecar 架构</p>
    </div>
    <nav class="nav-list">
      <p class="nav-group-title">总览与扩展</p>
      <RouterLink class="nav-item" :to="linkFor('/overview')">总览</RouterLink>
      <RouterLink class="nav-item" :to="linkFor('/lex')">Lex 输入</RouterLink>
      <RouterLink class="nav-item" :to="linkFor('/jimple')">中间代码</RouterLink>
      <div class="nav-divider"></div>
      <p class="nav-group-title">YACC Step Flow</p>
      <RouterLink class="nav-item" :to="linkFor('/step1')">Step 1</RouterLink>
      <RouterLink class="nav-item" :to="linkFor('/step2')">Step 2</RouterLink>
      <RouterLink class="nav-item" :to="linkFor('/step3')">Step 3</RouterLink>
      <RouterLink class="nav-item" :to="linkFor('/step4')">Step 4</RouterLink>
      <RouterLink class="nav-item" :to="linkFor('/step5')">Step 5</RouterLink>
      <RouterLink class="nav-item" :to="linkFor('/step6')">Step 6</RouterLink>
      <RouterLink class="nav-item" :to="linkFor('/step7')">Step 7</RouterLink>
      <RouterLink class="nav-item" :to="linkFor('/step8')">Step 8</RouterLink>
      <RouterLink class="nav-item" :to="linkFor('/step9')">Step 9</RouterLink>
      <RouterLink class="nav-item" :to="linkFor('/step10')">Step 10</RouterLink>
    </nav>
  </aside>
</template>

<script setup lang="ts">
import { useRoute } from "vue-router";
import { useYaccStore } from "../stores/yacc";

const route = useRoute();
const store = useYaccStore();

function linkFor(path: string): { path: string; query: Record<string, string> } {
  const query = { ...route.query };
  if (store.currentCaseId) {
    query.case = store.currentCaseId;
  }
  return { path, query: query as Record<string, string> };
}
</script>
