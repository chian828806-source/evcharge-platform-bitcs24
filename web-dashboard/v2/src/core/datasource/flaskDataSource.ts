import { HttpClient } from '../api/client';
import { dashboardEndpoints } from '../api/endpoints';
import type {
  ApiEnvelopeDto, DataQualitySummaryDto, EnergyTrendItemDto, HourlyHeatmapItemDto, ItemsDto,
  OverviewDto, PileStatusItemDto, PredictionItemDto, RevenueTrendItemDto, StationRankingItemDto, StationUtilizationItemDto
} from '../api/dto';
import type { DashboardDataSource, DashboardQuery } from './DashboardDataSource';

const asQuery = (query?: DashboardQuery): Record<string, string | number | undefined> => ({ from: query?.from, to: query?.to, stationId: query?.stationId });

export class FlaskDashboardDataSource implements DashboardDataSource {
  constructor(private readonly client: HttpClient) {}
  getOverview(query?: DashboardQuery): Promise<ApiEnvelopeDto<OverviewDto>> { return this.client.get(dashboardEndpoints.overview, asQuery(query)); }
  getEnergyTrend(query?: DashboardQuery): Promise<ApiEnvelopeDto<ItemsDto<EnergyTrendItemDto>>> { return this.client.get(dashboardEndpoints.energyTrend, asQuery(query)); }
  getRevenueTrend(query?: DashboardQuery): Promise<ApiEnvelopeDto<ItemsDto<RevenueTrendItemDto>>> { return this.client.get(dashboardEndpoints.revenueTrend, asQuery(query)); }
  getStationRanking(query?: DashboardQuery): Promise<ApiEnvelopeDto<ItemsDto<StationRankingItemDto>>> { return this.client.get(dashboardEndpoints.stationRanking, asQuery(query)); }
  getPileStatus(query?: DashboardQuery): Promise<ApiEnvelopeDto<ItemsDto<PileStatusItemDto>>> { return this.client.get(dashboardEndpoints.pileStatus, asQuery(query)); }
  getHourlyHeatmap(query?: DashboardQuery): Promise<ApiEnvelopeDto<ItemsDto<HourlyHeatmapItemDto>>> { return this.client.get(dashboardEndpoints.hourlyHeatmap, asQuery(query)); }
  getStationUtilization(query?: DashboardQuery): Promise<ApiEnvelopeDto<ItemsDto<StationUtilizationItemDto>>> { return this.client.get(dashboardEndpoints.stationUtilization, asQuery(query)); }
  getPrediction(query?: DashboardQuery): Promise<ApiEnvelopeDto<ItemsDto<PredictionItemDto>>> { return this.client.get(dashboardEndpoints.prediction, asQuery(query)); }
  getDataQualitySummary(): Promise<ApiEnvelopeDto<DataQualitySummaryDto>> { return this.client.get(dashboardEndpoints.dataQualitySummary); }
}
