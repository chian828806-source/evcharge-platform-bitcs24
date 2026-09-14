import { DashboardStore } from '../js/dashboard-store.js';
import { DashboardWebSocketClient } from '../js/websocket-client.js';
import { DashboardCharts, normalizePredictionItems } from '../js/charts.js';
import { DashboardController } from '../js/dashboard-controller.js';
import { DashboardMockSource, dashboardMockData } from '../mock/dashboard-mock.js';

const results = []; const check = (condition, name) => results.push(`${condition ? 'PASS' : 'FAIL'} ${name}`);
const warnings = []; const originalWarn = console.warn;
console.warn = (...args) => warnings.push(args.join(' '));

const store = new DashboardStore(); let snapshots = 0; store.subscribe(() => { snapshots += 1; });
check(store.updateTopic('summary', { todayEnergyKwh: 1 }), 'store accepts known object topic');
check(!store.updateTopic('unknown', {}), 'store rejects unknown topic');
check(store.snapshot().data.summary.todayEnergyKwh === 1 && snapshots >= 2, 'store retains latest valid data');

class FakeWebSocket { static OPEN = 1; static instances = []; constructor() { this.readyState = 0; this.sent = []; FakeWebSocket.instances.push(this); } send(value) { this.sent.push(JSON.parse(value)); } close() { this.readyState = 3; this.onclose?.(); } open() { this.readyState = 1; this.onopen?.(); } }
const client = new DashboardWebSocketClient({ url: 'ws://test/dashboard', WebSocketImpl: FakeWebSocket }); let update = null;
client.on('update', (message) => { update = message; }); client.connect(); client.socket.open();
check(client.socket.sent[0].type === 'DASHBOARD_SUBSCRIBE' && client.socket.sent[0].payload.topics.length === 4, 'client subscribes to four topics on open');
check(client.handleMessage(JSON.stringify({ type: 'DASHBOARD_UPDATE', topic: 'summary', data: { stationLoad: 0.5 } })) && update?.data.stationLoad === 0.5, 'client routes valid update');
check(!client.handleMessage('{'), 'client rejects invalid JSON'); client.disconnect();
const reconnectingClient = new DashboardWebSocketClient({ url: 'ws://test/dashboard', WebSocketImpl: FakeWebSocket, reconnectBaseDelayMs: 1, reconnectMaxDelayMs: 1 });
reconnectingClient.connect(); const firstSocket = reconnectingClient.socket; firstSocket.open(); firstSocket.close();
await new Promise((resolve) => setTimeout(resolve, 10)); const secondSocket = reconnectingClient.socket; secondSocket.open();
check(secondSocket !== firstSocket && secondSocket.sent[0].type === 'DASHBOARD_SUBSCRIBE', 'client reconnects and resubscribes'); reconnectingClient.disconnect();

let mockTopics = []; const mock = new DashboardMockSource({ onUpdate: ({ topic }) => mockTopics.push(topic) }); mock.refresh();
check(mockTopics.length === 4, 'mock source emits all four topics');
const mockData = dashboardMockData(); const mockTrend = mockData.revenueTrend.ranges;
check(mockTrend['7d'].items.length === 7 && mockTrend['30d'].items.length === 30 && mockTrend['30d'].items.every((item) => Number.isFinite(item.energyKwh) && Number.isFinite(item.revenueFen)), 'mock provides complete canonical 7d and 30d energy and revenue data');
check(Array.isArray(mockData.prediction.items) && !Object.hasOwn(mockData.prediction, 'predictions'), 'mock prediction uses the canonical items field');

const fakeInstances = [];
const fakeEcharts = { init: () => { const instance = { options: [], clears: 0, resizes: 0, setOption(option) { this.options.push(option); }, clear() { this.clears += 1; }, resize() { this.resizes += 1; } }; fakeInstances.push(instance); return instance; } };
const charts = new DashboardCharts({ pileStatus: {}, revenueTrend: {}, prediction: {} }, fakeEcharts);
charts.initOnce(); charts.initOnce();
check(fakeInstances.length === 3, 'charts initialize each instance only once');

const canonicalTrend = { ranges: {
  '7d': { range: '7d', items: [{ date: '2026-09-01', energyKwh: 128.5, revenueFen: 93600 }, { date: '2026-09-02', energyKwh: 0, revenueFen: 0 }] },
  '30d': { range: '30d', items: [{ date: '2026-08-04', energyKwh: 50, revenueFen: 6800 }] }
} };
check(charts.updateRevenueTrend(canonicalTrend, '7d'), 'revenue trend accepts the canonical ranges schema');
const revenueOption = fakeInstances[1].options.at(-1);
check(revenueOption.series.length === 2 && revenueOption.series[0].data[0] === 128.5 && revenueOption.series[1].data[0] === 936, 'revenue chart converts Fen only for display and renders both series');
check(revenueOption.xAxis.data.includes('2026-09-02') && revenueOption.series[0].data[1] === 0 && revenueOption.series[1].data[1] === 0, 'zero kWh and zero Fen dates remain in the trend chart');
check(charts.updateRevenueTrend(canonicalTrend, '30d') && fakeInstances[1].options.at(-1).xAxis.data[0] === '2026-08-04', 'revenue trend renders the canonical 30d range');
const warningCountBeforeLegacy = warnings.length;
check(charts.updateRevenueTrend({ range: '7d', items: [{ date: '2026-09-03', energyKwh: 1, revenueFen: 100 }] }, '7d'), 'legacy revenue fallback remains safe for temporary input');
check(warnings.length > warningCountBeforeLegacy && warnings.at(-1).includes('legacy payload'), 'legacy revenue fallback emits a warning');

const validPredictionRows = normalizePredictionItems({ items: [
  { stationId: 1, horizon: '1h', predictedLoad: 0.5, predictedAvailableCount: 2.5, peakLevel: 'LOW' },
  { stationId: 2, horizon: '1h', predictedLoad: 0.6, predictedAvailableCount: 2, peakLevel: 'BAD' },
  { stationId: 3, horizon: '1h', predictedLoad: 1.1, predictedAvailableCount: 2, peakLevel: 'HIGH' }
] });
check(validPredictionRows.length === 2 && validPredictionRows[0].predictedAvailableCount === null && validPredictionRows[1].peakLevel === 'UNKNOWN', 'prediction validation rejects invalid load and degrades invalid availability and peak level');
check(!charts.updatePrediction({ items: [{ stationId: 3, horizon: '1h', predictedLoad: -0.2, predictedAvailableCount: 1, peakLevel: 'LOW' }] }, { stationId: 'all', horizon: '1h' }), 'invalid predictedLoad is not plotted');

const summaryElements = { todayEnergy: {}, todayRevenue: {}, totalOrders: {}, stationLoad: {} };
const summaryController = new DashboardController({ charts: {}, elements: summaryElements });
summaryController.renderSummary({ stationLoad: 0.62 });
check(summaryElements.stationLoad.textContent === '62.0%', 'stationLoad in range is displayed as a percentage');
summaryController.renderSummary({ stationLoad: 1.4 }); const highLoad = summaryElements.stationLoad.textContent === '—';
summaryController.renderSummary({ stationLoad: -0.2 }); const lowLoad = summaryElements.stationLoad.textContent === '—';
check(highLoad && lowLoad, 'out-of-range stationLoad is displayed as an em dash');

const pileChart = fakeInstances[0]; const clearsBeforeEmpty = pileChart.clears;
check(!charts.updatePileStatus(null) && pileChart.clears === clearsBeforeEmpty + 1, 'empty chart data clears stale chart options');
const emptyMessage = { hidden: true }; summaryController.setEmptyState(emptyMessage, false);
const emptyIsShown = emptyMessage.hidden === false;
const pileRecovered = charts.updatePileStatus({ counts: { AVAILABLE: 1 } }); summaryController.setEmptyState(emptyMessage, pileRecovered);
check(emptyIsShown && emptyMessage.hidden === true && pileRecovered && pileChart.options.at(-1).series[0].type === 'pie', 'HTML empty state hides after data recovery and chart options are restored');
check(!charts.updateRevenueTrend(null, '7d') && !charts.updatePrediction(null, { stationId: 'all', horizon: '1h' }), 'remaining empty chart data is safe');
charts.resize(); check(fakeInstances.every((instance) => instance.resizes === 1), 'charts resize safely');

console.warn = originalWarn;
document.getElementById('result').textContent = `${results.join('\n')}\nTOTAL_FAILURES=${results.filter((line) => line.startsWith('FAIL')).length}`;
