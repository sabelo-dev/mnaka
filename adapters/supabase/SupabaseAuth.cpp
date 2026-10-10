#include "SupabaseAuth.h"
#include <QJsonDocument>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QUuid>

namespace mnaka {
QString SupabaseAuth::validateConfig(const QUrl &url, const QString &key) {
    const bool loopback = url.host() == "127.0.0.1" || url.host() == "localhost";
    if (!url.isValid() || url.host().isEmpty() || !url.userInfo().isEmpty() || url.hasQuery() || url.hasFragment()
        || (!url.path().isEmpty() && url.path() != "/")
        || (url.scheme() != "https" && !(loopback && url.scheme() == "http")))
        return "The Supabase project URL must be an https origin.";
    if (key.size() > 4096) return "The Supabase key is too long.";
    if (key.startsWith("sb_publishable_")) return {};
    // Legacy keys are JWTs; of those only the anon role is meant for clients.
    const auto parts = key.split('.');
    if (parts.size() == 3 && QJsonDocument::fromJson(QByteArray::fromBase64(parts[1].toLatin1(),
            QByteArray::Base64UrlEncoding)).object()["role"].toString() == "anon") return {};
    return "Only a Supabase publishable key may be used in the client.";
}
SupabaseAuth::SupabaseAuth(const QUrl &projectUrl, const QString &publishableKey, QObject *parent)
    : QObject(parent), m_projectUrl(projectUrl), m_key(publishableKey),
      m_configError(validateConfig(projectUrl, publishableKey)), m_network(new QNetworkAccessManager(this)) {
    m_refresh.setSingleShot(true);
    connect(&m_refresh, &QTimer::timeout, this, &SupabaseAuth::refreshNow);
}
void SupabaseAuth::refreshNow() {
    if (!signedIn()) return;
    if (busy()) { m_refresh.start(5000); return; }
    post(Request::Refresh, "/auth/v1/token?grant_type=refresh_token", {{"refresh_token", m_refreshToken}});
}
SupabaseAuth::~SupabaseAuth() { abortPending(); }
void SupabaseAuth::abortPending() {
    // Cleared first: abort() finishes the reply synchronously and its handler must ignore it.
    if (auto *pending = m_reply) { m_reply = nullptr; pending->abort(); }
}

bool SupabaseAuth::acceptCredentials(const QString &email, const QString &password) {
    if (!m_configError.isEmpty()) emit notice(m_configError);
    else if (busy()) emit notice("An account request is already in progress.");
    else if (signedIn()) emit notice("Sign out before using another account.");
    else if (email.size() < 3 || email.size() > 320 || !email.contains('@') || password.isEmpty() || password.size() > 1024)
        emit notice("Enter your email address and password.");
    else return true;
    return false;
}
void SupabaseAuth::signIn(const QString &email, const QString &password) {
    if (acceptCredentials(email.trimmed(), password))
        post(Request::SignIn, "/auth/v1/token?grant_type=password", {{"email", email.trimmed()}, {"password", password}});
}
void SupabaseAuth::signUp(const QString &email, const QString &password) {
    if (acceptCredentials(email.trimmed(), password))
        post(Request::SignUp, "/auth/v1/signup", {{"email", email.trimmed()}, {"password", password}});
}
void SupabaseAuth::signOut() {
    if (!signedIn()) return;
    // Best-effort server revocation; the local session ends regardless of the reply.
    QNetworkRequest request(QUrl(m_projectUrl.toString(QUrl::StripTrailingSlash) + "/auth/v1/logout?scope=local"));
    request.setRawHeader("apikey", m_key.toLatin1());
    request.setRawHeader("Authorization", "Bearer " + m_accessToken.toLatin1());
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::ManualRedirectPolicy);
    request.setTransferTimeout(15000);
    auto *reply = m_network->post(request, QByteArray());
    connect(reply, &QNetworkReply::finished, reply, &QObject::deleteLater);
    abortPending();
    clearSession();
    emit changed();
}
void SupabaseAuth::clearSession() {
    m_refresh.stop();
    m_accessToken.clear(); m_refreshToken.clear(); m_email.clear(); m_userId.clear();
}
void SupabaseAuth::post(Request kind, const QString &path, const QJsonObject &body) {
    QNetworkRequest request(QUrl(m_projectUrl.toString(QUrl::StripTrailingSlash) + path));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setRawHeader("apikey", m_key.toLatin1());
    // Credentials are never replayed to a redirect target.
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::ManualRedirectPolicy);
    request.setTransferTimeout(15000);
    auto *reply = m_reply = m_network->post(request, QJsonDocument(body).toJson(QJsonDocument::Compact));
    connect(reply, &QNetworkReply::finished, this, [this, reply, kind] {
        reply->deleteLater();
        if (reply != m_reply) return; // aborted by sign-out or destruction
        m_reply = nullptr;
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        finish(kind, status, status == 0, QJsonDocument::fromJson(reply->read(65536)).object());
    });
    emit changed();
}
void SupabaseAuth::finish(Request kind, int status, bool transportFailed, const QJsonObject &body) {
    const auto user = body.contains("user") ? body["user"].toObject() : body;
    const auto userId = QUuid(user["id"].toString());
    const auto expiresIn = body["expires_in"].toInt();
    if (status == 200 && !body["access_token"].toString().isEmpty() && !body["refresh_token"].toString().isEmpty()
        && !userId.isNull() && expiresIn > 0) {
        m_accessToken = body["access_token"].toString(); m_refreshToken = body["refresh_token"].toString();
        m_userId = userId.toString(QUuid::WithoutBraces); m_email = user["email"].toString().left(320);
        m_refresh.start(qMax(30, expiresIn - 60) * 1000);
        if (kind != Request::Refresh) emit notice({});
    } else if (kind == Request::Refresh) {
        // A network blip keeps the session until the access token would have lapsed anyway.
        if (transportFailed) m_refresh.start(30000);
        else { clearSession(); emit notice("Your session has ended. Sign in again."); }
    } else if (transportFailed) {
        emit notice("The account service could not be reached.");
    } else if (kind == Request::SignUp && status == 200 && !userId.isNull()) {
        emit notice("Check your email to confirm the account, then sign in.");
    } else {
        // Server text is untrusted: bounded here and rendered as plain text by the UI.
        auto message = body["msg"].toString();
        if (message.isEmpty()) message = body["error_description"].toString();
        if (message.isEmpty()) message = body["message"].toString();
        emit notice((kind == Request::SignUp ? "Account not created" : "Sign-in failed")
            + (message.isEmpty() ? QString(" (HTTP %1).").arg(status) : ": " + message.left(200)));
    }
    emit changed();
}
}
