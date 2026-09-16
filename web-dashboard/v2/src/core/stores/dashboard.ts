import { computed, ref, type Ref } from 'vue';
import { defineStore } from 'pinia';
import { DashboardApiError, type DashboardApiErrorShape } from '../api/client';
import type { ApiEnvelopeDto } from '../api/dto';
import { dashboardAdapter } from '../adapters/dashboardAdapter';
import { runtimeConfig, type DashboardDataMode } from '../config/runtime';
import { createDataSource } from '../datasource/createDataSource';
import type { DashboardDataSource } from '../datasource/DashboardDataSource';
import { createResource, type DashboardViewModel, type RealtimeSnapshot, type Resource } from '../models/dashboard';
import { QtDashboardWebSocket } from '../realtime/qtDashboardWebSocket';

const initialRealtime = (): RealtimeSnapshot => ({
  topics: {}, connection: { state: runtimeConfig.realtimeEnabled ? 'idle' : 'disabled', detail: '' }, lastMessageAt: null, error: null
});

const errorShape = (error: unknown): DashboardApiErrorShape => {
  if (error instanceof DashboardApiError) return { type: error.type, message: error.message, statusCode: error.statusCode, endpoint: error.endpoint };
  return { type: 'network', message: error instanceof Error ? error.message : 'Unknown dashboard data error.', statusCode: null, endpoint: 'unknown' };
};

export const useDashboardStore = defineStore('dashboard-core', () => {
  const dataMode = ref<DashboardDataMode>(runtimeConfig.dataMode);
  let dataSource: DashboardDataSource = createDataSource(runtimeConfig);
  let realtimeClient: QtDashboardWebSocket | null = null;

  const overview = ref(createResource<DashboardViewModel['overview']['data'] extends infer T ? NonNullable<T> : never>());
  const energyTrend = ref(createResource<DashboardViewModel['energyTrend']['data'] extends infer T ? NonNullable<T> : never>());
  const revenueTrend = ref(createResource<DashboardViewModel['revenueTrend']['data'] extends infer T ? NonNullable<T> : never>());
  const stationRanking = ref(createResource<DashboardViewModel['stationRanking']['data'] extends infer T ? NonNullable<T> : never>());
  const pileStatus = ref(createResource<DashboardViewModel['pileStatus']['data'] extends infer T ? NonNullable<T> : never>());
  const hourlyHeatmap = ref(createResource<DashboardViewModel['hourlyHeatmap']['data'] extends infer T ? NonNullable<T> : never>());
  const stationUtilization = ref(createResource<DashboardViewModel['stationUtilization']['data'] extends infer T ? NonNullable<T> : never>());
  const prediction = ref(createResource<DashboardViewModel['prediction']['data'] extends infer T ? NonNullable<T> : never>());
  const dataQuality = ref(createResource<DashboardViewModel['dataQuality']['data'] extends infer T ? NonNullable<T> : never>());
  const realtime = ref(initialRealtime());

  async function load<TDto, TModel>(resource: Ref<Resource<TModel>>, request: () => Promise<ApiEnvelopeDto<TDto>>, adapt: (dto: TDto) => TModel, isEmpty: (data: TModel) => boolean = () => false): Promise<void> {
    resource.value = { ...resource.value, status: 'loading', error: null };
    try {
      const response = await request();
      const data = adapt(response.data);
      resource.value = { data, status: isEmpty(data) ? 'empty' : 'success', error: null, lastUpdated: response.meta.generatedAt || new Date().toISOString() };
    } catch (error) {
      resource.value = { ...resource.value, status: 'error', error: errorShape(error) };
    }
  }

  const loadOverview = () => load(overview, () => dataSource.getOverview(), dashboardAdapter.overview);
  const loadEnergyTrend = () => load(energyTrend, () => dataSource.getEnergyTrend(), (dto) => dashboardAdapter.energyTrend(dto.items), (rows) => rows.length === 0);
  const loadRevenueTrend = () => load(revenueTrend, () => dataSource.getRevenueTrend(), (dto) => dashboardAdapter.revenueTrend(dto.items), (rows) => rows.length === 0);
  const loadStationRanking = () => load(stationRanking, () => dataSource.getStationRanking(), (dto) => dashboardAdapter.stationRanking(dto.items), (rows) => rows.length === 0);
  const loadPileStatus = () => load(pileStatus, () => dataSource.getPileStatus(), (dto) => dashboardAdapter.pileStatus(dto.items), (rows) => rows.length === 0);
  const loadHourlyHeatmap = () => load(hourlyHeatmap, () => dataSource.getHourlyHeatmap(), (dto) => dashboardAdapter.hourlyHeatmap(dto.items), (rows) => rows.length === 0);
  const loadStationUtilization = () => load(stationUtilization, () => dataSource.getStationUtilization(), (dto) => dashboardAdapter.stationUtilization(dto.items), (rows) => rows.length === 0);
  const loadPrediction = () => load(prediction, () => dataSource.getPrediction(), (dto) => dashboardAdapter.prediction(dto.items), (rows) => rows.length === 0);
  const loadDataQuality = () => load(dataQuality, () => dataSource.getDataQualitySummary(), dashboardAdapter.dataQuality);

  async function refreshAll(): Promise<void> {
    await Promise.all([loadOverview(), loadEnergyTrend(), loadRevenueTrend(), loadStationRanking(), loadPileStatus(), loadHourlyHeatmap(), loadStationUtilization(), loadPrediction(), loadDataQuality()]);
  }

  const refreshOverview = loadOverview;
  const refreshEnergyTrend = loadEnergyTrend;
  const refreshRevenueTrend = loadRevenueTrend;
  const refreshStationRanking = loadStationRanking;
  const refreshPileStatus = loadPileStatus;
  const refreshHourlyHeatmap = loadHourlyHeatmap;
  const refreshStationUtilization = loadStationUtilization;
  const refreshPrediction = loadPrediction;
  const refreshDataQuality = loadDataQuality;

  async function setDataMode(mode: DashboardDataMode): Promise<void> {
    dataMode.value = mode;
    dataSource = createDataSource({ ...runtimeConfig, dataMode: mode });
    await refreshAll();
  }

  function setDataSourceForTesting(source: DashboardDataSource): void { dataSource = source; }

  function startRealtime(): void {
    if (!runtimeConfig.realtimeEnabled || realtimeClient) return;
    realtimeClient = new QtDashboardWebSocket({
      url: runtimeConfig.realtimeWsUrl,
      onConnection: (state, detail) => { realtime.value = { ...realtime.value, connection: { state, detail } }; },
      onUpdate: (topic, data) => { realtime.value = { ...realtime.value, topics: { ...realtime.value.topics, [topic]: data }, lastMessageAt: new Date().toISOString(), error: null }; },
      onError: (message) => { realtime.value = { ...realtime.value, error: message }; }
    });
    realtimeClient.connect();
  }

  function stopRealtime(): void { realtimeClient?.disconnect(); realtimeClient = null; }

  const viewModel = computed<DashboardViewModel>(() => ({
    overview: overview.value, energyTrend: energyTrend.value, revenueTrend: revenueTrend.value,
    stationRanking: stationRanking.value, pileStatus: pileStatus.value, hourlyHeatmap: hourlyHeatmap.value,
    stationUtilization: stationUtilization.value, prediction: prediction.value, dataQuality: dataQuality.value,
    source: dataMode.value === 'mock' ? 'mock' : 'api', realtime: realtime.value
  }));

  return {
    dataMode, overview, energyTrend, revenueTrend, stationRanking, pileStatus, hourlyHeatmap,
    stationUtilization, prediction, dataQuality, realtime, viewModel,
    loadOverview, loadEnergyTrend, loadRevenueTrend, loadStationRanking, loadPileStatus,
    loadHourlyHeatmap, loadStationUtilization, loadPrediction, loadDataQuality, refreshAll,
    refreshOverview, refreshEnergyTrend, refreshRevenueTrend, refreshStationRanking, refreshPileStatus,
    refreshHourlyHeatmap, refreshStationUtilization, refreshPrediction, refreshDataQuality,
    setDataMode, setDataSourceForTesting, startRealtime, stopRealtime
  };
});
