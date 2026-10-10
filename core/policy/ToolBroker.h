#pragma once
#include "core/contracts/ToolIntent.h"
#include <QObject>
#include <QHash>
#include <QSet>

namespace mnaka {
struct BrokerSession {
    QUuid actorId, deviceId, executionHostId;
    QString resourceId, resourceVersion;
};

// Only fixture tools are executable at this stage. Disabled is the default.
// No OS handles, arbitrary SQL or process-launch API is exposed.
class ToolBroker : public QObject {
    Q_OBJECT
public:
    enum class Environment { Disabled, Fixture };
    ToolBroker(BrokerSession session, Environment environment = Environment::Disabled, QObject *parent = nullptr);
    QString validatePlan(const QJsonObject &plan, QDateTime now = QDateTime::currentDateTimeUtc()) const;
    QString submit(const ToolIntentV1 &intent);
    bool decide(const QUuid &approvalId, const QString &intentHash, const QString &nonce, bool allow);
    void cancel(const QUuid &taskId);
signals:
    void approvalRequested(QUuid taskId, QUuid stepId, QJsonObject presentation);
    void progress(QUuid taskId, QUuid stepId, QJsonObject event);
    void result(QUuid taskId, QUuid stepId, QJsonObject result);
private:
    struct Pending {
        ToolIntentV1 intent;
        QUuid approvalId;
        QString hash, nonce, resourceVersion;
        QDateTime expires;
    };
    QString validateIntent(const ToolIntentV1 &intent, QDateTime now) const;
    void finish(const QUuid &stepId, bool ok, const QString &code, const QString &summary);
    BrokerSession m_session;
    Environment m_environment;
    QHash<QUuid, Pending> m_pending;
    QSet<QUuid> m_seenSteps, m_cancelledTasks;
    QSet<QString> m_seenKeys;
    bool m_writeLocked = false;
};
}
