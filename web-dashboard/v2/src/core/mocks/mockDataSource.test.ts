import { describe, expect, it } from 'vitest';
import { MockDashboardDataSource } from './mockDataSource';

describe('MockDashboardDataSource', () => {
  it('returns the documented overview envelope', async () => {
    const result = await new MockDashboardDataSource().getOverview();
    expect(result.meta.batchId).toBe('MOCK-20260915-01');
    expect(result.data).toEqual({ orderCount: 42, energyKwh: 128.5, revenueFen: 93600, onlinePileCount: 22, utilizationRate: 0.62 });
  });

  it('returns weather through the same datasource boundary', async () => {
    const result = await new MockDashboardDataSource().getWeather();
    expect(result.data).toMatchObject({ city: '深圳', weatherText: '多云', available: true });
  });
});
