#include "WorkspaceModel.h"
#include <QtConcurrent>
#include <QFutureWatcher>
#include <QImage>
#include <QImageReader>
#include <QBuffer>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QSettings>
#include <QTimer>
#include "engines/skills/SkillValidator.h"
#include "engines/assistant/ConversationEngine.h"
#include "adapters/mock/MockModelProvider.h"
#include "adapters/supabase/SupabaseAuth.h"
#include "apps/client/PickedFile.h"
#include "adapters/supabase/SupabaseData.h"
#include "adapters/supabase/GatewayModelProvider.h"
#include "core/orchestrator/TaskOrchestrator.h"
#ifdef Q_OS_WASM
#include <QPointer>
#include <QtGui/private/qwasmlocalfileaccess_p.h>
#endif

using namespace mnaka;

namespace {
const qint64 maxImageBytes = 20 * 1024 * 1024;
// Where the sign-in is remembered between runs: the browser's storage for this site, the app's
// private storage on Android, or a file named by MNAKA_SESSION_FILE. Nowhere on a desktop by
// default, where there is no vault adapter for it yet.
std::unique_ptr<QSettings> sessionStore() {
    const auto file = qEnvironmentVariable("MNAKA_SESSION_FILE");
    if (!file.isEmpty()) return std::make_unique<QSettings>(file, QSettings::IniFormat);
#if defined(Q_OS_WASM) || defined(Q_OS_ANDROID)
    return std::make_unique<QSettings>();
#else
    return nullptr;
#endif
}
// What each engine entry means while an account session is running.
QHash<QString, QString> liveEngineStatus() {
    return {{"software", "Needs desktop app"}, {"control", "Needs desktop app"}, {"memory", "Saved to account"},
        {"skills", "Saved, not executed"}, {"vision", "Via the model"}, {"proactive", "Saved, not scheduled"}};
}
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

WorkspaceModel::WorkspaceModel(QObject *parent) : QObject(parent), m_developer(new DeveloperSession(this)), m_voice(new VoiceInput(this)) {
    // Deployment configuration, not a secret: the browser page or the desktop environment supplies it.
    const auto url = qEnvironmentVariable("MNAKA_SUPABASE_URL", QStringLiteral(MNAKA_DEFAULT_SUPABASE_URL)),
        key = qEnvironmentVariable("MNAKA_SUPABASE_PUBLISHABLE_KEY", QStringLiteral(MNAKA_DEFAULT_SUPABASE_KEY));
    if (url.isEmpty() && key.isEmpty()) return;
    m_auth = new SupabaseAuth(QUrl(url, QUrl::StrictMode), key, this);
    m_accountNotice = m_auth->configError();
    m_data = new SupabaseData(m_auth, this);
    m_token = std::make_shared<SessionToken>();
    connect(m_auth, &SupabaseAuth::changed, this, [this] {
        if (m_auth->signedIn()) rememberSession(m_auth->refreshToken()); // also follows token rotation
        else if (m_live) rememberSession({}); // the session ended: nothing left worth restoring
        syncAccount(); emit accountChanged();
    });
    connect(m_auth, &SupabaseAuth::restoreRejected, this, [this] { rememberSession({}); });
    m_auth->setRedirectUrl(QUrl(qEnvironmentVariable("MNAKA_SITE_URL"), QUrl::StrictMode));
    // An email link that failed returns here with a reason; say what to do next.
    const auto linkError = qEnvironmentVariable("MNAKA_AUTH_ERROR");
    if (!linkError.isEmpty() && m_accountNotice.isEmpty())
        m_accountNotice = linkError == "otp_expired"
            ? "That email link has expired or was already used. Enter your email and choose Resend confirmation email."
            : "That email link could not be used" + (qEnvironmentVariable("MNAKA_AUTH_ERROR_DESCRIPTION").isEmpty() ? QString(".")
                : ": " + qEnvironmentVariable("MNAKA_AUTH_ERROR_DESCRIPTION").left(200) + ".");
    // A session from an email link wins over a remembered one.
    if (const auto store = sessionStore()) m_rememberedToken = store->value("account/refreshToken").toString();
    const auto linkToken = qEnvironmentVariable("MNAKA_AUTH_REFRESH_TOKEN");
    m_auth->restore(linkToken.isEmpty() ? m_rememberedToken : linkToken);
    connect(m_auth, &SupabaseAuth::notice, this, [this](QString message) { m_accountNotice = message; emit accountChanged(); });
}
WorkspaceModel::~WorkspaceModel() { delete m_runtime; } // the orchestrator must go before its store

void WorkspaceModel::inspectSkill(QString text) {
    m_skillReport.clear(); m_validManifest = QJsonObject();
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
        m_validManifest = manifest;
        m_skillReport = "Manifest checks passed for host 0.1.0.\nRequested tools: " + permissions.join(", ")
            + "\nHash: " + manifest.value("artifact_hash").toString()
            + "\nNo permissions granted. Installation and execution are unavailable.";
    }
    emit skillReportChanged();
}

QString WorkspaceModel::accountEmail() const { return m_auth ? m_auth->email() : QString(); }
bool WorkspaceModel::accountBusy() const { return m_auth && m_auth->busy(); }
void WorkspaceModel::signIn(QString email, QString password) { if (m_auth) m_auth->signIn(email, password); }
void WorkspaceModel::signUp(QString email, QString password) { if (m_auth) m_auth->signUp(email, password); }
void WorkspaceModel::signOut() { if (!m_auth) return; rememberSession({}); m_auth->signOut(); }
void WorkspaceModel::resendConfirmation(QString email) { if (m_auth) m_auth->resendConfirmation(email); }
bool WorkspaceModel::sessionPersists() const { return sessionStore() != nullptr; }
void WorkspaceModel::rememberSession(const QString &refreshToken) {
    if (refreshToken == m_rememberedToken) return;
    m_rememberedToken = refreshToken;
    const auto store = sessionStore();
    if (!store) return;
    if (refreshToken.isEmpty()) store->remove("account/refreshToken"); else store->setValue("account/refreshToken", refreshToken);
    store->sync();
}

void WorkspaceModel::clearImage() {
    ++m_imageGeneration; m_imagePreview.clear(); m_imageNotice.clear(); m_imageLoading = false;
    m_imageAnswer.clear(); m_imageInterpreting = false; emit imageChanged();
}
void WorkspaceModel::selectImage(QUrl url) {
    clearImage();
    const auto path = pickedFilePath(url);
    if (path.isEmpty()) { m_imageNotice = "Select a local PNG or JPEG file."; emit imageChanged(); return; }
    loadImage([path]() -> QByteArray {
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
    if (m_registry) {
        auto result = m_registry->capabilities();
        if (!m_live) return result;
        // Account features work without being engines: they run no tools, so the registry keeps
        // them disabled. "available" tells the screens apart from engines that cannot work here.
        for (auto &value : result) {
            auto entry = value.toMap();
            entry["available"] = QStringList{"memory", "skills", "vision", "proactive"}.contains(entry["id"].toString());
            value = entry;
        }
        return result;
    }
    QVariantList result;
    for (const auto &id : QStringList{"assistant", "software", "control", "skills", "memory", "vision", "proactive"})
        result.append(QVariantMap{{"id", id}, {"enabled", false}, {"status", "Service unavailable"}});
    return result;
}
void WorkspaceModel::setMockSession(bool enabled) {
    if (enabled == m_mock || m_live) return;
    stopRuntime();
    m_mock = enabled;
    if (enabled) startRuntime(new MockModelProvider, "Mock provider (fixture)", {});
    emit messagesChanged(); emit tasksChanged(); emit capabilitiesChanged(); emit changed();
}
void WorkspaceModel::stopRuntime() {
    if (busy()) m_orchestrator->cancel(m_currentTask);
    delete m_runtime;
    m_runtime = nullptr; m_registry = nullptr; m_orchestrator = nullptr;
    m_store.reset();
    m_currentTask = QUuid(); m_pendingApproval = QJsonObject(); m_approval.clear();
    m_messages.clear(); m_tasks.clear(); m_events.clear();
    m_streamingText.clear(); m_notice.clear(); m_stateText = "Idle";
}
void WorkspaceModel::startRuntime(ModelProvider *provider, const QString &assistantStatus, const QHash<QString, QString> &engineStatus) {
    {
        m_runtime = new QObject(this);
        m_registry = new EngineRegistry(m_runtime);
        m_registry->registerService(new ConversationEngine(provider), true, assistantStatus);
        for (const auto &id : QStringList{"software", "control", "skills", "memory", "vision", "proactive"})
            m_registry->registerService(new UnavailableEngine(id), false, engineStatus.value(id, "Integration pending"));
        // The broker identity is local to this run. A conversation step has no effects, so the
        // account identity is enforced where the data lives: on the server, by the user's token.
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
}
void WorkspaceModel::syncAccount() {
    const bool signedIn = m_auth && m_auth->signedIn();
    m_token->set(signedIn ? m_auth->accessToken() : QString()); // also picks up refreshed tokens
    if (signedIn == m_live) return;
    stopRuntime();
    ++m_sessionGeneration;
    m_mock = false; m_live = signedIn;
    m_memories.clear(); m_skills.clear(); m_rules.clear(); m_dataNotice.clear();
    m_imageAnswer.clear(); m_imageInterpreting = false;
    m_modelStatus = "unknown"; m_modelName.clear();
    m_developer->setGateway(signedIn ? m_data : nullptr);
    if (signedIn) {
        startRuntime(new GatewayModelProvider(m_auth->projectUrl(), m_auth->publishableKey(), m_token), "Model via account gateway", liveEngineStatus());
        loadAccountData();
    }
    emit messagesChanged(); emit tasksChanged(); emit capabilitiesChanged(); emit dataChanged(); emit imageChanged();
    emit skillReportChanged(); emit changed();
}
void WorkspaceModel::failData(const QString &what) { m_dataNotice = what + " The account service did not accept the request."; emit dataChanged(); }
void WorkspaceModel::loadAccountData() {
    const auto generation = m_sessionGeneration;
    m_data->invoke("assistant", {{"probe", true}}, [this, generation](int status, QJsonDocument body) {
        if (generation != m_sessionGeneration || status != 200 || !body.object()["configured"].isBool()) return;
        m_modelStatus = body.object()["configured"].toBool() ? "ready" : "missing";
        m_modelName = body.object()["model"].toString().left(100);
        emit changed();
    });
    m_data->list("mnaka_messages", "select=role,content&order=created_at.desc&limit=50", [this, generation](int status, QJsonDocument body) {
        if (generation != m_sessionGeneration) return;
        if (status != 200) { m_notice = "Saved conversation could not be loaded."; emit changed(); return; }
        QVariantList loaded;
        for (const auto &value : body.array()) {
            const auto row = value.toObject();
            loaded.prepend(QVariantMap{{"role", row["role"].toString() == "user" ? "user" : "assistant"}, {"text", row["content"].toString()}, {"mock", false}});
        }
        m_messages = loaded + m_messages; // anything typed while loading stays last
        emit messagesChanged();
    });
    const auto load = [this, generation](const QString &table, const QString &query, QVariantList WorkspaceModel::*rows) {
        m_data->list(table, query, [this, generation, rows](int status, QJsonDocument body) {
            if (generation != m_sessionGeneration) return;
            if (status != 200) { failData("Saved data could not be loaded."); return; }
            (this->*rows).clear();
            for (const auto &value : body.array()) (this->*rows).append(value.toObject().toVariantMap());
            emit dataChanged();
        });
    };
    load("mnaka_memories", "select=id,content&order=created_at.asc&limit=200", &WorkspaceModel::m_memories);
    load("mnaka_skills", "select=id,skill_id,version,artifact_hash&order=created_at.asc&limit=100", &WorkspaceModel::m_skills);
    load("mnaka_rules", "select=id,title,schedule,enabled&order=created_at.asc&limit=100", &WorkspaceModel::m_rules);
}
void WorkspaceModel::addMemory(QString text) {
    text = text.trimmed();
    if (!m_live) return;
    if (text.isEmpty() || text.size() > 2000) { m_dataNotice = "A memory is 1 to 2000 characters."; emit dataChanged(); return; }
    const auto generation = m_sessionGeneration;
    m_data->insert("mnaka_memories", {{"content", text}}, [this, generation](int status, QJsonDocument body) {
        if (generation != m_sessionGeneration) return;
        if (status != 201 || body.array().isEmpty()) { failData("The memory was not saved."); return; }
        const auto row = body.array().first().toObject();
        m_memories.append(QVariantMap{{"id", row["id"].toString()}, {"content", row["content"].toString()}});
        m_dataNotice.clear(); emit dataChanged();
    });
}
void WorkspaceModel::removeRow(const QString &table, const QString &id, QVariantList &rows) {
    if (!m_live || QUuid(id).isNull()) return;
    const auto generation = m_sessionGeneration;
    m_data->remove(table, id, [this, generation, id, &rows](int status, QJsonDocument) {
        if (generation != m_sessionGeneration) return;
        if (status < 200 || status >= 300) { failData("Nothing was deleted."); return; }
        for (qsizetype i = 0; i < rows.size(); ++i) if (rows[i].toMap()["id"].toString() == id) { rows.removeAt(i); break; }
        m_dataNotice.clear(); emit dataChanged();
    });
}
void WorkspaceModel::deleteMemory(QString id) { removeRow("mnaka_memories", id, m_memories); }
void WorkspaceModel::deleteSkill(QString id) { removeRow("mnaka_skills", id, m_skills); }
void WorkspaceModel::deleteRule(QString id) { removeRow("mnaka_rules", id, m_rules); }
void WorkspaceModel::saveSkill() {
    if (!skillSavable()) return;
    const auto manifest = m_validManifest;
    const auto generation = m_sessionGeneration;
    m_data->insert("mnaka_skills", {{"skill_id", manifest["id"].toString()}, {"version", manifest["version"].toString()},
        {"artifact_hash", manifest["artifact_hash"].toString()}, {"manifest", manifest}}, [this, generation](int status, QJsonDocument body) {
        if (generation != m_sessionGeneration) return;
        if (status == 409) { m_dataNotice = "This manifest is already saved."; emit dataChanged(); return; }
        if (status != 201 || body.array().isEmpty()) { failData("The skill was not saved."); return; }
        const auto row = body.array().first().toObject();
        m_skills.append(QVariantMap{{"id", row["id"].toString()}, {"skill_id", row["skill_id"].toString()},
            {"version", row["version"].toString()}, {"artifact_hash", row["artifact_hash"].toString()}});
        m_dataNotice = "Saved. A saved manifest grants no permission and is not executed."; emit dataChanged();
    });
}
void WorkspaceModel::addRule(QString title, QString schedule) {
    title = title.trimmed(); schedule = schedule.trimmed();
    if (!m_live) return;
    if (title.isEmpty() || title.size() > 200 || schedule.isEmpty() || schedule.size() > 200) {
        m_dataNotice = "A rule needs a title and a schedule, each up to 200 characters."; emit dataChanged(); return;
    }
    const auto generation = m_sessionGeneration;
    m_data->insert("mnaka_rules", {{"title", title}, {"schedule", schedule}}, [this, generation](int status, QJsonDocument body) {
        if (generation != m_sessionGeneration) return;
        if (status != 201 || body.array().isEmpty()) { failData("The rule was not saved."); return; }
        const auto row = body.array().first().toObject();
        m_rules.append(QVariantMap{{"id", row["id"].toString()}, {"title", row["title"].toString()},
            {"schedule", row["schedule"].toString()}, {"enabled", row["enabled"].toBool()}});
        m_dataNotice.clear(); emit dataChanged();
    });
}
void WorkspaceModel::setRuleEnabled(QString id, bool enabled) {
    if (!m_live || QUuid(id).isNull()) return;
    const auto generation = m_sessionGeneration;
    m_data->update("mnaka_rules", id, {{"enabled", enabled}}, [this, generation, id, enabled](int status, QJsonDocument body) {
        if (generation != m_sessionGeneration) return;
        const bool saved = status == 200 && !body.array().isEmpty();
        for (auto &value : m_rules) {
            auto rule = value.toMap();
            if (rule["id"].toString() != id) continue;
            if (saved) { rule["enabled"] = enabled; value = rule; }
        }
        if (saved) m_dataNotice.clear(); else m_dataNotice = "The rule was not changed. The account service did not accept the request.";
        emit dataChanged(); // also resets the switch when the change was refused
    });
}
void WorkspaceModel::interpretImage(QString question) {
    question = question.trimmed();
    if (!m_live || m_imageInterpreting || m_imagePreview.isEmpty()) return;
    if (question.isEmpty()) question = "Describe this image.";
    if (question.size() > 4000) { m_imageAnswer = "The question is too long."; emit imageChanged(); return; }
    // A bounded JPEG copy is what leaves the device; the preview itself stays local.
    auto image = QImage::fromData(QByteArray::fromBase64(m_imagePreview.section(',', 1).toLatin1()));
    if (image.isNull()) return;
    if (image.width() > 1024 || image.height() > 1024) image = image.scaled(1024, 1024, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    QByteArray jpeg; QBuffer output(&jpeg); output.open(QIODevice::WriteOnly);
    if (!image.convertToFormat(QImage::Format_RGB32).save(&output, "JPEG", 82)) return;
    m_imageInterpreting = true; m_imageAnswer.clear(); emit imageChanged();
    const auto generation = m_sessionGeneration; const auto imageGeneration = m_imageGeneration;
    m_data->invoke("assistant", {{"text", question}, {"image", "data:image/jpeg;base64," + QString::fromLatin1(jpeg.toBase64())}},
        [this, generation, imageGeneration, question](int status, QJsonDocument body) {
        if (generation != m_sessionGeneration) return;
        const auto answer = body.object()["text"].toString();
        const bool ok = status == 200 && !answer.isEmpty();
        if (ok) { // the gateway stored this exchange, so the conversation shows it too
            m_messages.append(QVariantMap{{"role", "user"}, {"text", question + "\n[image attached]"}, {"mock", false}});
            m_messages.append(QVariantMap{{"role", "assistant"}, {"text", answer.left(65536)}, {"mock", false}});
            emit messagesChanged();
        }
        if (imageGeneration != m_imageGeneration) return;
        m_imageInterpreting = false;
        m_imageAnswer = ok ? answer.left(65536) : status == 422 ? "The model declined this request."
            : status == 503 || status == 502 ? "The model is not available." : "The image could not be interpreted.";
        emit imageChanged();
    });
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
            {"evidence", (m_mock ? "Mock fixture: " : "") + evidence}, {"createdAt", record.createdAt.toString(Qt::ISODate)},
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
    if (m_live) {
        // Restarting the runtime drops this session's tasks; the saved conversation is deleted on the server.
        const auto generation = m_sessionGeneration;
        stopRuntime();
        startRuntime(new GatewayModelProvider(m_auth->projectUrl(), m_auth->publishableKey(), m_token), "Model via account gateway", liveEngineStatus());
        m_data->remove("mnaka_messages", {}, [this, generation](int status, QJsonDocument) {
            if (generation != m_sessionGeneration || (status >= 200 && status < 300)) return;
            m_notice = "The saved conversation could not be deleted."; emit changed();
        });
        emit messagesChanged(); emit tasksChanged(); emit capabilitiesChanged(); emit changed();
        return;
    }
    m_notice.clear();
    emit changed();
}
