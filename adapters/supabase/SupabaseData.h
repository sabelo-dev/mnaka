#pragma once
#include <QJsonDocument>
#include <QJsonObject>
#include <QObject>
#include <functional>

class QNetworkAccessManager;

namespace mnaka {
class SupabaseAuth;

// The signed-in user's rows through the project's Data API. Row-level security on the server
// decides what is visible; this class only attaches the user's token. Status 0 means not sent
// or not reached.
class SupabaseData : public QObject {
    Q_OBJECT
public:
    using Done = std::function<void(int status, QJsonDocument body)>;
    explicit SupabaseData(SupabaseAuth *auth, QObject *parent = nullptr);
    void list(const QString &table, const QString &query, Done done);
    void insert(const QString &table, const QJsonObject &row, Done done);
    void update(const QString &table, const QString &id, const QJsonObject &patch, Done done);
    // An empty id removes every row the user owns in that table.
    void remove(const QString &table, const QString &id, Done done);
    void invoke(const QString &function, const QJsonObject &body, Done done);
private:
    void send(const QByteArray &verb, const QString &path, const QByteArray &body, int timeoutMs, Done done);
    SupabaseAuth *m_auth;
    QNetworkAccessManager *m_network;
};
}
