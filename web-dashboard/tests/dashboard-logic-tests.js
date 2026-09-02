import { DashboardStore } from '../js/dashboard-store.js';
import { DashboardWebSocketClient } from '../js/websocket-client.js';
import { DashboardCharts } from '../js/charts.js';
import { DashboardMockSource, dashboardMockData } from '../mock/dashboard-mock.js';

const results = []; const check = (condition, name) => results.push(`${condition ? 'PASS' : 'FAIL'} ${name}`);
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
const mockTrend = dashboardMockData().revenueTrend.ranges;
check(mockTrend['7d'].items.length === 7 && mockTrend['30d'].items.length === 30 && mockTrend['30d'].items.every((item) => Number.isFinite(item.energyKwh) && Number.isFinite(item.revenueFen)), 'mock provides complete 7d and 30d energy and revenue data');

const fakeInstances = [];
const fakeEcharts = { init: () => { const instance = { options: [], clears: 0, resizes: 0, setOption(option) { this.options.push(option); }, clear() { this.clears += 1; }, resize() { this.resizes += 1; } }; fakeInstances.push(instance); return instance; } };
const charts = new DashboardCharts({ pileStatus: {}, revenueTrend: {}, prediction: {} }, fakeEcharts);
charts.initOnce(); charts.initOnce();
check(fakeInstances.length === 3, 'charts initialize each instance only once');
check(charts.updateRevenueTrend({ range: '7d', items: [{ date: '2026-09-01', energyKwh: 128.5, revenueFen: 93600 }] }, '7d'), 'revenue trend accepts energyKwh and revenueFen');
const revenueOption = fakeInstances[1].options.at(-1);
check(revenueOption.series.length === 2 && revenueOption.series[0].data[0] === 128.5 && revenueOption.series[1].data[0] === 936, 'revenue chart converts Fen only for display and renders both series');
check(!charts.updatePileStatus(null) && !charts.updateRevenueTrend(null, '7d') && !charts.updatePrediction(null, { stationId: 'all', horizon: '1h' }), 'empty chart data is safe and clears stale options');
charts.resize(); check(fakeInstances.every((instance) => instance.resizes === 1), 'charts resize safely');
document.getElementById('result').textContent = `${results.join('\n')}\nTOTAL_FAILURES=${results.filter((line) => line.startsWith('FAIL')).length}`;
