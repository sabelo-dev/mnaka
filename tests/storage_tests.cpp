#include <QtTest>
#include "core/storage/PgConnection.h"
#include "core/storage/MigrationRunner.h"
#include "core/storage/ProjectRepository.h"

class StorageTests : public QObject {
    Q_OBJECT
private slots:
    void projectRepositoryFailsClosed() {
        mnaka::PgConnection connection;
        const auto actor = QUuid::createUuid();
        auto page = mnaka::ProjectRepository::list(connection, actor);
        QVERIFY(!page.ok);
        QVERIFY(page.projects.isEmpty());
        QVERIFY(page.nextCursor.isEmpty());
        QVERIFY(!page.error.isEmpty());
        page = mnaka::ProjectRepository::list(connection, actor, "../escape");
        QVERIFY(page.error.startsWith("INVALID_ARGUMENT"));
        page = mnaka::ProjectRepository::list(connection, actor, {}, 101);
        QVERIFY(page.error.startsWith("INVALID_ARGUMENT"));
        page = mnaka::ProjectRepository::list(connection, {});
        QVERIFY(page.error.startsWith("INVALID_ARGUMENT"));
    }
    void noConfigurationFailsClosed() {
        mnaka::PgConnection connection;
        QString error;
        QVERIFY(!connection.open({}, mnaka::PgConnection::Role::Runtime, error));
        QVERIFY(!connection.isOpen());
        QVERIFY(!error.isEmpty());
        QVERIFY(!connection.beginActorRequest(QUuid::createUuid(), error));
        QVERIFY(!mnaka::MigrationRunner::apply(connection, "migrations/desktop-postgres", false, error));
    }
    void remoteNeedsTLS() {
        mnaka::PgConnection connection;
        mnaka::PgConfig config; config.host = "db.example.invalid";
        config.database = "mnaka"; config.user = "mnaka_runtime";
        QString error;
        QVERIFY(!connection.open(config, mnaka::PgConnection::Role::Runtime, error));
        QVERIFY(error.contains("TLS"));
    }
    void noVaultFailsClosed() {
        mnaka::PgConnection connection;
        mnaka::PgConfig config; config.database = "mnaka"; config.user = "mnaka_runtime";
        config.credentialTarget = "mnaka-tests-nonexistent-" + QUuid::createUuid().toString();
        QString error;
        QVERIFY(!connection.open(config, mnaka::PgConnection::Role::Runtime, error));
        QVERIFY(!connection.isOpen());
    }
};
QTEST_GUILESS_MAIN(StorageTests)
#include "storage_tests.moc"
