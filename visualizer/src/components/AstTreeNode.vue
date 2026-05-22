<template>
  <li class="ast-node">
    <div
      class="ast-node-header"
      :class="{ selected: selectedId === node.id, leaf: isLeaf }"
      @click="$emit('select', node.id)"
    >
      <span class="node-type">{{ node.type }}</span>
      <span class="node-id">#{{ node.id }}</span>
      <span v-if="node.lexeme" class="node-lexeme">"{{ node.lexeme }}"</span>
    </div>
    <ul v-if="!isLeaf" class="ast-children">
      <AstTreeNode
        v-for="cid in node.children"
        :key="cid"
        :node-id="cid"
        :node-map="nodeMap"
        :selected-id="selectedId"
        @select="$emit('select', $event)"
      />
    </ul>
  </li>
</template>

<script setup lang="ts">
import { computed } from "vue";

const props = defineProps<{
  nodeId: number;
  nodeMap: Map<number, any>;
  selectedId: number;
}>();

defineEmits<{
  (e: "select", nodeId: number): void;
}>();

const node = computed(() => props.nodeMap.get(props.nodeId) ?? {
  id: props.nodeId,
  type: `missing#${props.nodeId}`,
  lexeme: "",
  children: []
});

const isLeaf = computed(() => !Array.isArray(node.value.children) || node.value.children.length === 0);
</script>

<style scoped>
.ast-node {
  position: relative;
  margin: 3px 0;
}

.ast-node::before {
  content: "";
  position: absolute;
  left: -10px;
  top: -6px;
  bottom: -6px;
  width: 1px;
  background: #c9d8ea;
}

.ast-node::after {
  content: "";
  position: absolute;
  left: -10px;
  top: 16px;
  width: 10px;
  height: 1px;
  background: #c9d8ea;
}

.ast-children > .ast-node:last-child::before {
  bottom: calc(100% - 16px);
}

.ast-node-header {
  font-family: ui-monospace, SFMono-Regular, Menlo, Monaco, Consolas, monospace;
  font-size: 12px;
  padding: 5px 8px;
  border-radius: 8px;
  cursor: pointer;
  display: inline-flex;
  align-items: center;
  gap: 6px;
  border: 1px solid #d5e2f0;
  background: #fff;
  line-height: 1.25;
}

.ast-node-header:hover {
  background: #f1f6fc;
  border-color: #b7cbe4;
}

.ast-node-header.selected {
  background: #0f766e;
  color: #fff;
  border-color: #0f766e;
  box-shadow: 0 0 0 2px rgba(15, 118, 110, 0.12);
}

.ast-node-header.leaf {
  border-style: dashed;
  background: #f9fbff;
}

.node-type {
  font-weight: 700;
}

.node-id {
  color: #64748b;
}

.ast-node-header.selected .node-id {
  color: #cffaf3;
}

.node-lexeme {
  color: #1d4ed8;
  background: #e9f0ff;
  border: 1px solid #d5e3ff;
  border-radius: 999px;
  padding: 1px 6px;
}

.ast-node-header.selected .node-lexeme {
  color: #fff;
  background: rgba(255, 255, 255, 0.18);
  border-color: rgba(255, 255, 255, 0.35);
}

.ast-children {
  list-style: none;
  margin: 0;
  padding-left: 18px;
}
</style>
