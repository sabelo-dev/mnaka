#include <QtTest>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTcpServer>
#include <QTcpSocket>
#include <QUrlQuery>
#include "apps/client/WorkspaceModel.h"

// Loopback stand-in for the Supabase project: Auth, the Data API tables and the assistant function.
class FakeProject : public QTcpServer {
public:
    QStringList calls;
    QJsonArray history;
    int assistantStatus = 200;
    QJsonObject lastAssistantBody;
    QByteArray lastAuthorization;
    FakeProject() {
        listen(QHostAddress::LocalHost);
        connect(this, &QTcpServer::newConnection, this, [this] {
            auto *socket = nextPendingConnection();
            auto *buffer = new QByteArray;
            connect(socket, &QObject::destroyed, this, [buffer] { delete buffer; });
            connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
            connect(socket, &QTcpSocket::readyRead, this, [this, socket, buffer] {
                buffer->append(socket->readAll());
                const auto headerEnd = buffer->indexOf("\r\n\r\n");
                if (headerEnd < 0) return;
                const auto headers = buffer->left(headerEnd);
                const auto lengthAt = headers.toLower().indexOf("content-length:");
                const auto length = lengthAt < 0 ? 0 : headers.mid(lengthAt + 15, headers.indexOf('\r', lengthAt) - lengthAt - 15).trimmed().toInt();
                if (buffer->size() < headerEnd + 4 + length) return;
                const auto verb = QString::fromLatin1(headers.split(' ').value(0));
                const auto target = QString::fromLatin1(headers.split(' ').value(1));
                const auto authAt = headers.toLower().indexOf("authorization:");
                lastAuthorization = authAt < 0 ? QByteArray() : headers.mid(authAt + 14, headers.indexOf('\r', authAt) - authAt - 14).trimmed();
                const auto body = QJsonDocument::fromJson(buffer->mid(headerEnd + 4, length)).object();
                buffer->clear();
                calls.append(verb + " " + target.section('?', 0, 0));
                int status = 200; QJsonDocument answer;
                const auto path = target.section('?', 0, 0);
                if (path == "/auth/v1/token") answer = QJsonDocument(QJsonObject{{"access_token", "access-1"}, {"refresh_token", "refresh-1"}, {"expires_in", 3600},
                    {"user", QJsonObject{{"id", "2f1c6f0e-9a53-4b7e-8d1a-3c5b7e9f0a12"}, {"email", "user@example.test"}}}});
                else if (path == "/auth/v1/logout") status = 204;
                else if (lastAuthorization != "Bearer access-1") { status = 401; answer = QJsonDocument(QJsonObject{{"code", "UNAUTHENTICATED"}}); }
                else if (path == "/functions/v1/assistant") {
                    lastAssistantBody = body; status = assistantStatus;
                    answer = QJsonDocument(status == 200 ? QJsonObject{{"text", "Reply to: " + body["text"].toString()}, {"model", "test-model"}, {"saved", true}}
                        : QJsonObject{{"code", "PROVIDER_UNAVAILABLE"}, {"message", "down"}});
                } else if (verb == "GET") answer = QJsonDocument(path.endsWith("mnaka_messages") ? history : QJsonArray{});
                else if (verb == "POST") {
                    status = 201; auto row = body; row["id"] = QUuid::createUuid().toString(QUuid::WithoutBraces);
                    if (path.endsWith("mnaka_rules")) row["enabled"] = false;
                    answer = QJsonDocument(QJsonArray{row});
                } else if (verb == "PATCH") {
                    auto row = body; row["id"] = QUrlQuery(target.section('?', 1)).queryItemValue("id").mid(3);
                    answer = QJsonDocument(QJsonArray{row});
                } else if (verb == "DELETE") status = 204;
                const auto payload = answer.isNull() ? QByteArray() : answer.toJson(QJsonDocument::Compact);
                socket->write("HTTP/1.1 " + QByteArray::number(status) + " X\r\nContent-Type: application/json\r\nContent-Length: "
                    + QByteArray::number(payload.size()) + "\r\nConnection: close\r\n\r\n" + payload);
                socket->disconnectFromHost();
            });
        });
    }
    void configure() const {
        qputenv("MNAKA_SUPABASE_URL", QString("http://127.0.0.1:%1").arg(serverPort()).toLatin1());
        qputenv("MNAKA_SUPABASE_PUBLISHABLE_KEY", "sb_publishable_test");
    }
};

class LiveTests : public QObject {
    Q_OBJECT
    static QString status(const WorkspaceModel &model, const QString &id) {
        for (const auto &value : model.capabilities()) if (value.toMap()["id"].toString() == id) return value.toMap()["status"].toString();
        return {};
    }
private slots:
    void cleanup() { qunsetenv("MNAKA_SUPABASE_URL"); qunsetenv("MNAKA_SUPABASE_PUBLISHABLE_KEY"); }
    void signedOutClientSendsNothing() {
        FakeProject project; project.configure();
        WorkspaceModel model;
        QVERIFY(model.accountConfigured() && !model.connected());
        QVERIFY(!model.submit("hello", "Assistant"));
        model.addMemory("x"); model.addRule("a", "b"); model.interpretImage("what"); model.saveSkill();
        QTest::qWait(150);
        QVERIFY(project.calls.isEmpty());
        QVERIFY(model.memories().isEmpty() && model.rules().isEmpty());
    }
    void signInLoadsHistoryAndTalksToTheModel() {
        FakeProject project; project.configure();
        // The Data API returns newest first; the conversation shows oldest first.
        project.history = {QJsonObject{{"role", "assistant"}, {"content", "earlier answer"}}, QJsonObject{{"role", "user"}, {"content", "earlier question"}}};
        WorkspaceModel model;
        model.signIn("user@example.test", "password");
        QTRY_VERIFY(model.connected());
        QVERIFY(!model.mockSession());
        QCOMPARE(model.accountEmail(), QString("user@example.test"));
        QTRY_COMPARE(model.messages().size(), 2);
        QCOMPARE(model.messages().first().toMap()["text"].toString(), QString("earlier question"));
        QVERIFY(status(model, "assistant").contains("Your model"));
        QVERIFY(status(model, "control").contains("desktop app"));

        QVERIFY(model.submit("What is MNAKA?", "Assistant"));
        QTRY_VERIFY_WITH_TIMEOUT(!model.busy(), 10000);
        QCOMPARE(model.messages().size(), 4);
        QCOMPARE(model.messages().last().toMap()["text"].toString(), QString("Reply to: What is MNAKA?"));
        QVERIFY(!model.messages().last().toMap()["mock"].toBool());
        QCOMPARE(project.lastAssistantBody["text"].toString(), QString("What is MNAKA?"));
        QCOMPARE(model.tasks().size(), 1);
        QCOMPARE(model.tasks().first().toMap()["state"].toString(), QString("succeeded"));
        QVERIFY(!model.tasks().first().toMap()["evidence"].toString().contains("Mock"));
        QVERIFY(model.notice().isEmpty());
    }
    void unavailableModelFailsTheTaskWithoutAReply() {
        FakeProject project; project.configure(); project.assistantStatus = 503;
        WorkspaceModel model;
        model.signIn("user@example.test", "password");
        QTRY_VERIFY(model.connected());
        QVERIFY(model.submit("hello", "Assistant"));
        QTRY_VERIFY_WITH_TIMEOUT(!model.busy(), 10000);
        QCOMPARE(model.tasks().first().toMap()["state"].toString(), QString("failed"));
        QVERIFY(model.notice().contains("PROVIDER_UNAVAILABLE"));
        QCOMPARE(model.messages().size(), 1); // the user's own message only
    }
    void savedRowsRoundTripAndSignOutClearsEverything() {
        FakeProject project; project.configure();
        WorkspaceModel model;
        model.signIn("user@example.test", "password");
        QTRY_VERIFY(model.connected());
        model.addMemory("  Prefers metric units  ");
        QTRY_COMPARE(model.memories().size(), 1);
        QCOMPARE(model.memories().first().toMap()["content"].toString(), QString("Prefers metric units"));
        model.addMemory(QString(2001, 'x'));
        QVERIFY(model.dataNotice().contains("2000"));
        QCOMPARE(model.memories().size(), 1);

        model.addRule("Morning build check", "Weekdays 08:00");
        QTRY_COMPARE(model.rules().size(), 1);
        QVERIFY(!model.rules().first().toMap()["enabled"].toBool());
        model.setRuleEnabled(model.rules().first().toMap()["id"].toString(), true);
        QTRY_VERIFY(model.rules().first().toMap()["enabled"].toBool());
        model.deleteRule(model.rules().first().toMap()["id"].toString());
        QTRY_VERIFY(model.rules().isEmpty());
        model.deleteMemory("not-a-uuid");
        QTest::qWait(100);
        QCOMPARE(model.memories().size(), 1);

        QVERIFY(!model.skillSavable()); // nothing has passed inspection
        model.inspectSkill("{}");
        QVERIFY(!model.skillSavable());

        QVERIFY(model.submit("hello", "Assistant"));
        QTRY_VERIFY_WITH_TIMEOUT(!model.busy(), 10000);
        model.clearHistory();
        QVERIFY(model.messages().isEmpty() && model.tasks().isEmpty());
        QTRY_VERIFY(project.calls.contains("DELETE /rest/v1/mnaka_messages"));
        QVERIFY(model.connected());

        model.signOut();
        QVERIFY(!model.connected());
        QVERIFY(model.memories().isEmpty() && model.messages().isEmpty() && model.tasks().isEmpty());
        QVERIFY(!model.submit("hello", "Assistant"));
    }
    void imageInterpretationSendsABoundedJpeg() {
        FakeProject project; project.configure();
        QTemporaryDir directory;
        const auto path = directory.filePath("image.png");
        QImage image(2000, 1000, QImage::Format_RGB32); image.fill(Qt::cyan);
        QVERIFY(image.save(path));
        WorkspaceModel model;
        model.selectImage(QUrl::fromLocalFile(path));
        QTRY_VERIFY(!model.imageLoading());
        model.interpretImage("What colour is this?"); // signed out: stays on the device
        QTest::qWait(100);
        QVERIFY(project.calls.isEmpty());
        model.signIn("user@example.test", "password");
        QTRY_VERIFY(model.connected());
        model.interpretImage("What colour is this?");
        QVERIFY(model.imageInterpreting());
        QTRY_VERIFY(!model.imageInterpreting());
        QCOMPARE(model.imageAnswer(), QString("Reply to: What colour is this?"));
        const auto sent = project.lastAssistantBody["image"].toString();
        QVERIFY(sent.startsWith("data:image/jpeg;base64,"));
        const auto decoded = QImage::fromData(QByteArray::fromBase64(sent.section(',', 1).toLatin1()));
        QCOMPARE(decoded.size(), QSize(1024, 512));
        model.clearImage();
        QVERIFY(model.imageAnswer().isEmpty());
    }
};
QTEST_MAIN(LiveTests)
#include "live_tests.moc"
