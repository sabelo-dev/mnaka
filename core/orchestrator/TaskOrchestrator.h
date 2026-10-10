#pragma once
#include "EngineRegistry.h"
#include "TaskStore.h"
#include "core/policy/ToolBroker.h"
#include <QTimer>
#include <memory>

namespace mnaka {
// Owns task state on this execution host: request -> plan -> broker steps -> result.
// Every state change is committed with its events before anything else happens;
// if the store refuses a commit the task stops (fail closed, reconciled by recover()).
// Events are a record for clients, never authority to execute a tool.
class TaskOrchestrator : public QObject {
    Q_OBJECT
public:
    // The registry, broker and store must outlive the orchestrator.
    TaskOrchestrator(BrokerSession session, EngineRegistry *registry, ToolBroker *broker,
                     TaskStore *store, QObject *parent = nullptr);
    // Empty on acceptance. A repeated idempotency key returns the original task
    // for the same normalized request and CONFLICT for a different one.
    QString submit(const QJsonObject &request, QUuid &taskId);
    void cancel(const QUuid &taskId);
    bool decideApproval(const QUuid &approvalId, const QString &intentHash, const QString &nonce, bool allow);
    // Marks tasks left active by a previous host run as interrupted; nothing is retried.
    void recover();
    bool isActive(const QUuid &taskId) const { return m_active.contains(taskId); }
    QList<TaskRecord> tasks() const;
    QList<QJsonObject> eventsAfter(const QUuid &taskId, qint64 sequence, int limit = 256) const;
    QJsonObject taskResult(const QUuid &taskId) const;
    void setExecutionDeadlineMs(int milliseconds) { m_deadlineMs = qBound(1, milliseconds, 600000); }
signals:
    void taskChanged(QUuid taskId);
    void eventAppended(QUuid taskId, QJsonObject event);
    void responseDelta(QUuid taskId, QString text);
    void responseReady(QUuid taskId, QString text, QJsonObject usage);
    void approvalRequested(QUuid taskId, QJsonObject presentation);
private:
    struct Step { QUuid id; ToolIntentV1 intent; QList<QUuid> dependencies; bool required = true; bool started = false; QString state; };
    struct Active {
        TaskRecord record;
        QList<Step> steps;
        qint64 sequence = 0;
        int progressEvents = 0;
        QString reply;
        QJsonObject usage;
        QUuid approvalId;
        QTimer *deadline = nullptr;
    };
    using Task = std::shared_ptr<Active>;
    QJsonObject event(const Task &task, const QString &type, QJsonObject payload) const;
    bool save(const Task &task, const QList<QJsonObject> &events);
    bool transition(const Task &task, const QString &to, QList<QJsonObject> events = {},
                    const QString &code = {}, const QString &summary = {});
    void fail(const Task &task, const QString &error, const QString &message = {});
    void release(const Task &task);
    void setStep(const Task &task, Step &step, const QString &state);
    void pump(const Task &task);
    void finishCancel(const Task &task);
    void onPlanReady(QUuid taskId, QJsonObject plan);
    void onEngineFailed(QUuid taskId, QString code, QString message);
    void onApprovalRequested(QUuid taskId, QUuid stepId, QJsonObject presentation);
    void onProgress(QUuid taskId, QUuid stepId, QJsonObject progress);
    void onResult(QUuid taskId, QUuid stepId, QJsonObject result);
    BrokerSession m_session;
    EngineRegistry *m_registry;
    ToolBroker *m_broker;
    TaskStore *m_store;
    QHash<QUuid, Task> m_active;
    int m_deadlineMs = 600000;
};
}
