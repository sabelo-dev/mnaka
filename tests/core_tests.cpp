#include <QtTest>
#include "core/policy/ToolBroker.h"
#include "core/contracts/SchemaValidator.h"
#include "core/orchestrator/TaskStates.h"
#include <QJsonArray>
using namespace mnaka;

class CoreTests : public QObject {
    Q_OBJECT
    BrokerSession session() {
        return {QUuid::createUuid(), QUuid::createUuid(), QUuid::createUuid(), "fixture:conversation", "v1"};
    }
    ToolIntentV1 intent(QString tool = "demo.echo") {
        return {"1.0", QUuid::createUuid(), QUuid::createUuid(), "assistant", tool, "1.0",
            {{"text", "fixture"}}, {"fixture:conversation"}, QUuid::createUuid().toString(QUuid::WithoutBraces),
            QDateTime::currentDateTimeUtc().addSecs(60)};
    }
    QJsonObject step(const ToolIntentV1 &i, const QJsonArray &dependencies = {}) {
        return {{"id", i.stepId.toString(QUuid::WithoutBraces)}, {"intent", i.toJson()},
            {"depends_on", dependencies}, {"required", true},
            {"postcondition", QJsonObject{{"predicate", "result.ok"}, {"arguments", QJsonObject{}}}}};
    }
    QJsonObject plan(QUuid taskId, const QJsonArray &steps) {
        return {{"schema_version", "1.0"}, {"task_id", taskId.toString(QUuid::WithoutBraces)}, {"steps", steps}};
    }
private slots:
    void terminalStateAndVerifiedResumption() {
        QVERIFY(canTransition("queued", "planning"));
        QVERIFY(canTransition("running", "cancelling"));
        QVERIFY(canTransition("cancelling", "cancelled"));
        QVERIFY(!canTransition("cancelled", "running"));
        QVERIFY(!canTransition("succeeded", "queued"));
        QVERIFY(!canTransition("failed", "planning"));
        QVERIFY(!canTransition("interrupted", "queued"));
        QVERIFY(canTransition("interrupted", "queued", true));
        QVERIFY(!canTransition("awaiting_approval", "succeeded"));
    }
    void skillSchemaRejectsAuthorityExpansion() {
        QJsonObject manifest{{"id", "mnaka.project_health"}, {"version", "1.0.0"}, {"min_host_version", "1.0.0"},
            {"description", "Recorded build health"}, {"permissions", QJsonArray{"build.status"}},
            {"inputs_schema", QJsonObject{{"type", "object"}, {"required", QJsonArray{"project_id"}},
                {"properties", QJsonObject{{"project_id", QJsonObject{{"type", "string"}, {"format", "uuid"}}}}},
                {"additionalProperties", false}}},
            {"workflow", QJsonArray{QJsonObject{{"tool", "build.status"}, {"args", QJsonObject{{"project_id", "${input.project_id}"}}}}}},
            {"limits", QJsonObject{{"max_steps", 8}, {"timeout_seconds", 120}}},
            {"artifact_hash", "sha256:" + QString(64, 'a')}};
        const auto schema = SchemaValidator::bundled("skill");
        QVERIFY(SchemaValidator::validate(manifest, schema).isEmpty());
        auto malicious = manifest; malicious["permissions"] = QJsonArray{"shell.execute"};
        QVERIFY(!SchemaValidator::validate(malicious, schema).isEmpty());
        malicious = manifest; malicious["workflow"] = QJsonArray{QJsonObject{{"tool", "build.status"},
            {"args", QJsonObject{{"project_id", "${eval('delete')}"}}}}};
        QVERIFY(!SchemaValidator::validate(malicious, schema).isEmpty());
        malicious = manifest; malicious["approved"] = true;
        QVERIFY(!SchemaValidator::validate(malicious, schema).isEmpty());
        QVERIFY(!SchemaValidator::validate(QJsonObject{}, QJsonObject{{"type", "object"}, {"allOf", QJsonArray{}}}).isEmpty());
    }
    void intentRoundTripAndAuthorityFields() {
        const auto original = intent();
        ToolIntentV1 parsed; QString error;
        QVERIFY(ToolIntentV1::fromJson(original.toJson(), parsed, error));
        QCOMPARE(parsed.toJson(), original.toJson());
        auto json = original.toJson(); json["approved"] = true;
        QVERIFY(!ToolIntentV1::fromJson(json, parsed, error));
        json = original.toJson(); json["task_id"] = "not-a-uuid";
        QVERIFY(!ToolIntentV1::fromJson(json, parsed, error));
        json = original.toJson(); json["deadline_utc"] = "2026-02-30T12:00:00Z";
        QVERIFY(!ToolIntentV1::fromJson(json, parsed, error));
        json = original.toJson(); json["tool_version"] = "2.0";
        QVERIFY(!ToolIntentV1::fromJson(json, parsed, error));
        QCOMPARE(normalizedHash(QJsonObject{{"b", 2}, {"a", 1}}), normalizedHash(QJsonObject{{"a", 1}, {"b", 2}}));
    }
    void denyByDefaultAndScope() {
        ToolBroker disabled(session());
        QVERIFY(disabled.submit(intent()).contains("PROVIDER_UNAVAILABLE"));
        ToolBroker fixture(session(), ToolBroker::Environment::Fixture);
        auto value = intent("project.apply_patch");
        QVERIFY(fixture.submit(value).contains("PERMISSION_DENIED"));
        value = intent(); value.arguments["shell"] = "anything";
        QVERIFY(fixture.submit(value).contains("INVALID_ARGUMENT"));
        value = intent(); value.resources = {"project:someone-else"};
        QVERIFY(fixture.submit(value).contains("PERMISSION_DENIED"));
        value = intent(); value.deadlineUtc = QDateTime::currentDateTimeUtc().addSecs(-1);
        QVERIFY(fixture.submit(value).contains("deadline"));
    }
    void planDAGValidation() {
        ToolBroker broker(session(), ToolBroker::Environment::Fixture);
        auto a = intent(); auto b = intent(); b.taskId = a.taskId;
        const QString aid = a.stepId.toString(QUuid::WithoutBraces), bid = b.stepId.toString(QUuid::WithoutBraces);
        QVERIFY(broker.validatePlan(plan(a.taskId, {step(a), step(b, {aid})})).isEmpty());
        QVERIFY(broker.validatePlan(plan(a.taskId, {step(a, {bid}), step(b, {aid})})).contains("cycle"));
        QVERIFY(broker.validatePlan(plan(a.taskId, {step(a), step(a)})).contains("duplicate"));
        QVERIFY(broker.validatePlan(plan(a.taskId, {step(a, {QUuid::createUuid().toString(QUuid::WithoutBraces)})})).contains("unknown dependency"));
        b.taskId = QUuid::createUuid();
        QVERIFY(broker.validatePlan(plan(a.taskId, {step(b)})).contains("IDs"));
        QJsonArray excessive; for (int i = 0; i < 31; ++i) excessive.append(step(a));
        QVERIFY(broker.validatePlan(plan(a.taskId, excessive)).contains("array limit"));
    }
    void approvalBindingAndReplay() {
        ToolBroker broker(session(), ToolBroker::Environment::Fixture);
        QSignalSpy approvals(&broker, &ToolBroker::approvalRequested), results(&broker, &ToolBroker::result);
        const auto value = intent("demo.review");
        QVERIFY(broker.submit(value).isEmpty());
        QCOMPARE(approvals.size(), 1); QCOMPARE(results.size(), 0);
        const auto approval = approvals.first()[2].toJsonObject();
        auto id = QUuid(approval["id"].toString());
        const auto hash = approval["intentHash"].toString(), nonce = approval["nonce"].toString();
        QVERIFY(!broker.decide(id, "changed-hash", nonce, true));
        QVERIFY(!broker.decide(id, hash, "changed-nonce", true));
        QCOMPARE(results.size(), 0);
        QVERIFY(broker.decide(id, hash, nonce, true));
        QCOMPARE(results.size(), 1);
        QVERIFY(!broker.decide(id, hash, nonce, true));
        QVERIFY(broker.submit(value).contains("CONFLICT"));
        QCOMPARE(results.size(), 1);
    }
    void cancelAndResourceLock() {
        ToolBroker broker(session(), ToolBroker::Environment::Fixture);
        QSignalSpy results(&broker, &ToolBroker::result), approvals(&broker, &ToolBroker::approvalRequested);
        auto value = intent("demo.review");
        QVERIFY(broker.submit(value).isEmpty());
        QVERIFY(broker.submit(intent("demo.review")).contains("RATE_LIMITED"));
        const auto approval = approvals.first()[2].toJsonObject();
        broker.cancel(value.taskId);
        QCOMPARE(results.size(), 1);
        QCOMPARE(results.first()[2].toJsonObject()["code"].toString(), "CANCELLED");
        QVERIFY(!broker.decide(QUuid(approval["id"].toString()), approval["intentHash"].toString(), approval["nonce"].toString(), true));
        value.stepId = QUuid::createUuid();
        QVERIFY(broker.submit(value).contains("CANCELLED"));
        QVERIFY(broker.submit(intent("demo.review")).isEmpty());
    }
    void expiryAndOutputTerminal() {
        ToolBroker broker(session(), ToolBroker::Environment::Fixture);
        QSignalSpy results(&broker, &ToolBroker::result);
        auto value = intent("demo.review"); value.deadlineUtc = QDateTime::currentDateTimeUtc().addMSecs(60);
        QVERIFY(broker.submit(value).isEmpty());
        QTRY_COMPARE(results.size(), 1);
        QVERIFY(!results.first()[2].toJsonObject()["ok"].toBool());
        QTest::qWait(100); QCOMPARE(results.size(), 1);
        QVERIFY(broker.submit(intent()).isEmpty());
        QTRY_COMPARE(results.size(), 2);
    }
};
QTEST_GUILESS_MAIN(CoreTests)
#include "core_tests.moc"
