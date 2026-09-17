<!-- Dashboard UI：只消费 Dashboard Core Store，不直接请求 Flask 或重算业务 KPI。 -->
<script setup lang="ts">
import type { EChartsOption } from 'echarts';
import { computed, onBeforeUnmount, onMounted, ref } from 'vue';
import EChartView from './components/EChartView.vue';
import type { Resource, ResourceStatus } from './core/models/dashboard';
import { useDashboardStore } from './core/stores/dashboard';

const store = useDashboardStore();
const predictionHorizons = ['1h', '6h', '24h'] as const;
const selectedHorizon = ref<typeof predictionHorizons[number]>('1h');
const refreshing = ref(false);
const now = ref(new Date());
let timer: number | undefined;
let rotationTimer: number | undefined;
const stationPage = ref(0);

const colors = ['#3157d5', '#6b87e8', '#93acf2', '#c2d0f8', '#e7ecfb', '#f2a766', '#dc6675'];
const axis = { axisLine: { lineStyle: { color: '#e5eaf2' } }, axisTick: { show: false }, axisLabel: { color: '#7e8799', fontSize: 11 }, splitLine: { lineStyle: { color: '#edf1f6', type: 'dashed' as const } } };
const tooltip = { trigger: 'axis' as const, backgroundColor: '#20283b', borderWidth: 0, textStyle: { color: '#fff' } };
const finite = (value: unknown): value is number => typeof value === 'number' && Number.isFinite(value);
const integer = (value: unknown) => finite(value) ? Math.max(0, Math.round(value)).toLocaleString('zh-CN') : '--';
const decimal = (value: unknown, digits = 1) => finite(value) ? Math.max(0, value).toLocaleString('zh-CN', { minimumFractionDigits: digits, maximumFractionDigits: digits }) : '--';
const temperature = (value: unknown) => finite(value) ? value.toFixed(1) + '℃' : '--℃';
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
const filteredStations = computed(() => store.stationUtilization.data ?? []);
const predictionRows = computed(() => [...(store.prediction.data ?? [])].sort((left, right) => {
  const timeOrder = left.predictionTime.localeCompare(right.predictionTime);
  if (timeOrder !== 0) return timeOrder;
  const horizonOrder = { '1h': 1, '6h': 6, '24h': 24 } as const;
  return horizonOrder[left.horizon] - horizonOrder[right.horizon] || left.stationId - right.stationId;
}));
const visiblePredictions = computed(() => predictionRows.value.filter(row => row.horizon === selectedHorizon.value));
const selectedPrediction = computed(() => visiblePredictions.value[0]);
const screenStations = computed(() => {
  const rows = filteredStations.value;
  const offset = (stationPage.value % Math.max(1, Math.ceil(rows.length / 5))) * 5;
  return rows.slice(offset, offset + 5);
});

async function refresh() {
  if (refreshing.value) return;
  refreshing.value = true;
  try { await store.refreshAll(); } finally { refreshing.value = false; now.value = new Date(); }
}

const operationOption = computed<EChartsOption>(() => ({
  tooltip, color: [colors[0], colors[5]], legend: { top: 2, left: 'center', itemGap: 22, itemWidth: 12, itemHeight: 8, textStyle: { color: '#667085', fontSize: 11 }, data: ['充电量', '会话启动量'] },
  grid: { left: 58, right: 58, top: 58, bottom: 36 }, xAxis: { type: 'category', boundaryGap: false, data: store.energyTrend.data?.map(row => shortDate(row.date)) ?? [], ...axis },
  yAxis: [{ type: 'value', name: 'kWh', nameGap: 14, nameTextStyle: { color: '#98a1b2', padding: [0, 0, 0, 6] }, ...axis }, { type: 'value', name: '启动量', nameGap: 14, nameTextStyle: { color: '#98a1b2', padding: [0, 6, 0, 0] }, ...axis }],
  series: [{ name: '充电量', type: 'line', smooth: true, symbol: 'circle', symbolSize: 7, lineStyle: { width: 3 }, areaStyle: { color: 'rgba(49,87,213,.11)' }, data: store.energyTrend.data?.map(row => row.energyKwh) ?? [] }, { name: '会话启动量', type: 'line', smooth: true, yAxisIndex: 1, symbol: 'none', lineStyle: { width: 2, type: 'dashed' }, data: store.energyTrend.data?.map(row => row.orderCount) ?? [] }]
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
  tooltip: {
    position: 'top',
    backgroundColor: '#20283b',
    borderWidth: 0,
    textStyle: { color: '#fff' },
    formatter: (params: any) => {
      const [hour, dayIndex, rate] = params.value as [number, number, number];
      const days = ['周一', '周二', '周三', '周四', '周五', '周六', '周日'];
      const level = rate >= .8 ? '高峰' : rate >= .6 ? '繁忙' : rate >= .4 ? '适中' : rate >= .2 ? '较低' : '空闲';
      return '<b>' + days[dayIndex] + ' ' + String(hour).padStart(2, '0') + ':00–' + String((hour + 1) % 24).padStart(2, '0') + ':00</b><br/>利用率：' + (rate * 100).toFixed(1) + '%<br/>状态：' + level;
    }
  },
  grid: { left: 56, right: 22, top: 20, bottom: 78 }, xAxis: { type: 'category', data: Array.from({ length: 24 }, (_, i) => String(i)), ...axis },
  yAxis: { type: 'category', data: ['周一', '周二', '周三', '周四', '周五', '周六', '周日'], ...axis },
  visualMap: {
    type: 'piecewise', selectedMode: false, orient: 'horizontal', left: 'center', bottom: 8, itemWidth: 14, itemHeight: 14, itemGap: 16,
    textStyle: { color: '#687386', fontSize: 10 },
    pieces: [
      { gte: 0, lt: .2, label: '空闲 0–20%', color: '#eef2ff' },
      { gte: .2, lt: .4, label: '较低 20–40%', color: '#cad7fa' },
      { gte: .4, lt: .6, label: '适中 40–60%', color: '#9fb5f2' },
      { gte: .6, lt: .8, label: '繁忙 60–80%', color: '#6888e4' },
      { gte: .8, lte: 1, label: '高峰 80–100%', color: '#3157d5' }
    ]
  },
  series: [{ type: 'heatmap', data: store.hourlyHeatmap.data?.map(row => [Math.min(23, Math.max(0, row.hour)), (Math.min(7, Math.max(1, row.dayOfWeek)) + 5) % 7, Math.min(1, Math.max(0, row.utilizationRate))]) ?? [], itemStyle: { borderColor: '#fff', borderWidth: 2, borderRadius: 3 } }]
}));
const predictionOption = computed<EChartsOption>(() => {
  const rows = visiblePredictions.value;
  return { tooltip, color: [colors[0]], grid: { left: 54, right: 24, top: 28, bottom: 64 }, xAxis: { type: 'category', data: rows.map(row => row.stationName + '\n' + row.horizon), ...axis, axisLabel: { color: '#7e8799', fontSize: 10, interval: 0, rotate: 18 } }, yAxis: { type: 'value', min: 0, max: 1, ...axis, axisLabel: { color: '#7e8799', formatter: (value: number) => Math.round(value * 100) + '%' } }, series: [{ name: '预测负荷', type: 'line', smooth: true, symbolSize: 9, lineStyle: { width: 3 }, areaStyle: { color: 'rgba(49,87,213,.10)' }, markLine: { silent: true, data: [{ yAxis: .8, name: '高峰线' }], lineStyle: { color: colors[6], type: 'dashed' } }, data: rows.map(row => ({ value: row.predictedLoad, itemStyle: { color: row.peakLevel === 'HIGH' ? colors[6] : row.peakLevel === 'MEDIUM' ? colors[5] : colors[0] } })) }] };
});
const qualityOption = computed<EChartsOption>(() => ({ color: [colors[0], colors[6]], tooltip: { trigger: 'item' }, series: [{ type: 'pie', radius: ['62%', '79%'], center: ['50%', '48%'], label: { show: false }, data: [{ name: '通过', value: store.dataQuality.data?.acceptedRows ?? 0 }, { name: '拒绝', value: store.dataQuality.data?.rejectedRows ?? 0 }] }] }));

onMounted(() => {
  void refresh(); store.startRealtime();
  timer = window.setInterval(refresh, 30_000);
  rotationTimer = window.setInterval(() => {
    selectedHorizon.value = predictionHorizons[(predictionHorizons.indexOf(selectedHorizon.value) + 1) % predictionHorizons.length];
    stationPage.value += 1;
  }, 10_000);
});
onBeforeUnmount(() => { if (timer) window.clearInterval(timer); if (rotationTimer) window.clearInterval(rotationTimer); store.stopRealtime(); });
</script>

<template>
  <div class="app-frame single-screen">
    <main class="workspace">
      <header class="workspace-header">
        <div class="page-heading"><p class="overline">EVCHARGE ANALYTICS</p><h1>充电运营数据大屏</h1><p>运营趋势 · 站点资源 · 智能预测 · 数据质量</p></div>
        <div class="header-tools"><div :class="['weather-pill', { stale: store.weather.data?.isStale }]" :title="store.weather.data?.isStale ? '当前显示缓存或降级数据' : 'Open-Meteo 实时天气'"><svg viewBox="0 0 24 24" aria-hidden="true"><path d="M7 16.5a4 4 0 0 1 .8-7.92A5.5 5.5 0 0 1 18.4 10.5 3 3 0 0 1 18 16.5Z" /><path d="M8 5.5 6.5 4M12 4V2M16 5.5 17.5 4" /></svg><div><strong>{{ store.weather.data?.city ?? '深圳' }} · {{ store.weather.data?.weatherText ?? (store.weather.status === 'loading' ? '天气加载中' : '天气暂不可用') }}</strong><small v-if="store.weather.data?.available">{{ temperature(store.weather.data.temperature) }} · 体感 {{ temperature(store.weather.data.apparentTemperature) }} · 湿度 {{ integer(store.weather.data.humidity) }}% · 风速 {{ decimal(store.weather.data.windSpeed) }} km/h</small><small v-else>实时天气暂不可用</small></div></div><span :class="['connection-pill', store.overview.status]"><i />{{ store.dataMode === 'mock' ? 'Mock 数据' : statusText[store.overview.status] }}</span><div class="profile"><span>EV</span><div><strong>运营中心</strong><small>{{ now.toLocaleDateString('zh-CN') }}</small></div></div></div>
      </header>
      <section class="page-view screen-view" aria-label="充电运营数据总览">
        <div class="summary-grid">
          <article class="metric-card metric-card--primary"><div class="metric-icon">01</div><span>累计充电会话</span><strong>{{ integer(store.overview.data?.orderCount) }}</strong><small>由占用量变化推导的启动量</small></article>
          <article class="metric-card"><div class="metric-icon">02</div><span>充电电量</span><strong>{{ decimal(store.overview.data?.energyKwh) }} <i>kWh</i></strong><small>统计周期总电量</small></article>
          <article class="metric-card"><div class="metric-icon">03</div><span>累计营收</span><strong>{{ money(store.overview.data?.revenueFen) }}</strong><small>已完成订单实付金额汇总</small></article>
          <article class="metric-card"><div class="metric-icon">04</div><span>统计桩位</span><strong>{{ integer(store.overview.data?.onlinePileCount) }} <i>台</i></strong><small>数据集站点容量，不代表心跳在线</small></article>
          <article class="metric-card"><div class="metric-icon">05</div><span>平均利用率</span><strong>{{ percent(store.overview.data?.utilizationRate) }}</strong><small>统一利用率口径</small></article>
        </div>
        <div class="screen-grid">
          <article class="card screen-trend"><header class="card-header"><div><h2>充电运营趋势</h2><p>充电量与会话启动量变化</p></div><span class="period-chip">近 14 日</span></header><div v-if="store.energyTrend.status === 'success'" class="chart-large"><EChartView :option="operationOption" /></div><div v-else :class="stateClass(store.energyTrend)"><span>{{ statusText[store.energyTrend.status] }}</span></div></article>
          <article class="card"><header class="card-header"><div><h2>电桩状态</h2><p>设备运行结构</p></div></header><div v-if="store.pileStatus.status === 'success'" class="donut-wrap"><EChartView :option="pileOption" /><div class="donut-label"><strong>{{ integer(totalPiles) }}</strong><span>电桩总数</span></div></div><div v-else :class="stateClass(store.pileStatus)"><span>{{ statusText[store.pileStatus.status] }}</span></div></article>
          <article class="card"><header class="card-header"><div><h2>营收趋势</h2><p>已完成订单实付金额变化</p></div></header><div v-if="store.revenueTrend.status === 'success'" class="chart-compact"><EChartView :option="revenueOption" /></div><div v-else :class="stateClass(store.revenueTrend)"><span>{{ statusText[store.revenueTrend.status] }}</span></div></article>
          <article class="card screen-heatmap"><header class="card-header"><div><h2>时段利用率热力图</h2><p>星期与小时两个维度交叉分析</p></div><span class="period-chip">7 × 24</span></header><div v-if="store.hourlyHeatmap.status === 'success'" class="chart-heatmap"><EChartView :option="heatmapOption" /></div><div v-else :class="stateClass(store.hourlyHeatmap)"><span>{{ statusText[store.hourlyHeatmap.status] }}</span></div></article>
          <article class="card screen-ranking"><header class="card-header"><div><h2>站点充电量排行</h2><p>按充电贡献对比</p></div><span class="period-chip">TOP 8</span></header><div v-if="store.stationRanking.status === 'success'" class="chart-full"><EChartView :option="rankingOption" /></div><div v-else :class="stateClass(store.stationRanking)"><span>{{ statusText[store.stationRanking.status] }}</span></div></article>
          <article class="card screen-stations"><header class="card-header"><div><h2>站点利用率</h2><p>高利用站点概览</p></div><span class="period-chip">自动轮播</span></header><div class="utilization-list compact"><div v-for="row in screenStations" :key="row.stationId + '-' + row.date" class="utilization-row"><div><strong>{{ row.stationName }}</strong><small>{{ integer(row.availableCount) }}/{{ integer(row.totalPileCount) }} 空闲</small></div><span class="progress"><i :style="{ width: progressWidth(row.utilizationRate) }" /></span><b>{{ percent(row.utilizationRate) }}</b></div></div></article>
          <article class="card screen-prediction"><dv-border-box-8 :dur="5"><div class="screen-prediction-inner">
          <header class="card-header prediction-header">
            <div><h2>{{ selectedHorizon }} 负荷预测</h2><p>同一预测窗口下的站点负荷对比</p></div>
            <div class="horizon-switch" aria-label="预测窗口，每10秒自动轮播"><span v-for="horizon in predictionHorizons" :key="horizon" :class="{ active: selectedHorizon === horizon }">{{ horizon }}</span></div>
          </header><div class="screen-prediction-summary"><strong>{{ percent(selectedPrediction?.predictedLoad) }}</strong><span>{{ selectedPrediction?.stationName ?? '暂无预测数据' }} · {{ peakName[selectedPrediction?.peakLevel ?? ''] ?? '--' }} · 可用 {{ integer(selectedPrediction?.predictedAvailableCount) }} 枪</span><small>MAE {{ decimal(selectedPrediction?.mae, 3) }} · RMSE {{ decimal(selectedPrediction?.rmse, 3) }}</small></div>
          <div v-if="store.prediction.status === 'success' && visiblePredictions.length" class="chart-full"><EChartView :option="predictionOption" /></div>
          <div v-else-if="store.prediction.status === 'success'" class="table-empty">{{ selectedHorizon }} 窗口暂无预测数据</div>
          <div v-else :class="stateClass(store.prediction)"><span>{{ statusText[store.prediction.status] }}</span></div>
        </div></dv-border-box-8></article>
          <article class="card screen-quality"><header class="card-header"><div><h2>清洗通过率</h2><p>有效数据占源数据比例</p></div><span class="quality-badge">QUALITY</span></header><div v-if="store.dataQuality.status === 'success'" class="quality-layout"><div class="quality-chart"><EChartView :option="qualityOption" /><div class="quality-center"><strong>{{ percent(qualityRate) }}</strong><span>通过率</span></div></div><dl><div><dt>源数据</dt><dd>{{ integer(store.dataQuality.data?.sourceRows) }}</dd></div><div><dt>通过</dt><dd class="success-text">{{ integer(store.dataQuality.data?.acceptedRows) }}</dd></div><div><dt>拒绝</dt><dd class="danger-text">{{ integer(store.dataQuality.data?.rejectedRows) }}</dd></div></dl></div><div v-else :class="stateClass(store.dataQuality)"><span>{{ statusText[store.dataQuality.status] }}</span></div></article>
          <article class="card screen-rules"><header class="card-header"><div><h2>异常规则统计</h2><p>按 DQ 规则追踪拒绝原因</p></div><span class="period-chip">{{ store.dataQuality.data?.rules.length ?? 0 }} 项规则</span></header><div v-if="store.dataQuality.status === 'success'" class="rule-list"><div v-for="rule in store.dataQuality.data?.rules" :key="rule.ruleId" class="rule-row"><span>{{ rule.ruleId }}</span><div class="rule-bar"><i :style="{ width: progressWidth((rule.count || 0) / Math.max(1, store.dataQuality.data?.rejectedRows || 1)) }" /></div><strong>{{ integer(rule.count) }}</strong></div><div v-if="!store.dataQuality.data?.rules.length" class="table-empty">当前批次没有异常规则记录</div></div><div v-else :class="stateClass(store.dataQuality)"><span>{{ statusText[store.dataQuality.status] }}</span></div></article>
        </div>
      </section>
      <footer><span>数据来源：Dashboard Core · {{ store.dataMode === 'mock' ? 'Mock 演示模式' : 'Flask API 模式' }}</span><span>预测窗口每10秒自动轮播 · 数据每30秒自动刷新 · {{ now.toLocaleString('zh-CN') }}</span></footer>
    </main>
  </div>
</template>
