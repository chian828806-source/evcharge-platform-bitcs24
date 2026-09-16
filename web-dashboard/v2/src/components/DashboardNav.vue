<script setup lang="ts">
export type DashboardSection = 'overview' | 'trends' | 'stations' | 'prediction' | 'quality';

defineProps<{ active: DashboardSection }>();
const emit = defineEmits<{ select: [section: DashboardSection] }>();

const items: Array<{ id: DashboardSection; label: string; path: string }> = [
  { id: 'overview', label: '运营总览', path: 'M4 4h6v6H4V4Zm10 0h6v6h-6V4ZM4 14h6v6H4v-6Zm10 0h6v6h-6v-6Z' },
  { id: 'trends', label: '趋势分析', path: 'M3 17l5-5 4 3 7-8m0 0h-5m5 0v5' },
  { id: 'stations', label: '站点分析', path: 'M12 21s7-5.2 7-12A7 7 0 1 0 5 9c0 6.8 7 12 7 12Zm0-9a3 3 0 1 0 0-6 3 3 0 0 0 0 6Z' },
  { id: 'prediction', label: '智能预测', path: 'M13 2 4.5 13h6L9 22l10-12h-6l0-8Z' },
  { id: 'quality', label: '数据质量', path: 'm4 12 5 5L20 6' }
];
</script>

<template>
  <aside class="side-rail" aria-label="大屏功能导航">
    <button class="rail-logo" type="button" title="返回运营总览" @click="emit('select', 'overview')">
      <svg viewBox="0 0 24 24" aria-hidden="true"><path d="M13 2 4.5 13h6L9 22l10-12h-6l0-8Z" /></svg>
    </button>
    <nav>
      <button
        v-for="item in items"
        :key="item.id"
        type="button"
        :class="['rail-item', { active: active === item.id }]"
        :title="item.label"
        :aria-label="item.label"
        :aria-current="active === item.id ? 'page' : undefined"
        @click="emit('select', item.id)"
      >
        <svg viewBox="0 0 24 24" aria-hidden="true"><path :d="item.path" /></svg>
        <span>{{ item.label }}</span>
      </button>
    </nav>
    <div class="rail-version">V2</div>
  </aside>
</template>
