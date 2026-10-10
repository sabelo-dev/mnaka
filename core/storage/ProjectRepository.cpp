#include "ProjectRepository.h"
#include <QSqlQuery>
#include <QSqlError>
#include <QDateTime>
#include <QRegularExpression>
namespace mnaka {
ProjectPage ProjectRepository::list(PgConnection &connection, const QUuid &actorId,
                                    const QString &afterId, int limit)
{
    ProjectPage page;
    if (actorId.isNull() || limit < 1 || limit > 100
        || (!afterId.isEmpty() && !QRegularExpression(
            "^[0-9a-fA-F]{8}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{12}$")
            .match(afterId).hasMatch())) {
        page.error = "INVALID_ARGUMENT: invalid project page request"; return page;
    }
    if (!connection.beginActorRequest(actorId, page.error)) return page;
    QVariantList rows;
    {
        QSqlQuery query(connection.m_database);
        const QString clause = afterId.isEmpty() ? QString() : " AND id > CAST(:cursor AS uuid)";
        if (!query.prepare("SELECT id, name, created_at FROM mnaka.projects WHERE actor_id=CAST(:actor AS uuid)"
                           + clause + " ORDER BY id LIMIT :limit")) {
            page.error = "INTERNAL: project query unavailable"; connection.rollback(); return page;
        }
        query.bindValue(":actor", actorId.toString(QUuid::WithoutBraces));
        if (!afterId.isEmpty()) query.bindValue(":cursor", afterId);
        query.bindValue(":limit", limit + 1);
        if (!query.exec()) {
            page.error = "INTERNAL: project listing failed"; connection.rollback(); return page;
        }
        while (query.next())
            rows.append(QVariantMap{{"id", query.value(0).toString()}, {"name", query.value(1).toString()},
                {"created_at", query.value(2).toDateTime().toUTC().toString(Qt::ISODateWithMs)}});
        if (query.lastError().isValid()) {
            page.error = "INTERNAL: project result unavailable"; connection.rollback(); return page;
        }
    }
    if (!connection.commit(page.error)) return page;
    if (rows.size() > limit) {
        rows.removeLast(); page.nextCursor = rows.last().toMap().value("id").toString();
    }
    page.projects = rows; page.ok = true; return page;
}
}
