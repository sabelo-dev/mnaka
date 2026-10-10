#include "TaskOrchestrator.h"
#include "TaskStates.h"
#include "core/contracts/SchemaValidator.h"
#include <QJsonArray>

namespace mnaka {
namespace {
QString idText(const QUuid &id) { return id.toString(QUuid::WithoutBraces); }
QString errorCode(const QString &error) {
    static const QStringList codes{"INVALID_ARGUMENT", "UNAUTHENTICATED", "PERMISSION_DENIED", "APPROVAL_REQUIRED",
        "CONFLICT", "DEVICE_OFFLINE", "PROVIDER_UNAVAILABLE", "RATE_LIMITED", "BUDGET_EXCEEDED", "CANCELLED", "INTERNAL"};
    const auto code = error.section(':', 0, 0).trimmed();
    return codes.contains(code) ? code : "INTERNAL";
}
QJsonObject errorEnvelope(const QString &code, const QString &message) {
    return {{"code", code}, {"message", message}, {"request_id", idText(QUuid::createUuid())},
        {"retryable", QStringList{"DEVICE_OFFLINE", "PROVIDER_UNAVAILABLE", "RATE_LIMITED"}.contains(code)}};
}
bool inFlight(const QString &stepState) { return stepState == "running" || stepState == "awaiting_approval"; }
}

TaskOrchestrator::TaskOrchestrator(BrokerSession session, EngineRegistry *registry, ToolBroker *broker,
                                   TaskStore *store, QObject *parent)
    : QObject(parent), m_session(std::move(session)), m_registry(registry), m_broker(broker), m_store(store) {
    connect(registry, &EngineRegistry::planReady, this, &TaskOrchestrator::onPlanReady);
    connect(registry, &EngineRegistry::failed, this, &TaskOrchestrator::onEngineFailed);
    connect(registry, &EngineRegistry::responseDelta, this, [this](QUuid taskId, QString text) {
        const auto task = m_active.value(taskId);
        if (task && task->record.state == "planning") emit responseDelta(taskId, text);
    });
    // The reply is held until the plan passes policy, so no answer outlives a rejected plan.
    connect(registry, &EngineRegistry::responseReady, this, [this](QUuid taskId, QString text, QJsonObject usage) {
        const auto task = m_active.value(taskId);
        if (task && task->record.state == "planning") { task->reply = text; task->usage = usage; }
    });
    connect(broker, &ToolBroker::approvalRequested, this, &TaskOrchestrator::onApprovalRequested);
    connect(broker, &ToolBroker::progress, this, &TaskOrchestrator::onProgress);
    connect(broker, &ToolBroker::result, this, &TaskOrchestrator::onResult);
}

QJsonObject TaskOrchestrator::event(const Task &task, const QString &type, QJsonObject payload) const {
    payload["schema_version"] = "1.0";
    payload["task_id"] = idText(task->record.id);
    return {{"event_id", idText(QUuid::createUuid())}, {"task_id", idText(task->record.id)},
        {"sequence", ++task->sequence}, {"type", type},
        {"occurred_at", QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)}, {"payload", payload}};
}

bool TaskOrchestrator::save(const Task &task, const QList<QJsonObject> &events) {
    static const QJsonObject schema = SchemaValidator::bundled("event");
    const auto id = task->record.id;
    bool valid = true;
    for (const auto &item : events) valid = valid && SchemaValidator::validate(item, schema).isEmpty();
    task->record.updatedAt = QDateTime::currentDateTimeUtc();
    if (!valid || !m_store->commit(task->record, events)) {
        // Fail closed: stop work now. The last committed state stands until recover().
        if (m_active.contains(id)) {
            release(task);
            m_registry->cancel(id);
            if (!task->steps.isEmpty()) m_broker->cancel(id);
        }
        emit taskChanged(id);
        return false;
    }
    for (const auto &item : events) emit eventAppended(id, item);
    emit taskChanged(id);
    return true;
}

bool TaskOrchestrator::transition(const Task &task, const QString &to, QList<QJsonObject> events,
                                  const QString &code, const QString &summary) {
    const auto from = task->record.state;
    if (!m_active.contains(task->record.id) || !canTransition(from, to)) return false;
    QJsonObject payload{{"previous_state", from}, {"new_state", to}};
    if (!code.isEmpty()) payload["code"] = code;
    if (!summary.isEmpty()) payload["summary"] = summary.left(4096);
    task->record.state = to;
    events.append(event(task, "task.state_changed", payload));
    if (isTerminal(to)) events.append(event(task, "task.completed", payload));
    if (isTerminal(to) || to == "interrupted") release(task);
    return save(task, events);
}

void TaskOrchestrator::release(const Task &task) {
    if (task->deadline) { task->deadline->stop(); task->deadline->deleteLater(); task->deadline = nullptr; }
    m_active.remove(task->record.id);
}

void TaskOrchestrator::setStep(const Task &task, Step &step, const QString &state) {
    step.state = state;
    task->record.steps[idText(step.id)] = state;
}

void TaskOrchestrator::fail(const Task &task, const QString &error, const QString &message) {
    const auto id = task->record.id;
    if (!m_active.contains(id)) return;
    const auto code = errorCode(error);
    auto text = message.isEmpty() ? error.section(':', 1).trimmed() : message;
    if (text.isEmpty()) text = "The task could not be completed.";
    text = text.left(4096);
    const bool planning = task->record.state == "planning";
    bool dispatched = false;
    for (auto &step : task->steps) {
        if (inFlight(step.state)) { dispatched = true; setStep(task, step, "cancelled"); }
        else if (step.state == "queued") setStep(task, step, "skipped");
    }
    task->record.error = errorEnvelope(code, text);
    transition(task, "failed", {}, code, text);
    // The task is already settled, so late engine or broker signals are ignored.
    if (planning) m_registry->cancel(id);
    if (dispatched) m_broker->cancel(id);
}

QString TaskOrchestrator::submit(const QJsonObject &request, QUuid &taskId) {
    taskId = QUuid();
    if (m_session.actorId.isNull() || m_session.deviceId.isNull() || m_session.executionHostId.isNull())
        return "UNAUTHENTICATED";
    if (!m_store || !m_store->available()) return "PROVIDER_UNAVAILABLE: task store unavailable; execution disabled";
    const auto invalid = SchemaValidator::validate(request, SchemaValidator::bundled("task-request"));
    if (!invalid.isEmpty()) return "INVALID_ARGUMENT: " + invalid;
    // A task runs only on the host it names; it is never silently rerouted.
    if (QUuid(request["execution_host_id"].toString()) != m_session.executionHostId)
        return "DEVICE_OFFLINE: the requested execution host is not this device";
    const auto key = request["idempotency_key"].toString().toLower();
    auto normalized = request;
    normalized.remove("idempotency_key");
    const auto hash = normalizedHash(normalized);
    TaskRecord existing;
    if (m_store->findByKey(key, existing)) {
        if (existing.requestHash != hash) return "CONFLICT: idempotency key reused with a different request";
        taskId = existing.id;
        return {};
    }
    if (m_active.size() >= 16) return "RATE_LIMITED: too many active tasks";
    const auto id = QUuid::createUuid();
    const auto now = QDateTime::currentDateTimeUtc();
    auto task = std::make_shared<Active>();
    // Modes select workflows, not privileges. Conversation is the only planning engine so far.
    task->record = {id, "assistant", request["mode"].toString(), "queued", key, hash, request, {}, {}, {}, now, now};
    m_active.insert(id, task);
    if (!save(task, {event(task, "task.created", {{"new_state", "queued"}})}))
        return "PROVIDER_UNAVAILABLE: task store unavailable; execution disabled";
    taskId = id;
    if (!m_active.contains(id)) return {};
    task->deadline = new QTimer(this);
    task->deadline->setSingleShot(true);
    connect(task->deadline, &QTimer::timeout, this, [this, id] {
        const auto task = m_active.value(id);
        if (!task) return;
        if (task->record.state != "cancelling") {
            fail(task, "CANCELLED", "The execution deadline elapsed before the task finished.");
            return;
        }
        // Cancellation was never confirmed: report it for review instead of claiming a clean stop.
        for (auto &step : task->steps) if (inFlight(step.state)) setStep(task, step, "interrupted");
        transition(task, "interrupted", {}, "CANCELLED", "Cancellation was not confirmed before the deadline; review is required.");
    });
    task->deadline->start(m_deadlineMs);
    if (!transition(task, "planning")) return {};
    const QJsonObject context{{"step_id", idText(QUuid::createUuid())}, {"resource_id", m_session.resourceId},
        {"deadline_utc", now.addMSecs(m_deadlineMs).toString(Qt::ISODateWithMs)}};
    const auto error = m_registry->submit(task->record.engine, id, context, request);
    if (!error.isEmpty()) fail(task, error);
    return {};
}

void TaskOrchestrator::onPlanReady(QUuid taskId, QJsonObject plan) {
    const auto task = m_active.value(taskId);
    if (!task || task->record.state != "planning") return;
    const auto error = m_broker->validatePlan(plan);
    if (!error.isEmpty()) { fail(task, error); return; }
    for (const auto value : plan["steps"].toArray()) {
        const auto object = value.toObject();
        Step step;
        QString ignored; // validatePlan has already checked every intent
        ToolIntentV1::fromJson(object["intent"].toObject(), step.intent, ignored);
        step.id = step.intent.stepId;
        step.required = object["required"].toBool();
        for (const auto dependency : object["depends_on"].toArray()) step.dependencies.append(QUuid(dependency.toString()));
        task->steps.append(step);
        setStep(task, task->steps.last(), "queued");
    }
    task->record.plan = plan;
    if (!transition(task, "running")) return;
    if (!task->reply.isEmpty()) emit responseReady(taskId, task->reply, task->usage);
    if (m_active.contains(taskId)) pump(task);
}

void TaskOrchestrator::pump(const Task &task) {
    const auto id = task->record.id;
    for (bool changed = true; changed;) {
        changed = false;
        for (auto &step : task->steps) {
            if (!m_active.contains(id) || task->record.state != "running") return;
            if (step.state != "queued") continue;
            bool ready = true, blocked = false;
            for (const auto &dependency : step.dependencies)
                for (const auto &other : std::as_const(task->steps)) {
                    if (other.id != dependency) continue;
                    if (other.state == "failed" || other.state == "skipped" || other.state == "cancelled") blocked = true;
                    else if (other.state != "succeeded") ready = false;
                }
            if (blocked) {
                setStep(task, step, "skipped");
                changed = true;
                if (step.required) {
                    fail(task, "INTERNAL", "A required step could not run because a step it depends on did not succeed.");
                    return;
                }
                continue;
            }
            if (!ready) continue;
            setStep(task, step, "running");
            if (!step.started) {
                step.started = true;
                if (!save(task, {event(task, "step.started", {{"step_id", idText(step.id)}, {"summary", step.intent.tool}})})) return;
                if (!m_active.contains(id) || task->record.state != "running") return;
            }
            // An approval request can arrive synchronously from inside submit().
            const auto error = m_broker->submit(step.intent);
            if (!m_active.contains(id)) return;
            if (error.startsWith("RATE_LIMITED")) { setStep(task, step, "queued"); continue; } // retried when capacity frees
            if (error.isEmpty()) continue;
            setStep(task, step, "failed");
            changed = true;
            const auto detail = error.section(':', 1).trimmed();
            if (!save(task, {event(task, "step.completed", {{"step_id", idText(step.id)}, {"code", errorCode(error)},
                    {"summary", detail.isEmpty() ? "The broker rejected this step." : detail.left(4096)}})})) return;
            if (step.required) { fail(task, error); return; }
        }
    }
    if (!m_active.contains(id) || task->record.state != "running") return;
    for (const auto &step : std::as_const(task->steps))
        if (step.state == "queued" || inFlight(step.state)) return;
    transition(task, "succeeded", {}, "OK", "All required steps met their postconditions.");
}

void TaskOrchestrator::onApprovalRequested(QUuid taskId, QUuid stepId, QJsonObject presentation) {
    const auto task = m_active.value(taskId);
    if (!task || task->record.state != "running") return;
    for (auto &step : task->steps) {
        if (step.id != stepId || step.state != "running") continue;
        setStep(task, step, "awaiting_approval");
        task->approvalId = QUuid(presentation["id"].toString());
        auto effect = presentation["effect"].toString().left(4096);
        if (effect.isEmpty()) effect = step.intent.tool;
        if (!transition(task, "awaiting_approval", {event(task, "approval.requested", {{"step_id", idText(stepId)},
                {"approval_id", idText(task->approvalId)}, {"summary", effect}})})) return;
        presentation["taskId"] = idText(taskId);
        presentation["stepId"] = idText(stepId);
        if (m_active.contains(taskId)) emit approvalRequested(taskId, presentation);
        return;
    }
}

void TaskOrchestrator::onProgress(QUuid taskId, QUuid stepId, QJsonObject progress) {
    const auto task = m_active.value(taskId);
    const auto summary = progress["summary"].toString().left(4096);
    if (!task || summary.isEmpty() || ++task->progressEvents > 200) return; // bounded event log
    for (const auto &step : std::as_const(task->steps))
        if (step.id == stepId && step.state == "running") {
            save(task, {event(task, "step.progress", {{"step_id", idText(stepId)}, {"summary", summary}})});
            return;
        }
}

void TaskOrchestrator::onResult(QUuid taskId, QUuid stepId, QJsonObject result) {
    const auto task = m_active.value(taskId);
    if (!task) return;
    Step *step = nullptr;
    for (auto &candidate : task->steps) if (candidate.id == stepId) step = &candidate;
    if (!step || !inFlight(step->state)) return;
    const bool ok = result["ok"].toBool(); // the registered result.ok postcondition
    const bool awaited = step->state == "awaiting_approval";
    if (awaited) task->approvalId = QUuid();
    QJsonObject payload{{"step_id", idText(stepId)}};
    const auto code = result["code"].toString().left(128), summary = result["summary"].toString().left(4096);
    if (!code.isEmpty()) payload["code"] = code;
    if (!summary.isEmpty()) payload["summary"] = summary;
    if (task->record.state == "cancelling") {
        setStep(task, *step, ok ? "succeeded" : "cancelled");
        if (save(task, {event(task, "step.completed", payload)})) finishCancel(task);
        return;
    }
    setStep(task, *step, ok ? "succeeded" : "failed");
    const auto completed = event(task, "step.completed", payload);
    // A decided approval lets the task continue unless a required step was refused.
    if (awaited && (ok || !step->required) && task->record.state == "awaiting_approval") {
        if (!transition(task, "running", {completed})) return;
    } else if (!save(task, {completed})) return;
    if (!m_active.contains(taskId)) return;
    if (!ok && step->required) { fail(task, code, summary); return; }
    pump(task);
    // Broker capacity was freed: let other tasks dispatch the steps they had to hold back.
    const auto active = m_active.values();
    for (const auto &other : active)
        if (other != task && m_active.contains(other->record.id)) pump(other);
}

void TaskOrchestrator::onEngineFailed(QUuid taskId, QString code, QString message) {
    const auto task = m_active.value(taskId);
    if (!task) return;
    if (task->record.state == "planning") fail(task, code, message);
    else if (task->record.state == "cancelling") finishCancel(task);
}

bool TaskOrchestrator::decideApproval(const QUuid &approvalId, const QString &intentHash, const QString &nonce, bool allow) {
    if (approvalId.isNull()) return false;
    for (const auto &task : std::as_const(m_active))
        if (task->approvalId == approvalId && task->record.state == "awaiting_approval")
            return m_broker->decide(approvalId, intentHash, nonce, allow);
    return false;
}

void TaskOrchestrator::cancel(const QUuid &taskId) {
    const auto task = m_active.value(taskId);
    if (!task || task->record.state == "cancelling") return;
    if (!transition(task, "cancelling", {}, "CANCELLED", "Cancellation requested.")) return;
    m_registry->cancel(taskId);
    if (!task->steps.isEmpty()) m_broker->cancel(taskId);
    finishCancel(task);
}

void TaskOrchestrator::finishCancel(const Task &task) {
    if (!m_active.contains(task->record.id) || task->record.state != "cancelling") return;
    int completed = 0;
    for (const auto &step : std::as_const(task->steps)) {
        if (inFlight(step.state)) return; // still waiting for the broker to confirm
        completed += step.state == "succeeded";
    }
    for (auto &step : task->steps) if (step.state == "queued") setStep(task, step, "skipped");
    const auto summary = QString("Cancelled. %1 step(s) had already completed; no further steps were started.").arg(completed);
    task->record.error = errorEnvelope("CANCELLED", summary);
    transition(task, "cancelled", {}, "CANCELLED", summary);
}

void TaskOrchestrator::recover() {
    if (!m_store || !m_store->available()) return;
    const auto records = m_store->tasks();
    for (const auto &record : records) {
        if (isTerminal(record.state) || record.state == "interrupted" || m_active.contains(record.id)) continue;
        auto task = std::make_shared<Active>();
        task->record = record;
        task->sequence = m_store->lastSequence(record.id);
        for (auto it = task->record.steps.begin(); it != task->record.steps.end(); ++it)
            if (inFlight(it.value().toString())) it.value() = "interrupted";
        m_active.insert(record.id, task);
        if (record.state == "queued") {
            // Nothing had started, so there is nothing to reconcile.
            const QString summary = "The host stopped before planning began. Submit the request again.";
            task->record.error = errorEnvelope("PROVIDER_UNAVAILABLE", summary);
            transition(task, "failed", {}, "PROVIDER_UNAVAILABLE", summary);
        } else transition(task, "interrupted", {}, "INTERNAL",
            "The host stopped while this task was active. Its effects must be reconciled before any retry.");
        m_active.remove(record.id);
    }
}

QList<TaskRecord> TaskOrchestrator::tasks() const { return m_store ? m_store->tasks() : QList<TaskRecord>{}; }

QList<QJsonObject> TaskOrchestrator::eventsAfter(const QUuid &taskId, qint64 sequence, int limit) const {
    return m_store ? m_store->eventsAfter(taskId, qMax<qint64>(0, sequence), qBound(1, limit, 1024)) : QList<QJsonObject>{};
}

QJsonObject TaskOrchestrator::taskResult(const QUuid &taskId) const {
    TaskRecord record;
    if (!m_store || !m_store->find(taskId, record)) return {};
    QJsonObject result{{"schema_version", "1.0"}, {"task_id", idText(record.id)},
        {"execution_host_id", idText(m_session.executionHostId)}, {"state", record.state}, {"artifacts", QJsonArray{}}};
    if (!record.error.isEmpty()) result["error"] = record.error;
    return result;
}
}
