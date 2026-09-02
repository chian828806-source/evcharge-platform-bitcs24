const asArray = (value) => Array.isArray(value) ? value : [];
const asNumber = (value) => Number.isFinite(Number(value)) ? Number(value) : null;
const text = (value, fallback = '—') => value === null || value === undefined || value === '' ? fallback : String(value);

export class DashboardController {
  constructor({ store, charts, elements }) {
    this.store = store;
    this.charts = charts;
    this.elements = elements;
    this.filters = { stationId: 'all', horizon: '1h', range: '7d' };
    this.source = null;
  }

  init() {
    this.charts.initOnce();
    this.elements.refreshButton.addEventListener('click', () => this.source?.refresh?.() || this.source?.reconnectNow?.());
    this.elements.stationSelect.addEventListener('change', (event) => { this.filters.stationId = event.target.value; this.render(this.store.snapshot()); });
    this.elements.horizonSelect.addEventListener('change', (event) => { this.filters.horizon = event.target.value; this.render(this.store.snapshot()); });
    this.elements.rangeButtons.forEach((button) => button.addEventListener('click', () => {
      this.filters.range = button.dataset.range;
      this.elements.rangeButtons.forEach((item) => item.classList.toggle('active', item === button));
      this.render(this.store.snapshot());
    }));
    window.addEventListener('resize', () => this.charts.resize());
    this.store.subscribe((state) => this.render(state));
  }

  setSource(source) { this.source = source; }

  render(state) {
    this.renderConnection(state);
    this.renderSummary(state.data.summary);
    this.renderPredictionFilters(state.data.prediction);
    this.elements.pileStatusEmpty.hidden = this.charts.updatePileStatus(state.data.pileStatus);
    this.elements.revenueTrendEmpty.hidden = this.charts.updateRevenueTrend(state.data.revenueTrend, this.filters.range);
    this.elements.predictionEmpty.hidden = this.charts.updatePrediction(state.data.prediction, this.filters);
    this.renderPredictionDetails(state.data.prediction);
  }

  renderConnection(state) {
    const connection = state.connection;
    this.elements.connectionState.textContent = connection.detail ? `${connection.state}: ${connection.detail}` : connection.state;
    this.elements.connectionState.dataset.state = connection.state;
    this.elements.lastUpdated.textContent = state.lastUpdated ? state.lastUpdated.toLocaleString() : '尚未收到数据';
    this.elements.error.textContent = state.lastError || '';
  }

  renderSummary(summary) {
    const energy = asNumber(summary?.todayEnergyKwh);
    const revenue = asNumber(summary?.todayRevenueFen);
    const orders = asNumber(summary?.totalOrderCount);
    const load = asNumber(summary?.stationLoad);
    this.elements.todayEnergy.textContent = energy === null ? '—' : `${energy.toFixed(1)} kWh`;
    this.elements.todayRevenue.textContent = revenue === null ? '—' : `¥${(revenue / 100).toFixed(2)}`;
    this.elements.totalOrders.textContent = orders === null ? '—' : String(Math.round(orders));
    this.elements.stationLoad.textContent = load === null ? '—' : `${(load * 100).toFixed(1)}%`;
  }

  renderPredictionFilters(prediction) {
    const rows = asArray(prediction?.predictions || prediction?.items || prediction);
    const knownStations = new Map(rows.map((row) => [String(row.stationId), row.stationName || `站点 ${row.stationId}`]));
    const previous = this.filters.stationId;
    this.elements.stationSelect.replaceChildren(new Option('全部站点', 'all'));
    knownStations.forEach((name, id) => this.elements.stationSelect.add(new Option(name, id)));
    this.filters.stationId = knownStations.has(previous) || previous === 'all' ? previous : 'all';
    this.elements.stationSelect.value = this.filters.stationId;
  }

  renderPredictionDetails(prediction) {
    const rows = asArray(prediction?.predictions || prediction?.items || prediction).filter((row) =>
      (this.filters.stationId === 'all' || String(row.stationId) === this.filters.stationId) && row.horizon === this.filters.horizon
    );
    this.elements.predictionBody.replaceChildren(...rows.map((row) => {
      const tr = document.createElement('tr');
      [row.stationName || `站点 ${text(row.stationId)}`, text(row.horizon), text(row.predictionTime),
        asNumber(row.predictedLoad) === null ? '—' : `${(asNumber(row.predictedLoad) * 100).toFixed(1)}%`,
        text(row.predictedAvailableCount), text(row.peakLevel)].forEach((value) => {
        const td = document.createElement('td'); td.textContent = value; tr.append(td);
      });
      return tr;
    }));
    this.elements.predictionEmpty.hidden = rows.length > 0;
  }
}
