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
});
