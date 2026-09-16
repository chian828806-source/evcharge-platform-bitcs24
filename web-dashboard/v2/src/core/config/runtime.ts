export type DashboardDataMode = 'mock' | 'real';

export interface RuntimeConfig {
  dataMode: DashboardDataMode;
  apiBaseUrl: string;
  requestTimeoutMs: number;
  realtimeEnabled: boolean;
  realtimeWsUrl: string;
}

const asMode = (value: string | undefined): DashboardDataMode => value === 'real' ? 'real' : 'mock';
const asBoolean = (value: string | undefined): boolean => value === 'true';
const asPositiveNumber = (value: string | undefined, fallback: number): number => {
  const number = Number(value);
  return Number.isFinite(number) && number > 0 ? number : fallback;
};

export const runtimeConfig: Readonly<RuntimeConfig> = Object.freeze({
  dataMode: asMode(import.meta.env.VITE_DATA_MODE),
  apiBaseUrl: import.meta.env.VITE_API_BASE_URL?.trim() ?? '',
  requestTimeoutMs: asPositiveNumber(import.meta.env.VITE_API_TIMEOUT_MS, 10_000),
  realtimeEnabled: asBoolean(import.meta.env.VITE_REALTIME_ENABLED),
  realtimeWsUrl: import.meta.env.VITE_REALTIME_WS_URL?.trim() ?? ''
});
