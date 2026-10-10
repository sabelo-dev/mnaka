#pragma once
#include "adapters/models/ModelProvider.h"
#include <QHash>
#include <QSet>
#include <QTimer>

namespace mnaka {
class MockModelProvider : public ModelProvider {
    Q_OBJECT
public:
    using ModelProvider::ModelProvider;
public slots:
    bool start(QUuid requestId, QJsonObject request) override;
    void cancel(QUuid requestId) override;
private:
    struct Pending { QJsonObject request; QTimer *timer; int ticks = 0; };
    QHash<QUuid, Pending> m_pending;
    QSet<QUuid> m_seen;
};
}
