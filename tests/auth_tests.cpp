#include <QtTest>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTcpServer>
#include <QTcpSocket>
#include "adapters/supabase/SupabaseAuth.h"

using namespace mnaka;

// Loopback stand-in for the Supabase Auth endpoints; records what the client sent.
class FakeAuthServer : public QTcpServer {
public:
    int status = 200;
    QJsonObject response;
    QString lastPath;
    QByteArray lastHeaders;
    QJsonObject lastBody;
    FakeAuthServer() {
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
                lastHeaders = headers;
                lastPath = QString::fromLatin1(headers.split(' ').value(1));
                lastBody = QJsonDocument::fromJson(buffer->mid(headerEnd + 4, length)).object();
                const auto body = QJsonDocument(response).toJson(QJsonDocument::Compact);
                socket->write("HTTP/1.1 " + QByteArray::number(status) + " X\r\nContent-Type: application/json\r\nContent-Length: "
                    + QByteArray::number(body.size()) + "\r\nConnection: close\r\n\r\n" + body);
                socket->disconnectFromHost();
            });
        });
    }
    QUrl url() const { return QUrl(QString("http://127.0.0.1:%1").arg(serverPort())); }
};

class AuthTests : public QObject {
    Q_OBJECT
    const QString key = "sb_publishable_test";
    static QString jwt(const QString &role) {
        return "e30." + QString::fromLatin1(QJsonDocument(QJsonObject{{"role", role}}).toJson(QJsonDocument::Compact)
            .toBase64(QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals)) + ".sig";
    }
    static QJsonObject session(int expiresIn = 3600) {
        return {{"access_token", "access-1"}, {"refresh_token", "refresh-1"}, {"expires_in", expiresIn},
            {"user", QJsonObject{{"id", "2f1c6f0e-9a53-4b7e-8d1a-3c5b7e9f0a12"}, {"email", "user@example.test"}}}};
    }
private slots:
    void onlyPublishableKeysAndHttpsOrigins() {
        const QUrl project("https://project.supabase.co");
        QVERIFY(SupabaseAuth::validateConfig(project, key).isEmpty());
        QVERIFY(SupabaseAuth::validateConfig(project, jwt("anon")).isEmpty());
        QVERIFY(!SupabaseAuth::validateConfig(project, "sb_secret_abc").isEmpty());
        QVERIFY(!SupabaseAuth::validateConfig(project, jwt("service_role")).isEmpty());
        QVERIFY(!SupabaseAuth::validateConfig(project, "").isEmpty());
        QVERIFY(!SupabaseAuth::validateConfig(QUrl("http://project.supabase.co"), key).isEmpty());
        QVERIFY(!SupabaseAuth::validateConfig(QUrl("https://user:pw@project.supabase.co"), key).isEmpty());
        QVERIFY(!SupabaseAuth::validateConfig(QUrl("https://project.supabase.co/rest/v1"), key).isEmpty());
        QVERIFY(!SupabaseAuth::validateConfig(QUrl(), key).isEmpty());
    }
    void misconfiguredClientSendsNothing() {
        FakeAuthServer server;
        SupabaseAuth auth(server.url(), "sb_secret_abc");
        QSignalSpy notices(&auth, &SupabaseAuth::notice);
        auth.signIn("user@example.test", "password");
        QCOMPARE(notices.size(), 1);
        QVERIFY(!auth.busy());
        QTest::qWait(100);
        QVERIFY(server.lastPath.isEmpty());
    }
    void signInThenSignOut() {
        FakeAuthServer server; server.response = session();
        SupabaseAuth auth(server.url(), key);
        auth.signIn(" user@example.test ", "correct horse");
        QVERIFY(auth.busy());
        QTRY_VERIFY(auth.signedIn());
        QVERIFY(!auth.busy());
        QCOMPARE(auth.email(), QString("user@example.test"));
        QCOMPARE(auth.userId(), QString("2f1c6f0e-9a53-4b7e-8d1a-3c5b7e9f0a12"));
        QCOMPARE(server.lastPath, QString("/auth/v1/token?grant_type=password"));
        QCOMPARE(server.lastBody["email"].toString(), QString("user@example.test"));
        QCOMPARE(server.lastBody["password"].toString(), QString("correct horse"));
        QVERIFY(server.lastHeaders.toLower().contains("apikey: sb_publishable_test"));
        server.response = {};
        auth.signOut();
        QVERIFY(!auth.signedIn());
        QVERIFY(auth.email().isEmpty() && auth.userId().isEmpty());
        QTRY_COMPARE(server.lastPath, QString("/auth/v1/logout?scope=local"));
        QVERIFY(server.lastHeaders.contains("Bearer access-1"));
    }
    void rejectedCredentialsAndMalformedSessions() {
        FakeAuthServer server; server.status = 400;
        server.response = {{"error_code", "invalid_credentials"}, {"msg", "Invalid login credentials"}};
        SupabaseAuth auth(server.url(), key);
        QSignalSpy notices(&auth, &SupabaseAuth::notice);
        auth.signIn("user@example.test", "wrong");
        QTRY_COMPARE(notices.size(), 1);
        QVERIFY(!auth.signedIn() && !auth.busy());
        QCOMPARE(notices.last().first().toString(), QString("Sign-in failed: Invalid login credentials"));
        // A 200 without a usable user ID or token is not a session.
        server.status = 200; server.response = {{"access_token", "a"}, {"refresh_token", "r"}, {"expires_in", 3600},
            {"user", QJsonObject{{"id", "not-a-uuid"}}}};
        auth.signIn("user@example.test", "password");
        QTRY_COMPARE(notices.size(), 2);
        QVERIFY(!auth.signedIn());
        auth.signIn("not-an-email", "password");
        QCOMPARE(notices.size(), 3);
        QVERIFY(!auth.busy());
    }
    void signUpAwaitingConfirmationIsNotASession() {
        FakeAuthServer server;
        server.response = {{"id", "2f1c6f0e-9a53-4b7e-8d1a-3c5b7e9f0a12"}, {"email", "user@example.test"}};
        SupabaseAuth auth(server.url(), key);
        QSignalSpy notices(&auth, &SupabaseAuth::notice);
        auth.signUp("user@example.test", "correct horse");
        QTRY_COMPARE(notices.size(), 1);
        QVERIFY(!auth.signedIn());
        QCOMPARE(server.lastPath, QString("/auth/v1/signup"));
        QVERIFY(notices.last().first().toString().contains("confirm"));
    }
    void unreachableServiceAndEndedSession() {
        QUrl dead;
        { QTcpServer probe; probe.listen(QHostAddress::LocalHost); dead = QUrl(QString("http://127.0.0.1:%1").arg(probe.serverPort())); }
        SupabaseAuth offline(dead, key);
        QSignalSpy offlineNotices(&offline, &SupabaseAuth::notice);
        offline.signIn("user@example.test", "password");
        QTRY_COMPARE_WITH_TIMEOUT(offlineNotices.size(), 1, 20000);
        QCOMPARE(offlineNotices.last().first().toString(), QString("The account service could not be reached."));

        // The refresh timer never fires sooner than 30 s, so invoke its slot directly.
        FakeAuthServer server; server.response = session();
        SupabaseAuth auth(server.url(), key);
        auth.signIn("user@example.test", "password");
        QTRY_VERIFY(auth.signedIn());
        server.status = 400; server.response = {{"msg", "Invalid Refresh Token"}};
        QVERIFY(QMetaObject::invokeMethod(&auth, "refreshNow"));
        QTRY_VERIFY(!auth.signedIn());
        QCOMPARE(server.lastPath, QString("/auth/v1/token?grant_type=refresh_token"));
        QCOMPARE(server.lastBody["refresh_token"].toString(), QString("refresh-1"));
    }
    void restoreSignsInAndARefusedTokenIsReported() {
        FakeAuthServer server; server.response = session();
        SupabaseAuth auth(server.url(), key);
        QSignalSpy rejected(&auth, &SupabaseAuth::restoreRejected);
        auth.restore("saved-1");
        QTRY_VERIFY(auth.signedIn());
        QCOMPARE(server.lastPath, QString("/auth/v1/token?grant_type=refresh_token"));
        QCOMPARE(server.lastBody["refresh_token"].toString(), QString("saved-1"));
        QCOMPARE(auth.refreshToken(), QString("refresh-1")); // the rotated token is the one to keep
        QCOMPARE(rejected.size(), 0);

        server.status = 400; server.response = {{"error_code", "refresh_token_not_found"}, {"msg", "Invalid Refresh Token"}};
        SupabaseAuth stale(server.url(), key);
        QSignalSpy staleRejected(&stale, &SupabaseAuth::restoreRejected);
        QSignalSpy notices(&stale, &SupabaseAuth::notice);
        stale.restore("old");
        QTRY_COMPARE(staleRejected.size(), 1);
        QVERIFY(!stale.signedIn());
        QVERIFY(notices.last().first().toString().contains("expired"));
        stale.restore(""); // nothing saved: nothing sent
        QVERIFY(!stale.busy());
    }
    void restoreWhileOfflineKeepsTheSavedSignIn() {
        QUrl dead;
        { QTcpServer probe; probe.listen(QHostAddress::LocalHost); dead = QUrl(QString("http://127.0.0.1:%1").arg(probe.serverPort())); }
        SupabaseAuth auth(dead, key);
        QSignalSpy rejected(&auth, &SupabaseAuth::restoreRejected);
        QSignalSpy notices(&auth, &SupabaseAuth::notice);
        auth.restore("saved-1");
        QTRY_COMPARE_WITH_TIMEOUT(notices.size(), 1, 20000);
        QCOMPARE(rejected.size(), 0);
        QVERIFY(notices.last().first().toString().contains("Could not reach"));
    }
    void resendConfirmationAndUnconfirmedSignIn() {
        FakeAuthServer server; server.response = {};
        SupabaseAuth auth(server.url(), key);
        QSignalSpy notices(&auth, &SupabaseAuth::notice);
        auth.resendConfirmation("not-an-email");
        QCOMPARE(notices.size(), 1);
        QVERIFY(server.lastPath.isEmpty());
        auth.setRedirectUrl(QUrl("http://evil.example/")); // not https: ignored
        auth.resendConfirmation(" user@example.test ");
        QTRY_COMPARE(notices.size(), 2);
        QCOMPARE(server.lastPath, QString("/auth/v1/resend"));
        QCOMPARE(server.lastBody["type"].toString(), QString("signup"));
        QCOMPARE(server.lastBody["email"].toString(), QString("user@example.test"));
        QVERIFY(notices.last().first().toString().contains("on its way"));
        auth.setRedirectUrl(QUrl("https://app.example/mnaka/#secret"));
        auth.resendConfirmation("user@example.test");
        QTRY_COMPARE(notices.size(), 3);
        QCOMPARE(server.lastPath, QString("/auth/v1/resend?redirect_to=https%3A%2F%2Fapp.example%2Fmnaka%2F"));
        server.status = 429;
        auth.resendConfirmation("user@example.test");
        QTRY_COMPARE(notices.size(), 4);
        QVERIFY(notices.last().first().toString().contains("recently"));

        server.status = 400; server.response = {{"error_code", "email_not_confirmed"}, {"msg", "Email not confirmed"}};
        auth.signIn("user@example.test", "password");
        QTRY_COMPARE(notices.size(), 5);
        QVERIFY(notices.last().first().toString().contains("Resend confirmation email"));
        QVERIFY(!auth.signedIn());
    }
};
QTEST_MAIN(AuthTests)
#include "auth_tests.moc"
