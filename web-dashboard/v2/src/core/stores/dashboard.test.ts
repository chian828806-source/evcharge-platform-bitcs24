import { beforeEach, describe, expect, it } from 'vitest';
import { createPinia, setActivePinia } from 'pinia';
import { MockDashboardDataSource } from '../mocks/mockDataSource';
import { useDashboardStore } from './dashboard';

describe('dashboard core store', () => {
  beforeEach(() => setActivePinia(createPinia()));

  it('moves from loading to success through its data source', async () => {
    const store = useDashboardStore();
    store.setDataSourceForTesting(new MockDashboardDataSource());
    const pending = store.loadOverview();
    expect(store.overview.status).toBe('loading');
    await pending;
    expect(store.overview.status).toBe('success');
    expect(store.overview.data?.revenueFen).toBe(93600);
  });

  it('represents an empty collection without requiring a UI decision', async () => {
    const source = new MockDashboardDataSource();
    source.getEnergyTrend = async () => ({ data: { items: [] }, meta: { batchId: 'empty', generatedAt: '2026-09-15 00:00:00' } });
    const store = useDashboardStore();
    store.setDataSourceForTesting(source);
    await store.loadEnergyTrend();
    expect(store.energyTrend.status).toBe('empty');
  });

  it('normalizes datasource failures into an error resource', async () => {
    const source = new MockDashboardDataSource();
    source.getOverview = async () => { throw new Error('offline'); };
    const store = useDashboardStore();
    store.setDataSourceForTesting(source);
    await store.loadOverview();
    expect(store.overview).toMatchObject({ status: 'error', error: { message: 'offline' } });
  });
});
