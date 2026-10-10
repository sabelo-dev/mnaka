#include "TaskStore.h"
#include "TaskStates.h"

namespace mnaka {
bool InMemoryTaskStore::commit(const TaskRecord &task, const QList<QJsonObject> &events) {
    if (!m_available || task.id.isNull()) return false;
    auto it = m_entries.find(task.id);
    const bool created = it == m_entries.end();
    qint64 sequence = created ? 0 : it->events.isEmpty() ? 0 : it->events.last()["sequence"].toInteger();
    if (!created && sequence + events.size() > 2048) return false;
    for (const auto &event : events)
        if (event["sequence"].toInteger() != ++sequence) return false;
    if (created) {
        if (m_order.size() >= 200) {
            // Evict only finished history; live tasks are never dropped.
            bool evicted = false;
            for (int i = 0; i < m_order.size() && !evicted; ++i) {
                if (!isTerminal(m_entries[m_order[i]].task.state)) continue;
                m_entries.remove(m_order.takeAt(i)); evicted = true;
            }
            if (!evicted) return false;
        }
        m_order.append(task.id);
        it = m_entries.insert(task.id, {});
    }
    it->task = task;
    it->events.append(events);
    return true;
}
bool InMemoryTaskStore::find(const QUuid &id, TaskRecord &out) const {
    const auto it = m_entries.find(id);
    if (!m_available || it == m_entries.end()) return false;
    out = it->task;
    return true;
}
bool InMemoryTaskStore::findByKey(const QString &key, TaskRecord &out) const {
    if (!m_available) return false;
    for (const auto &entry : m_entries)
        if (entry.task.idempotencyKey == key) { out = entry.task; return true; }
    return false;
}
QList<TaskRecord> InMemoryTaskStore::tasks() const {
    QList<TaskRecord> result;
    if (!m_available) return result;
    for (auto it = m_order.rbegin(); it != m_order.rend(); ++it) result.append(m_entries[*it].task);
    return result;
}
QList<QJsonObject> InMemoryTaskStore::eventsAfter(const QUuid &taskId, qint64 sequence, int limit) const {
    QList<QJsonObject> result;
    const auto it = m_entries.find(taskId);
    if (!m_available || it == m_entries.end()) return result;
    for (const auto &event : it->events) {
        if (result.size() >= limit) break;
        if (event["sequence"].toInteger() > sequence) result.append(event);
    }
    return result;
}
qint64 InMemoryTaskStore::lastSequence(const QUuid &taskId) const {
    const auto it = m_entries.find(taskId);
    return it == m_entries.end() || it->events.isEmpty() ? 0 : it->events.last()["sequence"].toInteger();
}
}
