#include "WorkspaceModel.h"
#include <QtConcurrent>
#include <QFutureWatcher>
#include <QImageReader>
#include <QBuffer>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QTimer>
#include "engines/skills/SkillValidator.h"
#include "engines/assistant/ConversationEngine.h"
#include "adapters/mock/MockModelProvider.h"
#include "core/orchestrator/TaskOrchestrator.h"
#ifdef Q_OS_WASM
#include <QPointer>
#include <QtGui/private/qwasmlocalfileaccess_p.h>
#endif

using namespace mnaka;

namespace {
const qint64 maxImageBytes = 20 * 1024 * 1024;
// Bounded validation and decode; runs off the UI thread wherever threads exist.
QPair<QString, QString> decodePreview(QByteArray bytes) {
    if (bytes.isEmpty()) return {{}, "Image unavailable or larger than 20 MiB."};
    if (bytes.size() > maxImageBytes) return {{}, "Image exceeds the size limit."};
    QBuffer input(&bytes); input.open(QIODevice::ReadOnly);
    QImageReader reader(&input);
    const auto format = reader.format();
    if (format != "png" && format != "jpeg" && format != "jpg")
        return {{}, "Only PNG and JPEG images are supported."};
    const auto size = reader.size();
    if (!size.isValid() || size.width() > 8192 || size.height() > 8192
        || qint64(size.width()) * size.height() > 16000000)
        return {{}, "Image dimensions exceed the 16 megapixel limit."};
    reader.setAutoTransform(true);
    if (size.width() > 1600 || size.height() > 1600) // bound large images; never upscale small ones
        reader.setScaledSize(size.scaled(1600, 1600, Qt::KeepAspectRatio));
    const auto image = reader.read();
    if (image.isNull()) return {{}, "The image could not be decoded."};
    QByteArray preview; QBuffer output(&preview); output.open(QIODevice::WriteOnly);
    if (!image.save(&output, "PNG")) return {{}, "The preview could not be created."};
    return {"data:image/png;base64," + QString::fromLatin1(preview.toBase64()),
            "Local preview only. Image interpretation is unavailable."};
}
}

WorkspaceModel::WorkspaceModel(QObject *parent) : QObject(parent) {}
WorkspaceModel::~WorkspaceModel() { delete m_runtime; } // the orchestrator must go before its store

void WorkspaceModel::inspectSkill(QString text) {
    m_skillReport.clear();
    if (text.toUtf8().size() > 65536) {
        m_skillReport = "Manifest exceeds the 64 KiB limit."; emit skillReportChanged(); return;
    }
    QJsonParseError parseError;
    const auto document = QJsonDocument::fromJson(text.toUtf8(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        m_skillReport = "Enter a valid JSON object."; emit skillReportChanged(); return;
    }
    const auto manifest = document.object();
    const auto error = mnaka::SkillValidator::inspect(manifest, "0.1.0");
    if (!error.isEmpty()) m_skillReport = "Validation failed: " + error;
    else {
        QStringList permissions;
        for (const auto &value : manifest.value("permissions").toArray()) permissions.append(value.toString());
        m_skillReport = "Manifest checks passed for host 0.1.0.\nRequested tools: " + permissions.join(", ")
            + "\nHash: " + manifest.value("artifact_hash").toString()
            + "\nNo permissions granted. Installation and execution are unavailable.";
    }
    emit skillReportChanged();
}

void WorkspaceModel::clearImage() {
    ++m_imageGeneration; m_imagePreview.clear(); m_imageNotice.clear(); m_imageLoading = false; emit imageChanged();
}
void WorkspaceModel::selectImage(QUrl url) {
    clearImage();
    if (!url.isLocalFile()) { m_imageNotice = "Select a local PNG or JPEG file."; emit imageChanged(); return; }
    loadImage([path = url.toLocalFile()]() -> QByteArray {
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly) || file.size() <= 0 || file.size() > maxImageBytes) return {};
        return file.read(maxImageBytes + 1);
    });
}
void WorkspaceModel::loadImage(std::function<QByteArray()> read) {
    const auto generation = m_imageGeneration;
    m_imageLoading = true; emit imageChanged();
    const auto finish = [this, generation](const QPair<QString, QString> &result) {
        if (generation != m_imageGeneration) return;
        m_imageLoading = false; m_imagePreview = result.first; m_imageNotice = result.second; emit imageChanged();
    };
#if QT_CONFIG(thread)
    auto *watcher = new QFutureWatcher<QPair<QString, QString>>(this);
    connect(watcher, &QFutureWatcherBase::finished, this, [watcher, finish] {
        const auto result = watcher->result(); watcher->deleteLater();
        finish(result);
    });
    watcher->setFuture(QtConcurrent::run([read = std::move(read)] { return decodePreview(read()); }));
#else
    // Single-threaded WebAssembly: a pooled job would never run, so decode on the next event-loop turn.
    QTimer::singleShot(0, this, [this, generation, finish, read = std::move(read)] {
        if (generation == m_imageGeneration) finish(decodePreview(read()));
    });
#endif
}
bool WorkspaceModel::browserImagePicker() const {
#ifdef Q_OS_WASM
    return true;
#else
    return false;
#endif
}
void WorkspaceModel::pickImage() {
#ifdef Q_OS_WASM
    // The browser hands over the bytes of the one file the user chose; nothing is uploaded.
    clearImage();
    const auto generation = m_imageGeneration;
    auto bytes = std::make_shared<QByteArray>();
    QPointer<WorkspaceModel> self(this);
    QWasmLocalFileAccess::openFile("image/png,image/jpeg", [](bool) {},
        [self, generation, bytes](uint64_t size, const std::string &) -> char * {
            if (!self || generation != self->m_imageGeneration) return nullptr;
            if (size == 0 || size > uint64_t(maxImageBytes)) {
                self->m_imageNotice = "Image unavailable or larger than 20 MiB."; emit self->imageChanged();
                return nullptr;
            }
            bytes->resize(qsizetype(size));
            return bytes->data();
        },
        [self, generation, bytes] {
            if (self && generation == self->m_imageGeneration) self->loadImage([bytes] { return *bytes; });
        });
#endif
}
QVariantList WorkspaceModel::capabilities() const {
    if (m_registry) return m_registry->capabilities();
    QVariantList result;
    for (const auto &id : QStringList{"assistant", "software", "control", "skills", "memory", "vision", "proactive"})
        result.append(QVariantMap{{"id", id}, {"enabled", false}, {"status", "Service unavailable"}});
    return result;
}
void WorkspaceModel::setMockSession(bool enabled) {
    if (enabled == mockSession()) return;
    if (busy()) m_orchestrator->cancel(m_currentTask);
    delete m_runtime;
    m_runtime = nullptr; m_registry = nullptr; m_orchestrator = nullptr;
    m_store.reset();
    m_currentTask = QUuid(); m_pendingApproval = QJsonObject(); m_approval.clear();
    m_messages.clear(); m_tasks.clear(); m_events.clear();
    m_streamingText.clear(); m_notice.clear(); m_stateText = "Idle";
    if (enabled) {
        m_runtime = new QObject(this);
        m_registry = new EngineRegistry(m_runtime);
        m_registry->registerService(new ConversationEngine(new MockModelProvider), true, "Mock provider (fixture)");
        for (const auto &id : QStringList{"software", "control", "skills", "memory", "vision", "proactive"})
            m_registry->registerService(new UnavailableEngine(id), false, "Integration pending");
        // Session-only identity for this desktop; nothing here is an authenticated account.
        m_executionHost = QUuid::createUuid();
        const BrokerSession session{QUuid::createUuid(), m_executionHost, m_executionHost, "fixture:conversation", "v1"};
        auto *broker = new ToolBroker(session, ToolBroker::Environment::Fixture, m_runtime);
        m_store = std::make_unique<InMemoryTaskStore>();
        m_orchestrator = new TaskOrchestrator(session, m_registry, broker, m_store.get(), m_runtime);
        connect(m_orchestrator, &TaskOrchestrator::taskChanged, this, &WorkspaceModel::settle);
        connect(m_orchestrator, &TaskOrchestrator::responseDelta, this, [this](QUuid id, QString text) {
            if (id != m_currentTask) return;
            m_streamingText = text; emit changed();
        });
        connect(m_orchestrator, &TaskOrchestrator::responseReady, this, [this](QUuid id, QString text, QJsonObject usage) {
            if (id != m_currentTask) return;
            m_streamingText.clear();
            m_messages.append(QVariantMap{{"role", "assistant"}, {"text", text}, {"mock", usage["mock"].toBool()}});
            emit messagesChanged(); emit changed();
        });
        connect(m_orchestrator, &TaskOrchestrator::approvalRequested, this, [this](QUuid id, QJsonObject presentation) {
            if (id != m_currentTask) return;
            m_pendingApproval = presentation;
            presentation.remove("nonce"); // the one-time nonce never reaches QML
            m_approval = presentation.toVariantMap();
            emit changed();
        });
    }
    emit messagesChanged(); emit tasksChanged(); emit capabilitiesChanged(); emit changed();
}
bool WorkspaceModel::busy() const { return m_orchestrator && m_orchestrator->isActive(m_currentTask); }
QString WorkspaceModel::state() const { return busy() ? m_stateText : "Idle"; }
void WorkspaceModel::settle(const QUuid &taskId) {
    if (taskId == m_currentTask && !busy()) {
        m_streamingText.clear(); m_pendingApproval = QJsonObject(); m_approval.clear();
        const auto error = m_orchestrator->taskResult(taskId)["error"].toObject();
        m_notice = error.isEmpty() ? QString() : error["code"].toString() == "CANCELLED" ? error["message"].toString()
            : "Task failed (" + error["code"].toString() + "): " + error["message"].toString();
    }
    refresh();
}
void WorkspaceModel::refresh() {
    m_tasks.clear(); m_events.clear();
    const auto records = m_orchestrator ? m_orchestrator->tasks() : QList<TaskRecord>{};
    for (const auto &record : records) {
        if (m_tasks.size() >= 50) break;
        const auto events = m_orchestrator->eventsAfter(record.id, 0);
        QString evidence;
        for (const auto &event : events) {
            const auto summary = event["payload"].toObject()["summary"].toString();
            if (!summary.isEmpty() && event["type"].toString() != "step.started") evidence = summary;
        }
        if (record.id == m_currentTask) {
            if (record.state != "awaiting_approval") { m_pendingApproval = QJsonObject(); m_approval.clear(); }
            m_stateText = QHash<QString, QString>{{"planning", "Planning"}, {"running", "Running"},
                {"awaiting_approval", "Awaiting approval"}, {"cancelling", "Cancelling"}}.value(record.state, "Idle");
            for (const auto &event : events.mid(qMax(0, events.size() - 40)))
                m_events.append(QVariantMap{{"sequence", event["sequence"].toInteger()}, {"type", event["type"].toString()},
                    {"payload", event["payload"].toObject().toVariantMap()}});
        }
        QVariantMap task{{"id", record.id.toString(QUuid::WithoutBraces)}, {"mode", record.mode}, {"state", record.state},
            {"title", record.request["input"].toObject()["text"].toString().left(160)},
            {"evidence", "Mock fixture: " + evidence}, {"createdAt", record.createdAt.toString(Qt::ISODate)},
            {"active", m_orchestrator->isActive(record.id)}};
        const auto steps = record.plan["steps"].toArray();
        if (!steps.isEmpty())
            task["plannedTool"] = steps[0].toObject()["intent"].toObject()["tool"].toString()
                + (steps.size() > 1 ? QString(" (+%1 more)").arg(steps.size() - 1) : QString());
        m_tasks.append(task);
    }
    emit tasksChanged(); emit changed();
}
bool WorkspaceModel::submit(QString text, QString mode) {
    text = text.trimmed();
    if (text.isEmpty() || text.size() > 4000
        || !QStringList{"Assistant", "Developer", "Autonomous"}.contains(mode)) {
        m_notice = "Enter a message of 1 to 4000 characters and select a valid mode.";
    } else if (!m_orchestrator) {
        m_notice = "No model provider or task service is connected. Your draft has not been submitted.";
    } else if (busy()) {
        m_notice = "A task is already active. Cancel it or wait for it to finish; your draft has not been submitted.";
    } else {
        QUuid id;
        const auto error = m_orchestrator->submit({{"schema_version", "1.0"},
            {"input", QJsonObject{{"type", "text"}, {"text", text}}}, {"mode", mode.toLower()},
            {"execution_host_id", m_executionHost.toString(QUuid::WithoutBraces)},
            {"idempotency_key", QUuid::createUuid().toString(QUuid::WithoutBraces)}}, id);
        if (error.isEmpty()) {
            m_notice.clear();
            m_currentTask = id;
            m_messages.append(QVariantMap{{"role", "user"}, {"text", text}, {"mock", false}});
            while (m_messages.size() > 200) m_messages.removeFirst();
            emit messagesChanged();
            settle(id);
            return true;
        }
        m_notice = "Your draft has not been submitted (" + error + ").";
    }
    emit changed();
    return false;
}
void WorkspaceModel::cancel() {
    if (busy()) m_orchestrator->cancel(m_currentTask);
}
void WorkspaceModel::decideApproval(bool allow) {
    if (!m_orchestrator || m_pendingApproval.isEmpty()) return;
    const auto approval = m_pendingApproval;
    if (m_orchestrator->decideApproval(QUuid(approval["id"].toString()), approval["intentHash"].toString(),
            approval["nonce"].toString(), allow)) return;
    m_pendingApproval = QJsonObject(); m_approval.clear();
    m_notice = "This approval is no longer valid. Nothing was executed.";
    emit changed();
}
QVariantList WorkspaceModel::eventsAfter(QString taskId, int sequence) const {
    QVariantList result;
    if (!m_orchestrator) return result;
    for (const auto &event : m_orchestrator->eventsAfter(QUuid(taskId), sequence)) result.append(event.toVariantMap());
    return result;
}
void WorkspaceModel::clearHistory() {
    if (mockSession()) { setMockSession(false); setMockSession(true); return; }
    m_notice.clear();
    emit changed();
}
