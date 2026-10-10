#pragma once
#include <QJsonObject>
#include <QStringList>

namespace mnaka {
// Validation only. This class cannot install skills or dispatch tools.
class SkillValidator {
public:
    static QString payloadHash(QJsonObject manifest);
    static QString inspect(const QJsonObject &manifest, const QString &hostVersion);
    static QString validate(const QJsonObject &manifest, const QStringList &grantedTools,
                            const QString &hostVersion);
    static QString bindInputs(const QJsonObject &manifest, const QJsonObject &inputs,
                              const QStringList &grantedTools, const QString &hostVersion,
                              QJsonArray &steps);
};
}
