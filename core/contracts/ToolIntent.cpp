#include "ToolIntent.h"
#include "SchemaValidator.h"
#include <QJsonDocument>
#include <QJsonArray>
#include <QCryptographicHash>
#include <algorithm>

namespace mnaka {
QByteArray canonicalJson(const QJsonValue &v) {
    if (v.isObject()) {
        auto object = v.toObject(); auto keys = object.keys();
        std::sort(keys.begin(), keys.end());
        QByteArray result = "{";
        for (const auto &key : keys) {
            if (result.size() > 1) result += ',';
            result += canonicalJson(key) + ':' + canonicalJson(object[key]);
        }
        return result + '}';
    }
    if (v.isArray()) {
        QByteArray result = "[";
        for (const auto item : v.toArray()) {
            if (result.size() > 1) result += ',';
            result += canonicalJson(item);
        }
        return result + ']';
    }
    QByteArray scalar = QJsonDocument(QJsonArray{v}).toJson(QJsonDocument::Compact);
    return scalar.mid(1, scalar.size() - 2);
}
QString normalizedHash(const QJsonValue &v) {
    return QString::fromLatin1(QCryptographicHash::hash(canonicalJson(v), QCryptographicHash::Sha256).toHex());
}
QJsonObject ToolIntentV1::toJson() const {
    return {{"schema_version", schemaVersion}, {"task_id", taskId.toString(QUuid::WithoutBraces)},
        {"step_id", stepId.toString(QUuid::WithoutBraces)}, {"engine", engine}, {"tool", tool},
        {"tool_version", toolVersion}, {"arguments", arguments},
        {"resources", QJsonArray::fromStringList(resources)}, {"idempotency_key", idempotencyKey},
        {"deadline_utc", deadlineUtc.toUTC().toString(Qt::ISODateWithMs)}};
}
bool ToolIntentV1::fromJson(const QJsonObject &json, ToolIntentV1 &out, QString &error) {
    error = SchemaValidator::validate(json, SchemaValidator::bundled("intent"));
    if (!error.isEmpty()) return false;
    ToolIntentV1 value;
    value.schemaVersion = json["schema_version"].toString();
    value.taskId = QUuid(json["task_id"].toString()); value.stepId = QUuid(json["step_id"].toString());
    value.engine = json["engine"].toString(); value.tool = json["tool"].toString();
    value.toolVersion = json["tool_version"].toString(); value.arguments = json["arguments"].toObject();
    for (const auto resource : json["resources"].toArray()) value.resources.append(resource.toString());
    value.idempotencyKey = json["idempotency_key"].toString();
    value.deadlineUtc = QDateTime::fromString(json["deadline_utc"].toString(), Qt::ISODateWithMs).toUTC();
    if (value.taskId.isNull() || value.stepId.isNull()) { error = "Null identifiers are forbidden"; return false; }
    out = value;
    return true;
}
}
