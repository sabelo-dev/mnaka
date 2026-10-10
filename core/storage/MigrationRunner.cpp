#include "MigrationRunner.h"
#include <QDir>
#include <QFile>
#include <QCryptographicHash>
#include <QSqlQuery>
#include <QVariant>
#include <QRegularExpression>

namespace mnaka {
bool MigrationRunner::apply(PgConnection &connection, const QString &directory, bool relay, QString &error) {
    error.clear();
    if (!connection.isOpen() || connection.m_role != PgConnection::Role::Migration) { error = "Migration owner connection required"; return false; }
    const QString schema = relay ? "relay" : "mnaka";
    QDir dir(directory);
    const auto files = dir.entryList({"*.sql"}, QDir::Files, QDir::Name);
    if (files.isEmpty()) { error = "No migration files found"; return false; }
    QList<QByteArray> scripts;
    QStringList hashes;
    for (int i = 0; i < files.size(); ++i) {
        auto match = QRegularExpression("^(\\d{3})_[a-z_]+\\.sql$").match(files[i]);
        if (!match.hasMatch() || match.captured(1).toInt() != i + 1) { error = "Migrations must be contiguous and ordered"; return false; }
        QFile file(dir.filePath(files[i]));
        if (!file.open(QIODevice::ReadOnly) || file.size() > 1024 * 1024) { error = "Migration file unavailable or oversized"; return false; }
        scripts.append(file.readAll());
        hashes.append(QString::fromLatin1(QCryptographicHash::hash(scripts.last(), QCryptographicHash::Sha256).toHex()));
    }
    QSqlQuery query(connection.m_database);
    const qlonglong lock = relay ? 0x4d4e414b4102LL : 0x4d4e414b4101LL;
    query.prepare("SELECT pg_try_advisory_lock(:lock)"); query.bindValue(":lock", lock);
    if (!query.exec() || !query.next() || !query.value(0).toBool()) { error = "Migration advisory lock unavailable"; return false; }
    struct Unlock {
        QSqlDatabase database; qlonglong lock;
        ~Unlock() { QSqlQuery q(database); q.prepare("SELECT pg_advisory_unlock(:lock)"); q.bindValue(":lock", lock); q.exec(); }
    } unlock{connection.m_database, lock};
    if (!query.exec("CREATE SCHEMA IF NOT EXISTS " + schema)
        || !query.exec("CREATE TABLE IF NOT EXISTS " + schema + ".schema_migrations "
                      "(version BIGINT PRIMARY KEY, checksum TEXT NOT NULL, applied_at TIMESTAMPTZ NOT NULL)")) {
        error = "Migration ledger setup failed"; return false;
    }
    if (!query.exec("SELECT version, checksum FROM " + schema + ".schema_migrations ORDER BY version")) {
        error = "Migration ledger unavailable"; return false;
    }
    int applied = 0;
    while (query.next()) {
        const auto version = query.value(0).toLongLong();
        if (version != applied + 1 || version > files.size() || query.value(1).toString() != hashes[int(version - 1)]) {
            error = "Migration checksum/version conflict"; return false;
        }
        ++applied;
    }
    query.finish();
    for (int i = applied; i < files.size(); ++i) {
        if (!connection.m_database.transaction()) { error = "Migration transaction failed"; return false; }
        connection.m_transaction = true;
        // Migration timeout is explicitly separate from ordinary requests.
        if (!query.exec("SET LOCAL statement_timeout = '120s'") || !query.exec("SET LOCAL lock_timeout = '10s'")
            || !query.exec(QString::fromUtf8(scripts[i]))) {
            connection.rollback(); error = "Migration " + QString::number(i + 1) + " failed; rolled back"; return false;
        }
        query.prepare("INSERT INTO " + schema + ".schema_migrations(version,checksum,applied_at) VALUES(:version,:checksum,now())");
        query.bindValue(":version", i + 1); query.bindValue(":checksum", hashes[i]);
        if (!query.exec()) { connection.rollback(); error = "Migration ledger write failed; rolled back"; return false; }
        if (!connection.commit(error)) return false;
    }
    return true;
}
}
