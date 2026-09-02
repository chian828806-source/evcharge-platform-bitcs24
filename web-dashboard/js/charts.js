const STATUSES = ['AVAILABLE', 'RESERVED', 'CHARGING', 'FAULT', 'OFFLINE', 'RESTARTING'];
const asNumber = (value, fallback = 0) => Number.isFinite(Number(value)) ? Number(value) : fallback;
const asArray = (value) => Array.isArray(value) ? value : [];

export class DashboardCharts {
  constructor(elements) {
    this.elements = elements;
    this.echarts = window.echarts;
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
    if (!chart) return;
    const counts = data?.counts || data || {};
    chart.setOption({
      tooltip: { trigger: 'item' },
      series: [{ type: 'pie', radius: ['42%', '70%'], data: STATUSES.map((name) => ({ name, value: asNumber(counts[name]) })) }]
    }, true);
  }

  updateRevenueTrend(data, range) {
    const chart = this.instances.revenueTrend;
    if (!chart) return;
    const rows = asArray(data?.[range] || data?.items || data).map((row) => ({
      label: row.date || row.label || '-',
      revenueYuan: asNumber(row.revenueFen ?? row.amountFen ?? row.value) / 100
    }));
    chart.setOption({
      tooltip: { trigger: 'axis', valueFormatter: (value) => `¥${value}` },
      xAxis: { type: 'category', data: rows.map((row) => row.label) },
      yAxis: { type: 'value', name: '元' },
      series: [{ type: 'line', smooth: true, data: rows.map((row) => row.revenueYuan), areaStyle: {} }]
    }, true);
  }

  updatePrediction(data, { stationId, horizon }) {
    const chart = this.instances.prediction;
    if (!chart) return;
    const rows = asArray(data?.predictions || data?.items || data).filter((row) =>
      (stationId === 'all' || String(row.stationId) === String(stationId)) &&
      (!horizon || row.horizon === horizon)
    );
    chart.setOption({
      tooltip: { trigger: 'axis', valueFormatter: (value) => `${value}%` },
      xAxis: { type: 'category', data: rows.map((row) => row.stationName || `站点 ${row.stationId}`) },
      yAxis: { type: 'value', min: 0, max: 100, name: '预测负荷 (%)' },
      series: [{ type: 'bar', data: rows.map((row) => Math.round(asNumber(row.predictedLoad) * 10000) / 100) }]
    }, true);
  }

  resize() {
    Object.values(this.instances).forEach((chart) => chart.resize());
  }
}
