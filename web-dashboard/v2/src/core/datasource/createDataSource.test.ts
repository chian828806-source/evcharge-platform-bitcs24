import { describe, expect, it } from 'vitest';
import { createDataSource } from './createDataSource';
import { MockDashboardDataSource } from '../mocks/mockDataSource';
import { FlaskDashboardDataSource } from './flaskDataSource';

const config = { apiBaseUrl: 'https://api.example.test/api/v1', requestTimeoutMs: 1000, realtimeEnabled: false, realtimeWsUrl: '' };

describe('createDataSource', () => {
  it('selects mock or Flask without changing the Store API', () => {
    expect(createDataSource({ ...config, dataMode: 'mock' })).toBeInstanceOf(MockDashboardDataSource);
    expect(createDataSource({ ...config, dataMode: 'real' })).toBeInstanceOf(FlaskDashboardDataSource);
  });
});
