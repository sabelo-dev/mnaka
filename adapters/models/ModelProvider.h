#pragma once
#include <QObject>
#include <QJsonObject>
#include <QUuid>

namespace mnaka {
class ModelProvider : public QObject {
    Q_OBJECT
public:
    using QObject::QObject;
public slots:
    // false means not accepted (duplicate ID or bounded capacity); no terminal event.
    virtual bool start(QUuid requestId, QJsonObject request) = 0;
    virtual void cancel(QUuid requestId) = 0;
signals:
    void delta(QUuid requestId, QString text);
    void completed(QUuid requestId, QJsonObject structuredResult, QJsonObject usage);
    void failed(QUuid requestId, QString code);
};
}
