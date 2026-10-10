#pragma once
#include <QJsonObject>
#include <QString>

namespace mnaka {
// Validates the closed subset used by bundled contracts; never fetches remote refs.
class SchemaValidator {
public:
    static QString validate(const QJsonValue &value, const QJsonObject &schema,
                            const QString &path = "$", int depth = 0);
    static QJsonObject bundled(const QString &name);
};
}
