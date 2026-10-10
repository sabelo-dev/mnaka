#pragma once
#include "EngineService.h"
#include <QHash>
#include <QThread>
#include <QVariantList>
#include <QSet>

namespace mnaka {
class EngineRegistry : public QObject {
    Q_OBJECT
public:
    using QObject::QObject;
    ~EngineRegistry() override;
    // Ownership transfers only on success. Enabled services get a dedicated worker where threads exist.
    bool registerService(EngineService *service, bool enabled, const QString &status);
    QVariantList capabilities() const;
    QString submit(QString engineId, QUuid taskId, QJsonObject context, QJsonObject request);
    void cancel(QUuid taskId);
signals:
    void planReady(QUuid taskId, QJsonObject validatedPlan);
    void failed(QUuid taskId, QString code, QString safeMessage);
    void responseReady(QUuid taskId, QString text, QJsonObject usage);
    void responseDelta(QUuid taskId, QString text);
private:
    struct Entry { EngineService *service; QThread *worker; bool enabled; QString status; QStringList tools; };
    QHash<QString, Entry> m_entries;
    QHash<QUuid, QString> m_active;
    QSet<QUuid> m_seen;
};
}
