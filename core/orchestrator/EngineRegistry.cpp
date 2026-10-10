#include "EngineRegistry.h"
#include "core/contracts/SchemaValidator.h"
#include <QJsonArray>
#include <QSet>

namespace mnaka {
EngineRegistry::~EngineRegistry() {
    // Services use cancellable async work, never blocking provider calls.
    for (const auto &entry : m_entries) if (entry.worker) entry.worker->quit();
    for (const auto &entry : m_entries) if (entry.worker) entry.worker->wait();
}
bool EngineRegistry::registerService(EngineService *service, bool enabled, const QString &status) {
    if (!service || service->parent() || service->thread() != thread() || QThread::currentThread() != thread()) return false;
    const auto id = service->engineId();
    if (!QStringList{"assistant", "software", "control", "skills", "memory", "vision", "proactive"}.contains(id)
        || m_entries.contains(id)) return false;
    const auto tools = service->supportedTools();
    if (enabled && tools.isEmpty()) return false;
    QThread *worker = nullptr;
    // Single-threaded WebAssembly has no workers: the service stays on this thread and the
    // queued calls below still keep planning asynchronous.
    if (enabled && QT_CONFIG(thread)) {
        worker = new QThread(this); worker->setObjectName("mnaka-plan-" + id);
        service->moveToThread(worker);
        connect(worker, &QThread::finished, service, &QObject::deleteLater);
    } else service->setParent(this);
    m_entries.insert(id, {service, worker, enabled, status, tools});
    connect(service, &EngineService::responseReady, this, [this, id](QUuid taskId, QString text, QJsonObject usage) {
        if (m_active.value(taskId) == id && text.size() <= 65536) emit responseReady(taskId, text, usage);
    }, Qt::QueuedConnection);
    connect(service, &EngineService::responseDelta, this, [this, id](QUuid taskId, QString text) {
        if (m_active.value(taskId) == id && text.size() <= 65536) emit responseDelta(taskId, text);
    }, Qt::QueuedConnection);
    connect(service, &EngineService::planReady, this, [this, id](QUuid taskId, QJsonObject plan) {
        if (m_active.value(taskId) != id) return;
        m_active.remove(taskId);
        QString error = SchemaValidator::validate(plan, SchemaValidator::bundled("plan"));
        if (QUuid(plan["task_id"].toString()) != taskId) error = "Plan task identity mismatch";
        for (const auto step : plan["steps"].toArray()) {
            const auto intent = step.toObject()["intent"].toObject();
            if (intent["engine"].toString() != id || !m_entries[id].tools.contains(intent["tool"].toString()))
                error = "Plan exceeds registered engine tools";
        }
        if (!error.isEmpty()) emit failed(taskId, "INVALID_ARGUMENT", "Engine returned an invalid plan.");
        else emit planReady(taskId, plan);
    }, Qt::QueuedConnection);
    connect(service, &EngineService::failed, this, [this, id](QUuid taskId, QString code, QString message) {
        if (m_active.value(taskId) != id) return;
        m_active.remove(taskId);
        emit failed(taskId, code, message.left(4096));
    }, Qt::QueuedConnection);
    if (worker) worker->start();
    return true;
}
QVariantList EngineRegistry::capabilities() const {
    QVariantList result;
    auto ids = m_entries.keys(); ids.sort();
    for (const auto &id : ids) {
        const auto entry = m_entries.value(id);
        result.append(QVariantMap{{"id", id}, {"enabled", entry.enabled}, {"status", entry.status}, {"tools", entry.tools}});
    }
    return result;
}
QString EngineRegistry::submit(QString engineId, QUuid taskId, QJsonObject context, QJsonObject request) {
    if (QThread::currentThread() != thread()) return "INTERNAL: registry thread ownership";
    if (taskId.isNull() || !m_entries.contains(engineId)) return "INVALID_ARGUMENT";
    if (!m_entries[engineId].enabled) return "PROVIDER_UNAVAILABLE: engine disabled";
    if (m_seen.contains(taskId)) return "CONFLICT: task already submitted";
    if (m_active.size() >= 16 || m_seen.size() >= 1000) return "RATE_LIMITED";
    const auto error = SchemaValidator::validate(request, SchemaValidator::bundled("task-request"));
    if (!error.isEmpty()) return "INVALID_ARGUMENT: " + error;
    m_seen.insert(taskId); m_active.insert(taskId, engineId);
    auto service = m_entries[engineId].service;
    QMetaObject::invokeMethod(service, [service, taskId, context, request] { service->plan(taskId, context, request); }, Qt::QueuedConnection);
    return {};
}
void EngineRegistry::cancel(QUuid taskId) {
    if (!m_active.contains(taskId)) return;
    auto service = m_entries[m_active.take(taskId)].service;
    QMetaObject::invokeMethod(service, [service, taskId] { service->cancel(taskId); }, Qt::QueuedConnection);
    // Cancelled tasks no longer accept any late provider output or plans.
    emit failed(taskId, "CANCELLED", "Planning cancelled; no tools dispatched.");
}
}
