#pragma once
#include <QDateTime>
#include <QHash>
#include <QJsonObject>
#include <QList>
#include <QUuid>

namespace mnaka {
struct TaskRecord {
    QUuid id;
    QString engine, mode, state, idempotencyKey, requestHash;
    QJsonObject request, plan, steps, error; // steps maps step ID to step state
    QDateTime createdAt, updatedAt;
};
// Task/event persistence contract. A PostgreSQL implementation must commit the
// task row and its events in one transaction; no implementation may fall back.
class TaskStore {
public:
    virtual ~TaskStore() = default;
    virtual bool available() const = 0;
    // Atomic: on false neither the task state nor any event was recorded.
    virtual bool commit(const TaskRecord &task, const QList<QJsonObject> &events) = 0;
    virtual bool find(const QUuid &id, TaskRecord &out) const = 0;
    virtual bool findByKey(const QString &idempotencyKey, TaskRecord &out) const = 0;
    virtual QList<TaskRecord> tasks() const = 0; // newest first
    virtual QList<QJsonObject> eventsAfter(const QUuid &taskId, qint64 sequence, int limit) const = 0;
    virtual qint64 lastSequence(const QUuid &taskId) const = 0;
};
// Session-only store for mock sessions and tests. Bounded; discarded on exit.
// It is not a persistence fallback and must never back production execution.
class InMemoryTaskStore : public TaskStore {
public:
    bool available() const override { return m_available; }
    bool commit(const TaskRecord &task, const QList<QJsonObject> &events) override;
    bool find(const QUuid &id, TaskRecord &out) const override;
    bool findByKey(const QString &idempotencyKey, TaskRecord &out) const override;
    QList<TaskRecord> tasks() const override;
    QList<QJsonObject> eventsAfter(const QUuid &taskId, qint64 sequence, int limit) const override;
    qint64 lastSequence(const QUuid &taskId) const override;
    void setAvailable(bool available) { m_available = available; }
private:
    struct Entry { TaskRecord task; QList<QJsonObject> events; };
    QHash<QUuid, Entry> m_entries;
    QList<QUuid> m_order;
    bool m_available = true;
};
}
