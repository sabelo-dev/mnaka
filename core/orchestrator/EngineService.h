#pragma once
#include <QObject>
#include <QJsonObject>
#include <QUuid>
#include <QStringList>

namespace mnaka {
class EngineService : public QObject {
    Q_OBJECT
public:
    using QObject::QObject;
    virtual QString engineId() const = 0;
    virtual QStringList supportedTools() const = 0;
public slots:
    virtual void plan(QUuid taskId, QJsonObject context, QJsonObject request) = 0;
    virtual void cancel(QUuid taskId) = 0;
signals:
    void planReady(QUuid taskId, QJsonObject validatedPlan);
    void failed(QUuid taskId, QString errorCode, QString safeMessage);
    void responseReady(QUuid taskId, QString text, QJsonObject usage);
    void responseDelta(QUuid taskId, QString text);
};
}
