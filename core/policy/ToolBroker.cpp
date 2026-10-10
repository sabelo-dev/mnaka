#include "ToolBroker.h"
#include "core/contracts/SchemaValidator.h"
#include <QJsonArray>
#include <QTimer>

namespace mnaka {
ToolBroker::ToolBroker(BrokerSession session, Environment environment, QObject *parent)
    : QObject(parent), m_session(std::move(session)), m_environment(environment) {
    qRegisterMetaType<ToolIntentV1>();
}
QString ToolBroker::validateIntent(const ToolIntentV1 &intent, QDateTime now) const {
    ToolIntentV1 checked; QString error;
    if (!ToolIntentV1::fromJson(intent.toJson(), checked, error)) return "INVALID_ARGUMENT: " + error;
    if (m_session.actorId.isNull() || m_session.deviceId.isNull() || m_session.executionHostId.isNull())
        return "UNAUTHENTICATED";
    if (intent.deadlineUtc <= now || intent.deadlineUtc > now.addSecs(600)) return "INVALID_ARGUMENT: deadline";
    if (m_cancelledTasks.contains(intent.taskId)) return "CANCELLED";
    if (intent.resources != QStringList{m_session.resourceId}) return "PERMISSION_DENIED: resource scope";
    if (intent.engine != "assistant" || (intent.tool != "demo.echo" && intent.tool != "demo.review"))
        return "PERMISSION_DENIED: tool is not registered";
    const QJsonObject argsSchema{{"type", "object"}, {"additionalProperties", false},
        {"required", QJsonArray{"text"}}, {"properties", QJsonObject{{"text", QJsonObject{
            {"type", "string"}, {"minLength", 1}, {"maxLength", 4000}}}}}};
    error = SchemaValidator::validate(intent.arguments, argsSchema);
    if (!error.isEmpty()) return "INVALID_ARGUMENT: " + error;
    if (m_environment != Environment::Fixture) return "PROVIDER_UNAVAILABLE: persistence and production tools disabled";
    return {};
}
QString ToolBroker::validatePlan(const QJsonObject &plan, QDateTime now) const {
    auto error = SchemaValidator::validate(plan, SchemaValidator::bundled("plan"));
    if (!error.isEmpty()) return "INVALID_ARGUMENT: " + error;
    const QUuid taskId(plan["task_id"].toString());
    QHash<QString, QStringList> dependencies;
    QSet<QString> keys;
    const auto steps = plan["steps"].toArray();
    for (auto value : steps) {
        const auto step = value.toObject();
        const auto id = QUuid(step["id"].toString()).toString(QUuid::WithoutBraces);
        if (dependencies.contains(id)) return "INVALID_ARGUMENT: duplicate step";
        ToolIntentV1 intent;
        if (!ToolIntentV1::fromJson(step["intent"].toObject(), intent, error)) return "INVALID_ARGUMENT: " + error;
        if (intent.taskId != taskId || intent.stepId.toString(QUuid::WithoutBraces) != id)
            return "INVALID_ARGUMENT: intent IDs do not match plan";
        if (!(error = validateIntent(intent, now)).isEmpty()) return error;
        if (keys.contains(intent.idempotencyKey.toLower())) return "INVALID_ARGUMENT: duplicate idempotency key";
        keys.insert(intent.idempotencyKey.toLower());
        QStringList parents;
        for (auto parent : step["depends_on"].toArray()) parents.append(QUuid(parent.toString()).toString(QUuid::WithoutBraces));
        dependencies.insert(id, parents);
    }
    for (const auto &parents : dependencies)
        for (const auto &parent : parents) if (!dependencies.contains(parent)) return "INVALID_ARGUMENT: unknown dependency";
    QSet<QString> resolved;
    while (resolved.size() < dependencies.size()) {
        const auto previous = resolved.size();
        for (auto it = dependencies.begin(); it != dependencies.end(); ++it) {
            bool ready = true;
            for (const auto &parent : it.value()) ready = ready && resolved.contains(parent);
            if (ready) resolved.insert(it.key());
        }
        if (resolved.size() == previous) return "INVALID_ARGUMENT: dependency cycle";
    }
    return {};
}
QString ToolBroker::submit(const ToolIntentV1 &intent) {
    auto error = validateIntent(intent, QDateTime::currentDateTimeUtc());
    if (!error.isEmpty()) return error;
    if (m_seenSteps.contains(intent.stepId) || m_seenKeys.contains(intent.idempotencyKey.toLower())) return "CONFLICT: duplicate submission";
    if (m_pending.size() >= 3 || m_seenSteps.size() >= 1000 || m_cancelledTasks.size() >= 1000
        || (intent.tool == "demo.review" && m_writeLocked)) return "RATE_LIMITED";
    Pending pending{intent, {}, normalizedHash(intent.toJson()), {}, m_session.resourceVersion, {}};
    m_seenSteps.insert(intent.stepId); m_seenKeys.insert(intent.idempotencyKey.toLower());
    if (intent.tool == "demo.review") {
        m_writeLocked = true;
        pending.approvalId = QUuid::createUuid();
        pending.nonce = QUuid::createUuid().toString(QUuid::WithoutBraces);
        pending.expires = qMin(QDateTime::currentDateTimeUtc().addSecs(300), intent.deadlineUtc);
        m_pending.insert(intent.stepId, pending);
        emit approvalRequested(intent.taskId, intent.stepId, {
            {"id", pending.approvalId.toString(QUuid::WithoutBraces)}, {"intentHash", pending.hash},
            {"nonce", pending.nonce}, {"resourceVersion", pending.resourceVersion},
            {"expiresAt", pending.expires.toString(Qt::ISODateWithMs)},
            {"destination", m_session.resourceId}, {"tool", intent.tool},
            {"effect", "Simulate review of a draft. No filesystem writes or permissions granted."},
            {"dataLeavingDevice", "None"}, {"fixture", true}});
        const int remaining = int(QDateTime::currentDateTimeUtc().msecsTo(pending.expires));
        QTimer::singleShot(qMax(1, remaining), this, [this, id = intent.stepId] {
            if (m_pending.contains(id)) finish(id, false, "APPROVAL_REQUIRED", "Fixture approval expired.");
        });
    } else {
        m_pending.insert(intent.stepId, pending);
        emit progress(intent.taskId, intent.stepId, {{"summary", "Fixture read queued"}});
        QTimer::singleShot(0, this, [this, id = intent.stepId] {
            if (!m_pending.contains(id)) return;
            if (m_pending[id].intent.deadlineUtc <= QDateTime::currentDateTimeUtc())
                finish(id, false, "CANCELLED", "Deadline elapsed.");
            else finish(id, true, "OK", "Validated fixture read completed; no OS access.");
        });
    }
    return {};
}
bool ToolBroker::decide(const QUuid &approvalId, const QString &intentHash, const QString &nonce, bool allow) {
    for (auto it = m_pending.begin(); it != m_pending.end(); ++it) {
        const auto &pending = it.value();
        if (pending.approvalId != approvalId || approvalId.isNull()) continue;
        if (pending.hash != intentHash || pending.nonce != nonce
            || pending.resourceVersion != m_session.resourceVersion
            || pending.hash != normalizedHash(pending.intent.toJson())) return false;
        const auto step = it.key();
        if (pending.expires <= QDateTime::currentDateTimeUtc()) {
            finish(step, false, "APPROVAL_REQUIRED", "Fixture approval expired."); return false;
        }
        // Consume by removing the pending record before emitting a terminal result.
        finish(step, allow, allow ? "OK" : "PERMISSION_DENIED",
            allow ? "Fixture draft review approved. No files changed." : "Fixture draft review denied. No files changed.");
        return true;
    }
    return false;
}
void ToolBroker::finish(const QUuid &stepId, bool ok, const QString &code, const QString &summary) {
    auto it = m_pending.find(stepId);
    if (it == m_pending.end()) return;
    const auto pending = it.value(); m_pending.erase(it);
    if (pending.intent.tool == "demo.review") m_writeLocked = false;
    emit result(pending.intent.taskId, stepId, {{"ok", ok}, {"code", code}, {"summary", summary}, {"fixture", true}});
}
void ToolBroker::cancel(const QUuid &taskId) {
    m_cancelledTasks.insert(taskId);
    const auto keys = m_pending.keys();
    for (const auto &id : keys)
        if (m_pending[id].intent.taskId == taskId) finish(id, false, "CANCELLED", "Fixture cancelled; no external effects.");
}
}
