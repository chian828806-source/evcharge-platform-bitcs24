<!-- Dashboard UI：只消费 Dashboard Core Store，不直接请求 Flask 或重算业务 KPI。 -->
<script setup lang="ts">
import type { EChartsOption } from 'echarts';
import { computed, onBeforeUnmount, onMounted, ref } from 'vue';
import DashboardNav, { type DashboardSection } from './components/DashboardNav.vue';
import EChartView from './components/EChartView.vue';
import type { Resource, ResourceStatus } from './core/models/dashboard';
import { useDashboardStore } from './core/stores/dashboard';

const store = useDashboardStore();
const activeSection = ref<DashboardSection>('overview');
const refreshing = ref(false);
const now = ref(new Date());
const searchText = ref('');
let timer: number | undefined;

const sectionMeta: Record<DashboardSection, { eyebrow: string; title: string; description: string }> = {
  overview: { eyebrow: 'EVCHARGE ANALYTICS', title: '运营数据总览', description: '核心经营指标与设备运行态势' },
  trends: { eyebrow: 'ENERGY & REVENUE', title: '趋势分析', description: '充电电量、订单与营收的周期变化' },
  stations: { eyebrow: 'STATION INSIGHTS', title: '站点分析', description: '站点贡献、利用率与资源分布' },
  prediction: { eyebrow: 'ML FORECAST', title: '智能负载预测', description: '1h / 6h / 24h 负荷与可用资源预测' },
  quality: { eyebrow: 'DATA GOVERNANCE', title: '数据质量', description: '清洗结果、异常规则与数据可信度' }
};
const colors = ['#3157d5', '#6b87e8', '#93acf2', '#c2d0f8', '#e7ecfb', '#f2a766', '#dc6675'];
const axis = { axisLine: { lineStyle: { color: '#e5eaf2' } }, axisTick: { show: false }, axisLabel: { color: '#7e8799', fontSize: 11 }, splitLine: { lineStyle: { color: '#edf1f6', type: 'dashed' as const } } };
const tooltip = { trigger: 'axis' as const, backgroundColor: '#20283b', borderWidth: 0, textStyle: { color: '#fff' } };
const finite = (value: unknown): value is number => typeof value === 'number' && Number.isFinite(value);
const integer = (value: unknown) => finite(value) ? Math.max(0, Math.round(value)).toLocaleString('zh-CN') : '--';
const decimal = (value: unknown, digits = 1) => finite(value) ? Math.max(0, value).toLocaleString('zh-CN', { minimumFractionDigits: digits, maximumFractionDigits: digits }) : '--';
const money = (fen: unknown) => finite(fen) ? '¥' + Math.max(0, fen / 100).toLocaleString('zh-CN', { minimumFractionDigits: 2, maximumFractionDigits: 2 }) : '--';
const percent = (value: unknown) => finite(value) ? (Math.min(1, Math.max(0, value)) * 100).toFixed(1) + '%' : '--';
const progressWidth = (value: unknown) => finite(value) ? Math.min(100, Math.max(0, value * 100)) + '%' : '0%';
const shortDate = (value: string) => value?.length > 5 ? value.slice(5) : value || '--';
const statusText: Record<ResourceStatus, string> = { idle: '等待加载', loading: '数据加载中', success: '数据正常', empty: '暂无数据', error: '加载失败' };
const stateClass = (resource: Resource<unknown>) => 'resource-state resource-state--' + resource.status;
const pileName: Record<string, string> = { AVAILABLE: '空闲', CHARGING: '充电中', FAULT: '故障', OFFLINE: '离线', RESERVED: '已预约', RESTARTING: '重启中' };
const peakName: Record<string, string> = { LOW: '低负荷', MEDIUM: '中负荷', HIGH: '高负荷' };
const totalPiles = computed(() => (store.pileStatus.data ?? []).reduce((sum, item) => sum + (finite(item.count) ? item.count : 0), 0));
const qualityRate = computed(() => {
  const source = store.dataQuality.data?.sourceRows;
  const accepted = store.dataQuality.data?.acceptedRows;
  return finite(source) && source > 0 && finite(accepted) ? accepted / source : null;
});
const filteredStations = computed(() => {
  const keyword = searchText.value.trim().toLowerCase();
  const rows = store.stationUtilization.data ?? [];
  return keyword ? rows.filter(row => (row.stationName + ' ' + (row.date ?? '')).toLowerCase().includes(keyword)) : rows;
});

async function refresh() {
  if (refreshing.value) return;
  refreshing.value = true;
  try { await store.refreshAll(); } finally { refreshing.value = false; now.value = new Date(); }
}

const operationOption = computed<EChartsOption>(() => ({
  tooltip, color: [colors[0], colors[5]], legend: { top: 2, left: 'center', itemGap: 22, itemWidth: 12, itemHeight: 8, textStyle: { color: '#667085', fontSize: 11 }, data: ['充电量', '订单量'] },
  grid: { left: 58, right: 58, top: 58, bottom: 36 }, xAxis: { type: 'category', boundaryGap: false, data: store.energyTrend.data?.map(row => shortDate(row.date)) ?? [], ...axis },
  yAxis: [{ type: 'value', name: 'kWh', nameGap: 14, nameTextStyle: { color: '#98a1b2', padding: [0, 0, 0, 6] }, ...axis }, { type: 'value', name: '订单数', nameGap: 14, nameTextStyle: { color: '#98a1b2', padding: [0, 6, 0, 0] }, ...axis }],
  series: [{ name: '充电量', type: 'line', smooth: true, symbol: 'circle', symbolSize: 7, lineStyle: { width: 3 }, areaStyle: { color: 'rgba(49,87,213,.11)' }, data: store.energyTrend.data?.map(row => row.energyKwh) ?? [] }, { name: '订单量', type: 'line', smooth: true, yAxisIndex: 1, symbol: 'none', lineStyle: { width: 2, type: 'dashed' }, data: store.energyTrend.data?.map(row => row.orderCount) ?? [] }]
}));
const revenueOption = computed<EChartsOption>(() => ({
  tooltip, color: [colors[0]], grid: { left: 56, right: 22, top: 24, bottom: 36 }, xAxis: { type: 'category', data: store.revenueTrend.data?.map(row => shortDate(row.date)) ?? [], ...axis },
  yAxis: { type: 'value', name: '元', nameTextStyle: { color: '#98a1b2' }, ...axis }, series: [{ name: '营收', type: 'bar', barMaxWidth: 26, data: store.revenueTrend.data?.map(row => finite(row.revenueFen) ? row.revenueFen / 100 : 0) ?? [], itemStyle: { borderRadius: [7, 7, 0, 0], color: colors[0] } }]
}));
const pileOption = computed<EChartsOption>(() => ({
  color: colors, tooltip: { trigger: 'item', formatter: '{b}<br/>{c} 台 · {d}%' }, legend: { orient: 'vertical', right: 6, top: 'middle', itemWidth: 10, itemHeight: 10, textStyle: { color: '#667085', fontSize: 11 } },
  series: [{ type: 'pie', radius: ['55%', '76%'], center: ['37%', '49%'], padAngle: 3, itemStyle: { borderRadius: 7 }, label: { show: false }, data: store.pileStatus.data?.map(row => ({ name: pileName[row.status] ?? row.status, value: row.count })) ?? [] }]
}));
const rankingOption = computed<EChartsOption>(() => {
  const rows = [...(store.stationRanking.data ?? [])].sort((a, b) => a.rank - b.rank).slice(0, 8).reverse();
  return { tooltip, grid: { left: 132, right: 32, top: 14, bottom: 28 }, xAxis: { type: 'value', name: 'kWh', ...axis }, yAxis: { type: 'category', data: rows.map(row => row.stationName), ...axis, axisLabel: { color: '#667085', width: 116, overflow: 'truncate' } }, series: [{ name: '充电量', type: 'bar', barWidth: 13, data: rows.map(row => row.energyKwh), itemStyle: { color: colors[0], borderRadius: 7 } }] };
});
const heatmapOption = computed<EChartsOption>(() => ({
  tooltip: { position: 'top' }, grid: { left: 56, right: 22, top: 20, bottom: 58 }, xAxis: { type: 'category', data: Array.from({ length: 24 }, (_, i) => String(i)), ...axis },
  yAxis: { type: 'category', data: ['周一', '周二', '周三', '周四', '周五', '周六', '周日'], ...axis }, visualMap: { min: 0, max: 1, orient: 'horizontal', left: 'center', bottom: 4, itemWidth: 12, itemHeight: 110, text: ['高', '低'], textStyle: { color: '#7e8799', fontSize: 10 }, inRange: { color: ['#edf1ff', '#9eb4f3', '#3157d5'] } },
  series: [{ type: 'heatmap', data: store.hourlyHeatmap.data?.map(row => [Math.min(23, Math.max(0, row.hour)), Math.min(6, Math.max(0, row.dayOfWeek - 1)), Math.min(1, Math.max(0, row.utilizationRate))]) ?? [], itemStyle: { borderColor: '#fff', borderWidth: 2, borderRadius: 3 } }]
}));
const predictionOption = computed<EChartsOption>(() => {
  const rows = (store.prediction.data ?? []).slice(0, 24);
  return { tooltip, color: [colors[0]], grid: { left: 54, right: 24, top: 28, bottom: 64 }, xAxis: { type: 'category', data: rows.map(row => row.stationName + '\n' + row.horizon), ...axis, axisLabel: { color: '#7e8799', fontSize: 10, interval: 0, rotate: 18 } }, yAxis: { type: 'value', min: 0, max: 1, ...axis, axisLabel: { color: '#7e8799', formatter: (value: number) => Math.round(value * 100) + '%' } }, series: [{ name: '预测负荷', type: 'line', smooth: true, symbolSize: 9, lineStyle: { width: 3 }, areaStyle: { color: 'rgba(49,87,213,.10)' }, markLine: { silent: true, data: [{ yAxis: .8, name: '高峰线' }], lineStyle: { color: colors[6], type: 'dashed' } }, data: rows.map(row => ({ value: row.predictedLoad, itemStyle: { color: row.peakLevel === 'HIGH' ? colors[6] : row.peakLevel === 'MEDIUM' ? colors[5] : colors[0] } })) }] };
});
const qualityOption = computed<EChartsOption>(() => ({ color: [colors[0], colors[6]], tooltip: { trigger: 'item' }, series: [{ type: 'pie', radius: ['62%', '79%'], center: ['50%', '48%'], label: { show: false }, data: [{ name: '通过', value: store.dataQuality.data?.acceptedRows ?? 0 }, { name: '拒绝', value: store.dataQuality.data?.rejectedRows ?? 0 }] }] }));

onMounted(async () => { await refresh(); store.startRealtime(); timer = window.setInterval(refresh, 30_000); });
onBeforeUnmount(() => { if (timer) window.clearInterval(timer); store.stopRealtime(); });
</script>

<template>
  <div class="app-frame">
    <DashboardNav :active="activeSection" @select="activeSection = $event" />
    <main class="workspace">
      <header class="workspace-header">
        <div class="page-heading"><p class="overline">{{ sectionMeta[activeSection].eyebrow }}</p><h1>{{ sectionMeta[activeSection].title }}</h1><p>{{ sectionMeta[activeSection].description }}</p></div>
        <label class="search-box"><svg viewBox="0 0 24 24"><circle cx="11" cy="11" r="7" /><path d="m20 20-4-4" /></svg><input v-model="searchText" aria-label="搜索站点" placeholder="搜索站点名称或日期" /></label>
        <div class="header-tools"><span :class="['connection-pill', store.overview.status]"><i />{{ store.dataMode === 'mock' ? 'Mock 数据' : statusText[store.overview.status] }}</span><button class="icon-button" type="button" title="刷新全部数据" :disabled="refreshing" @click="refresh"><svg viewBox="0 0 24 24"><path d="M20 11a8 8 0 1 0-2.34 5.66M20 4v7h-7" /></svg></button><div class="profile"><span>EV</span><div><strong>运营中心</strong><small>{{ now.toLocaleDateString('zh-CN') }}</small></div></div></div>
      </header>

      <section v-if="activeSection === 'overview'" class="page-view">
        <div class="summary-grid">
          <article class="metric-card metric-card--primary"><div class="metric-icon">01</div><span>累计充电会话</span><strong>{{ integer(store.overview.data?.orderCount) }}</strong><small>统计周期订单总量</small></article>
          <article class="metric-card"><div class="metric-icon">02</div><span>充电电量</span><strong>{{ decimal(store.overview.data?.energyKwh) }} <i>kWh</i></strong><small>统计周期总电量</small></article>
          <article class="metric-card"><div class="metric-icon">03</div><span>运营收入</span><strong>{{ money(store.overview.data?.revenueFen) }}</strong><small>金额按接口分值转换</small></article>
          <article class="metric-card"><div class="metric-icon">04</div><span>在线电桩</span><strong>{{ integer(store.overview.data?.onlinePileCount) }} <i>台</i></strong><small>当前在线设备</small></article>
          <article class="metric-card"><div class="metric-icon">05</div><span>平均利用率</span><strong>{{ percent(store.overview.data?.utilizationRate) }}</strong><small>统一利用率口径</small></article>
        </div>
        <div class="overview-grid">
          <article class="card span-2"><header class="card-header"><div><h2>充电运营趋势</h2><p>充电量与订单量变化</p></div><span class="period-chip">近 30 日</span></header><div v-if="store.energyTrend.status === 'success'" class="chart-large"><EChartView :option="operationOption" /></div><div v-else :class="stateClass(store.energyTrend)"><span>{{ statusText[store.energyTrend.status] }}</span></div></article>
          <article class="card"><header class="card-header"><div><h2>电桩状态</h2><p>设备运行结构</p></div></header><div v-if="store.pileStatus.status === 'success'" class="donut-wrap"><EChartView :option="pileOption" /><div class="donut-label"><strong>{{ integer(totalPiles) }}</strong><span>电桩总数</span></div></div><div v-else :class="stateClass(store.pileStatus)"><span>{{ statusText[store.pileStatus.status] }}</span></div></article>
          <article class="prediction-hero"><dv-border-box-8 :dur="5"><div class="prediction-content"><span class="prediction-label">AI LOAD FORECAST</span><h2>下一时段负荷</h2><div class="prediction-number"><strong>{{ percent(store.prediction.data?.[0]?.predictedLoad) }}</strong><span>{{ store.prediction.data?.[0]?.stationName ?? '等待预测数据' }}</span></div><div class="prediction-meta"><span>{{ store.prediction.data?.[0]?.horizon ?? '--' }}</span><span>预计可用 {{ integer(store.prediction.data?.[0]?.predictedAvailableCount) }} 枪</span></div></div></dv-border-box-8></article>
          <article class="card"><header class="card-header"><div><h2>营收概览</h2><p>每日收入变化</p></div></header><div v-if="store.revenueTrend.status === 'success'" class="chart-compact"><EChartView :option="revenueOption" /></div><div v-else :class="stateClass(store.revenueTrend)"><span>{{ statusText[store.revenueTrend.status] }}</span></div></article>
          <article class="card"><header class="card-header"><div><h2>站点利用率</h2><p>高利用站点概览</p></div><button class="text-button" @click="activeSection = 'stations'">查看全部</button></header><div class="utilization-list compact"><div v-for="row in filteredStations.slice(0, 4)" :key="row.stationId + '-' + row.date" class="utilization-row"><div><strong>{{ row.stationName }}</strong><small>{{ integer(row.availableCount) }}/{{ integer(row.totalPileCount) }} 空闲</small></div><span class="progress"><i :style="{ width: progressWidth(row.utilizationRate) }" /></span><b>{{ percent(row.utilizationRate) }}</b></div></div></article>
        </div>
      </section>

      <section v-else-if="activeSection === 'trends'" class="page-view two-column">
        <article class="card wide-card"><header class="card-header"><div><h2>充电量与订单趋势</h2><p>两个维度的同期对比分析</p></div><span class="period-chip">Energy / Orders</span></header><div v-if="store.energyTrend.status === 'success'" class="chart-full"><EChartView :option="operationOption" /></div><div v-else :class="stateClass(store.energyTrend)"><span>{{ statusText[store.energyTrend.status] }}</span></div></article>
        <article class="card wide-card"><header class="card-header"><div><h2>营收趋势</h2><p>每日已完成订单营收</p></div><span class="period-chip">Revenue</span></header><div v-if="store.revenueTrend.status === 'success'" class="chart-full"><EChartView :option="revenueOption" /></div><div v-else :class="stateClass(store.revenueTrend)"><span>{{ statusText[store.revenueTrend.status] }}</span></div></article>
        <article class="card full-row"><header class="card-header"><div><h2>时段利用率热力图</h2><p>星期与小时两个维度交叉分析</p></div><span class="period-chip">7 × 24</span></header><div v-if="store.hourlyHeatmap.status === 'success'" class="chart-heatmap"><EChartView :option="heatmapOption" /></div><div v-else :class="stateClass(store.hourlyHeatmap)"><span>{{ statusText[store.hourlyHeatmap.status] }}</span></div></article>
      </section>

      <section v-else-if="activeSection === 'stations'" class="page-view two-column">
        <article class="card wide-card"><header class="card-header"><div><h2>站点充电量排行</h2><p>按充电贡献对比</p></div><span class="period-chip">TOP 8</span></header><div v-if="store.stationRanking.status === 'success'" class="chart-full"><EChartView :option="rankingOption" /></div><div v-else :class="stateClass(store.stationRanking)"><span>{{ statusText[store.stationRanking.status] }}</span></div></article>
        <article class="card wide-card"><header class="card-header"><div><h2>电桩状态分布</h2><p>空闲、充电、故障及离线</p></div><span class="period-chip">{{ integer(totalPiles) }} 台</span></header><div v-if="store.pileStatus.status === 'success'" class="chart-full pile-full"><EChartView :option="pileOption" /></div><div v-else :class="stateClass(store.pileStatus)"><span>{{ statusText[store.pileStatus.status] }}</span></div></article>
        <article class="card full-row"><header class="card-header"><div><h2>站点资源明细</h2><p>名称、日期、利用率与可用电桩</p></div><span class="period-chip">{{ filteredStations.length }} 条</span></header><div v-if="store.stationUtilization.status === 'success'" class="station-table"><div class="table-row table-head"><span>站点</span><span>日期</span><span>空闲 / 总数</span><span>利用率</span></div><div v-for="row in filteredStations" :key="row.stationId + '-' + row.date" class="table-row"><strong>{{ row.stationName }}</strong><span>{{ row.date || '--' }}</span><span>{{ integer(row.availableCount) }} / {{ integer(row.totalPileCount) }}</span><div class="table-progress"><span class="progress"><i :style="{ width: progressWidth(row.utilizationRate) }" /></span><b>{{ percent(row.utilizationRate) }}</b></div></div><div v-if="filteredStations.length === 0" class="table-empty">没有匹配的站点</div></div><div v-else :class="stateClass(store.stationUtilization)"><span>{{ statusText[store.stationUtilization.status] }}</span></div></article>
      </section>

      <section v-else-if="activeSection === 'prediction'" class="page-view prediction-grid">
        <article class="prediction-hero prediction-summary"><dv-border-box-8 :dur="5"><div class="prediction-content"><span class="prediction-label">SPARK MLLIB</span><h2>负荷预测摘要</h2><p>模型结果经 Flask 与 Dashboard Core 进入当前视图。</p><div class="prediction-number"><strong>{{ percent(store.prediction.data?.[0]?.predictedLoad) }}</strong><span>{{ peakName[store.prediction.data?.[0]?.peakLevel ?? ''] ?? '暂无等级' }}</span></div><div class="prediction-meta"><span>MAE {{ decimal(store.prediction.data?.[0]?.mae, 3) }}</span><span>RMSE {{ decimal(store.prediction.data?.[0]?.rmse, 3) }}</span></div></div></dv-border-box-8></article>
        <article class="card prediction-chart-card"><header class="card-header"><div><h2>1h / 6h / 24h 负荷预测</h2><p>站点与预测窗口对比</p></div><span class="period-chip">MLlib</span></header><div v-if="store.prediction.status === 'success'" class="chart-full"><EChartView :option="predictionOption" /></div><div v-else :class="stateClass(store.prediction)"><span>{{ statusText[store.prediction.status] }}</span></div></article>
        <article class="card full-row"><header class="card-header"><div><h2>预测明细</h2><p>预测时刻、负荷、可用桩与模型评价</p></div></header><div v-if="store.prediction.status === 'success'" class="station-table prediction-table"><div class="table-row table-head"><span>站点 / 时刻</span><span>窗口</span><span>峰值</span><span>预测负荷</span><span>可用桩</span><span>模型</span></div><div v-for="row in store.prediction.data" :key="row.stationId + '-' + row.predictionTime + '-' + row.horizon" class="table-row"><strong>{{ row.stationName }}<small>{{ row.predictionTime }}</small></strong><span>{{ row.horizon }}</span><span :class="'peak peak-' + row.peakLevel.toLowerCase()">{{ peakName[row.peakLevel] }}</span><b>{{ percent(row.predictedLoad) }}</b><span>{{ integer(row.predictedAvailableCount) }}</span><span>{{ row.modelName ?? '--' }}</span></div></div><div v-else :class="stateClass(store.prediction)"><span>{{ statusText[store.prediction.status] }}</span></div></article>
      </section>

      <section v-else class="page-view quality-grid">
        <article class="card quality-summary"><header class="card-header"><div><h2>清洗通过率</h2><p>有效数据占源数据比例</p></div><span class="quality-badge">QUALITY</span></header><div v-if="store.dataQuality.status === 'success'" class="quality-layout"><div class="quality-chart"><EChartView :option="qualityOption" /><div class="quality-center"><strong>{{ percent(qualityRate) }}</strong><span>通过率</span></div></div><dl><div><dt>源数据</dt><dd>{{ integer(store.dataQuality.data?.sourceRows) }}</dd></div><div><dt>通过</dt><dd class="success-text">{{ integer(store.dataQuality.data?.acceptedRows) }}</dd></div><div><dt>拒绝</dt><dd class="danger-text">{{ integer(store.dataQuality.data?.rejectedRows) }}</dd></div></dl></div><div v-else :class="stateClass(store.dataQuality)"><span>{{ statusText[store.dataQuality.status] }}</span></div></article>
        <article class="card quality-rules"><header class="card-header"><div><h2>异常规则统计</h2><p>按 DQ 规则追踪拒绝原因</p></div><span class="period-chip">{{ store.dataQuality.data?.rules.length ?? 0 }} 项规则</span></header><div v-if="store.dataQuality.status === 'success'" class="rule-list"><div v-for="rule in store.dataQuality.data?.rules" :key="rule.ruleId" class="rule-row"><span>{{ rule.ruleId }}</span><div class="rule-bar"><i :style="{ width: progressWidth((rule.count || 0) / Math.max(1, store.dataQuality.data?.rejectedRows || 1)) }" /></div><strong>{{ integer(rule.count) }}</strong></div><div v-if="!store.dataQuality.data?.rules.length" class="table-empty">当前批次没有异常规则记录</div></div><div v-else :class="stateClass(store.dataQuality)"><span>{{ statusText[store.dataQuality.status] }}</span></div></article>
      </section>
      <footer><span>数据来源：Dashboard Core · {{ store.dataMode === 'mock' ? 'Mock 演示模式' : 'Flask API 模式' }}</span><span>最近刷新：{{ now.toLocaleString('zh-CN') }}</span></footer>
    </main>
  </div>
</template>
