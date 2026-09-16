export interface DashboardApiErrorShape {
  type: 'configuration' | 'network' | 'http' | 'timeout' | 'parse';
  message: string;
  statusCode: number | null;
  endpoint: string;
}

export class DashboardApiError extends Error implements DashboardApiErrorShape {
  constructor(
    public readonly type: DashboardApiErrorShape['type'],
    message: string,
    public readonly statusCode: number | null,
    public readonly endpoint: string
  ) {
    super(message);
    this.name = 'DashboardApiError';
  }
}

type FetchLike = typeof fetch;

export class HttpClient {
  constructor(
    private readonly baseUrl: string,
    private readonly timeoutMs: number,
    // Chromium 的原生 fetch 作为对象成员调用时必须保留 Window/globalThis 上下文。
    private readonly fetchImpl: FetchLike = globalThis.fetch.bind(globalThis)
  ) {}

  async get<T>(endpoint: string, query?: Record<string, string | number | undefined>): Promise<T> {
    if (!this.baseUrl) throw new DashboardApiError('configuration', 'VITE_API_BASE_URL is required in real mode.', null, endpoint);
    // Production may use an absolute Flask URL, while local Vite development uses
    // the same-origin `/api/v1` proxy. Resolve both forms against the browser origin.
    const origin = globalThis.location?.origin ?? 'http://localhost';
    const base = new URL(`${this.baseUrl.replace(/\/$/, '')}/`, origin);
    const url = new URL(endpoint.replace(/^\//, ''), base);
    Object.entries(query ?? {}).forEach(([key, value]) => {
      if (value !== undefined) url.searchParams.set(key, String(value));
    });
    const controller = new AbortController();
    const timer = setTimeout(() => controller.abort(), this.timeoutMs);
    try {
      const response = await this.fetchImpl(url, { method: 'GET', headers: { Accept: 'application/json' }, signal: controller.signal });
      const body = await response.text();
      const parsed = body ? this.parseJson(body, endpoint) : null;
      if (!response.ok) {
        const message = this.messageFrom(parsed) ?? `HTTP ${response.status}`;
        throw new DashboardApiError('http', message, response.status, endpoint);
      }
      return parsed as T;
    } catch (error) {
      if (error instanceof DashboardApiError) throw error;
      if (error instanceof DOMException && error.name === 'AbortError') {
        throw new DashboardApiError('timeout', `Request timed out after ${this.timeoutMs}ms.`, null, endpoint);
      }
      throw new DashboardApiError('network', error instanceof Error ? error.message : 'Network request failed.', null, endpoint);
    } finally {
      clearTimeout(timer);
    }
  }

  private parseJson(value: string, endpoint: string): unknown {
    try { return JSON.parse(value); } catch { throw new DashboardApiError('parse', 'Response is not valid JSON.', null, endpoint); }
  }

  private messageFrom(value: unknown): string | null {
    if (!value || typeof value !== 'object') return null;
    const error = (value as { error?: { message?: unknown } }).error;
    return typeof error?.message === 'string' ? error.message : null;
  }
}
