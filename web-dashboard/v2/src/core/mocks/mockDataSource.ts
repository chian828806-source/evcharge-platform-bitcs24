import { dashboardFixtures } from './fixtures/dashboard';
import type { DashboardDataSource } from '../datasource/DashboardDataSource';

export class MockDashboardDataSource implements DashboardDataSource {
  getOverview = async () => dashboardFixtures.overview();
  getEnergyTrend = async () => dashboardFixtures.energyTrend();
  getRevenueTrend = async () => dashboardFixtures.revenueTrend();
  getStationRanking = async () => dashboardFixtures.stationRanking();
  getPileStatus = async () => dashboardFixtures.pileStatus();
  getHourlyHeatmap = async () => dashboardFixtures.hourlyHeatmap();
  getStationUtilization = async () => dashboardFixtures.stationUtilization();
  getPrediction = async () => dashboardFixtures.prediction();
  getDataQualitySummary = async () => dashboardFixtures.dataQuality();
  getWeather = async () => dashboardFixtures.weather();
}
