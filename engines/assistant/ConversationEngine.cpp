#include "ConversationEngine.h"
#include "core/contracts/ToolIntent.h"
#include "core/contracts/SchemaValidator.h"
#include <QJsonArray>

namespace mnaka {
ConversationEngine::ConversationEngine(ModelProvider *provider, QObject *parent) : EngineService(parent), m_provider(provider) {
    Q_ASSERT(provider && !provider->parent());
    provider->setParent(this);
    connect(m_provider, &ModelProvider::delta, this, &EngineService::responseDelta);
    connect(m_provider, &ModelProvider::failed, this, [this](QUuid taskId, QString code) {
        if (!m_contexts.contains(taskId)) return;
        m_contexts.remove(taskId); m_requests.remove(taskId);
        emit failed(taskId, code, code == "CANCELLED" ? "Planning cancelled."
            : code == "UNAUTHENTICATED" ? "Sign in to use the assistant."
            : code == "PROVIDER_UNAVAILABLE" ? "The model endpoint is not available."
            : "Model provider rejected input.");
    });
    connect(m_provider, &ModelProvider::completed, this, [this](QUuid taskId, QJsonObject result, QJsonObject usage) {
        if (!m_contexts.contains(taskId)) return;
        const auto context = m_contexts.take(taskId);
        const auto text = m_requests.take(taskId);
        const QJsonObject schema{{"type", "object"}, {"additionalProperties", false},
            {"required", QJsonArray{"schema_version", "text", "tool"}},
            {"properties", QJsonObject{{"schema_version", QJsonObject{{"type", "string"}, {"const", "1.0"}}},
                {"text", QJsonObject{{"type", "string"}, {"minLength", 1}, {"maxLength", 65536}}},
                {"tool", QJsonObject{{"type", "string"}, {"enum", QJsonArray{"demo.echo", "demo.review"}}}}}}};
        if (!SchemaValidator::validate(result, schema).isEmpty()) {
            emit failed(taskId, "INVALID_ARGUMENT", "Invalid provider output."); return;
        }
        ToolIntentV1 intent{"1.0", taskId, QUuid(context["step_id"].toString()), "assistant", result["tool"].toString(), "1.0",
            {{"text", text}}, {context["resource_id"].toString()}, QUuid::createUuid().toString(QUuid::WithoutBraces),
            QDateTime::fromString(context["deadline_utc"].toString(), Qt::ISODateWithMs)};
        QJsonObject plan{{"schema_version", "1.0"}, {"task_id", taskId.toString(QUuid::WithoutBraces)},
            {"steps", QJsonArray{QJsonObject{{"id", intent.stepId.toString(QUuid::WithoutBraces)}, {"intent", intent.toJson()},
                {"depends_on", QJsonArray{}}, {"required", true},
                {"postcondition", QJsonObject{{"predicate", "result.ok"}, {"arguments", QJsonObject{}}}}}}}};
        if (!SchemaValidator::validate(plan, SchemaValidator::bundled("plan")).isEmpty()) {
            emit failed(taskId, "INVALID_ARGUMENT", "Invalid fixture plan."); return;
        }
        emit responseReady(taskId, result["text"].toString(), usage);
        emit planReady(taskId, plan);
    });
}
void ConversationEngine::plan(QUuid taskId, QJsonObject context, QJsonObject request) {
    const QJsonObject contextSchema{{"type", "object"}, {"additionalProperties", false},
        {"required", QJsonArray{"step_id", "resource_id", "deadline_utc"}},
        {"properties", QJsonObject{{"step_id", QJsonObject{{"type", "string"}, {"format", "uuid"}}},
            {"resource_id", QJsonObject{{"type", "string"}, {"minLength", 1}, {"maxLength", 256}}},
            {"deadline_utc", QJsonObject{{"type", "string"}, {"format", "date-time"}}}}}};
    if (taskId.isNull() || !SchemaValidator::validate(context, contextSchema).isEmpty()
        || !SchemaValidator::validate(request, SchemaValidator::bundled("task-request")).isEmpty()) {
        emit failed(taskId, "INVALID_ARGUMENT", "Invalid planning request."); return;
    }
    if (m_contexts.contains(taskId)) return;
    const auto text = request["input"].toObject()["text"].toString();
    m_contexts.insert(taskId, context); m_requests.insert(taskId, text);
    if (!m_provider->start(taskId, {{"text", text}})) {
        m_contexts.remove(taskId); m_requests.remove(taskId);
        emit failed(taskId, "RATE_LIMITED", "Model provider capacity reached.");
    }
}
void ConversationEngine::cancel(QUuid taskId) { m_provider->cancel(taskId); }
}
