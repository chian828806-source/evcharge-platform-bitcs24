import { DASHBOARD_TOPICS } from './config.js';

const isObject = (value) => value !== null && typeof value === 'object' && !Array.isArray(value);

export class DashboardStore {
  constructor() {
    this.state = {
      data: Object.fromEntries(DASHBOARD_TOPICS.map((topic) => [topic, null])),
      connection: { state: 'idle', detail: '' },
      lastUpdated: null,
      lastError: ''
    };
    this.listeners = new Set();
  }

  subscribe(listener) {
    this.listeners.add(listener);
    listener(this.snapshot());
    return () => this.listeners.delete(listener);
  }

  updateTopic(topic, data) {
    if (!DASHBOARD_TOPICS.includes(topic) || !isObject(data)) {
      this.setError(`Ignored invalid dashboard payload for topic: ${topic}`);
      return false;
    }
    this.state.data[topic] = data;
    this.state.lastUpdated = new Date();
    this.state.lastError = '';
    this.notify();
    return true;
  }

  setConnection(state, detail = '') {
    this.state.connection = { state, detail };
    this.notify();
  }

  setError(message) {
    this.state.lastError = message;
    this.notify();
  }

  snapshot() {
    return {
      ...this.state,
      data: { ...this.state.data },
      connection: { ...this.state.connection }
    };
  }

  notify() {
    const snapshot = this.snapshot();
    this.listeners.forEach((listener) => listener(snapshot));
  }
}
