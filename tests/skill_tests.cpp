#include <QtTest>
#include <QJsonArray>
#include "engines/skills/SkillValidator.h"
using mnaka::SkillValidator;
class SkillTests : public QObject {
    Q_OBJECT
    QJsonObject fixture() {
        QJsonObject m{{"id", "mnaka.project_health"}, {"version", "1.0.0"},
            {"min_host_version", "1.0.0"}, {"description", "Recorded build health"},
            {"permissions", QJsonArray{"build.status"}},
            {"inputs_schema", QJsonObject{{"type", "object"}, {"required", QJsonArray{"project_id"}},
                {"properties", QJsonObject{{"project_id", QJsonObject{{"type", "string"}, {"format", "uuid"}}}}},
                {"additionalProperties", false}}},
            {"workflow", QJsonArray{QJsonObject{{"tool", "build.status"},
                {"args", QJsonObject{{"project_id", "${input.project_id}"}}}}}},
            {"limits", QJsonObject{{"max_steps", 1}, {"timeout_seconds", 120}}}};
        m["artifact_hash"] = SkillValidator::payloadHash(m);
        return m;
    }
private slots:
    void validBinding() {
        auto m = fixture();
        QVERIFY(SkillValidator::inspect(m, "1.0.0").isEmpty());
        QVERIFY(!SkillValidator::validate(m, {}, "1.0.0").isEmpty());
        QJsonArray steps;
        const QString id = "aaaaaaaa-aaaa-4aaa-8aaa-aaaaaaaaaaaa";
        QVERIFY(SkillValidator::bindInputs(m, {{"project_id", id}}, {"build.status"}, "1.0.0", steps).isEmpty());
        QCOMPARE(steps.first().toObject().value("args").toObject().value("project_id").toString(), id);
        QVERIFY(!SkillValidator::bindInputs(m, {{"project_id", "../secret"}}, {"build.status"}, "1.0.0", steps).isEmpty());
        QVERIFY(steps.isEmpty());
        QVERIFY(!SkillValidator::bindInputs(m, {{"project_id", id}, {"extra", true}}, {"build.status"}, "1.0.0", steps).isEmpty());
        QVERIFY(steps.isEmpty());
    }
    void adversarialManifest() {
        auto m = fixture();
        QVERIFY(!SkillValidator::validate(m, {}, "1.0.0").isEmpty());
        QVERIFY(!SkillValidator::validate(m, {"build.status"}, "0.9.0").isEmpty());
        m["description"] = "tampered";
        QVERIFY(!SkillValidator::validate(m, {"build.status"}, "1.0.0").isEmpty());
        m = fixture();
        m["workflow"] = QJsonArray{QJsonObject{{"tool", "build.status"}, {"args", QJsonObject{{"project_id", "${input.project_id + script()}"}}}}};
        m["artifact_hash"] = SkillValidator::payloadHash(m);
        QVERIFY(!SkillValidator::validate(m, {"build.status"}, "1.0.0").isEmpty());
        m = fixture();
        m["workflow"] = QJsonArray{QJsonObject{{"tool", "shell.run"}, {"args", QJsonObject{{"project_id", "${input.project_id}"}}}}};
        m["artifact_hash"] = SkillValidator::payloadHash(m);
        QVERIFY(!SkillValidator::validate(m, {"build.status"}, "1.0.0").isEmpty());
        m = fixture();
        auto inputSchema = m.value("inputs_schema").toObject();
        inputSchema["required"] = QJsonArray{"missing"};
        m["inputs_schema"] = inputSchema;
        m["artifact_hash"] = SkillValidator::payloadHash(m);
        QVERIFY(!SkillValidator::validate(m, {"build.status"}, "1.0.0").isEmpty());
        m = fixture();
        m["dependencies"] = QJsonArray{"cycle"};
        m["artifact_hash"] = SkillValidator::payloadHash(m);
        QVERIFY(!SkillValidator::validate(m, {"build.status"}, "1.0.0").isEmpty());
    }
};
QTEST_GUILESS_MAIN(SkillTests)
#include "skill_tests.moc"
