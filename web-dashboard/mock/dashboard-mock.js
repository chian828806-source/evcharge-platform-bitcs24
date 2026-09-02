const now = () => new Date().toISOString();

export const dashboardMockData = () => ({
  summary: { todayEnergyKwh: 128.5, todayRevenueFen: 93600, totalOrderCount: 42, stationLoad: 0.62 },
  pileStatus: { counts: { AVAILABLE: 12, RESERVED: 2, CHARGING: 8, FAULT: 1, OFFLINE: 1, RESTARTING: 0 } },
  revenueTrend: {
    days7: [
      { date: '08-27', revenueFen: 8200 }, { date: '08-28', revenueFen: 10600 }, { date: '08-29', revenueFen: 9300 },
      { date: '08-30', revenueFen: 12100 }, { date: '08-31', revenueFen: 11200 }, { date: '09-01', revenueFen: 14900 }, { date: '09-02', revenueFen: 93600 }
    ],
    days30: []
  },
  prediction: {
    generatedAt: now(),
    predictions: [
      { stationId: 1, stationName: '东软软件园充电站', predictionTime: '2026-09-02 13:00:00', horizon: '1h', predictedLoad: 0.62, predictedAvailableCount: 3, peakLevel: 'MEDIUM' },
      { stationId: 1, stationName: '东软软件园充电站', predictionTime: '2026-09-02 18:00:00', horizon: '6h', predictedLoad: 0.78, predictedAvailableCount: 1, peakLevel: 'HIGH' },
      { stationId: 2, stationName: '河口湾产业园充电站', predictionTime: '2026-09-02 13:00:00', horizon: '1h', predictedLoad: 0.35, predictedAvailableCount: 6, peakLevel: 'LOW' },
      { stationId: 2, stationName: '河口湾产业园充电站', predictionTime: '2026-09-03 12:00:00', horizon: '24h', predictedLoad: 0.52, predictedAvailableCount: 4, peakLevel: 'MEDIUM' }
    ]
  }
});

export class DashboardMockSource {
  constructor({ onUpdate, onState, intervalMs = 10000 } = {}) {
    this.onUpdate = onUpdate;
    this.onState = onState;
    this.intervalMs = intervalMs;
    this.timer = null;
  }

  start() {
    this.onState?.({ state: 'mock', detail: 'Mock mode is active; no Qt server connection is used.' });
    this.refresh();
    this.timer = setInterval(() => this.refresh(), this.intervalMs);
  }

  stop() {
    clearInterval(this.timer);
    this.timer = null;
  }

  refresh() {
    const payload = dashboardMockData();
    Object.entries(payload).forEach(([topic, data]) => this.onUpdate?.({ topic, data }));
  }
}
