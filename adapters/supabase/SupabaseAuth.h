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
    void signIn(const QString &email, const QString &password);
    void signUp(const QString &email, const QString &password);
    void signOut();
signals:
    void changed();
    void notice(QString safeMessage);
private slots:
    void refreshNow(); // normally fired by the timer shortly before the access token lapses
private:
    enum class Request { SignIn, SignUp, Refresh };
    bool acceptCredentials(const QString &email, const QString &password);
    void post(Request kind, const QString &path, const QJsonObject &body);
    void finish(Request kind, int status, bool transportFailed, const QJsonObject &body);
    void clearSession();
    void abortPending();
    QUrl m_projectUrl;
    QString m_key, m_configError;
    QNetworkAccessManager *m_network;
    QNetworkReply *m_reply = nullptr;
    QString m_accessToken, m_refreshToken, m_email, m_userId;
    QTimer m_refresh;
};
}
