import { HttpClient } from '../api/client';
import type { RuntimeConfig } from '../config/runtime';
import { MockDashboardDataSource } from '../mocks/mockDataSource';
import type { DashboardDataSource } from './DashboardDataSource';
import { FlaskDashboardDataSource } from './flaskDataSource';

export const createDataSource = (config: RuntimeConfig): DashboardDataSource =>
  config.dataMode === 'mock'
    ? new MockDashboardDataSource()
    : new FlaskDashboardDataSource(new HttpClient(config.apiBaseUrl, config.requestTimeoutMs));
