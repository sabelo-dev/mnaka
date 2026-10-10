#include "SkillValidator.h"
#include "core/contracts/SchemaValidator.h"
#include "core/contracts/ToolIntent.h"
#include <QJsonArray>
#include <QVersionNumber>
#include <QRegularExpression>

namespace mnaka {
QString SkillValidator::payloadHash(QJsonObject manifest)
{
    manifest.remove("artifact_hash");
    return "sha256:" + normalizedHash(manifest);
}

QString SkillValidator::validate(const QJsonObject &manifest, const QStringList &grants,
                                 const QString &hostVersion)
{
    auto error = inspect(manifest, hostVersion);
    if (!error.isEmpty()) return error;
    for (const auto &permission : manifest.value("permissions").toArray())
        if (!grants.contains(permission.toString())) return "PERMISSION_DENIED: skill exceeds granted tools";
    return {};
}

QString SkillValidator::inspect(const QJsonObject &manifest, const QString &hostVersion)
{
    auto error = SchemaValidator::validate(manifest, SchemaValidator::bundled("skill"));
    if (!error.isEmpty()) return error;
    if (payloadHash(manifest) != manifest.value("artifact_hash").toString())
        return "CONFLICT: skill payload hash does not match";
    qsizetype suffix = 0;
    auto host = QVersionNumber::fromString(hostVersion, &suffix);
    if (host.segmentCount() != 3 || suffix != hostVersion.size())
        return "INVALID_ARGUMENT: host version must contain three numeric segments";
    if (host < QVersionNumber::fromString(manifest.value("min_host_version").toString()))
        return "INVALID_ARGUMENT: skill requires a newer host";
    const auto permissions = manifest.value("permissions").toArray();
    const auto inputSchema = manifest.value("inputs_schema").toObject();
    const auto properties = inputSchema.value("properties").toObject();
    const auto required = inputSchema.value("required").toArray();
    if (properties.size() > 16) return "INVALID_ARGUMENT: too many inputs";
    for (auto it = properties.begin(); it != properties.end(); ++it) {
        if (!QRegularExpression("^[a-zA-Z_][a-zA-Z0-9_]{0,63}$").match(it.key()).hasMatch())
            return "INVALID_ARGUMENT: invalid input name";
        auto definition = it.value().toObject();
        const auto type = definition.value("type").toString();
        if ((definition.contains("format") || definition.contains("minLength") || definition.contains("maxLength"))
            && type != "string") return "INVALID_ARGUMENT: string constraints require string input";
        if ((definition.contains("minimum") || definition.contains("maximum")) && type != "integer")
            return "INVALID_ARGUMENT: numeric constraints require integer input";
        if (definition.contains("minimum") && definition.contains("maximum")
            && definition.value("minimum").toDouble() > definition.value("maximum").toDouble())
            return "INVALID_ARGUMENT: inconsistent input bounds";
        if (definition.contains("minLength") && definition.contains("maxLength")
            && definition.value("minLength").toInt() > definition.value("maxLength").toInt())
            return "INVALID_ARGUMENT: inconsistent input lengths";
    }
    for (const auto &name : required)
        if (!properties.contains(name.toString())) return "INVALID_ARGUMENT: required input is undefined";
    const auto workflow = manifest.value("workflow").toArray();
    if (workflow.size() > manifest.value("limits").toObject().value("max_steps").toInt())
        return "INVALID_ARGUMENT: workflow exceeds step limit";
    for (const auto &value : workflow) {
        auto step = value.toObject();
        if (!permissions.contains(step.value("tool"))) return "PERMISSION_DENIED: workflow tool is undeclared";
        // The initial closed catalogue supports only build.status and UUID project inputs.
        const auto reference = step.value("args").toObject().value("project_id").toString();
        const auto name = reference.mid(8, reference.size() - 9);
        const auto property = properties.value(name).toObject();
        if (!required.contains(name) || property.value("type") != "string"
            || property.value("format") != "uuid")
            return "INVALID_ARGUMENT: project input must be required and UUID typed";
    }
    return {};
}

QString SkillValidator::bindInputs(const QJsonObject &manifest, const QJsonObject &inputs,
                                   const QStringList &grants, const QString &hostVersion,
                                   QJsonArray &steps)
{
    steps = {};
    auto error = validate(manifest, grants, hostVersion);
    if (!error.isEmpty()) return error;
    error = SchemaValidator::validate(inputs, manifest.value("inputs_schema").toObject());
    if (!error.isEmpty()) return error;
    QJsonArray bound;
    for (const auto &value : manifest.value("workflow").toArray()) {
        auto step = value.toObject();
        auto args = step.value("args").toObject();
        const auto reference = args.value("project_id").toString();
        args["project_id"] = inputs.value(reference.mid(8, reference.size() - 9));
        step["args"] = args;
        bound.append(step);
    }
    steps = bound;
    return {};
}
}
