import type { DashboardApiErrorShape } from '../api/client';

export type ResourceStatus = 'idle' | 'loading' | 'success' | 'empty' | 'error';

export interface Resource<T> {
  data: T | null;
  status: ResourceStatus;
  error: DashboardApiErrorShape | null;
  lastUpdated: string | null;
}

export interface DashboardOverview { orderCount: number; energyKwh: number; revenueFen: number; onlinePileCount: number; utilizationRate: number; }
export interface EnergyTrendItem { date: string; energyKwh: number; orderCount: number; }
export interface RevenueTrendItem { date: string; revenueFen: number; orderCount: number; }
export interface StationRankingItem { stationId: number; stationName: string; district: string | null; energyKwh: number; revenueFen: number; utilizationRate: number; rank: number; }
export interface PileStatusItem { status: string; count: number; ratio: number; }
export interface HourlyHeatmapItem { dayOfWeek: number; hour: number; energyKwh: number; utilizationRate: number; }
export interface StationUtilizationItem { stationId: number; stationName: string; date: string; utilizationRate: number; availableCount: number; totalPileCount: number; }
export interface PredictionItem { stationId: number; stationName: string; predictionTime: string; horizon: '1h' | '6h' | '24h'; predictedLoad: number; predictedAvailableCount: number; peakLevel: 'LOW' | 'MEDIUM' | 'HIGH'; modelName: string | null; mae: number | null; rmse: number | null; }
export interface DataQualityRule { ruleId: string; count: number; }
export interface DataQualitySummary { sourceRows: number; acceptedRows: number; rejectedRows: number; rules: DataQualityRule[]; }
export interface WeatherContext {
  city: string;
  temperature: number | null;
  apparentTemperature: number | null;
  humidity: number | null;
  precipitation: number | null;
  windSpeed: number | null;
  weatherCode: number | null;
  weatherText: string;
  updatedAt: string | null;
  available: boolean;
  isStale: boolean;
  source: 'open-meteo' | 'cache' | 'unavailable' | 'mock';
}

export type RealtimeTopic = 'summary' | 'pileStatus' | 'revenueTrend' | 'prediction';
export interface RealtimeSnapshot { topics: Partial<Record<RealtimeTopic, unknown>>; connection: { state: string; detail: string }; lastMessageAt: string | null; error: string | null; }

export interface DashboardViewModel {
  overview: Resource<DashboardOverview>;
  energyTrend: Resource<EnergyTrendItem[]>;
  revenueTrend: Resource<RevenueTrendItem[]>;
  stationRanking: Resource<StationRankingItem[]>;
  pileStatus: Resource<PileStatusItem[]>;
  hourlyHeatmap: Resource<HourlyHeatmapItem[]>;
  stationUtilization: Resource<StationUtilizationItem[]>;
  prediction: Resource<PredictionItem[]>;
  dataQuality: Resource<DataQualitySummary>;
  weather: Resource<WeatherContext>;
  source: 'mock' | 'api';
  realtime: RealtimeSnapshot;
}

export const createResource = <T>(): Resource<T> => ({ data: null, status: 'idle', error: null, lastUpdated: null });
