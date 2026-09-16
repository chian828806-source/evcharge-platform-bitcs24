import type {
  DataQualitySummaryDto, EnergyTrendItemDto, OverviewDto, PileStatusItemDto, PredictionItemDto,
  RevenueTrendItemDto, StationRankingItemDto, StationUtilizationItemDto, HourlyHeatmapItemDto, WeatherDto
} from '../api/dto';
import type {
  DashboardOverview, DataQualitySummary, EnergyTrendItem, HourlyHeatmapItem, PileStatusItem,
  PredictionItem, RevenueTrendItem, StationRankingItem, StationUtilizationItem, WeatherContext
} from '../models/dashboard';

const finite = (value: number, fallback = 0): number => Number.isFinite(value) ? value : fallback;
const unitInterval = (value: number): number | null => Number.isFinite(value) && value >= 0 && value <= 1 ? value : null;
const text = (value: string | null | undefined): string => value?.trim() || '—';

export const dashboardAdapter = {
  overview(dto: OverviewDto): DashboardOverview {
    return { orderCount: finite(dto.orderCount), energyKwh: finite(dto.energyKwh), revenueFen: finite(dto.revenueFen), onlinePileCount: finite(dto.onlinePileCount), utilizationRate: unitInterval(dto.utilizationRate) ?? 0 };
  },
  energyTrend(rows: EnergyTrendItemDto[]): EnergyTrendItem[] {
    return rows.filter((row) => Boolean(row.date)).map((row) => ({ date: row.date, energyKwh: finite(row.energyKwh), orderCount: finite(row.orderCount) }));
  },
  revenueTrend(rows: RevenueTrendItemDto[]): RevenueTrendItem[] {
    return rows.filter((row) => Boolean(row.date)).map((row) => ({ date: row.date, revenueFen: finite(row.revenueFen), orderCount: finite(row.orderCount) }));
  },
  stationRanking(rows: StationRankingItemDto[]): StationRankingItem[] {
    return rows.map((row) => ({ ...row, stationName: text(row.stationName), district: row.district || null, energyKwh: finite(row.energyKwh), revenueFen: finite(row.revenueFen), utilizationRate: unitInterval(row.utilizationRate) ?? 0, rank: finite(row.rank) }));
  },
  pileStatus(rows: PileStatusItemDto[]): PileStatusItem[] {
    return rows.map((row) => ({ status: text(row.status), count: finite(row.count), ratio: unitInterval(row.ratio) ?? 0 }));
  },
  hourlyHeatmap(rows: HourlyHeatmapItemDto[]): HourlyHeatmapItem[] {
    return rows.map((row) => ({ dayOfWeek: finite(row.dayOfWeek), hour: finite(row.hour), energyKwh: finite(row.energyKwh), utilizationRate: unitInterval(row.utilizationRate) ?? 0 }));
  },
  stationUtilization(rows: StationUtilizationItemDto[]): StationUtilizationItem[] {
    return rows.map((row) => ({ ...row, stationName: text(row.stationName), utilizationRate: unitInterval(row.utilizationRate) ?? 0, availableCount: finite(row.availableCount), totalPileCount: finite(row.totalPileCount) }));
  },
  prediction(rows: PredictionItemDto[]): PredictionItem[] {
    return rows.map((row) => ({ ...row, stationName: text(row.stationName), predictedLoad: unitInterval(row.predictedLoad) ?? 0, predictedAvailableCount: Math.max(0, finite(row.predictedAvailableCount)), modelName: row.modelName || null, mae: row.mae ?? null, rmse: row.rmse ?? null }));
  },
  dataQuality(dto: DataQualitySummaryDto): DataQualitySummary {
    return { sourceRows: finite(dto.sourceRows), acceptedRows: finite(dto.acceptedRows), rejectedRows: finite(dto.rejectedRows), rules: (dto.rules ?? []).map((rule) => ({ ruleId: text(rule.ruleId), count: finite(rule.count) })) };
  },
  weather(dto: WeatherDto): WeatherContext {
    const nullable = (value: number | null): number | null => value !== null && Number.isFinite(value) ? value : null;
    return {
      city: text(dto.city), temperature: nullable(dto.temperature), apparentTemperature: nullable(dto.apparentTemperature),
      humidity: nullable(dto.humidity), precipitation: nullable(dto.precipitation), windSpeed: nullable(dto.windSpeed),
      weatherCode: nullable(dto.weatherCode), weatherText: text(dto.weatherText), updatedAt: dto.updatedAt || null,
      available: Boolean(dto.available), isStale: Boolean(dto.isStale), source: dto.source
    };
  }
};
