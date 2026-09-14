const STATUSES = ['AVAILABLE', 'RESERVED', 'CHARGING', 'FAULT', 'OFFLINE', 'RESTARTING'];
const PEAK_LEVELS = new Set(['LOW', 'MEDIUM', 'HIGH']);
const asArray = (value) => Array.isArray(value) ? value : [];
const isObject = (value) => value !== null && typeof value === 'object' && !Array.isArray(value);

const parseFiniteNumber = (value) => {
  if (value === null || value === undefined || value === '') return null;
  const number = Number(value);
  return Number.isFinite(number) ? number : null;
};

const asNumber = (value, fallback = 0) => parseFiniteNumber(value) ?? fallback;

const isValidDate = (value) => {
  if (typeof value !== 'string' || !/^\d{4}-\d{2}-\d{2}$/.test(value)) return false;
  const [year, month, day] = value.split('-').map(Number);
  const date = new Date(Date.UTC(year, month - 1, day));
  return date.getUTCFullYear() === year && date.getUTCMonth() === month - 1 && date.getUTCDate() === day;
};

const warnLegacy = (topic) => console.warn(`[Dashboard] ${topic} received a legacy payload; send the V1 canonical structure instead.`);
const warnInvalid = (field) => console.warn(`[Dashboard] Ignored invalid ${field} value from dashboard payload.`);

function revenueItems(data, range) {
  const canonicalRange = data?.ranges?.[range];
  if (isObject(data?.ranges)) {
    if (canonicalRange?.range === range && Array.isArray(canonicalRange.items)) return canonicalRange.items;
    warnInvalid(`revenueTrend.ranges.${range}`);
    return [];
  }

  if (!isObject(data)) return [];
  const legacyRange = range === '7d' ? 'days7' : 'days30';
  const fallback = data[legacyRange] || (data.range === range ? data : null);
  if (!fallback) return [];
  warnLegacy('revenueTrend');
  return asArray(fallback.items || fallback);
}

function normalizeRevenueRows(data, range) {
  return revenueItems(data, range).map((row) => {
    if (!isObject(row) || !isValidDate(row.date)) {
      warnInvalid('revenueTrend item/date');
      return null;
    }
    const energyKwh = parseFiniteNumber(row.energyKwh);
    const revenueFen = parseFiniteNumber(row.revenueFen);
    if (energyKwh === null || revenueFen === null) {
      warnInvalid('revenueTrend energyKwh/revenueFen');
      return null;
    }
    return { label: row.date, energyKwh, revenueYuan: revenueFen / 100 };
  }).filter(Boolean);
}

function predictionItems(data) {
  if (Array.isArray(data?.items)) return data.items;
  if (Array.isArray(data?.predictions)) {
    warnLegacy('prediction');
    return data.predictions;
  }
  if (Array.isArray(data)) {
    warnLegacy('prediction');
    return data;
  }
  return [];
}

export function normalizePredictionItems(data) {
  return predictionItems(data).map((row) => {
    if (!isObject(row)) {
      warnInvalid('prediction item');
      return null;
    }
    const predictedLoad = parseFiniteNumber(row.predictedLoad);
    if (predictedLoad === null || predictedLoad < 0 || predictedLoad > 1) {
      warnInvalid('predictedLoad');
      return null;
    }

    const available = parseFiniteNumber(row.predictedAvailableCount);
    const predictedAvailableCount = Number.isInteger(available) && available >= 0 ? available : null;
    if (predictedAvailableCount === null) warnInvalid('predictedAvailableCount');

    const peakLevel = PEAK_LEVELS.has(row.peakLevel) ? row.peakLevel : 'UNKNOWN';
    if (peakLevel === 'UNKNOWN') warnInvalid('peakLevel');

    return { ...row, predictedLoad, predictedAvailableCount, peakLevel };
  }).filter(Boolean);
}

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
    const counts = isObject(data?.counts) ? data.counts : isObject(data) ? (warnLegacy('pileStatus'), data) : null;
    const hasData = counts && STATUSES.some((name) => parseFiniteNumber(counts[name]) !== null);
    if (!hasData) return this.renderEmpty(chart);
    chart.setOption({
      tooltip: { trigger: 'item' },
      series: [{ type: 'pie', radius: ['42%', '70%'], data: STATUSES.map((name) => ({ name, value: asNumber(counts[name]) })) }]
    }, true);
    return true;
  }

  updateRevenueTrend(data, range) {
    const chart = this.instances.revenueTrend;
    if (!chart) return false;
    const rows = normalizeRevenueRows(data, range);
    if (!rows.length) return this.renderEmpty(chart);
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
    const rows = normalizePredictionItems(data).filter((row) =>
      (stationId === 'all' || String(row.stationId) === String(stationId)) &&
      (!horizon || row.horizon === horizon)
    );
    if (!rows.length) return this.renderEmpty(chart);
    chart.setOption({
      tooltip: { trigger: 'axis', valueFormatter: (value) => `${value}%` },
      xAxis: { type: 'category', data: rows.map((row) => row.stationName || `站点 ${row.stationId}`) },
      yAxis: { type: 'value', min: 0, max: 100, name: '预测负荷 (%)' },
      series: [{ type: 'bar', data: rows.map((row) => Math.round(row.predictedLoad * 10000) / 100) }]
    }, true);
    return true;
  }

  resize() {
    Object.values(this.instances).forEach((chart) => chart.resize());
  }

  renderEmpty(chart) {
    chart.clear();
    return false;
  }
}
