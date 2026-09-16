<!-- 通用 ECharts 容器：负责初始化、响应式更新、尺寸变化和销毁。 -->
<script setup lang="ts">
import * as echarts from 'echarts';
import type { EChartsOption } from 'echarts';
import { nextTick, onBeforeUnmount, onMounted, ref, watch } from 'vue';

const props = defineProps<{ option: EChartsOption }>();
const container = ref<HTMLDivElement | null>(null);
let chart: echarts.ECharts | null = null;
let observer: ResizeObserver | null = null;

const render = async () => {
  await nextTick();
  if (!container.value) return;
  chart ??= echarts.init(container.value);
  chart.setOption(props.option, { notMerge: true });
};

watch(() => props.option, render, { deep: true });
onMounted(() => {
  render();
  observer = new ResizeObserver(() => chart?.resize());
  if (container.value) observer.observe(container.value);
});
onBeforeUnmount(() => { observer?.disconnect(); chart?.dispose(); });
</script>

<template><div ref="container" class="chart-view" /></template>
