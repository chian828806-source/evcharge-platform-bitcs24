import type {
  ApiEnvelopeDto, DataQualitySummaryDto, EnergyTrendItemDto, HourlyHeatmapItemDto, ItemsDto,
  OverviewDto, PileStatusItemDto, PredictionItemDto, RevenueTrendItemDto, StationRankingItemDto, StationUtilizationItemDto,
  WeatherDto
} from '../api/dto';

export interface DashboardQuery { from?: string; to?: string; stationId?: number; }

export interface DashboardDataSource {
  getOverview(query?: DashboardQuery): Promise<ApiEnvelopeDto<OverviewDto>>;
  getEnergyTrend(query?: DashboardQuery): Promise<ApiEnvelopeDto<ItemsDto<EnergyTrendItemDto>>>;
  getRevenueTrend(query?: DashboardQuery): Promise<ApiEnvelopeDto<ItemsDto<RevenueTrendItemDto>>>;
  getStationRanking(query?: DashboardQuery): Promise<ApiEnvelopeDto<ItemsDto<StationRankingItemDto>>>;
  getPileStatus(query?: DashboardQuery): Promise<ApiEnvelopeDto<ItemsDto<PileStatusItemDto>>>;
  getHourlyHeatmap(query?: DashboardQuery): Promise<ApiEnvelopeDto<ItemsDto<HourlyHeatmapItemDto>>>;
  getStationUtilization(query?: DashboardQuery): Promise<ApiEnvelopeDto<ItemsDto<StationUtilizationItemDto>>>;
  getPrediction(query?: DashboardQuery): Promise<ApiEnvelopeDto<ItemsDto<PredictionItemDto>>>;
  getDataQualitySummary(): Promise<ApiEnvelopeDto<DataQualitySummaryDto>>;
  getWeather(): Promise<ApiEnvelopeDto<WeatherDto>>;
}
