#pragma once
#include "PgConnection.h"
#include <QVariantList>
namespace mnaka {
struct ProjectPage {
    bool ok = false;
    QString error, nextCursor;
    QVariantList projects;
};
// Host-only: actor must come from an authenticated session, never model input.
class ProjectRepository {
public:
    static ProjectPage list(PgConnection &connection, const QUuid &actorId,
                            const QString &afterId = {}, int limit = 50);
};
}
