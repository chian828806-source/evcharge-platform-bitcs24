import { describe, expect, it } from 'vitest';
import { parseQtDashboardMessage } from './qtDashboardWebSocket';

describe('parseQtDashboardMessage', () => {
  it('accepts the Phase 1 DASHBOARD_UPDATE envelope', () => {
    expect(parseQtDashboardMessage('{"type":"DASHBOARD_UPDATE","topic":"summary","data":{"todayEnergyKwh":1}}'))
      .toEqual({ type: 'update', topic: 'summary', data: { todayEnergyKwh: 1 } });
  });

  it('rejects unknown topics without redefining the Qt protocol', () => {
    expect(parseQtDashboardMessage('{"type":"DASHBOARD_UPDATE","topic":"unknown","data":{}}').type).toBe('error');
  });
});
