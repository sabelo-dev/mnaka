#pragma once
#include <QDateTime>
#include <QJsonObject>
#include <QMetaType>
#include <QStringList>
#include <QUuid>

namespace mnaka {
struct ToolIntentV1 {
    QString schemaVersion;
    QUuid taskId, stepId;
    QString engine, tool, toolVersion;
    QJsonObject arguments;
    QStringList resources;
    QString idempotencyKey;
    QDateTime deadlineUtc;
    QJsonObject toJson() const;
    static bool fromJson(const QJsonObject &json, ToolIntentV1 &out, QString &error);
};
// Deterministic canonical form for this contract subset, not an RFC 8785 claim.
QByteArray canonicalJson(const QJsonValue &value);
QString normalizedHash(const QJsonValue &value);
}
Q_DECLARE_METATYPE(mnaka::ToolIntentV1)
