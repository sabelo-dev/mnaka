#pragma once
#include <QObject>
#include <QJsonObject>
#include <QTimer>
#include <QUrl>

class QNetworkAccessManager;
class QNetworkReply;

namespace mnaka {
// Email/password client for Supabase Auth. Tokens live in this object, in memory only;
// callers see the account's email and user ID, never a token.
class SupabaseAuth : public QObject {
    Q_OBJECT
public:
    // Empty when usable. Only a publishable (or legacy anon) key may ship in a client.
    static QString validateConfig(const QUrl &projectUrl, const QString &publishableKey);
    SupabaseAuth(const QUrl &projectUrl, const QString &publishableKey, QObject *parent = nullptr);
    ~SupabaseAuth() override;
    QString configError() const { return m_configError; }
    bool busy() const { return m_reply != nullptr; }
    bool signedIn() const { return !m_accessToken.isEmpty(); }
    QString email() const { return m_email; }
    QString userId() const { return m_userId; }
    // For trusted C++ callers that attach the session to requests; never exposed to QML.
    QString accessToken() const { return m_accessToken; }
    QUrl projectUrl() const { return m_projectUrl; }
    QString publishableKey() const { return m_key; }
    // Kept by the caller to restore the session later; never exposed to QML.
    QString refreshToken() const { return m_refreshToken; }
    // Where confirmation emails send people back to. Ignored unless it is an https (or loopback) URL.
    void setRedirectUrl(const QUrl &url);
    // Signs in again from a refresh token: one saved earlier, or one delivered by an email link.
    void restore(const QString &refreshToken);
    void resendConfirmation(const QString &email);
    void signIn(const QString &email, const QString &password);
    void signUp(const QString &email, const QString &password);
    void signOut();
signals:
    void changed();
    void notice(QString safeMessage);
    // The server refused a token given to restore(): it is expired or revoked and should be forgotten.
    void restoreRejected();
private slots:
    void refreshNow(); // normally fired by the timer shortly before the access token lapses
private:
    enum class Request { SignIn, SignUp, Refresh, Restore, Resend };
    QString redirectQuery(const QString &separator) const;
    bool acceptCredentials(const QString &email, const QString &password);
    void post(Request kind, const QString &path, const QJsonObject &body);
    void finish(Request kind, int status, bool transportFailed, const QJsonObject &body);
    void clearSession();
    void abortPending();
    QUrl m_projectUrl, m_redirect;
    QString m_key, m_configError;
    QNetworkAccessManager *m_network;
    QNetworkReply *m_reply = nullptr;
    QString m_accessToken, m_refreshToken, m_email, m_userId;
    QTimer m_refresh;
};
}
