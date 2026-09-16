export interface ApiMetaDto {
  batchId?: string;
  generatedAt: string;
  source?: string;
  cacheTtlSeconds?: number;
}

export interface ApiEnvelopeDto<T> {
  data: T;
  meta: ApiMetaDto;
}

export interface OverviewDto {
  orderCount: number;
  energyKwh: number;
  revenueFen: number;
  onlinePileCount: number;
  utilizationRate: number;
}

export interface EnergyTrendItemDto { date: string; energyKwh: number; orderCount: number; }
export interface RevenueTrendItemDto { date: string; revenueFen: number; orderCount: number; }
export interface StationRankingItemDto { stationId: number; stationName: string; district: string | null; energyKwh: number; revenueFen: number; utilizationRate: number; rank: number; }
export interface PileStatusItemDto { status: string; count: number; ratio: number; }
export interface HourlyHeatmapItemDto { dayOfWeek: number; hour: number; energyKwh: number; utilizationRate: number; }
export interface StationUtilizationItemDto { stationId: number; stationName: string; date: string; utilizationRate: number; availableCount: number; totalPileCount: number; }
export interface PredictionItemDto { stationId: number; stationName: string; predictionTime: string; horizon: '1h' | '6h' | '24h'; predictedLoad: number; predictedAvailableCount: number; peakLevel: 'LOW' | 'MEDIUM' | 'HIGH'; modelName: string | null; mae: number | null; rmse: number | null; }
export interface DataQualityRuleDto { ruleId: string; count: number; }
export interface DataQualitySummaryDto { sourceRows: number; acceptedRows: number; rejectedRows: number; rules: DataQualityRuleDto[]; }
export interface WeatherDto {
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
export interface ItemsDto<T> { items: T[]; }
