#include <QtTest>
#include <QJsonArray>
#include "core/orchestrator/TaskOrchestrator.h"
#include "core/contracts/SchemaValidator.h"
#include "engines/assistant/ConversationEngine.h"
#include "adapters/mock/MockModelProvider.h"

using namespace mnaka;
namespace {
QString idText(const QUuid &id) { return id.toString(QUuid::WithoutBraces); }
struct ScriptStep { QString tool; QList<int> dependencies; bool required = true; };
// Emits a fixed multi-step plan so DAG ordering and failure handling can be exercised.
class ScriptedEngine : public EngineService {
    Q_OBJECT
public:
    explicit ScriptedEngine(QList<ScriptStep> script) : m_script(std::move(script)) {}
    QString engineId() const override { return "assistant"; }
    QStringList supportedTools() const override { return {"demo.echo", "demo.review"}; }
public slots:
    void plan(QUuid taskId, QJsonObject context, QJsonObject) override {
        QList<QUuid> ids;
        for (int i = 0; i < m_script.size(); ++i) ids.append(QUuid::createUuid());
        QJsonArray steps;
        for (int i = 0; i < m_script.size(); ++i) {
            ToolIntentV1 intent{"1.0", taskId, ids[i], "assistant", m_script[i].tool, "1.0", {{"text", "step"}},
                {context["resource_id"].toString()}, idText(QUuid::createUuid()),
                QDateTime::fromString(context["deadline_utc"].toString(), Qt::ISODateWithMs)};
            QJsonArray dependencies;
            for (int index : m_script[i].dependencies) dependencies.append(idText(ids[index]));
            steps.append(QJsonObject{{"id", idText(ids[i])}, {"intent", intent.toJson()}, {"depends_on", dependencies},
                {"required", m_script[i].required},
                {"postcondition", QJsonObject{{"predicate", "result.ok"}, {"arguments", QJsonObject{}}}}});
        }
        emit planReady(taskId, {{"schema_version", "1.0"}, {"task_id", idText(taskId)}, {"steps", steps}});
    }
    void cancel(QUuid) override {}
private:
    QList<ScriptStep> m_script;
};
struct Host {
    BrokerSession session{QUuid::createUuid(), QUuid::createUuid(), QUuid::createUuid(), "fixture:conversation", "v1"};
    InMemoryTaskStore store;
    EngineRegistry registry;
    ToolBroker broker{session, ToolBroker::Environment::Fixture};
    TaskOrchestrator orchestrator{session, &registry, &broker, &store};
    explicit Host(EngineService *engine = new ConversationEngine(new MockModelProvider)) { registry.registerService(engine, true, "fixture"); }
    QJsonObject request(const QString &text = "hello") const {
        return {{"schema_version", "1.0"}, {"input", QJsonObject{{"type", "text"}, {"text", text}}}, {"mode", "assistant"},
            {"execution_host_id", idText(session.executionHostId)}, {"idempotency_key", idText(QUuid::createUuid())}};
    }
    QString state(const QUuid &id) const { TaskRecord record; return store.find(id, record) ? record.state : QString(); }
    QStringList types(const QUuid &id) const {
        QStringList result;
        for (const auto &event : orchestrator.eventsAfter(id, 0)) {
            const auto payload = event["payload"].toObject();
            result.append(event["type"].toString() == "task.state_changed"
                ? "state:" + payload["new_state"].toString() : event["type"].toString());
        }
        return result;
    }
};
}

class OrchestratorTests : public QObject {
    Q_OBJECT
private slots:
    void mockTaskRunsThroughBrokerWithOrderedEvents() {
        Host host;
        QSignalSpy replies(&host.orchestrator, &TaskOrchestrator::responseReady);
        QUuid id;
        QVERIFY(host.orchestrator.submit(host.request(), id).isEmpty());
        QVERIFY(!id.isNull());
        QCOMPARE(host.state(id), "planning");
        QVERIFY(host.orchestrator.isActive(id));
        QTRY_COMPARE(host.state(id), "succeeded");
        QVERIFY(!host.orchestrator.isActive(id));
        QCOMPARE(replies.size(), 1);
        QVERIFY(replies.first()[2].toJsonObject()["mock"].toBool());
        QCOMPARE(host.types(id), (QStringList{"task.created", "state:planning", "state:running", "step.started",
            "step.progress", "step.completed", "state:succeeded", "task.completed"}));
        const auto events = host.orchestrator.eventsAfter(id, 0);
        QSet<QString> eventIds;
        for (int i = 0; i < events.size(); ++i) {
            QCOMPARE(events[i]["sequence"].toInteger(), qint64(i + 1));
            QVERIFY(SchemaValidator::validate(events[i], SchemaValidator::bundled("event")).isEmpty());
            eventIds.insert(events[i]["event_id"].toString());
        }
        QCOMPARE(eventIds.size(), events.size());
        // Replay resumes after the client's last acknowledged sequence.
        const auto replay = host.orchestrator.eventsAfter(id, 5);
        QCOMPARE(replay.size(), events.size() - 5);
        QCOMPARE(replay.first()["sequence"].toInteger(), qint64(6));
        QVERIFY(host.orchestrator.eventsAfter(QUuid::createUuid(), 0).isEmpty());
        const auto result = host.orchestrator.taskResult(id);
        QVERIFY(SchemaValidator::validate(result, SchemaValidator::bundled("task-result")).isEmpty());
        QVERIFY(!result.contains("error"));
    }
    void idempotentSubmissionAndConflict() {
        Host host;
        const auto request = host.request();
        QUuid first, second, third;
        QVERIFY(host.orchestrator.submit(request, first).isEmpty());
        QVERIFY(host.orchestrator.submit(request, second).isEmpty());
        QCOMPARE(second, first);
        QCOMPARE(host.orchestrator.tasks().size(), 1);
        auto changed = request;
        changed["input"] = QJsonObject{{"type", "text"}, {"text", "something else"}};
        QVERIFY(host.orchestrator.submit(changed, third).startsWith("CONFLICT"));
        QVERIFY(third.isNull());
        QTRY_COMPARE(host.state(first), "succeeded");
        // The original is still returned after completion; nothing runs twice.
        QVERIFY(host.orchestrator.submit(request, second).isEmpty());
        QCOMPARE(second, first);
        QCOMPARE(host.orchestrator.tasks().size(), 1);
        QCOMPARE(host.types(first).count("step.started"), 1);
    }
    void rejectedSubmissionsCreateNoTask() {
        Host host;
        QUuid id;
        auto request = host.request();
        request["execution_host_id"] = idText(QUuid::createUuid());
        QVERIFY(host.orchestrator.submit(request, id).startsWith("DEVICE_OFFLINE"));
        request = host.request(); request["actor_id"] = idText(QUuid::createUuid());
        QVERIFY(host.orchestrator.submit(request, id).startsWith("INVALID_ARGUMENT"));
        request = host.request(); request["approved"] = true;
        QVERIFY(host.orchestrator.submit(request, id).startsWith("INVALID_ARGUMENT"));
        host.store.setAvailable(false);
        QVERIFY(host.orchestrator.submit(host.request(), id).startsWith("PROVIDER_UNAVAILABLE"));
        host.store.setAvailable(true);
        QVERIFY(id.isNull());
        QVERIFY(host.orchestrator.tasks().isEmpty());
        InMemoryTaskStore store;
        TaskOrchestrator anonymous({}, &host.registry, &host.broker, &store);
        QCOMPARE(anonymous.submit(host.request(), id), "UNAUTHENTICATED");
    }
    void approvalIsBoundAndSingleUse() {
        Host host;
        QSignalSpy approvals(&host.orchestrator, &TaskOrchestrator::approvalRequested);
        QUuid id;
        QVERIFY(host.orchestrator.submit(host.request("run the review demo"), id).isEmpty());
        QTRY_COMPARE(host.state(id), "awaiting_approval");
        QCOMPARE(approvals.size(), 1);
        const auto approval = approvals.first()[1].toJsonObject();
        const QUuid approvalId(approval["id"].toString());
        const auto hash = approval["intentHash"].toString(), nonce = approval["nonce"].toString();
        QVERIFY(!host.orchestrator.decideApproval(approvalId, "changed", nonce, true));
        QVERIFY(!host.orchestrator.decideApproval(approvalId, hash, "changed", true));
        QVERIFY(!host.orchestrator.decideApproval(QUuid::createUuid(), hash, nonce, true));
        QCOMPARE(host.state(id), "awaiting_approval");
        QVERIFY(host.orchestrator.decideApproval(approvalId, hash, nonce, true));
        QCOMPARE(host.state(id), "succeeded");
        QVERIFY(!host.orchestrator.decideApproval(approvalId, hash, nonce, true));
        QCOMPARE(host.types(id), (QStringList{"task.created", "state:planning", "state:running", "step.started",
            "approval.requested", "state:awaiting_approval", "step.completed", "state:running", "state:succeeded",
            "task.completed"}));
    }
    void deniedApprovalFailsTask() {
        Host host;
        QSignalSpy approvals(&host.orchestrator, &TaskOrchestrator::approvalRequested);
        QUuid id;
        QVERIFY(host.orchestrator.submit(host.request("run the review demo"), id).isEmpty());
        QTRY_COMPARE(approvals.size(), 1);
        const auto approval = approvals.first()[1].toJsonObject();
        QVERIFY(host.orchestrator.decideApproval(QUuid(approval["id"].toString()), approval["intentHash"].toString(),
            approval["nonce"].toString(), false));
        QCOMPARE(host.state(id), "failed");
        const auto result = host.orchestrator.taskResult(id);
        QVERIFY(SchemaValidator::validate(result, SchemaValidator::bundled("task-result")).isEmpty());
        QCOMPARE(result["error"].toObject()["code"].toString(), "PERMISSION_DENIED");
        QVERIFY(!result["error"].toObject()["retryable"].toBool());
    }
    void cancelDuringPlanningDiscardsLateOutput() {
        Host host;
        QSignalSpy replies(&host.orchestrator, &TaskOrchestrator::responseReady);
        QUuid id;
        QVERIFY(host.orchestrator.submit(host.request(), id).isEmpty());
        host.orchestrator.cancel(id);
        host.orchestrator.cancel(id);
        QCOMPARE(host.state(id), "cancelled");
        const auto types = host.types(id);
        QTest::qWait(900);
        QCOMPARE(host.types(id), types);
        QCOMPARE(types, (QStringList{"task.created", "state:planning", "state:cancelling", "state:cancelled", "task.completed"}));
        QCOMPARE(replies.size(), 0);
        QCOMPARE(host.orchestrator.taskResult(id)["error"].toObject()["code"].toString(), "CANCELLED");
    }
    void cancelWhileAwaitingApprovalRevokesIt() {
        Host host;
        QSignalSpy approvals(&host.orchestrator, &TaskOrchestrator::approvalRequested);
        QUuid id;
        QVERIFY(host.orchestrator.submit(host.request("run the review demo"), id).isEmpty());
        QTRY_COMPARE(approvals.size(), 1);
        const auto approval = approvals.first()[1].toJsonObject();
        host.orchestrator.cancel(id);
        QCOMPARE(host.state(id), "cancelled");
        QVERIFY(!host.orchestrator.decideApproval(QUuid(approval["id"].toString()), approval["intentHash"].toString(),
            approval["nonce"].toString(), true));
        QVERIFY(!host.broker.decide(QUuid(approval["id"].toString()), approval["intentHash"].toString(),
            approval["nonce"].toString(), true));
        QCOMPARE(host.state(id), "cancelled");
        QVERIFY(host.types(id).contains("step.completed"));
    }
    void dependenciesOrderStepsBeyondBrokerCapacity() {
        // Five parallel reads exceed the broker's three-read limit; the sixth waits for all of them.
        Host host(new ScriptedEngine({{"demo.echo"}, {"demo.echo"}, {"demo.echo"}, {"demo.echo"}, {"demo.echo"},
            {"demo.echo", {0, 1, 2, 3, 4}}}));
        QUuid id;
        QVERIFY(host.orchestrator.submit(host.request(), id).isEmpty());
        QTRY_COMPARE(host.state(id), "succeeded");
        TaskRecord record;
        QVERIFY(host.store.find(id, record));
        const auto last = record.plan["steps"].toArray()[5].toObject()["id"].toString();
        int completedBeforeLast = 0, started = 0;
        for (const auto &event : host.orchestrator.eventsAfter(id, 0)) {
            const auto step = event["payload"].toObject()["step_id"].toString();
            if (event["type"] == "step.started") { ++started; if (step == last) QCOMPARE(completedBeforeLast, 5); }
            if (event["type"] == "step.completed" && step != last) ++completedBeforeLast;
        }
        QCOMPARE(started, 6);
        for (const auto &state : std::as_const(record.steps)) QCOMPARE(state.toString(), "succeeded");
    }
    void optionalFailureSkipsDependentsOnly() {
        Host host(new ScriptedEngine({{"demo.review", {}, false}, {"demo.echo", {0}, false}, {"demo.echo"}}));
        QSignalSpy approvals(&host.orchestrator, &TaskOrchestrator::approvalRequested);
        QUuid id;
        QVERIFY(host.orchestrator.submit(host.request(), id).isEmpty());
        QTRY_COMPARE(approvals.size(), 1);
        const auto approval = approvals.first()[1].toJsonObject();
        QVERIFY(host.orchestrator.decideApproval(QUuid(approval["id"].toString()), approval["intentHash"].toString(),
            approval["nonce"].toString(), false));
        QTRY_COMPARE(host.state(id), "succeeded");
        TaskRecord record;
        QVERIFY(host.store.find(id, record));
        const auto steps = record.plan["steps"].toArray();
        QCOMPARE(record.steps[steps[0].toObject()["id"].toString()].toString(), "failed");
        QCOMPARE(record.steps[steps[1].toObject()["id"].toString()].toString(), "skipped");
        QCOMPARE(record.steps[steps[2].toObject()["id"].toString()].toString(), "succeeded");
    }
    void requiredStepBehindFailedDependencyFailsTask() {
        Host host(new ScriptedEngine({{"demo.review", {}, false}, {"demo.echo", {0}, true}}));
        QSignalSpy approvals(&host.orchestrator, &TaskOrchestrator::approvalRequested);
        QUuid id;
        QVERIFY(host.orchestrator.submit(host.request(), id).isEmpty());
        QTRY_COMPARE(approvals.size(), 1);
        const auto approval = approvals.first()[1].toJsonObject();
        QVERIFY(host.orchestrator.decideApproval(QUuid(approval["id"].toString()), approval["intentHash"].toString(),
            approval["nonce"].toString(), false));
        QTRY_COMPARE(host.state(id), "failed");
        QCOMPARE(host.types(id).count("step.started"), 1);
    }
    void deadlineFailsTaskOnce() {
        Host host;
        host.orchestrator.setExecutionDeadlineMs(400);
        QUuid id;
        QVERIFY(host.orchestrator.submit(host.request("run the review demo"), id).isEmpty());
        QTRY_COMPARE_WITH_TIMEOUT(host.state(id), "failed", 3000);
        const auto types = host.types(id);
        QCOMPARE(types.count("task.completed"), 1);
        QTest::qWait(500);
        QCOMPARE(host.types(id), types);
    }
    void storeFailureStopsWorkAndRestartInterrupts() {
        Host host;
        QUuid id;
        QVERIFY(host.orchestrator.submit(host.request("run the review demo"), id).isEmpty());
        QTRY_COMPARE(host.state(id), "awaiting_approval");
        const auto before = host.types(id);
        // The store disappears: the pending approval must not be executable any more.
        host.store.setAvailable(false);
        host.orchestrator.cancel(id);
        QVERIFY(!host.orchestrator.isActive(id));
        host.store.setAvailable(true);
        QCOMPARE(host.state(id), "awaiting_approval");
        QCOMPARE(host.types(id), before);
        QUuid blocked;
        QVERIFY(host.orchestrator.submit(host.request(), blocked).isEmpty());
        QTRY_COMPARE(host.state(blocked), "succeeded");
        // A restarted host reconciles instead of resuming or replaying.
        TaskOrchestrator restarted(host.session, &host.registry, &host.broker, &host.store);
        restarted.recover();
        QCOMPARE(host.state(id), "interrupted");
        QCOMPARE(host.state(blocked), "succeeded");
        TaskRecord record;
        QVERIFY(host.store.find(id, record));
        for (const auto &state : std::as_const(record.steps)) QCOMPARE(state.toString(), "interrupted");
        const auto events = host.orchestrator.eventsAfter(id, 0);
        QCOMPARE(events.last()["sequence"].toInteger(), qint64(events.size()));
        QVERIFY(!host.types(id).contains("task.completed"));
        restarted.recover();
        QCOMPARE(host.orchestrator.eventsAfter(id, 0).size(), events.size());
        QVERIFY(SchemaValidator::validate(restarted.taskResult(id), SchemaValidator::bundled("task-result")).isEmpty());
    }
    void hundredQueuedTasksStayBounded() {
        Host host;
        QList<QUuid> ids;
        int limited = 0;
        for (int i = 0; i < 100; ++i) {
            QUuid id;
            const auto error = host.orchestrator.submit(host.request(), id);
            if (error.isEmpty()) ids.append(id); else { QVERIFY(error.startsWith("RATE_LIMITED")); ++limited; }
        }
        QCOMPARE(ids.size(), 16);
        QCOMPARE(limited, 84);
        for (const auto &id : std::as_const(ids)) QTRY_COMPARE_WITH_TIMEOUT(host.state(id), "succeeded", 10000);
        for (const auto &id : std::as_const(ids)) QVERIFY(host.orchestrator.eventsAfter(id, 0).size() <= 16);
    }
};
QTEST_GUILESS_MAIN(OrchestratorTests)
#include "orchestrator_tests.moc"
