#pragma once
#include <QSqlDatabase>
#include <QThread>
#include <QUuid>

namespace mnaka {
struct PgConfig {
    QString host = "127.0.0.1";
    int port = 5432;
    QString database, user, credentialTarget;
    QString sslRootCertificate;
};
// One RAII connection per owning thread. Never expose this object to QML.
class PgConnection {
public:
    enum class Role { Runtime, Migration };
    PgConnection();
    ~PgConnection();
    PgConnection(const PgConnection &) = delete;
    PgConnection &operator=(const PgConnection &) = delete;
    bool open(const PgConfig &config, Role role, QString &safeError);
    bool beginActorRequest(const QUuid &actorId, QString &safeError);
    bool commit(QString &safeError);
    void rollback();
    bool isOpen() const;
private:
    friend class MigrationRunner;
    friend class ProjectRepository;
    bool ownsThread() const;
    QString m_name;
    QSqlDatabase m_database;
    QThread *m_thread;
    bool m_transaction = false;
    Role m_role = Role::Runtime;
};
}
