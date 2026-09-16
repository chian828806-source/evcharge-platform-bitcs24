import type { RealtimeTopic } from '../models/dashboard';

export const QT_DASHBOARD_TOPICS: RealtimeTopic[] = ['summary', 'pileStatus', 'revenueTrend', 'prediction'];

export interface QtDashboardUpdate { type: 'update'; topic: RealtimeTopic; data: Record<string, unknown>; }
export interface QtDashboardProtocolError { type: 'error'; message: string; }
export type QtDashboardMessage = QtDashboardUpdate | QtDashboardProtocolError;

const isObject = (value: unknown): value is Record<string, unknown> => value !== null && typeof value === 'object' && !Array.isArray(value);

export const parseQtDashboardMessage = (raw: string): QtDashboardMessage => {
  try {
    const message: unknown = JSON.parse(raw);
    if (!isObject(message)) return { type: 'error', message: 'Ignored non-object dashboard message.' };
    if (message.type !== 'DASHBOARD_UPDATE') return { type: 'error', message: `Ignored unknown dashboard message type: ${String(message.type)}.` };
    if (!QT_DASHBOARD_TOPICS.includes(message.topic as RealtimeTopic) || !isObject(message.data)) {
      return { type: 'error', message: 'Ignored dashboard update with an unknown topic or invalid data.' };
    }
    return { type: 'update', topic: message.topic as RealtimeTopic, data: message.data };
  } catch { return { type: 'error', message: 'Ignored invalid JSON received from dashboard server.' }; }
};

export interface WebSocketLike {
  readonly OPEN: number;
  readyState: number;
  onopen: ((event: Event) => void) | null;
  onmessage: ((event: MessageEvent<string>) => void) | null;
  onerror: ((event: Event) => void) | null;
  onclose: ((event: CloseEvent) => void) | null;
  send(value: string): void;
  close(): void;
}

export interface QtDashboardWebSocketOptions {
  url: string;
  factory?: (url: string) => WebSocketLike;
  reconnectBaseDelayMs?: number;
  reconnectMaxDelayMs?: number;
  onConnection: (state: string, detail: string) => void;
  onUpdate: (topic: RealtimeTopic, data: Record<string, unknown>) => void;
  onError: (message: string) => void;
}

export class QtDashboardWebSocket {
  private socket: WebSocketLike | null = null;
  private reconnectTimer: ReturnType<typeof setTimeout> | null = null;
  private attempt = 0;
  private shouldReconnect = true;
  private readonly factory: (url: string) => WebSocketLike;
  private readonly baseDelay: number;
  private readonly maxDelay: number;

  constructor(private readonly options: QtDashboardWebSocketOptions) {
    this.factory = options.factory ?? ((url) => new WebSocket(url));
    this.baseDelay = options.reconnectBaseDelayMs ?? 1_000;
    this.maxDelay = options.reconnectMaxDelayMs ?? 15_000;
  }

  connect(): void {
    if (!this.options.url) { this.options.onConnection('disabled', 'VITE_REALTIME_WS_URL is not configured.'); return; }
    if (this.socket) return;
    this.options.onConnection('connecting', this.options.url);
    try {
      const socket = this.factory(this.options.url);
      this.socket = socket;
      socket.onopen = () => { this.attempt = 0; this.options.onConnection('connected', this.options.url); this.subscribe(); };
      socket.onmessage = (event) => this.handleMessage(event.data);
      socket.onerror = () => this.options.onConnection('error', 'WebSocket transport error.');
      socket.onclose = () => { this.socket = null; this.options.onConnection('disconnected', 'Connection closed; realtime data is retained.'); this.scheduleReconnect(); };
    } catch (error) { this.options.onError(error instanceof Error ? error.message : 'WebSocket setup failed.'); this.scheduleReconnect(); }
  }

  disconnect(): void {
    this.shouldReconnect = false;
    if (this.reconnectTimer) clearTimeout(this.reconnectTimer);
    this.reconnectTimer = null;
    this.socket?.close();
    this.socket = null;
    this.options.onConnection('closed', 'Disconnected by user.');
  }

  private subscribe(): void {
    if (!this.socket || this.socket.readyState !== this.socket.OPEN) return;
    this.socket.send(JSON.stringify({ requestId: `DASH-${crypto.randomUUID?.() ?? Date.now()}`, type: 'DASHBOARD_SUBSCRIBE', payload: { topics: QT_DASHBOARD_TOPICS } }));
  }

  private handleMessage(raw: string): void {
    const message = parseQtDashboardMessage(raw);
    if (message.type === 'error') this.options.onError(message.message);
    else this.options.onUpdate(message.topic, message.data);
  }

  private scheduleReconnect(): void {
    if (!this.shouldReconnect || this.reconnectTimer) return;
    const delay = Math.min(this.baseDelay * 2 ** this.attempt++, this.maxDelay);
    this.options.onConnection('reconnecting', `Retrying in ${Math.round(delay / 1000)}s.`);
    this.reconnectTimer = setTimeout(() => { this.reconnectTimer = null; this.connect(); }, delay);
  }
}
