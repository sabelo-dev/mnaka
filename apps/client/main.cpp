#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QTimer>
#include <QQuickWindow>
#include <QQmlError>
#include <QTextStream>
#include "apps/client/WorkspaceModel.h"

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    QCoreApplication::setOrganizationName("MNAKA");
    QCoreApplication::setApplicationName("MNAKA");

    int qmlWarnings = 0;
    QQmlApplicationEngine engine;
    QObject::connect(&engine, &QQmlApplicationEngine::warnings, &app,
        [&qmlWarnings](const QList<QQmlError> &warnings) { qmlWarnings += warnings.size(); });
    QObject::connect(
        &engine,
        &QQmlApplicationEngine::objectCreationFailed,
        &app,
        []() { QCoreApplication::exit(-1); },
        Qt::QueuedConnection);
    engine.loadFromModule("Mnaka.App", "Main");

    // Starts the labelled fixture session; without it the app stays disconnected.
    const bool mockSession = app.arguments().contains("--mock-session");
    if (mockSession && !engine.rootObjects().isEmpty())
        if (auto *workspace = engine.rootObjects().first()->findChild<WorkspaceModel *>()) workspace->setMockSession(true);

    // Headless startup/navigation probe for local CI; only fixture tools can run.
    if (app.arguments().contains("--smoke-test") && !engine.rootObjects().isEmpty()) {
        auto *root = engine.rootObjects().first();
        for (int i = 0; i < 9; ++i)
            QTimer::singleShot(100 * i, root, [root, i] { root->setProperty("page", i); });
        QTimer::singleShot(950, root, [root] { root->setProperty("width", 400); root->setProperty("page", 0); });
        auto *workspace = root->findChild<WorkspaceModel *>();
        if (mockSession && workspace) {
            // One plain task, then one that renders the approval card before it is approved.
            QTimer::singleShot(150, workspace, [workspace] { workspace->submit("Mnaka, hello", "Assistant"); });
            QTimer::singleShot(1000, workspace, [workspace] { workspace->submit("Mnaka, run the review demo", "Developer"); });
            QTimer::singleShot(1900, root, [root] { root->setProperty("page", 1); });
            QTimer::singleShot(2200, workspace, [workspace] { workspace->decideApproval(true); });
        }
        QTimer::singleShot(mockSession ? 2700 : 2300, &app, [&qmlWarnings, workspace, mockSession] {
            bool ok = qmlWarnings == 0 && workspace && !workspace->busy();
            if (ok && mockSession)
                ok = workspace->messages().size() == 4 && workspace->tasks().size() == 2
                    && workspace->tasks().first().toMap()["state"].toString() == "succeeded"
                    && workspace->tasks().last().toMap()["state"].toString() == "succeeded";
            else if (ok) ok = workspace->tasks().isEmpty() && workspace->messages().isEmpty();
            QCoreApplication::exit(ok ? 0 : 2);
        });
    }

    return QGuiApplication::exec();
}


