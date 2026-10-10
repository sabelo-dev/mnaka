#include <QtTest>
#include <QJsonArray>
#include <atomic>
#include "core/orchestrator/EngineRegistry.h"
#include "engines/assistant/ConversationEngine.h"
#include "adapters/mock/MockModelProvider.h"

using namespace mnaka;
class EngineTests : public QObject {
    Q_OBJECT
    QJsonObject request(QString text = "hello") {
        return {{"schema_version", "1.0"}, {"input", QJsonObject{{"type", "text"}, {"text", text}}},
            {"mode", "assistant"}, {"execution_host_id", QUuid::createUuid().toString(QUuid::WithoutBraces)},
            {"idempotency_key", QUuid::createUuid().toString(QUuid::WithoutBraces)}};
    }
    QJsonObject context() {
        return {{"step_id", QUuid::createUuid().toString(QUuid::WithoutBraces)}, {"resource_id", "fixture:conversation"},
            {"deadline_utc", QDateTime::currentDateTimeUtc().addSecs(60).toString(Qt::ISODateWithMs)}};
    }
private slots:
    void duplicateRegistryAndDisabledCapabilities() {
        EngineRegistry registry;
        QVERIFY(registry.registerService(new ConversationEngine(new MockModelProvider), true, "fixture"));
        auto duplicate = new ConversationEngine(new MockModelProvider);
        QVERIFY(!registry.registerService(duplicate, true, "fixture")); delete duplicate;
        for (const auto &id : QStringList{"software", "control", "skills", "memory", "vision", "proactive"}) {
            QVERIFY(registry.registerService(new UnavailableEngine(id), false, "pending"));
            QVERIFY(registry.submit(id, QUuid::createUuid(), context(), request()).contains("PROVIDER_UNAVAILABLE"));
        }
        QCOMPARE(registry.capabilities().size(), 7);
        int enabled = 0;
        for (auto value : registry.capabilities()) enabled += value.toMap()["enabled"].toBool();
        QCOMPARE(enabled, 1);
    }
    void planningOccursOffGuiThread() {
        EngineRegistry registry;
        auto engine = new ConversationEngine(new MockModelProvider);
        std::atomic<QThread *> observed = nullptr;
        connect(engine, &EngineService::planReady, engine, [&observed](QUuid, QJsonObject) {
            observed.store(QThread::currentThread());
        }, Qt::DirectConnection);
        QVERIFY(registry.registerService(engine, true, "fixture"));
        QSignalSpy plans(&registry, &EngineRegistry::planReady), answers(&registry, &EngineRegistry::responseReady);
        auto id = QUuid::createUuid();
        QVERIFY(registry.submit("assistant", id, context(), request("traffic on N1")).isEmpty());
        QVERIFY(registry.submit("assistant", id, context(), request()).contains("CONFLICT"));
        QTRY_COMPARE(plans.size(), 1);
        QCOMPARE(answers.size(), 1);
        QVERIFY(observed.load() != nullptr);
        QVERIFY(observed.load() != QThread::currentThread());
        QVERIFY(answers.first()[1].toString().contains("direction"));
        QVERIFY(answers.first()[2].toJsonObject()["mock"].toBool());
        const auto plan = plans.first()[1].toJsonObject();
        QCOMPARE(plan["steps"].toArray().size(), 1);
    }
    void cancelledPlanningDiscardsLateSignals() {
        EngineRegistry registry;
        QVERIFY(registry.registerService(new ConversationEngine(new MockModelProvider), true, "fixture"));
        QSignalSpy plans(&registry, &EngineRegistry::planReady), answers(&registry, &EngineRegistry::responseReady),
            failures(&registry, &EngineRegistry::failed);
        auto id = QUuid::createUuid();
        QVERIFY(registry.submit("assistant", id, context(), request()).isEmpty());
        registry.cancel(id); registry.cancel(id);
        QCOMPARE(failures.size(), 1);
        QTest::qWait(800);
        QCOMPARE(plans.size(), 0); QCOMPARE(answers.size(), 0); QCOMPARE(failures.size(), 1);
    }
    void providerCancellationDuringDeltaIsTerminalOnce() {
        MockModelProvider provider;
        QSignalSpy complete(&provider, &ModelProvider::completed), failed(&provider, &ModelProvider::failed);
        connect(&provider, &ModelProvider::delta, &provider, [&provider](QUuid id, QString) { provider.cancel(id); });
        auto id = QUuid::createUuid();
        QVERIFY(provider.start(id, {{"text", "hello"}}));
        QVERIFY(!provider.start(id, {{"text", "changed"}}));
        QTRY_COMPARE(failed.size(), 1);
        provider.cancel(id); QTest::qWait(400);
        QCOMPARE(failed.size(), 1); QCOMPARE(complete.size(), 0);
        QCOMPARE(failed.first()[1].toString(), "CANCELLED");
    }
    void malformedPlanningInputFailsWithoutPlan() {
        EngineRegistry registry;
        QVERIFY(registry.registerService(new ConversationEngine(new MockModelProvider), true, "fixture"));
        QSignalSpy plans(&registry, &EngineRegistry::planReady), failures(&registry, &EngineRegistry::failed);
        QVERIFY(registry.submit("assistant", QUuid::createUuid(), {}, request()).isEmpty());
        QTRY_COMPARE(failures.size(), 1);
        QCOMPARE(failures.first()[1].toString(), "INVALID_ARGUMENT");
        QCOMPARE(plans.size(), 0);
        auto bad = request(); bad["actor_id"] = QUuid::createUuid().toString();
        QVERIFY(registry.submit("assistant", QUuid::createUuid(), context(), bad).contains("INVALID_ARGUMENT"));
    }
};
QTEST_GUILESS_MAIN(EngineTests)
#include "engine_tests.moc"
