const STATUSES = ['AVAILABLE', 'RESERVED', 'CHARGING', 'FAULT', 'OFFLINE', 'RESTARTING'];
const asNumber = (value, fallback = 0) => Number.isFinite(Number(value)) ? Number(value) : fallback;
const asArray = (value) => Array.isArray(value) ? value : [];

export class DashboardCharts {
  constructor(elements, echarts = window.echarts) {
    this.elements = elements;
    this.echarts = echarts;
    this.instances = {};
  }

  initOnce() {
    if (!this.echarts) return;
    ['pileStatus', 'revenueTrend', 'prediction'].forEach((name) => {
      if (this.elements[name] && !this.instances[name]) this.instances[name] = this.echarts.init(this.elements[name]);
    });
  }

  updatePileStatus(data) {
    const chart = this.instances.pileStatus;
    if (!chart) return false;
    const counts = data?.counts || data || {};
    const hasData = STATUSES.some((name) => Number.isFinite(Number(counts[name])));
    if (!hasData) return this.renderEmpty(chart, '暂无电桩状态数据');
    chart.setOption({
      tooltip: { trigger: 'item' },
      series: [{ type: 'pie', radius: ['42%', '70%'], data: STATUSES.map((name) => ({ name, value: asNumber(counts[name]) })) }]
    }, true);
    return true;
  }

  updateRevenueTrend(data, range) {
    const chart = this.instances.revenueTrend;
    if (!chart) return false;
    const legacyRange = range === '7d' ? 'days7' : 'days30';
    const source = data?.ranges?.[range] || data?.[legacyRange] || (data?.range === range ? data : null);
    const rows = asArray(source?.items || source).map((row) => ({
      label: row.date || row.label || '-',
      energyKwh: asNumber(row.energyKwh),
      revenueYuan: asNumber(row.revenueFen ?? row.amountFen ?? row.value) / 100
    })).filter((row) => row.label !== '-' && (row.energyKwh !== 0 || row.revenueYuan !== 0));
    if (!rows.length) return this.renderEmpty(chart, '当前范围暂无趋势数据');
    chart.setOption({
      tooltip: { trigger: 'axis' },
      xAxis: { type: 'category', data: rows.map((row) => row.label) },
      yAxis: [{ type: 'value', name: 'kWh' }, { type: 'value', name: '元' }],
      series: [
        { name: '充电量', type: 'bar', data: rows.map((row) => row.energyKwh) },
        { name: '营收', type: 'line', smooth: true, yAxisIndex: 1, data: rows.map((row) => row.revenueYuan) }
      ]
    }, true);
    return true;
  }

  updatePrediction(data, { stationId, horizon }) {
    const chart = this.instances.prediction;
    if (!chart) return false;
    const rows = asArray(data?.predictions || data?.items || data).filter((row) =>
      (stationId === 'all' || String(row.stationId) === String(stationId)) &&
      (!horizon || row.horizon === horizon)
    );
    if (!rows.length) return this.renderEmpty(chart, '当前筛选条件没有预测数据');
    chart.setOption({
      tooltip: { trigger: 'axis', valueFormatter: (value) => `${value}%` },
      xAxis: { type: 'category', data: rows.map((row) => row.stationName || `站点 ${row.stationId}`) },
      yAxis: { type: 'value', min: 0, max: 100, name: '预测负荷 (%)' },
      series: [{ type: 'bar', data: rows.map((row) => Math.round(asNumber(row.predictedLoad) * 10000) / 100) }]
    }, true);
    return true;
  }

  resize() {
    Object.values(this.instances).forEach((chart) => chart.resize());
  }

  renderEmpty(chart, message) {
    chart.clear();
    chart.setOption({ graphic: [{ type: 'text', left: 'center', top: 'middle', style: { text: message, fill: '#667085' } }] }, true);
    return false;
  }
}
