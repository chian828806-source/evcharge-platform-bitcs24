<!-- 第二阶段数据大屏：只消费 Dashboard Core Store，不直接耦合 Flask。 -->
<script setup lang="ts">
import type { EChartsOption } from 'echarts';
import { computed, onBeforeUnmount, onMounted, ref } from 'vue';
import EChartView from './components/EChartView.vue';
import { useDashboardStore } from './core/stores/dashboard';

const store = useDashboardStore();
const refreshing = ref(false);
const now = ref(new Date());
let timer: number | undefined;
const palette = ['#23d9b7', '#39a8ff', '#ffc857', '#ff718d', '#8c7cff'];
const axis = { axisLine: { lineStyle: { color: '#365b78' } }, axisLabel: { color: '#a9c7dc' }, splitLine: { lineStyle: { color: '#17364d' } } };
const tooltip = { trigger: 'axis' as const, backgroundColor: '#09283d', borderColor: '#28d7bf', textStyle: { color: '#e9f9ff' } };
const money = (fen = 0) => `¥${(fen / 100).toLocaleString('zh-CN', { maximumFractionDigits: 0 })}`;
const percent = (value = 0) => `${(value * 100).toFixed(1)}%`;

async function refresh() {
  if (refreshing.value) return;
  refreshing.value = true;
  try { await store.refreshAll(); } finally { refreshing.value = false; now.value = new Date(); }
}

const energyOption = computed<EChartsOption>(() => ({
  tooltip, color: palette, grid: { left: 48, right: 24, top: 35, bottom: 36 },
  legend: { data: ['充电量', '会话'], textStyle: { color: '#b9d5e7' } },
  xAxis: { type: 'category', data: store.energyTrend.data?.map(row => row.date.slice(5)) ?? [], ...axis },
  yAxis: [{ type: 'value', name: 'kWh', ...axis }, { type: 'value', name: '次', ...axis }],
  series: [{ name: '充电量', type: 'line', smooth: true, areaStyle: { opacity: .15 }, data: store.energyTrend.data?.map(row => row.energyKwh) ?? [] },
    { name: '会话', type: 'bar', yAxisIndex: 1, barMaxWidth: 18, data: store.energyTrend.data?.map(row => row.orderCount) ?? [] }]
}));
const revenueOption = computed<EChartsOption>(() => ({
  tooltip, color: [palette[2]], grid: { left: 55, right: 20, top: 20, bottom: 36 },
  xAxis: { type: 'category', data: store.revenueTrend.data?.map(row => row.date.slice(5)) ?? [], ...axis }, yAxis: { type: 'value', ...axis },
  series: [{ type: 'line', smooth: true, symbolSize: 7, areaStyle: { opacity: .18 }, data: store.revenueTrend.data?.map(row => row.revenueFen / 100) ?? [] }]
}));
const pileOption = computed<EChartsOption>(() => ({
  color: palette, tooltip: { trigger: 'item' }, legend: { bottom: 0, textStyle: { color: '#b9d5e7' } },
  series: [{ type: 'pie', radius: ['48%', '72%'], center: ['50%', '44%'], label: { color: '#d9f4ff', formatter: '{b}\n{c}' },
    data: store.pileStatus.data?.map(row => ({ name: row.status === 'CHARGING' ? '充电中' : row.status === 'AVAILABLE' ? '空闲' : row.status, value: row.count })) ?? [] }]
}));
const rankingOption = computed<EChartsOption>(() => {
  const rows = [...(store.stationRanking.data ?? [])].slice(0, 8).reverse();
  return { tooltip, color: [palette[0]], grid: { left: 110, right: 25, top: 15, bottom: 30 }, xAxis: { type: 'value', ...axis },
    yAxis: { type: 'category', ...axis, data: rows.map(row => row.stationName.replace('UrbanEV区域 ', '区域 ')) },
    series: [{ type: 'bar', data: rows.map(row => row.energyKwh), barMaxWidth: 16 }] };
});
const utilizationOption = computed<EChartsOption>(() => ({
  tooltip, color: [palette[1]], grid: { left: 45, right: 18, top: 25, bottom: 55 },
  xAxis: { type: 'category', ...axis, axisLabel: { color: '#a9c7dc', rotate: 30 }, data: store.stationUtilization.data?.map(row => row.stationName.replace('UrbanEV区域 ', '区域 ')) ?? [] },
  yAxis: { type: 'value', min: 0, max: 1, ...axis, axisLabel: { color: '#a9c7dc', formatter: (value: number) => `${value * 100}%` } },
  series: [{ type: 'bar', barMaxWidth: 24, data: store.stationUtilization.data?.map(row => row.utilizationRate) ?? [] }]
}));
const heatmapOption = computed<EChartsOption>(() => ({
  tooltip: { position: 'top' }, grid: { left: 55, right: 25, top: 20, bottom: 48 },
  xAxis: { type: 'category', data: Array.from({ length: 24 }, (_, i) => `${i}时`), ...axis },
  yAxis: { type: 'category', data: ['周日', '周一', '周二', '周三', '周四', '周五', '周六'], ...axis },
  visualMap: { min: 0, max: 1, calculable: false, orient: 'horizontal', left: 'center', bottom: 0, textStyle: { color: '#b9d5e7' }, inRange: { color: ['#09283d', '#168e9d', '#3df2be'] } },
  series: [{ type: 'heatmap', data: store.hourlyHeatmap.data?.map(row => [row.hour, row.dayOfWeek - 1, row.utilizationRate]) ?? [] }]
}));
const predictionOption = computed<EChartsOption>(() => {
  const rows = (store.prediction.data ?? []).slice(0, 24);
  return { tooltip, color: [palette[4]], grid: { left: 45, right: 20, top: 20, bottom: 58 },
    xAxis: { type: 'category', ...axis, axisLabel: { color: '#a9c7dc', rotate: 35 }, data: rows.map(row => `${row.stationId}-${row.horizon}`) },
    yAxis: { type: 'value', min: 0, max: 1, ...axis }, series: [{ type: 'line', smooth: true, data: rows.map(row => row.predictedLoad) }] };
});
const qualityOption = computed<EChartsOption>(() => ({
  color: [palette[0], palette[3]], tooltip: { trigger: 'item' }, legend: { bottom: 0, textStyle: { color: '#b9d5e7' } },
  series: [{ type: 'pie', radius: ['46%', '70%'], label: { color: '#d9f4ff' }, data: [
    { name: '通过', value: store.dataQuality.data?.acceptedRows ?? 0 }, { name: '拒绝', value: store.dataQuality.data?.rejectedRows ?? 0 }
  ] }]
}));

onMounted(async () => { await refresh(); store.startRealtime(); timer = window.setInterval(refresh, 15_000); });
onBeforeUnmount(() => { if (timer) clearInterval(timer); store.stopRealtime(); });
</script>

<template>
  <main class="dashboard-shell">
    <header class="topbar">
      <div><span class="eyebrow">EVCHARGE · BIG DATA</span><h1>电动汽车充电运营分析平台</h1></div>
      <div class="header-status"><span :class="['status-dot', store.overview.status]" />{{ store.overview.status === 'success' ? '数据链路正常' : store.overview.status === 'error' ? `接口异常：${store.overview.error?.message}` : '数据加载中' }}<button @click="refresh">{{ refreshing ? '刷新中' : '立即刷新' }}</button><time>{{ now.toLocaleString('zh-CN') }}</time></div>
    </header>
    <div class="notice">端到端模拟批次 · 数据已依次经过 HDFS ODS、质量治理、DWD、DWS 与 Spark MLlib</div>
    <section class="kpis">
      <dv-border-box-8><div class="kpi"><span>累计充电会话</span><strong>{{ store.overview.data?.orderCount?.toLocaleString() ?? '--' }}</strong><small>SESSION STARTS</small></div></dv-border-box-8>
      <dv-border-box-8><div class="kpi"><span>估算充电量</span><strong>{{ store.overview.data?.energyKwh?.toLocaleString(undefined, { maximumFractionDigits: 0 }) ?? '--' }} <i>kWh</i></strong><small>ENERGY</small></div></dv-border-box-8>
      <dv-border-box-8><div class="kpi"><span>估算营收</span><strong>{{ money(store.overview.data?.revenueFen) }}</strong><small>REVENUE</small></div></dv-border-box-8>
      <dv-border-box-8><div class="kpi"><span>在线桩位</span><strong>{{ store.overview.data?.onlinePileCount ?? '--' }}</strong><small>ONLINE PILES</small></div></dv-border-box-8>
      <dv-border-box-8><div class="kpi"><span>平均利用率</span><strong>{{ percent(store.overview.data?.utilizationRate) }}</strong><small>UTILIZATION</small></div></dv-border-box-8>
    </section>
    <section class="grid">
      <article class="panel wide"><h2>充电量与会话趋势</h2><EChartView :option="energyOption" /></article>
      <article class="panel"><h2>实时桩位状态</h2><EChartView :option="pileOption" /></article>
      <article class="panel"><h2>营收趋势（估算）</h2><EChartView :option="revenueOption" /></article>
      <article class="panel"><h2>区域充电量排行</h2><EChartView :option="rankingOption" /></article>
      <article class="panel wide"><h2>星期 × 小时利用率热力图</h2><EChartView :option="heatmapOption" /></article>
      <article class="panel"><h2>区域利用率</h2><EChartView :option="utilizationOption" /></article>
      <article class="panel wide"><h2>MLlib 1h / 6h / 24h 负荷预测</h2><EChartView :option="predictionOption" /></article>
      <article class="panel"><h2>数据质量</h2><EChartView :option="qualityOption" /><p class="quality">源数据 {{ store.dataQuality.data?.sourceRows ?? '--' }} · 拒绝 {{ store.dataQuality.data?.rejectedRows ?? '--' }}</p></article>
    </section>
  </main>
</template>
