#pragma once
#include <QObject>
#include <QVariantList>
#include <QUrl>
#include <QUuid>
#include <QJsonObject>
#include <QHash>
#include <QtQml/qqmlregistration.h>
#include <functional>
#include <memory>
#include "apps/client/DeveloperSession.h"
#include "apps/client/VoiceInput.h"

namespace mnaka {
class TaskOrchestrator;
class EngineRegistry;
class TaskStore;
class SupabaseAuth;
class SupabaseData;
class SessionToken;
class ModelProvider;
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
    // True where the sign-in is kept on this device until the user signs out.
    Q_PROPERTY(bool sessionPersists READ sessionPersists CONSTANT)
    // True while a signed-in account session runs against the project's own model.
    Q_PROPERTY(bool connected READ connected NOTIFY changed)
    // The account gateway's model: "unknown", "ready", "missing" (none set) or "failing" (one is
    // set but its provider rejects it; modelProblem says why).
    Q_PROPERTY(QString modelStatus READ modelStatus NOTIFY changed)
    // The model the gateway reports, e.g. "claude-sonnet-5-5"; empty until known.
    Q_PROPERTY(QString modelName READ modelName NOTIFY changed)
    Q_PROPERTY(QString modelProblem READ modelProblem NOTIFY changed)
    // The signed-in user's saved rows. Saving a skill or rule records it; nothing executes it.
    Q_PROPERTY(QVariantList memories READ memories NOTIFY dataChanged)
    Q_PROPERTY(QVariantList skills READ skills NOTIFY dataChanged)
    Q_PROPERTY(QVariantList rules READ rules NOTIFY dataChanged)
    Q_PROPERTY(QString dataNotice READ dataNotice NOTIFY dataChanged)
    Q_PROPERTY(bool skillSavable READ skillSavable NOTIFY skillReportChanged)
    Q_PROPERTY(QString imageAnswer READ imageAnswer NOTIFY imageChanged)
    Q_PROPERTY(bool imageInterpreting READ imageInterpreting NOTIFY imageChanged)
    Q_PROPERTY(DeveloperSession *developer READ developer CONSTANT)
    Q_PROPERTY(VoiceInput *voice READ voice CONSTANT)
    // Fixture session for automated tests and --mock-session; the application has no switch for it.: mock provider, fixture broker tools, nothing persisted.
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
    bool sessionPersists() const;
    bool connected() const { return m_live; }
    QString modelStatus() const { return m_modelStatus; }
    QString modelName() const { return m_modelName; }
    QString modelProblem() const { return m_modelProblem; }
    DeveloperSession *developer() const { return m_developer; }
    VoiceInput *voice() const { return m_voice; }
    QVariantList memories() const { return m_memories; }
    QVariantList skills() const { return m_skills; }
    QVariantList rules() const { return m_rules; }
    QString dataNotice() const { return m_dataNotice; }
    bool skillSavable() const { return m_live && !m_validManifest.isEmpty(); }
    QString imageAnswer() const { return m_imageAnswer; }
    bool imageInterpreting() const { return m_imageInterpreting; }
    bool mockSession() const { return m_mock; }
    void setMockSession(bool enabled);
    Q_INVOKABLE void inspectSkill(QString manifest);
    Q_INVOKABLE void clearSkillReport() { m_skillReport.clear(); m_validManifest = QJsonObject(); emit skillReportChanged(); }
    Q_INVOKABLE void saveSkill();
    Q_INVOKABLE void deleteSkill(QString id);
    Q_INVOKABLE void addMemory(QString text);
    Q_INVOKABLE void deleteMemory(QString id);
    Q_INVOKABLE void addRule(QString title, QString schedule);
    Q_INVOKABLE void setRuleEnabled(QString id, bool enabled);
    Q_INVOKABLE void deleteRule(QString id);
    // Sends the selected image and question to the model. The image leaves this device.
    Q_INVOKABLE void interpretImage(QString question);
    Q_INVOKABLE void selectImage(QUrl file);
    Q_INVOKABLE void pickImage();
    Q_INVOKABLE void clearImage();
    Q_INVOKABLE void signIn(QString email, QString password);
    Q_INVOKABLE void signUp(QString email, QString password);
    Q_INVOKABLE void signOut();
    Q_INVOKABLE void resendConfirmation(QString email);
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
    void dataChanged();
private:
    void syncAccount();
    void rememberSession(const QString &refreshToken);
    void loadAccountData();
    void startRuntime(mnaka::ModelProvider *provider, const QString &assistantStatus, const QHash<QString, QString> &engineStatus);
    void stopRuntime();
    void removeRow(const QString &table, const QString &id, QVariantList &rows);
    void failData(const QString &what);
    void loadImage(std::function<QByteArray()> read);
    void refresh();
    void settle(const QUuid &taskId);
    QString m_notice;
    QString m_imagePreview, m_imageNotice;
    QString m_skillReport;
    mnaka::SupabaseAuth *m_auth = nullptr;
    QString m_accountNotice;
    mnaka::SupabaseData *m_data = nullptr;
    DeveloperSession *m_developer;
    VoiceInput *m_voice;
    std::shared_ptr<mnaka::SessionToken> m_token;
    bool m_mock = false, m_live = false, m_imageInterpreting = false;
    quint64 m_sessionGeneration = 0;
    QVariantList m_memories, m_skills, m_rules;
    QString m_dataNotice, m_imageAnswer, m_modelStatus = "unknown", m_modelName, m_modelProblem, m_rememberedToken;
    QJsonObject m_validManifest;
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
