#include "SupabaseData.h"
#include "SupabaseAuth.h"
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QRegularExpression>
#include <QTimer>
#include <QUuid>

namespace mnaka {
namespace {
bool safeName(const QString &name) {
    static const QRegularExpression pattern("^[a-z_]{1,64}$");
    return pattern.match(name).hasMatch();
}
}
SupabaseData::SupabaseData(SupabaseAuth *auth, QObject *parent)
    : QObject(parent), m_auth(auth), m_network(new QNetworkAccessManager(this)) {}

void SupabaseData::list(const QString &table, const QString &query, Done done) {
    send("GET", safeName(table) ? "/rest/v1/" + table + "?" + query : QString(), {}, 20000, std::move(done));
}
void SupabaseData::insert(const QString &table, const QJsonObject &row, Done done) {
    send("POST", safeName(table) ? "/rest/v1/" + table : QString(), QJsonDocument(row).toJson(QJsonDocument::Compact), 20000, std::move(done));
}
void SupabaseData::update(const QString &table, const QString &id, const QJsonObject &patch, Done done) {
    const bool ok = safeName(table) && !QUuid(id).isNull();
    send("PATCH", ok ? "/rest/v1/" + table + "?id=eq." + QUuid(id).toString(QUuid::WithoutBraces) : QString(),
        QJsonDocument(patch).toJson(QJsonDocument::Compact), 20000, std::move(done));
}
void SupabaseData::remove(const QString &table, const QString &id, Done done) {
    // The Data API refuses an unfiltered delete; "every row" is spelled as a filter that always matches.
    const bool ok = safeName(table) && (id.isEmpty() || !QUuid(id).isNull());
    const auto filter = id.isEmpty() ? QString("id=not.is.null") : "id=eq." + QUuid(id).toString(QUuid::WithoutBraces);
    send("DELETE", ok ? "/rest/v1/" + table + "?" + filter : QString(), {}, 20000, std::move(done));
}
void SupabaseData::invoke(const QString &function, const QJsonObject &body, Done done) {
    send("POST", safeName(function) ? "/functions/v1/" + function : QString(), QJsonDocument(body).toJson(QJsonDocument::Compact), 120000, std::move(done));
}
void SupabaseData::send(const QByteArray &verb, const QString &path, const QByteArray &body, int timeoutMs, Done done) {
    if (path.isEmpty() || !m_auth || !m_auth->signedIn()) {
        // Always asynchronous, so callers never re-enter themselves.
        QTimer::singleShot(0, this, [done = std::move(done)] { done(0, {}); });
        return;
    }
    QNetworkRequest request(QUrl(m_auth->projectUrl().toString(QUrl::StripTrailingSlash) + path));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setRawHeader("apikey", m_auth->publishableKey().toLatin1());
    request.setRawHeader("Authorization", "Bearer " + m_auth->accessToken().toLatin1());
    request.setRawHeader("Prefer", "return=representation");
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::ManualRedirectPolicy);
    request.setTransferTimeout(timeoutMs);
    auto *reply = m_network->sendCustomRequest(request, verb, body);
    connect(reply, &QNetworkReply::finished, this, [reply, done = std::move(done)] {
        reply->deleteLater();
        done(reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt(), QJsonDocument::fromJson(reply->read(4 * 1024 * 1024)));
    });
}
}
