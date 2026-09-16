import type {
  ApiEnvelopeDto, DataQualitySummaryDto, EnergyTrendItemDto, HourlyHeatmapItemDto, ItemsDto,
  OverviewDto, PileStatusItemDto, PredictionItemDto, RevenueTrendItemDto, StationRankingItemDto, StationUtilizationItemDto,
  WeatherDto
} from '../../api/dto';

const meta = { batchId: 'MOCK-20260915-01', generatedAt: '2026-09-15 09:00:00' };
const envelope = <T>(data: T): ApiEnvelopeDto<T> => ({ data, meta: { ...meta } });

export const dashboardFixtures = {
  overview: (): ApiEnvelopeDto<OverviewDto> => envelope({ orderCount: 42, energyKwh: 128.5, revenueFen: 93_600, onlinePileCount: 22, utilizationRate: 0.62 }),
  energyTrend: (): ApiEnvelopeDto<ItemsDto<EnergyTrendItemDto>> => envelope({ items: [{ date: '2026-09-14', energyKwh: 128.5, orderCount: 42 }] }),
  revenueTrend: (): ApiEnvelopeDto<ItemsDto<RevenueTrendItemDto>> => envelope({ items: [{ date: '2026-09-14', revenueFen: 93_600, orderCount: 42 }] }),
  stationRanking: (): ApiEnvelopeDto<ItemsDto<StationRankingItemDto>> => envelope({ items: [{ stationId: 1, stationName: '东软软件园充电站', district: '甘井子区', energyKwh: 128.5, revenueFen: 93_600, utilizationRate: 0.62, rank: 1 }] }),
  pileStatus: (): ApiEnvelopeDto<ItemsDto<PileStatusItemDto>> => envelope({ items: [{ status: 'AVAILABLE', count: 12, ratio: 0.5 }, { status: 'CHARGING', count: 8, ratio: 0.33 }] }),
  hourlyHeatmap: (): ApiEnvelopeDto<ItemsDto<HourlyHeatmapItemDto>> => envelope({ items: [{ dayOfWeek: 1, hour: 9, energyKwh: 24.5, utilizationRate: 0.62 }] }),
  stationUtilization: (): ApiEnvelopeDto<ItemsDto<StationUtilizationItemDto>> => envelope({ items: [{ stationId: 1, stationName: '东软软件园充电站', date: '2026-09-14', utilizationRate: 0.62, availableCount: 3, totalPileCount: 8 }] }),
  prediction: (): ApiEnvelopeDto<ItemsDto<PredictionItemDto>> => envelope({ items: [{ stationId: 1, stationName: '东软软件园充电站', predictionTime: '2026-09-15 10:00:00', horizon: '1h', predictedLoad: 0.68, predictedAvailableCount: 3, peakLevel: 'MEDIUM', modelName: 'mock-baseline', mae: 0.04, rmse: 0.06 }] }),
  dataQuality: (): ApiEnvelopeDto<DataQualitySummaryDto> => envelope({ sourceRows: 10_000, acceptedRows: 9_750, rejectedRows: 250, rules: [{ ruleId: 'DQ-001', count: 120 }, { ruleId: 'DQ-002', count: 130 }] }),
  weather: (): ApiEnvelopeDto<WeatherDto> => envelope({ city: '深圳', temperature: 28.4, apparentTemperature: 31.2, humidity: 76, precipitation: 0, windSpeed: 12.3, weatherCode: 2, weatherText: '多云', updatedAt: '2026-09-16T18:30:00+08:00', available: true, isStale: false, source: 'mock' })
};
