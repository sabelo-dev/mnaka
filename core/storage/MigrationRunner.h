#pragma once
#include "PgConnection.h"

namespace mnaka {
class MigrationRunner {
public:
    static bool apply(PgConnection &connection, const QString &directory,
                      bool relay, QString &safeError);
};
}
