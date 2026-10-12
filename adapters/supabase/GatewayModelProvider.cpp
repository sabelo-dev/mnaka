#include "GatewayModelProvider.h"
#include <QJsonDocument>
#include <QNetworkAccessManager>
#include <QNetworkReply>

namespace mnaka {
GatewayModelProvider::GatewayModelProvider(QUrl projectUrl, QString publishableKey, std::shared_ptr<SessionToken> token)
    : m_projectUrl(std::move(projectUrl)), m_key(std::move(publishableKey)), m_token(std::move(token)) {}

QUrl GatewayModelProvider::endpoint(const QUrl &projectUrl) {
    return QUrl(projectUrl.toString(QUrl::StripTrailingSlash) + "/functions/v1/assistant");
}
bool GatewayModelProvider::start(QUuid id, QJsonObject request) {
    if (id.isNull() || m_replies.contains(id) || m_replies.size() >= 16) return false;
    const auto text = request["text"].toString();
    if (text.isEmpty() || text.size() > 4000) { emit failed(id, "INVALID_ARGUMENT"); return true; }
    const auto token = m_token ? m_token->get() : QString();
    if (token.isEmpty()) { emit failed(id, "UNAUTHENTICATED"); return true; }
    // Created here, not in the constructor: the provider has moved to the planning thread by now.
    if (!m_network) m_network = new QNetworkAccessManager(this);
    QNetworkRequest call(endpoint(m_projectUrl));
    call.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    call.setRawHeader("apikey", m_key.toLatin1());
    call.setRawHeader("Authorization", "Bearer " + token.toLatin1());
    call.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::ManualRedirectPolicy);
    call.setTransferTimeout(120000);
    auto *reply = m_network->post(call, QJsonDocument(QJsonObject{{"text", text}}).toJson(QJsonDocument::Compact));
    m_replies.insert(id, reply);
    connect(reply, &QNetworkReply::finished, this, [this, id, reply] {
        reply->deleteLater();
        if (m_replies.value(id) != reply) return; // cancelled
        m_replies.remove(id);
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const auto body = QJsonDocument::fromJson(reply->read(262144)).object();
        const auto answer = body["text"].toString();
        if (status == 200 && !answer.isEmpty()) {
            // Model text is untrusted output: it is shown, never executed. The step it maps to has no effects.
            emit completed(id, {{"schema_version", "1.0"}, {"text", answer.left(65536)}, {"tool", "demo.echo"}},
                {{"provider", "gateway"}, {"model", body["model"].toString().left(200)}, {"mock", false}});
        } else {
            const auto code = body["code"].toString();
            // A model's refusal is not an outage; the task contract's closest code is PERMISSION_DENIED.
            emit failed(id, code == "REFUSED" ? QString("PERMISSION_DENIED")
                : QStringList{"INVALID_ARGUMENT", "UNAUTHENTICATED", "RATE_LIMITED"}.contains(code) ? code : "PROVIDER_UNAVAILABLE");
        }
    });
    emit delta(id, "Waiting for the model…");
    return true;
}
void GatewayModelProvider::cancel(QUuid id) {
    auto *reply = m_replies.take(id);
    if (!reply) return;
    reply->abort();
    emit failed(id, "CANCELLED");
}
}
