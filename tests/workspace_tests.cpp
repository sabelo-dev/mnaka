#include <QtTest>
#include "apps/client/WorkspaceModel.h"
#include <QTemporaryDir>
#include <QImage>
#include <QFile>
class WorkspaceTests : public QObject {
    Q_OBJECT
private slots:
    void skillInspectionRejectsInvalidInputWithoutExecution() {
        WorkspaceModel model;
        model.inspectSkill("not JSON");
        QVERIFY(model.skillReport().contains("valid JSON"));
        model.inspectSkill(QString(65537, 'x'));
        QVERIFY(model.skillReport().contains("64 KiB"));
        model.inspectSkill("{}");
        QVERIFY(model.skillReport().contains("Validation failed"));
        model.clearSkillReport();
        QVERIFY(model.skillReport().isEmpty());
        QVERIFY(model.tasks().isEmpty());
        QVERIFY(model.approval().isEmpty());
    }
    void imagePreviewAndClear() {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const auto path = directory.filePath("image.png");
        QImage image(32, 32, QImage::Format_RGB32); image.fill(Qt::cyan);
        QVERIFY(image.save(path));
        WorkspaceModel model;
        model.selectImage(QUrl::fromLocalFile(path));
        QTRY_VERIFY(!model.imageLoading());
        QVERIFY(model.imagePreview().startsWith("data:image/png;base64,"));
        // Small images are previewed at their own size, never upscaled.
        QCOMPARE(QImage::fromData(QByteArray::fromBase64(model.imagePreview().section(',', 1).toLatin1())).size(), QSize(32, 32));
        QVERIFY(model.imageNotice().contains("Local preview"));
        model.clearImage();
        QVERIFY(model.imagePreview().isEmpty());
        model.selectImage(QUrl::fromLocalFile(path));
        model.clearImage();
        QTest::qWait(100);
        QVERIFY(model.imagePreview().isEmpty());
    }
    void imageInputRejected() {
        WorkspaceModel model;
        model.selectImage(QUrl("https://example.invalid/image.png"));
        QVERIFY(!model.imageLoading());
        QVERIFY(model.imagePreview().isEmpty());
        QVERIFY(model.imageNotice().contains("local"));
        QVERIFY(model.notice().isEmpty());
        QTemporaryDir directory;
        QFile file(directory.filePath("image.png"));
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("not an image"); file.close();
        model.selectImage(QUrl::fromLocalFile(file.fileName()));
        QTRY_VERIFY(!model.imageLoading());
        QVERIFY(model.imagePreview().isEmpty());
        QVERIFY(model.imageNotice().contains("supported"));
    }
    void emptyWorkspace() {
        WorkspaceModel model;
        QVERIFY(model.messages().isEmpty());
        QVERIFY(model.tasks().isEmpty());
        QVERIFY(model.events().isEmpty());
        QVERIFY(model.approval().isEmpty());
        for (const auto &capability : model.capabilities())
            QVERIFY(!capability.toMap()["enabled"].toBool());
    }
    void unavailableSubmissionPreservesEmptyState() {
        WorkspaceModel model;
        QVERIFY(!model.submit("hello", "Assistant"));
        QVERIFY(model.notice().contains("not been submitted"));
        QVERIFY(!model.busy());
        QVERIFY(model.messages().isEmpty());
        QVERIFY(model.tasks().isEmpty());
        model.decideApproval(true);
        model.cancel();
        QVERIFY(model.events().isEmpty());
    }
    void mockSessionRunsLabelledTaskAndReplaysEvents() {
        WorkspaceModel model;
        model.setMockSession(true);
        QVERIFY(model.mockSession());
        int enabled = 0;
        for (const auto &capability : model.capabilities()) enabled += capability.toMap()["enabled"].toBool();
        QCOMPARE(enabled, 1);
        QVERIFY(model.submit("Mnaka, tell me about traffic on the N1", "Assistant"));
        QVERIFY(model.busy());
        QCOMPARE(model.state(), "Planning");
        QVERIFY(!model.submit("second", "Assistant"));
        QVERIFY(model.notice().contains("already active"));
        QCOMPARE(model.tasks().size(), 1);
        QTRY_VERIFY(!model.busy());
        QCOMPARE(model.state(), "Idle");
        QCOMPARE(model.messages().size(), 2);
        const auto reply = model.messages().last().toMap();
        QVERIFY(reply["mock"].toBool());
        QVERIFY(reply["text"].toString().contains("direction"));
        QVERIFY(reply["text"].toString().contains("No live traffic provider"));
        const auto task = model.tasks().first().toMap();
        QCOMPARE(task["state"].toString(), "succeeded");
        QVERIFY(task["evidence"].toString().startsWith("Mock fixture"));
        const auto replay = model.eventsAfter(task["id"].toString(), 2);
        QVERIFY(!replay.isEmpty());
        QCOMPARE(replay.first().toMap()["sequence"].toInt(), 3);
        QCOMPARE(replay.size() + 2, model.events().size());
        model.setMockSession(false);
        QVERIFY(model.messages().isEmpty());
        QVERIFY(model.tasks().isEmpty());
        QVERIFY(!model.submit("hello", "Assistant"));
    }
    void mockApprovalAllowDenyAndCancel() {
        WorkspaceModel model;
        model.setMockSession(true);
        QVERIFY(model.submit("run the review demo", "Developer"));
        QTRY_COMPARE(model.state(), "Awaiting approval");
        QCOMPARE(model.approval()["tool"].toString(), "demo.review");
        QVERIFY(!model.approval().contains("nonce"));
        QVERIFY(!model.approval()["intentHash"].toString().isEmpty());
        model.decideApproval(true);
        QVERIFY(!model.busy());
        QVERIFY(model.approval().isEmpty());
        QCOMPARE(model.tasks().first().toMap()["state"].toString(), "succeeded");
        model.decideApproval(true);
        QCOMPARE(model.tasks().size(), 1);

        QVERIFY(model.submit("run the review demo", "Developer"));
        QTRY_COMPARE(model.state(), "Awaiting approval");
        model.decideApproval(false);
        QCOMPARE(model.tasks().first().toMap()["state"].toString(), "failed");
        QVERIFY(model.notice().contains("PERMISSION_DENIED"));

        QVERIFY(model.submit("run the review demo", "Developer"));
        QTRY_COMPARE(model.state(), "Awaiting approval");
        model.cancel();
        QVERIFY(!model.busy());
        QVERIFY(model.approval().isEmpty());
        QCOMPARE(model.tasks().first().toMap()["state"].toString(), "cancelled");
        model.decideApproval(true);
        QCOMPARE(model.tasks().first().toMap()["state"].toString(), "cancelled");
    }
    void mockCancelDuringPlanningShowsNoLateReply() {
        WorkspaceModel model;
        model.setMockSession(true);
        QVERIFY(model.submit("hello", "Assistant"));
        model.cancel();
        QVERIFY(!model.busy());
        QTest::qWait(900);
        QCOMPARE(model.messages().size(), 1);
        QVERIFY(model.streamingText().isEmpty());
        QCOMPARE(model.tasks().first().toMap()["state"].toString(), "cancelled");
        QVERIFY(model.submit("hello again", "Assistant"));
        model.clearHistory();
        QVERIFY(model.mockSession());
        QVERIFY(model.messages().isEmpty());
        QVERIFY(model.tasks().isEmpty());
        QTest::qWait(900);
        QVERIFY(model.messages().isEmpty());
    }
    void invalidInput() {
        WorkspaceModel model;
        QVERIFY(!model.submit(" ", "Assistant"));
        QVERIFY(!model.submit(QString(4001, 'x'), "Assistant"));
        QVERIFY(!model.submit("hello", "Unknown"));
        QVERIFY(model.tasks().isEmpty());
    }
};
QTEST_GUILESS_MAIN(WorkspaceTests)
#include "workspace_tests.moc"
