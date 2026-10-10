#include "core/storage/MigrationRunner.h"
#include <QCoreApplication>
#include <QTextStream>

int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    const auto args = app.arguments();
    if (args.size() == 2 && args[1] == "--driver-check") {
        bool loaded = false;
        {
            auto database = QSqlDatabase::addDatabase("QPSQL", "mnaka-driver-probe");
            loaded = database.isValid();
        }
        QSqlDatabase::removeDatabase("mnaka-driver-probe");
        QTextStream(stdout) << (loaded ? "QPSQL driver loaded; server connection not tested.\n"
            : "QPSQL/libpq cannot load; PostgreSQL execution unavailable.\n");
        return loaded ? 0 : 1;
    }
    if (args.size() != 3 || (args[1] != "desktop" && args[1] != "relay")) {
        QTextStream(stderr) << "Usage: mnaka_migrate desktop|relay <migration-directory>\n";
        return 2;
    }
    mnaka::PgConfig config;
    config.host = qEnvironmentVariable("MNAKA_DB_HOST", "127.0.0.1");
    config.port = qEnvironmentVariableIntValue("MNAKA_DB_PORT");
    if (!config.port) config.port = 5432;
    config.database = qEnvironmentVariable("MNAKA_DB_NAME");
    config.user = qEnvironmentVariable("MNAKA_DB_USER");
    config.credentialTarget = qEnvironmentVariable("MNAKA_DB_VAULT_REF");
    config.sslRootCertificate = qEnvironmentVariable("MNAKA_DB_TLS_CA");
    mnaka::PgConnection connection;
    QString error;
    if (!connection.open(config, mnaka::PgConnection::Role::Migration, error)
        || !mnaka::MigrationRunner::apply(connection, args[2], args[1] == "relay", error)) {
        QTextStream(stderr) << error << '\n'; return 1;
    }
    QTextStream(stdout) << "Migrations applied or checksum-verified.\n";
    return 0;
}
