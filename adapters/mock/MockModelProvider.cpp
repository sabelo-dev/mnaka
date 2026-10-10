#include "MockModelProvider.h"
#include "core/contracts/SchemaValidator.h"
#include <QJsonArray>

namespace mnaka {
bool MockModelProvider::start(QUuid id, QJsonObject request) {
    if (id.isNull() || m_seen.contains(id) || m_pending.size() >= 16 || m_seen.size() >= 1000) return false;
    m_seen.insert(id);
    const QJsonObject schema{{"type", "object"}, {"additionalProperties", false}, {"required", QJsonArray{"text"}},
        {"properties", QJsonObject{{"text", QJsonObject{{"type", "string"}, {"minLength", 1}, {"maxLength", 4000}}}}}};
    if (!SchemaValidator::validate(request, schema).isEmpty()) { emit failed(id, "INVALID_ARGUMENT"); return true; }
    auto timer = new QTimer(this); timer->setInterval(325);
    m_pending.insert(id, {request, timer, 0});
    connect(timer, &QTimer::timeout, this, [this, id] {
        auto it = m_pending.find(id);
        if (it == m_pending.end()) return;
        if (++it->ticks == 1) { emit delta(id, "Preparing a deterministic mock response…"); return; }
        const auto request = it->request;
        const auto text = request["text"].toString().toLower();
        QString answer;
        if (text.contains("traffic") || text.contains("n1")) {
            answer = "Which stretch of the N1, origin, destination and direction do you mean, and when are you departing? No live traffic provider is configured, so I cannot report current incidents or travel times.";
        } else if (text.contains("build") || text.contains("code") || text.contains("project")
            || text.contains("patch") || text.contains("file") || text.contains("run")) {
            answer = "Project execution is unavailable. PostgreSQL persistence, the production tool broker and a qualified isolated runner are required before project tasks can execute. No repository has been inspected or changed, and nothing has been compiled.";
        } else answer = "Mock response: I received your message. This demonstrates typed conversation and cancellation. A reasoning provider is not connected. You can explore all six engine workspaces from the navigation.";
        it->timer->stop(); it->timer->deleteLater(); m_pending.erase(it);
        emit completed(id, {{"schema_version", "1.0"}, {"text", answer},
            {"tool", text.contains("review demo") ? "demo.review" : "demo.echo"}},
            {{"provider", "MockModelProvider"}, {"mock", true}, {"input_tokens", 0}, {"output_tokens", 0}, {"cost", 0}});
    });
    timer->start();
    return true;
}
void MockModelProvider::cancel(QUuid id) {
    auto it = m_pending.find(id);
    if (it == m_pending.end()) return;
    it->timer->stop(); it->timer->deleteLater(); m_pending.erase(it);
    emit failed(id, "CANCELLED");
}
}
