import { describe, expect, it } from 'vitest';
import { DashboardApiError, HttpClient } from './client';

describe('HttpClient', () => {
  it('normalizes HTTP errors with endpoint and status', async () => {
    const client = new HttpClient('https://api.example.test/api/v1', 1000, async () => new Response(JSON.stringify({ error: { message: 'Unavailable' } }), { status: 503 }));
    await expect(client.get('/dashboard/overview')).rejects.toMatchObject({ type: 'http', statusCode: 503, endpoint: '/dashboard/overview', message: 'Unavailable' });
  });

  it('fails clearly when real mode has no API base URL', async () => {
    await expect(new HttpClient('', 1000).get('/dashboard/overview')).rejects.toMatchObject({ type: 'configuration' });
  });

  it('resolves a same-origin relative API base used by the Vite proxy', async () => {
    let requestedUrl = '';
    const client = new HttpClient('/api/v1', 1000, async (input) => {
      requestedUrl = String(input);
      return new Response(JSON.stringify({ data: {}, meta: {} }), { status: 200 });
    });

    await client.get('/dashboard/overview', { stationId: 1076 });

    const requested = new URL(requestedUrl);
    expect(requested.pathname).toBe('/api/v1/dashboard/overview');
    expect(requested.searchParams.get('stationId')).toBe('1076');
  });
});
