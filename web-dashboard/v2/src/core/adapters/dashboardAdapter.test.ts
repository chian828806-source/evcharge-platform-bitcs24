import { describe, expect, it } from 'vitest';
import { dashboardAdapter } from './dashboardAdapter';

describe('dashboardAdapter', () => {
  it('maps DTO fields to a null-safe ViewModel without recomputing KPI values', () => {
    expect(dashboardAdapter.overview({ orderCount: 7, energyKwh: 12.5, revenueFen: 3500, onlinePileCount: 4, utilizationRate: 0.5 }))
      .toEqual({ orderCount: 7, energyKwh: 12.5, revenueFen: 3500, onlinePileCount: 4, utilizationRate: 0.5 });
  });

  it('normalizes missing presentation strings without changing prediction semantics', () => {
    const [prediction] = dashboardAdapter.prediction([{ stationId: 1, stationName: '', predictionTime: '2026-09-15 10:00:00', horizon: '1h', predictedLoad: 0.6, predictedAvailableCount: 3, peakLevel: 'MEDIUM', modelName: null, mae: null, rmse: null }]);
    expect(prediction).toMatchObject({ stationName: '—', predictedLoad: 0.6, peakLevel: 'MEDIUM' });
  });
});
