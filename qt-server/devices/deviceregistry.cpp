#include "deviceregistry.h"
#include "devicesession.h"
#include "deviceprotocol.h"

DeviceRegistry::DeviceRegistry(QObject *parent) : QObject(parent) {}
void DeviceRegistry::registerSession(qint64 id, DeviceSession *s) { auto &d=m_devices[id]; if(d.session && d.session!=s) d.session->close(); d.pileId=id; d.session=s; d.managed=true; d.lastHeartbeat=QDateTime::currentDateTime(); }
void DeviceRegistry::unregisterSession(DeviceSession *s) { for(auto i=m_devices.begin();i!=m_devices.end();++i) if(i->session==s) { i->session=nullptr; emit becameOffline(i.key()); return; } }
void DeviceRegistry::heartbeat(qint64 id) { if(m_devices.contains(id)) m_devices[id].lastHeartbeat=QDateTime::currentDateTime(); }
void DeviceRegistry::telemetry(qint64 id,const QJsonObject &t) { if(m_devices.contains(id)) m_devices[id].telemetry=t; }
void DeviceRegistry::fault(qint64 id,const QString &c,const QString &m) { if(m_devices.contains(id)) {m_devices[id].faultCode=c;m_devices[id].faultMessage=m;} }
void DeviceRegistry::clearFault(qint64 id) { if(m_devices.contains(id)) {m_devices[id].faultCode.clear();m_devices[id].faultMessage.clear();} }
bool DeviceRegistry::isManaged(qint64 id) const { return m_devices.value(id).managed; }
bool DeviceRegistry::isOnline(qint64 id) const { const auto d=m_devices.value(id); return d.session && d.lastHeartbeat.secsTo(QDateTime::currentDateTime()) <= DeviceProtocol::HeartbeatTimeoutSeconds; }
DeviceSession *DeviceRegistry::session(qint64 id) const { return isOnline(id) ? m_devices.value(id).session : nullptr; }
QJsonObject DeviceRegistry::liveFields(qint64 id) const { const auto d=m_devices.value(id); return {{QStringLiteral("deviceOnline"),isOnline(id)},{QStringLiteral("lastHeartbeatAt"),d.lastHeartbeat.isValid()?d.lastHeartbeat.toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")):QString()},{QStringLiteral("measuredPowerKw"),d.telemetry.value(QStringLiteral("powerKw"))},{QStringLiteral("temperatureC"),d.telemetry.value(QStringLiteral("temperatureC"))},{QStringLiteral("faultCode"),d.faultCode},{QStringLiteral("faultMessage"),d.faultMessage}}; }
QList<qint64> DeviceRegistry::expiredPiles() const { QList<qint64> r; for(auto i=m_devices.cbegin();i!=m_devices.cend();++i) if(i->managed && !isOnline(i.key())) r.append(i.key()); return r; }
