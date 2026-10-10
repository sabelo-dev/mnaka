#include "SchemaValidator.h"
#include <QJsonArray>
#include <QJsonDocument>
#include <QFile>
#include <QDateTime>
#include <QRegularExpression>
#include <cmath>

static void registerSchemas() { Q_INIT_RESOURCE(contracts); }
static void initializeSchemas() {
    static const bool initialized = [] { registerSchemas(); return true; }();
    Q_UNUSED(initialized)
}
namespace mnaka {
QJsonObject SchemaValidator::bundled(const QString &name) {
    initializeSchemas();
    if (!QRegularExpression("^[a-z-]+$").match(name).hasMatch()) return {};
    QFile file(":/mnaka/schemas/" + name + ".schema.json");
    if (!file.open(QIODevice::ReadOnly)) return {};
    return QJsonDocument::fromJson(file.readAll()).object();
}
QString SchemaValidator::validate(const QJsonValue &v, const QJsonObject &s,
                                  const QString &path, int depth) {
    if (s.isEmpty() || depth > 40) return path + ": invalid schema or nesting limit";
    const QStringList supported{"$schema", "$id", "type", "const", "enum", "properties", "required",
        "additionalProperties", "minItems", "maxItems", "uniqueItems", "items", "minLength", "maxLength",
        "pattern", "format", "minimum", "maximum", "description", "title"};
    for (auto it = s.begin(); it != s.end(); ++it)
        if (!supported.contains(it.key())) return path + ": unsupported schema keyword " + it.key();
    const QString type = s["type"].toString();
    bool matches = (type == "object" && v.isObject()) || (type == "array" && v.isArray())
        || (type == "string" && v.isString()) || (type == "boolean" && v.isBool())
        || (type == "null" && v.isNull()) || (type == "number" && v.isDouble())
        || (type == "integer" && v.isDouble() && std::isfinite(v.toDouble()) && std::floor(v.toDouble()) == v.toDouble());
    if (!matches) return path + ": expected " + type;
    if (s.contains("const") && v != s["const"]) return path + ": unexpected constant";
    if (s.contains("enum") && !s["enum"].toArray().contains(v)) return path + ": value not allowed";
    if (v.isObject()) {
        const auto object = v.toObject();
        const auto properties = s["properties"].toObject();
        for (const auto required : s["required"].toArray())
            if (!object.contains(required.toString())) return path + ": missing " + required.toString();
        for (auto it = object.begin(); it != object.end(); ++it) {
            if (!properties.contains(it.key())) {
                if (s["additionalProperties"].isBool() && !s["additionalProperties"].toBool())
                    return path + ": unknown field " + it.key();
                if (s["additionalProperties"].isObject()) {
                    auto error = validate(it.value(), s["additionalProperties"].toObject(), path + "." + it.key(), depth + 1);
                    if (!error.isEmpty()) return error;
                }
            } else {
                auto error = validate(it.value(), properties[it.key()].toObject(), path + "." + it.key(), depth + 1);
                if (!error.isEmpty()) return error;
            }
        }
    } else if (v.isArray()) {
        auto array = v.toArray();
        if (array.size() < s["minItems"].toInt(0) || array.size() > s["maxItems"].toInt(10000))
            return path + ": array limit";
        for (int i = 0; i < array.size(); ++i) {
            if (s["uniqueItems"].toBool())
                for (int j = 0; j < i; ++j) if (array[i] == array[j]) return path + ": duplicate item";
            auto error = validate(array[i], s["items"].toObject(), path + "[" + QString::number(i) + "]", depth + 1);
            if (!error.isEmpty()) return error;
        }
    } else if (v.isString()) {
        const auto text = v.toString();
        if (text.size() < s["minLength"].toInt(0) || text.size() > s["maxLength"].toInt(65536))
            return path + ": string limit";
        if (s.contains("pattern") && !QRegularExpression(s["pattern"].toString()).match(text).hasMatch())
            return path + ": invalid pattern";
        const auto format = s["format"].toString();
        if (!format.isEmpty() && format != "uuid" && format != "date-time") return path + ": unsupported format";
        if (format == "uuid" && !QRegularExpression("^[0-9a-fA-F]{8}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{12}$").match(text).hasMatch())
            return path + ": invalid UUID";
        if (format == "date-time" && (!QRegularExpression("^\\d{4}-\\d{2}-\\d{2}T\\d{2}:\\d{2}:\\d{2}(\\.\\d+)?(Z|[+-]\\d{2}:\\d{2})$").match(text).hasMatch()
            || !QDateTime::fromString(text, Qt::ISODateWithMs).isValid())) return path + ": invalid timestamp";
    } else if (v.isDouble()) {
        const double number = v.toDouble();
        if (!std::isfinite(number) || (s.contains("minimum") && number < s["minimum"].toDouble())
            || (s.contains("maximum") && number > s["maximum"].toDouble())) return path + ": numeric limit";
    }
    return {};
}
}
