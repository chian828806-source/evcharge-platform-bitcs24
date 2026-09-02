export const DASHBOARD_TOPICS = Object.freeze([
  'summary',
  'pileStatus',
  'revenueTrend',
  'prediction'
]);

const parameters = new URLSearchParams(window.location.search);
const overrides = window.EVCHARGE_DASHBOARD_CONFIG || {};
const host = overrides.host || parameters.get('host') || window.location.hostname || '127.0.0.1';
const port = Number(overrides.port || parameters.get('port') || 18081);

export const DashboardConfig = Object.freeze({
  mockEnabled: overrides.mockEnabled ?? parameters.get('mock') !== '0',
  websocketUrl: overrides.websocketUrl || `ws://${host}:${port}/dashboard`,
  reconnectBaseDelayMs: 1000,
  reconnectMaxDelayMs: 15000,
  mockIntervalMs: 10000,
  topics: DASHBOARD_TOPICS
});
