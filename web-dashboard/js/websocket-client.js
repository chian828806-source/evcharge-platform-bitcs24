import { DASHBOARD_TOPICS } from './config.js';

const isObject = (value) => value !== null && typeof value === 'object' && !Array.isArray(value);

export class DashboardWebSocketClient {
  constructor({ url, topics = DASHBOARD_TOPICS, reconnectBaseDelayMs = 1000, reconnectMaxDelayMs = 15000, WebSocketImpl = window.WebSocket } = {}) {
    this.url = url;
    this.topics = topics;
    this.WebSocketImpl = WebSocketImpl;
    this.reconnectBaseDelayMs = reconnectBaseDelayMs;
    this.reconnectMaxDelayMs = reconnectMaxDelayMs;
    this.socket = null;
    this.reconnectTimer = null;
    this.attempt = 0;
    this.shouldReconnect = true;
    this.listeners = new Map();
  }

  on(event, listener) {
    const listeners = this.listeners.get(event) || new Set();
    listeners.add(listener);
    this.listeners.set(event, listeners);
    return () => listeners.delete(listener);
  }

  emit(event, payload) {
    (this.listeners.get(event) || []).forEach((listener) => listener(payload));
  }

  connect() {
    if (!this.WebSocketImpl) {
      this.emit('state', { state: 'unsupported', detail: 'This browser does not support WebSocket.' });
      return;
    }
    if (this.socket && (this.socket.readyState === 0 || this.socket.readyState === 1)) {
      return;
    }
    clearTimeout(this.reconnectTimer);
    this.emit('state', { state: 'connecting', detail: this.url });
    try {
      const socket = new this.WebSocketImpl(this.url);
      this.socket = socket;
      socket.onopen = () => {
        this.attempt = 0;
        this.emit('state', { state: 'connected', detail: this.url });
        this.subscribe();
      };
      socket.onmessage = (event) => this.handleMessage(event.data);
      socket.onerror = () => this.emit('state', { state: 'error', detail: 'WebSocket transport error.' });
      socket.onclose = () => {
        this.socket = null;
        this.emit('state', { state: 'disconnected', detail: 'Connection closed; last dashboard data is retained.' });
        if (this.shouldReconnect) this.scheduleReconnect();
      };
    } catch (error) {
      this.emit('protocolError', `WebSocket setup failed: ${error.message}`);
      this.scheduleReconnect();
    }
  }

  disconnect() {
    this.shouldReconnect = false;
    clearTimeout(this.reconnectTimer);
    if (this.socket) this.socket.close();
    this.socket = null;
    this.emit('state', { state: 'closed', detail: 'Disconnected by user.' });
  }

  reconnectNow() {
    this.shouldReconnect = true;
    clearTimeout(this.reconnectTimer);
    if (this.socket) {
      this.socket.close();
      return;
    }
    this.connect();
  }

  subscribe() {
    if (!this.socket || this.socket.readyState !== 1) return;
    this.socket.send(JSON.stringify({
      requestId: `DASH-${this.createRequestId()}`,
      type: 'DASHBOARD_SUBSCRIBE',
      payload: { topics: this.topics }
    }));
  }

  handleMessage(rawMessage) {
    let message;
    try {
      message = JSON.parse(rawMessage);
    } catch {
      this.emit('protocolError', 'Ignored invalid JSON received from dashboard server.');
      return false;
    }
    if (!isObject(message)) {
      this.emit('protocolError', 'Ignored non-object dashboard message.');
      return false;
    }
    if (message.type === 'DASHBOARD_UPDATE') {
      if (!DASHBOARD_TOPICS.includes(message.topic) || !isObject(message.data)) {
        this.emit('protocolError', 'Ignored dashboard update with an unknown topic or invalid data.');
        return false;
      }
      this.emit('update', { topic: message.topic, data: message.data });
      return true;
    }
    if (typeof message.code === 'number') {
      if (message.code !== 200) this.emit('protocolError', message.message || 'Dashboard subscription was rejected.');
      else this.emit('subscription', message);
      return true;
    }
    this.emit('protocolError', `Ignored unknown dashboard message type: ${String(message.type)}`);
    return false;
  }

  scheduleReconnect() {
    if (!this.shouldReconnect || this.reconnectTimer) return;
    const delay = Math.min(this.reconnectBaseDelayMs * (2 ** this.attempt), this.reconnectMaxDelayMs);
    this.attempt += 1;
    this.emit('state', { state: 'reconnecting', detail: `Retrying in ${Math.round(delay / 1000)}s.` });
    this.reconnectTimer = setTimeout(() => {
      this.reconnectTimer = null;
      this.connect();
    }, delay);
  }

  createRequestId() {
    if (window.crypto?.randomUUID) return window.crypto.randomUUID();
    return `${Date.now()}-${Math.random().toString(16).slice(2)}`;
  }
}
