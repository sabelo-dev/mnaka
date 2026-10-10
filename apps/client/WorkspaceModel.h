#pragma once
#include <QObject>
#include <QVariantList>
#include <QUrl>
#include <QUuid>
#include <QJsonObject>
#include <QtQml/qqmlregistration.h>
#include <functional>
#include <memory>

namespace mnaka {
class TaskOrchestrator;
class EngineRegistry;
class TaskStore;
class SupabaseAuth;
}

class WorkspaceModel : public QObject {
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(QVariantList messages READ messages NOTIFY messagesChanged)
    Q_PROPERTY(QVariantList tasks READ tasks NOTIFY tasksChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
    Q_PROPERTY(QString state READ state NOTIFY changed)
    Q_PROPERTY(QString notice READ notice NOTIFY changed)
    Q_PROPERTY(QVariantMap approval READ approval NOTIFY changed)
    Q_PROPERTY(QVariantList events READ events NOTIFY tasksChanged)
    Q_PROPERTY(QVariantList capabilities READ capabilities NOTIFY capabilitiesChanged)
    Q_PROPERTY(QString streamingText READ streamingText NOTIFY changed)
    Q_PROPERTY(QString imagePreview READ imagePreview NOTIFY imageChanged)
    Q_PROPERTY(bool imageLoading READ imageLoading NOTIFY imageChanged)
    Q_PROPERTY(QString imageNotice READ imageNotice NOTIFY imageChanged)
    // True in the browser, where the page's own file chooser replaces the Qt dialog.
    Q_PROPERTY(bool browserImagePicker READ browserImagePicker CONSTANT)
    Q_PROPERTY(QString skillReport READ skillReport NOTIFY skillReportChanged)
    // Account identity only: signing in enables no engine, tool or storage. Tokens stay in C++.
    Q_PROPERTY(bool accountConfigured READ accountConfigured CONSTANT)
    Q_PROPERTY(QString accountEmail READ accountEmail NOTIFY accountChanged)
    Q_PROPERTY(bool accountBusy READ accountBusy NOTIFY accountChanged)
    Q_PROPERTY(QString accountNotice READ accountNotice NOTIFY accountChanged)
    // Opt-in fixture session: mock provider, fixture broker tools, nothing persisted.
    Q_PROPERTY(bool mockSession READ mockSession WRITE setMockSession NOTIFY changed)
public:
    explicit WorkspaceModel(QObject *parent = nullptr);
    ~WorkspaceModel() override;
    QVariantList messages() const { return m_messages; }
    QVariantList tasks() const { return m_tasks; }
    bool busy() const;
    QString state() const;
    QString notice() const { return m_notice; }
    QVariantMap approval() const { return m_approval; }
    QVariantList events() const { return m_events; }
    QVariantList capabilities() const;
    QString streamingText() const { return m_streamingText; }
    QString imagePreview() const { return m_imagePreview; }
    bool imageLoading() const { return m_imageLoading; }
    QString imageNotice() const { return m_imageNotice; }
    bool browserImagePicker() const;
    QString skillReport() const { return m_skillReport; }
    bool accountConfigured() const { return m_auth != nullptr; }
    QString accountEmail() const;
    bool accountBusy() const;
    QString accountNotice() const { return m_accountNotice; }
    bool mockSession() const { return m_orchestrator != nullptr; }
    void setMockSession(bool enabled);
    Q_INVOKABLE void inspectSkill(QString manifest);
    Q_INVOKABLE void clearSkillReport() { m_skillReport.clear(); emit skillReportChanged(); }
    Q_INVOKABLE void selectImage(QUrl file);
    Q_INVOKABLE void pickImage();
    Q_INVOKABLE void clearImage();
    Q_INVOKABLE void signIn(QString email, QString password);
    Q_INVOKABLE void signUp(QString email, QString password);
    Q_INVOKABLE void signOut();
    Q_INVOKABLE bool submit(QString text, QString mode);
    Q_INVOKABLE void cancel();
    Q_INVOKABLE void clearHistory();
    Q_INVOKABLE void decideApproval(bool allow);
    Q_INVOKABLE QVariantList eventsAfter(QString taskId, int sequence) const;
signals:
    // Lists notify separately so a streaming delta does not rebuild every delegate.
    void changed();
    void messagesChanged();
    void tasksChanged();
    void capabilitiesChanged();
    void imageChanged();
    void skillReportChanged();
    void accountChanged();
private:
    void loadImage(std::function<QByteArray()> read);
    void refresh();
    void settle(const QUuid &taskId);
    QString m_notice;
    QString m_imagePreview, m_imageNotice;
    QString m_skillReport;
    mnaka::SupabaseAuth *m_auth = nullptr;
    QString m_accountNotice;
    bool m_imageLoading = false;
    quint64 m_imageGeneration = 0;
    // Engine, broker and store handles stay on this side of the QML boundary.
    QObject *m_runtime = nullptr;
    mnaka::EngineRegistry *m_registry = nullptr;
    mnaka::TaskOrchestrator *m_orchestrator = nullptr;
    std::unique_ptr<mnaka::TaskStore> m_store;
    QUuid m_executionHost, m_currentTask;
    QJsonObject m_pendingApproval;
    QVariantList m_messages, m_tasks, m_events;
    QVariantMap m_approval;
    QString m_streamingText, m_stateText = "Idle";
};
