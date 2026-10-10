#include "PgConnection.h"
#include <QSqlQuery>
#include <QVariant>
#ifdef Q_OS_WIN
#include <windows.h>
#include <wincred.h>
#endif

namespace {
QString vaultSecret(const QString &target) {
#ifdef Q_OS_WIN
    PCREDENTIALW credential = nullptr;
    if (target.isEmpty() || !CredReadW(reinterpret_cast<LPCWSTR>(target.utf16()), CRED_TYPE_GENERIC, 0, &credential)) return {};
    // MNAKA generic credential blobs are UTF-8, provisioned outside the repository.
    const QString value = QString::fromUtf8(reinterpret_cast<const char *>(credential->CredentialBlob), credential->CredentialBlobSize);
    CredFree(credential);
    return value;
#else
    Q_UNUSED(target)
    return {}; // Linux/OS keyring adapter not yet qualified; fail closed.
#endif
}
}
namespace mnaka {
PgConnection::PgConnection() : m_name("mnaka-pg-" + QUuid::createUuid().toString(QUuid::WithoutBraces)), m_thread(QThread::currentThread()) {}
PgConnection::~PgConnection() {
    Q_ASSERT(ownsThread());
    rollback();
    m_database.close();
    m_database = QSqlDatabase();
    if (QSqlDatabase::contains(m_name)) QSqlDatabase::removeDatabase(m_name);
}
bool PgConnection::ownsThread() const { return m_thread == QThread::currentThread(); }
bool PgConnection::isOpen() const { return ownsThread() && m_database.isOpen(); }
bool PgConnection::open(const PgConfig &config, Role role, QString &error) {
    error.clear();
    if (!ownsThread() || m_database.isValid()) { error = "Connection ownership or lifecycle violation"; return false; }
    if (config.database.isEmpty() || config.user.isEmpty() || config.port < 1 || config.port > 65535) {
        error = "PostgreSQL configuration is incomplete"; return false;
    }
    const bool loopback = config.host == "127.0.0.1" || config.host == "::1";
    if (!loopback && (config.host.isEmpty() || config.sslRootCertificate.isEmpty()
        || config.sslRootCertificate.contains(';'))) { error = "Managed PostgreSQL requires a trusted TLS certificate"; return false; }
    if (!QSqlDatabase::isDriverAvailable("QPSQL")) { error = "QPSQL/libpq unavailable; execution disabled"; return false; }
    QString password = vaultSecret(config.credentialTarget);
    if (password.isEmpty()) { error = "PostgreSQL OS-vault credential unavailable; execution disabled"; return false; }
    m_database = QSqlDatabase::addDatabase("QPSQL", m_name);
    m_database.setHostName(config.host); m_database.setPort(config.port);
    m_database.setDatabaseName(config.database); m_database.setUserName(config.user);
    m_database.setConnectOptions(loopback ? "connect_timeout=5;sslmode=disable" :
        "connect_timeout=5;sslmode=verify-full;sslrootcert=" + config.sslRootCertificate);
    bool ok = m_database.open(config.user, password);
    password.fill(QChar(0)); password.clear();
    // Qt/libpq may hold internal copies; never claim guaranteed secret erasure.
    if (!ok) { error = "PostgreSQL connection failed; execution disabled"; return false; }
    QSqlQuery query(m_database);
    if (!query.exec("SHOW server_version_num") || !query.next() || query.value(0).toInt() / 10000 != 17) {
        error = "PostgreSQL 17 is required"; m_database.close(); return false;
    }
    if (!query.exec("SET TIME ZONE 'UTC'") || !query.exec("SET statement_timeout = '15s'")
        || !query.exec("SET lock_timeout = '3s'")) { error = "PostgreSQL session setup failed"; m_database.close(); return false; }
    if (role == Role::Runtime) {
        if (!query.exec("SELECT rolsuper, rolbypassrls, rolcreaterole, rolcreatedb FROM pg_roles WHERE rolname=current_user")
            || !query.next() || query.value(0).toBool() || query.value(1).toBool()
            || query.value(2).toBool() || query.value(3).toBool()) {
            error = "Unsafe PostgreSQL runtime role"; m_database.close(); return false;
        }
        if (!query.exec("SELECT EXISTS(SELECT 1 FROM pg_class c JOIN pg_namespace n ON n.oid=c.relnamespace "
                        "WHERE n.nspname IN ('mnaka','relay') AND pg_has_role(current_user,c.relowner,'MEMBER'))")
            || !query.next() || query.value(0).toBool()) {
            error = "Runtime role may not own application tables or inherit the migration owner"; m_database.close(); return false;
        }
    }
    m_role = role;
    return true;
}
bool PgConnection::beginActorRequest(const QUuid &actorId, QString &error) {
    error.clear();
    if (!isOpen() || m_role != Role::Runtime || m_transaction || actorId.isNull()) {
        error = "Authenticated PostgreSQL request unavailable"; return false;
    }
    if (!m_database.transaction()) { error = "PostgreSQL transaction failed"; return false; }
    m_transaction = true;
    QSqlQuery query(m_database);
    query.prepare("SELECT set_config('mnaka.actor_id', :actor_id, true)");
    query.bindValue(":actor_id", actorId.toString(QUuid::WithoutBraces));
    if (!query.exec()) { error = "PostgreSQL identity setup failed"; rollback(); return false; }
    return true;
}
bool PgConnection::commit(QString &error) {
    error.clear();
    if (!isOpen() || !m_transaction || !m_database.commit()) { error = "PostgreSQL commit failed"; rollback(); return false; }
    m_transaction = false;
    return true;
}
void PgConnection::rollback() {
    if (ownsThread() && m_transaction) { m_database.rollback(); m_transaction = false; }
}
}
