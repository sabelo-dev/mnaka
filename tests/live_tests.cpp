#include <QtTest>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTcpServer>
#include <QTcpSocket>
#include <QSettings>
#include <QUrlQuery>
#include "apps/client/WorkspaceModel.h"
#include "engines/software/LineDiff.h"

// Loopback stand-in for the Supabase project: Auth, the Data API tables and the assistant function.
class FakeProject : public QTcpServer {
public:
    QStringList calls;
    QJsonArray history;
    int assistantStatus = 200;
    bool modelConfigured = true, modelWorking = true;
    int tokenStatus = 200;
    QJsonObject lastAssistantBody, lastDeveloperBody, developerReply, lastDeviceBody;
    QJsonArray devices;
    int developerStatus = 200;
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
                if (path == "/auth/v1/token" && tokenStatus != 200) { status = tokenStatus; answer = QJsonDocument(QJsonObject{{"msg", "Invalid Refresh Token"}}); }
                else if (path == "/auth/v1/token") answer = QJsonDocument(QJsonObject{{"access_token", "access-1"}, {"refresh_token", "refresh-1"}, {"expires_in", 3600},
                    {"user", QJsonObject{{"id", "2f1c6f0e-9a53-4b7e-8d1a-3c5b7e9f0a12"}, {"email", "user@example.test"}}}});
                else if (path == "/auth/v1/logout") status = 204;
                else if (lastAuthorization != "Bearer access-1") { status = 401; answer = QJsonDocument(QJsonObject{{"code", "UNAUTHENTICATED"}}); }
                else if (path == "/functions/v1/assistant" && body["probe"].toBool())
                    answer = QJsonDocument(QJsonObject{{"configured", modelConfigured}, {"provider", "anthropic"}, {"model", modelConfigured ? "test-model" : ""},
                        {"working", modelConfigured && modelWorking}, {"problem", modelWorking ? "" : "The provider rejected the key."}});
                else if (path == "/functions/v1/assistant") {
                    lastAssistantBody = body; status = assistantStatus;
                    answer = QJsonDocument(status == 200 ? QJsonObject{{"text", "Reply to: " + body["text"].toString()}, {"model", "test-model"}, {"saved", true}}
                        : QJsonObject{{"code", "PROVIDER_UNAVAILABLE"}, {"message", "down"}});
                } else if (path == "/functions/v1/developer") {
                    lastDeveloperBody = body; status = developerStatus;
                    answer = QJsonDocument(status == 200 ? developerReply : QJsonObject{{"code", "PROVIDER_UNAVAILABLE"}});
                } else if (path.endsWith("mnaka_devices") && verb == "GET") answer = QJsonDocument(devices);
                else if (path.endsWith("mnaka_devices") && verb == "POST") {
                    lastDeviceBody = body; status = 201; auto row = body;
                    row["id"] = QUuid::createUuid().toString(QUuid::WithoutBraces); row["online"] = true; row["last_seen"] = "2026-10-12T10:00:00.000Z";
                    devices.append(row); answer = QJsonDocument(QJsonArray{row});
                } else if (path.endsWith("mnaka_devices") && verb == "DELETE") {
                    const auto id = QUrlQuery(target.section('?', 1)).queryItemValue("id").mid(3);
                    for (qsizetype i = 0; i < devices.size(); ++i) if (devices[i].toObject()["id"].toString() == id) { devices.removeAt(i); break; }
                    status = 204;
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
    void cleanup() {
        for (const char *name : {"MNAKA_SUPABASE_URL", "MNAKA_SUPABASE_PUBLISHABLE_KEY", "MNAKA_SESSION_FILE", "MNAKA_AUTH_ERROR",
                "MNAKA_AUTH_ERROR_DESCRIPTION", "MNAKA_AUTH_REFRESH_TOKEN"}) qunsetenv(name);
    }
    void sessionIsRememberedRestoredAndForgotten() {
        FakeProject project; project.configure();
        { WorkspaceModel model; QVERIFY(!model.sessionPersists()); } // a desktop keeps nothing unless told where
        QTemporaryDir directory;
        const auto file = directory.filePath("session.ini");
        qputenv("MNAKA_SESSION_FILE", file.toUtf8());
        const auto saved = [&file] { return QSettings(file, QSettings::IniFormat).value("account/refreshToken").toString(); };
        {
            WorkspaceModel model;
            QVERIFY(model.sessionPersists());
            QTest::qWait(100);
            QVERIFY(project.calls.isEmpty()); // nothing saved yet: nothing to restore
            model.signIn("user@example.test", "password");
            QTRY_VERIFY(model.connected());
            QCOMPARE(saved(), QString("refresh-1"));
        }
        {
            WorkspaceModel model; // a later start: signed in again without a password
            QTRY_VERIFY(model.connected());
            QCOMPARE(model.accountEmail(), QString("user@example.test"));
            model.signOut();
            QVERIFY(!model.connected());
            QVERIFY(saved().isEmpty());
        }
        { QSettings store(file, QSettings::IniFormat); store.setValue("account/refreshToken", "revoked"); }
        project.tokenStatus = 400;
        {
            WorkspaceModel model;
            QTRY_VERIFY(model.accountNotice().contains("expired"));
            QVERIFY(!model.connected());
            QVERIFY(saved().isEmpty()); // a refused token is not offered again
        }
    }
    void emailLinkResultsAreHandled() {
        FakeProject project; project.configure();
        qputenv("MNAKA_AUTH_ERROR", "otp_expired");
        {
            WorkspaceModel model;
            QVERIFY(model.accountNotice().contains("Resend confirmation email"));
            QVERIFY(!model.connected());
            model.resendConfirmation("user@example.test");
            QTRY_VERIFY(project.calls.contains("POST /auth/v1/resend"));
        }
        qputenv("MNAKA_AUTH_ERROR", "access_denied"); qputenv("MNAKA_AUTH_ERROR_DESCRIPTION", "Link is invalid");
        { WorkspaceModel model; QVERIFY(model.accountNotice().contains("Link is invalid")); }
        qunsetenv("MNAKA_AUTH_ERROR"); qunsetenv("MNAKA_AUTH_ERROR_DESCRIPTION");
        qputenv("MNAKA_AUTH_REFRESH_TOKEN", "from-email-link"); // a confirmation link signs the user in
        { WorkspaceModel model; QTRY_VERIFY(model.connected()); QCOMPARE(model.accountEmail(), QString("user@example.test")); }
    }
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
        QVERIFY(status(model, "assistant").contains("Model via"));
        QTRY_COMPARE(model.modelStatus(), QString("ready"));
        QCOMPARE(model.modelName(), QString("test-model"));
        QVERIFY(status(model, "control").contains("desktop app"));
        for (const auto &value : model.capabilities()) {
            const auto entry = value.toMap();
            const auto id = entry["id"].toString();
            QCOMPARE(entry["enabled"].toBool(), id == "assistant");
            QCOMPARE(entry["available"].toBool(), QStringList({"memory", "skills", "vision", "proactive"}).contains(id));
        }

        QVERIFY(model.submit("What is MNAKA?", "Assistant"));
        QTRY_VERIFY_WITH_TIMEOUT(!model.busy(), 10000);
        QCOMPARE(model.messages().size(), 4);
        QCOMPARE(model.messages().last().toMap()["text"].toString(), QString("Reply to: What is MNAKA?"));
        QVERIFY(!model.messages().last().toMap()["mock"].toBool());
        QCOMPARE(project.lastAssistantBody["text"].toString(), QString("What is MNAKA?"));
        QVERIFY(!project.lastAssistantBody.contains("mode")); // plain conversation is the gateway's default
        QCOMPARE(model.tasks().size(), 1);
        QCOMPARE(model.tasks().first().toMap()["state"].toString(), QString("succeeded"));
        QVERIFY(!model.tasks().first().toMap()["evidence"].toString().contains("Mock"));
        QVERIFY(model.notice().isEmpty());

        // The selected mode reaches the gateway, which runs a different workflow for it.
        QVERIFY(model.submit("Plan a launch", "Autonomous"));
        QTRY_VERIFY_WITH_TIMEOUT(!model.busy(), 10000);
        QCOMPARE(project.lastAssistantBody["mode"].toString(), QString("autonomous"));
        QVERIFY(model.submit("Write a parser", "Developer"));
        QTRY_VERIFY_WITH_TIMEOUT(!model.busy(), 10000);
        QCOMPARE(project.lastAssistantBody["mode"].toString(), QString("developer"));
    }
    void connectingThisDeviceRegistersItAndTellsTheModel() {
        QTemporaryDir folder;
        qputenv("MNAKA_DEVICE_FILE", folder.filePath("device.ini").toUtf8());
        FakeProject project; project.configure();
        // Another of the account's devices, connected earlier from somewhere else.
        project.devices.append(QJsonObject{{"id", "7c0f6c1e-1111-4222-8333-444455556666"}, {"install_key", QString(32, 'a')}, {"name", "Phone"},
            {"kind", "android"}, {"facts", QJsonObject{{"os", "Android 14"}, {"cpu", "arm64"}}}, {"online", false}, {"last_seen", "2026-10-11T08:00:00.000Z"}});
        WorkspaceModel model;
        QVERIFY(!model.deviceFactLines().isEmpty() && !model.deviceSuggestedName().isEmpty());
        model.connectDevice("Signed out"); // nothing is sent without an account
        model.signIn("user@example.test", "password");
        QTRY_VERIFY(model.connected());
        QTRY_COMPARE(model.devices().size(), 1);
        QVERIFY(!model.thisDeviceConnected()); // signing in does not connect a device by itself
        QVERIFY(!project.calls.contains("POST /rest/v1/mnaka_devices"));

        model.connectDevice("   ");
        QVERIFY(model.deviceNotice().contains("1 to 60"));
        model.connectDevice("Study PC");
        QTRY_VERIFY(model.thisDeviceConnected());
        QCOMPARE(model.devices().size(), 2);
        const auto here = model.devices().last().toMap();
        QCOMPARE(here["name"].toString(), QString("Study PC"));
        QVERIFY(here["thisDevice"].toBool() && here["online"].toBool());
        QVERIFY(!model.devices().first().toMap()["thisDevice"].toBool() && !model.devices().first().toMap()["online"].toBool());
        QVERIFY(QRegularExpression("^[0-9a-f]{32}$").match(project.lastDeviceBody["install_key"].toString()).hasMatch());
        QCOMPARE(project.lastDeviceBody["kind"].toString(), QString("desktop"));
        QVERIFY(!project.lastDeviceBody["facts"].toObject()["os"].toString().isEmpty());
        // Only what the screen says is shared is sent.
        for (const auto &key : project.lastDeviceBody["facts"].toObject().keys())
            QVERIFY(QStringList({"os", "cpu", "locale", "timezone", "screen"}).contains(key));

        QVERIFY(model.submit("What am I using?", "Assistant"));
        QTRY_VERIFY_WITH_TIMEOUT(!model.busy(), 10000);
        QCOMPARE(project.lastAssistantBody["device"].toString(), here["id"].toString());

        // The same install finds its own row again after signing back in.
        model.signOut();
        QTRY_VERIFY(!model.connected());
        QVERIFY(!model.thisDeviceConnected() && model.devices().isEmpty());
        model.signIn("user@example.test", "password");
        QTRY_VERIFY(model.thisDeviceConnected());

        model.disconnectDevice(here["id"].toString());
        QTRY_VERIFY(!model.thisDeviceConnected());
        QCOMPARE(model.devices().size(), 1);
        QVERIFY(model.submit("And now?", "Assistant"));
        QTRY_VERIFY_WITH_TIMEOUT(!model.busy(), 10000);
        QVERIFY(!project.lastAssistantBody.contains("device"));
        qunsetenv("MNAKA_DEVICE_FILE");
    }
    void unavailableModelFailsTheTaskWithoutAReply() {
        FakeProject project; project.configure(); project.assistantStatus = 503; project.modelConfigured = false;
        WorkspaceModel model;
        QCOMPARE(model.modelStatus(), QString("unknown"));
        model.signIn("user@example.test", "password");
        QTRY_VERIFY(model.connected());
        QTRY_COMPARE(model.modelStatus(), QString("missing"));
        model.signOut();
        project.modelConfigured = true; project.modelWorking = false; // a key is set, but the provider refuses it
        model.signIn("user@example.test", "password");
        QTRY_VERIFY(model.connected());
        QTRY_COMPARE(model.modelStatus(), QString("failing"));
        QVERIFY(model.modelProblem().contains("rejected"));
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
    void wakeWordIsRecognisedOnlyAtTheStart() {
        const auto request = [](const QString &spoken) { bool woke = false; const auto text = VoiceInput::requestAfterWakeWord(spoken, &woke); return woke ? "[" + text + "]" : QString("ignored"); };
        QCOMPARE(request("Mnaka please check the weather for me"), QString("[please check the weather for me]"));
        QCOMPARE(request("Mnaka, open settings."), QString("[open settings.]"));
        QCOMPARE(request("hey manaka what time is it"), QString("[what time is it]"));
        QCOMPARE(request("OK Monaka: cancel"), QString("[cancel]"));
        QCOMPARE(request("em naka open tasks"), QString("[open tasks]"));
        // What a recogniser actually writes for a name it has never seen.
        for (const char *heard : {"Monica", "monika", "manuka", "Monaco", "Manaka", "menaka", "munaka", "naka", "nakka", "my naka", "em naka", "I'm naka", "in aka"})
            QCOMPARE(request(QString(heard) + " please check the weather for me"), QString("[please check the weather for me]"));
        for (const char *other : {"monk", "monkey", "manic", "mink", "mango", "monday", "many", "banana", "maniac", "knock", "Nikki"})
            QCOMPARE(request(QString(other) + " please check the weather"), QString("ignored"));
        QCOMPARE(request("Mnaka"), QString("[]"));
        QCOMPARE(request("Mnaka."), QString("[]"));
        // Not addressed to MNAKA: the name later in a sentence, or ordinary words.
        QCOMPARE(request("I told Mnaka to check the weather"), QString("ignored"));
        QCOMPARE(request("make a note of this"), QString("ignored"));
        QCOMPARE(request("what a nice day"), QString("ignored"));
        QCOMPARE(request("hey there"), QString("ignored"));
        QCOMPARE(request("banana smoothie please"), QString("ignored"));
        QCOMPARE(request(""), QString("ignored"));
    }
    void handsFreeActsOnlyOnAddressedSpeech() {
        VoiceInput voice;
        QSignalSpy heard(&voice, &VoiceInput::recognized);
        voice.setHandsFree(true);
        QVERIFY(voice.openMicrophone() && voice.awaitingRequest()); // hands-free listens for requests from the start
        voice.setOpenMicrophone(false); // answering only to its name
        voice.handleEvent("final", "did you see the match last night");
        voice.handleEvent("final", "I think Mnaka is listening");
        QCOMPARE(heard.size(), 0); // bystander speech goes nowhere
        QVERIFY(voice.partial().isEmpty());
        QVERIFY(voice.notice().contains("Heard \"I…\"")); // one word only, to show a misheard name
        QVERIFY(!voice.notice().contains("listening"));
        voice.handleEvent("final", "Mnaka please check the weather for me");
        QCOMPARE(heard.size(), 1);
        QCOMPARE(heard.last().first().toString(), QString("please check the weather for me"));
        // The name on its own opens a short window for the request.
        voice.handleEvent("final", "Mnaka");
        QVERIFY(voice.awaitingRequest());
        QCOMPARE(heard.size(), 1);
        voice.handleEvent("final", "open settings");
        QCOMPARE(heard.size(), 2);
        QCOMPARE(heard.last().first().toString(), QString("open settings"));
        QVERIFY(!voice.awaitingRequest());
        voice.handleEvent("final", "open settings"); // window closed: ignored again
        QCOMPARE(heard.size(), 2);
        // Open microphone: every utterance is a request, for as long as it stays latched.
        voice.setOpenMicrophone(true);
        QVERIFY(voice.openMicrophone() && voice.awaitingRequest() && voice.handsFree());
        voice.handleEvent("final", "what is the capital of Kenya");
        voice.handleEvent("final", "Mnaka open tasks");
        voice.handleEvent("final", "and one more thing");
        QCOMPARE(heard.size(), 5);
        QCOMPARE(heard[2].first().toString(), QString("what is the capital of Kenya"));
        QCOMPARE(heard[3].first().toString(), QString("open tasks")); // the name is still stripped
        QVERIFY(voice.awaitingRequest()); // it does not time out or close after a request
        voice.handleEvent("end", ""); // the browser ending a session does not unlatch it
        voice.stop();
        QVERIFY(voice.openMicrophone() && voice.awaitingRequest());
        voice.setOpenMicrophone(false);
        QVERIFY(!voice.awaitingRequest() && voice.handsFree());
        voice.handleEvent("final", "nobody asked");
        QCOMPARE(heard.size(), 5); // back to answering only to its name
        voice.setOpenMicrophone(true);
        // A refused microphone turns hands-free off rather than retrying for ever.
        voice.handleEvent("error", "not-allowed");
        QVERIFY(!voice.handsFree() && !voice.openMicrophone() && !voice.awaitingRequest());
        QVERIFY(voice.notice().contains("refused"));
        // Without hands-free, a pressed microphone sends whatever was said, name or not.
        voice.handleEvent("final", "what is the capital of France");
        QCOMPARE(heard.last().first().toString(), QString("what is the capital of France"));
        voice.handleEvent("final", "Mnaka open tasks");
        QCOMPARE(heard.last().first().toString(), QString("open tasks"));
    }
    void lineDiffReportsExactChanges() {
        const auto same = mnaka::diffLines("a\nb\n", "a\nb\n");
        QVERIFY(same.hunks.isEmpty() && same.added == 0 && same.removed == 0);
        QString before, after;
        for (int i = 1; i <= 20; ++i) { before += QString("line %1\n").arg(i); after += i == 10 ? QString("changed\nextra\n") : QString("line %1\n").arg(i); }
        const auto diff = mnaka::diffLines(before, after);
        QCOMPARE(diff.added, 2); QCOMPARE(diff.removed, 1);
        QCOMPARE(diff.hunks.size(), 1);
        QCOMPARE(diff.hunks.first().header(), QString("@@ -7,7 +7,8 @@"));
        QCOMPARE(diff.hunks.first().lines.size(), 9); // three before, one removed, two added, three after
        QCOMPARE(diff.hunks.first().lines[3].sign, QChar('-')); QCOMPARE(diff.hunks.first().lines[3].number, 10);
        QCOMPARE(diff.hunks.first().lines[5].text, QString("extra")); QCOMPARE(diff.hunks.first().lines[5].number, 11);
        const auto created = mnaka::diffLines("", "x\ny\n");
        QCOMPARE(created.added, 2); QCOMPARE(created.hunks.first().header(), QString("@@ -0,0 +1,2 @@"));
        const auto far = mnaka::diffLines(before, QString(before).replace("line 2\n", "two\n").replace("line 19\n", "nineteen\n"));
        QCOMPARE(far.hunks.size(), 2);
        QCOMPARE(mnaka::diffLines("a\r\nb\r\n", "a\nb\n").hunks.size(), 0); // line endings alone are not a change
    }
    void developerRejectsUnsafeFilesAndModelOutput() {
        DeveloperSession session;
        QVERIFY(session.addFile("main.cpp", "int main() { return 0; }\n").isEmpty());
        QVERIFY(!session.addFile("main.cpp", "again").isEmpty());
        QVERIFY(!session.addFile("../secret.txt", "x").isEmpty());
        QVERIFY(!session.addFile("dir/file.txt", "x").isEmpty());
        QVERIFY(!session.addFile("binary.bin", QByteArray("a\0b", 3)).isEmpty());
        QVERIFY(!session.addFile("latin1.txt", QByteArray("caf\xe9")).isEmpty());
        QVERIFY(!session.addFile("big.txt", QByteArray(65537, 'x')).isEmpty());
        QCOMPARE(session.files().size(), 1);

        const QMap<QString, QString> files{{"main.cpp", "int main() { return 0; }\n"}};
        QMap<QString, QString> changes; QString summary;
        const auto file = [](const QString &path, const QJsonValue &content) { return QJsonObject{{"path", path}, {"content", content}}; };
        QVERIFY(!DeveloperSession::readProposal({{"summary", "x"}}, files, changes, summary).isEmpty());
        QVERIFY(!DeveloperSession::readProposal({{"files", QJsonArray{file("../../etc/passwd", "x")}}}, files, changes, summary).isEmpty());
        QVERIFY(!DeveloperSession::readProposal({{"files", QJsonArray{file("C:\\x.txt", "x")}}}, files, changes, summary).isEmpty());
        QVERIFY(!DeveloperSession::readProposal({{"files", QJsonArray{file("main.cpp", 5)}}}, files, changes, summary).isEmpty());
        QVERIFY(!DeveloperSession::readProposal({{"files", QJsonArray{file("main.cpp", QString(70000, 'x'))}}}, files, changes, summary).isEmpty());
        QVERIFY(!DeveloperSession::readProposal({{"files", QJsonArray{file("a.txt", "1"), file("a.txt", "2")}}}, files, changes, summary).isEmpty());
        QVERIFY(DeveloperSession::readProposal({{"summary", "none"}, {"files", QJsonArray{file("main.cpp", "int main() { return 0; }\n")}}}, files, changes, summary).isEmpty());
        QVERIFY(changes.isEmpty()); // identical content is not a change
        QCOMPARE(summary, QString("none"));

        session.request("do something"); // signed out: nothing is sent
        QVERIFY(session.notice().contains("Sign in"));
        QVERIFY(!session.busy());
    }
    void developerRequestDiffApplyAndSave() {
        FakeProject project; project.configure();
        QTemporaryDir directory;
        const auto source = directory.filePath("main.cpp");
        { QFile file(source); QVERIFY(file.open(QIODevice::WriteOnly)); file.write("#include <cstdio>\nint main()\n{\n    return 0;\n}\n"); }
        WorkspaceModel model;
        auto *dev = model.developer();
        dev->addFiles({QUrl::fromLocalFile(source), QUrl::fromLocalFile(directory.filePath("missing.txt"))});
        QCOMPARE(dev->files().size(), 1);
        QVERIFY(dev->notice().contains("missing.txt"));
        QCOMPARE(dev->steps().first().toMap()["state"].toString(), QString("done"));
        QCOMPARE(dev->steps()[3].toMap()["state"].toString(), QString("skipped")); // tests are never claimed

        model.signIn("user@example.test", "password");
        QTRY_VERIFY(model.connected());
        project.developerReply = {{"summary", "Prints a greeting and adds a notes file."}, {"files", QJsonArray{
            QJsonObject{{"path", "main.cpp"}, {"content", "#include <cstdio>\nint main()\n{\n    std::puts(\"hello\");\n    return 0;\n}\n"}},
            QJsonObject{{"path", "NOTES.md"}, {"content", "# Notes\n"}}}}};
        dev->request("Print hello");
        QVERIFY(dev->busy());
        QTRY_VERIFY(!dev->busy());
        QCOMPARE(project.lastDeveloperBody["request"].toString(), QString("Print hello"));
        QCOMPARE(project.lastDeveloperBody["files"].toArray().size(), 1);
        QCOMPARE(project.lastDeveloperBody["files"].toArray().first().toObject()["path"].toString(), QString("main.cpp")); // name only, no folder
        QVERIFY(dev->hasProposal());
        QCOMPARE(dev->proposal().size(), 2);
        QCOMPARE(dev->added(), 2); QCOMPARE(dev->removed(), 0);
        QCOMPARE(dev->summary(), QString("Prints a greeting and adds a notes file."));
        QCOMPARE(dev->files().size(), 2); // the proposed new file is listed, marked new
        // Nothing has changed yet.
        { QFile file(source); QVERIFY(file.open(QIODevice::ReadOnly)); QVERIFY(!file.readAll().contains("hello")); }

        dev->applyProposal();
        QVERIFY(!dev->hasProposal());
        QCOMPARE(dev->steps().last().toMap()["state"].toString(), QString("done"));
        int unsaved = 0;
        for (const auto &value : dev->files()) if (value.toMap()["unsaved"].toBool()) ++unsaved;
        QCOMPARE(unsaved, 2);
        // Still nothing on disk until the user saves.
        { QFile file(source); QVERIFY(file.open(QIODevice::ReadOnly)); QVERIFY(!file.readAll().contains("hello")); }
        const auto target = directory.filePath("out.cpp");
        dev->saveFile("main.cpp", QUrl::fromLocalFile(target));
        { QFile file(target); QVERIFY(file.open(QIODevice::ReadOnly)); QVERIFY(file.readAll().contains("std::puts(\"hello\")")); }

        // A refused or malformed reply leaves the project untouched.
        project.developerStatus = 503;
        dev->request("Another change");
        QTRY_VERIFY(!dev->busy());
        QVERIFY(!dev->hasProposal() && dev->notice().contains("not available"));
        project.developerStatus = 200;
        project.developerReply = {{"summary", "x"}, {"files", QJsonArray{QJsonObject{{"path", "../evil.sh"}, {"content", "rm -rf"}}}}};
        dev->request("Another change");
        QTRY_VERIFY(!dev->busy());
        QVERIFY(!dev->hasProposal());
        QCOMPARE(dev->files().size(), 2);

        project.developerReply = {{"summary", "ok"}, {"files", QJsonArray{QJsonObject{{"path", "NOTES.md"}, {"content", "# Notes\nmore\n"}}}}};
        dev->request("Extend notes");
        QTRY_VERIFY(dev->hasProposal());
        dev->discardProposal();
        QVERIFY(!dev->hasProposal());
        model.signOut();
        dev->request("x");
        QVERIFY(dev->notice().contains("Sign in"));
        QCOMPARE(dev->files().size(), 2); // files survive sign-out; they never left this object
    }
};
QTEST_MAIN(LiveTests)
#include "live_tests.moc"
