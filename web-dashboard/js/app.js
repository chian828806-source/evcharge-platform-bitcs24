import { DashboardConfig } from './config.js';
import { DashboardStore } from './dashboard-store.js';
import { DashboardWebSocketClient } from './websocket-client.js';
import { DashboardCharts } from './charts.js';
import { DashboardController } from './dashboard-controller.js';
import { DashboardMockSource } from '../mock/dashboard-mock.js';
import * as echarts from '../vendor/echarts.esm.min.js';

const byId = (id) => document.getElementById(id);

document.addEventListener('DOMContentLoaded', () => {
  const store = new DashboardStore();
  const elements = {
    connectionState: byId('connection-state'), lastUpdated: byId('last-updated'), error: byId('dashboard-error'), refreshButton: byId('refresh-button'),
    todayEnergy: byId('today-energy'), todayRevenue: byId('today-revenue'), totalOrders: byId('total-orders'), stationLoad: byId('station-load'),
    stationSelect: byId('station-select'), horizonSelect: byId('horizon-select'), rangeButtons: document.querySelectorAll('[data-range]'), predictionBody: byId('prediction-body'), predictionEmpty: byId('prediction-empty'), pileStatusEmpty: byId('pile-status-empty'), revenueTrendEmpty: byId('revenue-trend-empty'),
    pileStatus: byId('pile-status-chart'), revenueTrend: byId('revenue-trend-chart'), prediction: byId('prediction-chart')
  };
  const charts = new DashboardCharts(elements, echarts);
  const controller = new DashboardController({ store, charts, elements });
  controller.init();

  const handleUpdate = ({ topic, data }) => store.updateTopic(topic, data);
  let source;
  if (DashboardConfig.mockEnabled) {
    source = new DashboardMockSource({ onUpdate: handleUpdate, onState: ({ state, detail }) => store.setConnection(state, detail), intervalMs: DashboardConfig.mockIntervalMs });
    source.start();
  } else {
    source = new DashboardWebSocketClient(DashboardConfig);
    source.on('state', ({ state, detail }) => store.setConnection(state, detail));
    source.on('update', handleUpdate);
    source.on('protocolError', (message) => store.setError(message));
    source.connect();
  }
  controller.setSource(source);
  window.addEventListener('beforeunload', () => source.stop?.() || source.disconnect?.());
});
