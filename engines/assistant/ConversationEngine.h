#pragma once
#include "core/orchestrator/EngineService.h"
#include "adapters/models/ModelProvider.h"
#include <QHash>

namespace mnaka {
class ConversationEngine : public EngineService {
    Q_OBJECT
public:
    // Takes ownership of an unparented provider so both move to the planning thread together.
    explicit ConversationEngine(ModelProvider *provider, QObject *parent = nullptr);
    QString engineId() const override { return "assistant"; }
    QStringList supportedTools() const override { return {"demo.echo", "demo.review"}; }
public slots:
    void plan(QUuid taskId, QJsonObject context, QJsonObject request) override;
    void cancel(QUuid taskId) override;
private:
    ModelProvider *m_provider;
    QHash<QUuid, QJsonObject> m_contexts;
    QHash<QUuid, QString> m_requests;
};
// Explicitly unavailable engines advertise scope without pretending to implement it.
class UnavailableEngine : public EngineService {
    Q_OBJECT
public:
    explicit UnavailableEngine(QString id) : m_id(std::move(id)) {}
    QString engineId() const override { return m_id; }
    QStringList supportedTools() const override { return {}; }
public slots:
    void plan(QUuid id, QJsonObject, QJsonObject) override { emit failed(id, "PROVIDER_UNAVAILABLE", "Engine integration pending."); }
    void cancel(QUuid) override {}
private:
    QString m_id;
};
}
