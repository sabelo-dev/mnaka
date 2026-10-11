#include "DeveloperSession.h"
#include "adapters/supabase/SupabaseData.h"
#include "apps/client/PickedFile.h"
#include "engines/software/LineDiff.h"
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QRegularExpression>
#include <QSaveFile>
#include <memory>
#ifdef Q_OS_WASM
#include <QPointer>
#include <QtGui/private/qwasmlocalfileaccess_p.h>
#endif

using namespace mnaka;

namespace {
bool safeName(const QString &name) {
    static const QRegularExpression pattern("^[A-Za-z0-9._-]{1,120}$");
    return pattern.match(name).hasMatch() && name != "." && name != "..";
}
}

DeveloperSession::DeveloperSession(QObject *parent) : QObject(parent) {}

void DeveloperSession::setGateway(SupabaseData *gateway) {
    if (gateway == m_gateway) return;
    m_gateway = gateway;
    ++m_generation; // a reply to the previous session is no longer wanted
    m_busy = false;
    if (!gateway) { m_proposal.clear(); m_summary.clear(); m_asked = false; rebuildProposalView(); }
    emit changed();
}
bool DeveloperSession::browserFiles() const {
#ifdef Q_OS_WASM
    return true;
#else
    return false;
#endif
}
qsizetype DeveloperSession::totalBytes() const {
    qsizetype total = 0;
    for (const auto &content : m_files) total += content.toUtf8().size();
    return total;
}
QString DeveloperSession::addFile(const QString &name, const QByteArray &bytes) {
    if (!safeName(name)) return name.left(60) + ": file names may use letters, digits, dot, dash and underscore only.";
    if (m_files.contains(name)) return name + " is already in the project.";
    if (m_files.size() >= maxFiles) return QString("A project holds at most %1 files.").arg(maxFiles);
    if (bytes.size() > maxFileBytes) return name + " is larger than 64 KiB.";
    if (totalBytes() + bytes.size() > maxTotalBytes) return "The project would exceed 256 KiB.";
    // Text only: the bytes must survive a UTF-8 round trip exactly (a leading BOM is dropped).
    const auto body = bytes.startsWith("\xEF\xBB\xBF") ? bytes.mid(3) : bytes;
    const QString text = QString::fromUtf8(body);
    if (body.contains('\0') || text.toUtf8() != body) return name + " is not a UTF-8 text file.";
    m_files.insert(name, text);
    return {};
}
void DeveloperSession::addFiles(QList<QUrl> localFiles) {
    QStringList refused;
    for (const auto &url : localFiles) {
        const auto source = pickedFilePath(url);
        QFile file(source);
        if (source.isEmpty() || !file.open(QIODevice::ReadOnly) || file.size() > maxFileBytes) {
            refused.append(QFileInfo(source).fileName() + " could not be read or is larger than 64 KiB.");
            continue;
        }
        // Only the file name is kept: folders on this device are not sent to the model.
        const auto problem = addFile(QFileInfo(file).fileName(), file.readAll());
        if (!problem.isEmpty()) refused.append(problem);
    }
    say(refused.join(' '));
}
void DeveloperSession::pickFiles() {
#ifdef Q_OS_WASM
    // The browser hands over the bytes of the files the user chose; nothing is uploaded here.
    struct Incoming { QString name; QByteArray bytes; };
    auto incoming = std::make_shared<Incoming>();
    QPointer<DeveloperSession> self(this);
    QWasmLocalFileAccess::openFiles("", QWasmLocalFileAccess::FileSelectMode::MultipleFiles, [](int) {},
        [self, incoming](uint64_t size, const std::string &name) -> char * {
            if (!self) return nullptr;
            incoming->name = QString::fromStdString(name);
            if (size > uint64_t(maxFileBytes)) { self->say(incoming->name + " is larger than 64 KiB."); return nullptr; }
            incoming->bytes.resize(qsizetype(size));
            return incoming->bytes.data();
        },
        [self, incoming] {
            if (!self) return;
            self->say(self->addFile(incoming->name, incoming->bytes));
        });
#endif
}
void DeveloperSession::removeFile(QString path) {
    if (m_busy || !m_files.remove(path)) return;
    m_unsaved.remove(path);
    if (m_proposal.remove(path)) rebuildProposalView();
    say({});
}
void DeveloperSession::clearProject() {
    if (m_busy) return;
    m_files.clear(); m_unsaved.clear(); m_proposal.clear(); m_summary.clear(); m_lastRequest.clear();
    m_asked = false; m_applied = false;
    rebuildProposalView();
    say({});
}
QString DeveloperSession::readProposal(const QJsonObject &reply, const QMap<QString, QString> &files,
        QMap<QString, QString> &changes, QString &summary) {
    changes.clear();
    summary = reply["summary"].toString().left(2000);
    if (!reply["files"].isArray()) return "The model did not return a usable change set.";
    // Model output is untrusted. Only plain, bounded text files with safe names are accepted,
    // and nothing here is executed or written.
    for (const auto &value : reply["files"].toArray()) {
        const auto entry = value.toObject();
        const auto path = entry["path"].toString();
        if (!entry["content"].isString() || !safeName(path)) return "The model named a file this workspace cannot accept.";
        const auto content = entry["content"].toString();
        if (content.toUtf8().size() > maxFileBytes || content.contains(QChar(0))) return path + ": the proposed content is too large or not text.";
        if (changes.contains(path)) return "The model returned " + path + " twice.";
        if (files.value(path) == content && files.contains(path)) continue; // identical: not a change
        changes.insert(path, content);
    }
    int newFiles = 0;
    for (auto it = changes.cbegin(); it != changes.cend(); ++it) if (!files.contains(it.key())) ++newFiles;
    if (files.size() + newFiles > maxFiles) return "The proposal adds more files than a project can hold.";
    return {};
}
void DeveloperSession::request(QString text) {
    text = text.trimmed();
    if (m_busy) return;
    if (!m_gateway) { say("Sign in under Settings to ask for a change."); return; }
    if (m_files.isEmpty()) { say("Add the files the change concerns first."); return; }
    if (text.isEmpty() || text.size() > 4000) { say("Describe the change in 1 to 4000 characters."); return; }
    QJsonArray files;
    for (auto it = m_files.cbegin(); it != m_files.cend(); ++it) files.append(QJsonObject{{"path", it.key()}, {"content", it.value()}});
    m_busy = true; m_asked = true; m_applied = false; m_lastRequest = text;
    m_proposal.clear(); m_summary.clear(); m_notice.clear();
    rebuildProposalView();
    emit changed();
    const auto generation = ++m_generation;
    m_gateway->invoke("developer", {{"request", text}, {"files", files}}, [this, generation](int status, QJsonDocument body) {
        if (generation != m_generation) return;
        m_busy = false;
        if (status != 200) {
            m_asked = false;
            say(status == 502 || status == 503 ? "The model endpoint is not available." : status == 401 ? "Sign in again to ask for a change."
                : "The request was not accepted.");
            return;
        }
        QMap<QString, QString> changes;
        const auto problem = readProposal(body.object(), m_files, changes, m_summary);
        if (!problem.isEmpty()) { m_summary.clear(); m_asked = false; say(problem); return; }
        m_proposal = changes;
        rebuildProposalView();
        say(changes.isEmpty() ? "The model proposed no changes." : QString());
    });
}
void DeveloperSession::rebuildProposalView() {
    m_proposalView.clear(); m_added = 0; m_removed = 0;
    for (auto it = m_proposal.cbegin(); it != m_proposal.cend(); ++it) {
        const auto diff = diffLines(m_files.value(it.key()), it.value());
        m_added += diff.added; m_removed += diff.removed;
        QVariantList hunks;
        int shown = 0;
        for (const auto &hunk : diff.hunks) {
            QVariantList lines;
            for (const auto &line : hunk.lines) {
                if (++shown > 800) break; // a bound on what one file can put on screen
                lines.append(QVariant(QVariantList{line.number, QString(line.sign), line.text}));
            }
            hunks.append(QVariantMap{{"header", hunk.header()}, {"lines", lines}});
            if (shown > 800) break;
        }
        m_proposalView.append(QVariantMap{{"path", it.key()}, {"added", diff.added}, {"removed", diff.removed},
            {"isNew", !m_files.contains(it.key())}, {"truncated", shown > 800}, {"hunks", hunks}});
    }
}
void DeveloperSession::applyProposal() {
    if (m_busy || m_proposal.isEmpty()) return;
    for (auto it = m_proposal.cbegin(); it != m_proposal.cend(); ++it) { m_files.insert(it.key(), it.value()); m_unsaved.insert(it.key()); }
    m_proposal.clear(); m_applied = true;
    rebuildProposalView();
    say("Applied to the files in this session. Save each changed file to keep it.");
}
void DeveloperSession::discardProposal() {
    if (m_busy || m_proposal.isEmpty()) return;
    m_proposal.clear(); m_summary.clear(); m_asked = false;
    rebuildProposalView();
    say("Proposal discarded. Your files are unchanged.");
}
void DeveloperSession::saveFile(QString path, QUrl destination) {
    if (!m_files.contains(path)) return;
    const auto bytes = m_files.value(path).toUtf8();
#ifdef Q_OS_WASM
    Q_UNUSED(destination)
    QWasmLocalFileAccess::saveFile(bytes, path.toStdString());
#else
    const auto target = pickedFilePath(destination);
    if (target.isEmpty()) { say("Choose where to save " + path + "."); return; }
    if (destination.isLocalFile()) {
        QSaveFile file(target);
        if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size() || !file.commit()) { say(path + " could not be saved."); return; }
    } else {
        // An Android document URI: there is no temporary sibling to rename, so write in place.
        QFile file(target);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate) || file.write(bytes) != bytes.size()) { say(path + " could not be saved."); return; }
    }
#endif
    m_unsaved.remove(path);
    say({});
}
QVariantList DeveloperSession::files() const {
    QVariantList result;
    for (auto it = m_files.cbegin(); it != m_files.cend(); ++it)
        result.append(QVariantMap{{"path", it.key()}, {"lines", int(it.value().count('\n') + (it.value().isEmpty() || it.value().endsWith('\n') ? 0 : 1))},
            {"proposed", m_proposal.contains(it.key())}, {"unsaved", m_unsaved.contains(it.key())}});
    for (auto it = m_proposal.cbegin(); it != m_proposal.cend(); ++it)
        if (!m_files.contains(it.key())) result.append(QVariantMap{{"path", it.key()}, {"lines", 0}, {"proposed", true}, {"unsaved", false}, {"isNew", true}});
    return result;
}
QVariantList DeveloperSession::steps() const {
    const auto step = [](const QString &title, const QString &detail, const QString &state) {
        return QVariantMap{{"title", title}, {"detail", detail}, {"state", state}};
    };
    const bool loaded = !m_files.isEmpty();
    const bool answered = m_asked && !m_busy;
    QVariantList result;
    result.append(step("Load project files", loaded ? QString("%1 file(s), %2 KiB, held in this session only.").arg(m_files.size()).arg((totalBytes() + 1023) / 1024)
        : "Add the text files the change concerns.", loaded ? "done" : "pending"));
    result.append(step("Ask your model", m_busy ? "Waiting for your model…" : answered ? "Request sent with the project files." : "Describe the change below.",
        m_busy ? "active" : answered ? "done" : "pending"));
    result.append(step("Prepare patch", hasProposal() ? QString("%1 file(s) changed.").arg(m_proposal.size()) : m_applied ? "Patch applied." : "No proposal yet.",
        hasProposal() || m_applied ? "done" : "pending"));
    result.append(step("Run tests", "Not run. There is no build sandbox: build and test the result yourself.", "skipped"));
    result.append(step("Your approval", m_applied ? "Applied. Save each changed file to keep it." : hasProposal() ? "Review the changes, then apply or discard."
        : "Nothing to approve.", m_applied ? "done" : hasProposal() ? "active" : "pending"));
    return result;
}
