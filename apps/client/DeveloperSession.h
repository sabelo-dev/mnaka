#pragma once
#include <QJsonObject>
#include <QMap>
#include <QObject>
#include <QSet>
#include <QUrl>
#include <QVariantList>
#include <QtQml/qqmlregistration.h>

namespace mnaka { class SupabaseData; }

// The Developer workspace: text files the user chose, a change the user's model proposes for
// them, and the user's decision. Files live in this object only. A proposal changes nothing
// until the user applies it, and nothing reaches a disk until the user saves a file.
// No build or test is ever run: there is no sandbox to run one in.
class DeveloperSession : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Owned by WorkspaceModel")
    Q_PROPERTY(QVariantList files READ files NOTIFY changed)
    Q_PROPERTY(QVariantList proposal READ proposal NOTIFY changed)
    Q_PROPERTY(QVariantList steps READ steps NOTIFY changed)
    Q_PROPERTY(QString summary READ summary NOTIFY changed)
    Q_PROPERTY(QString notice READ notice NOTIFY changed)
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
    Q_PROPERTY(bool hasProposal READ hasProposal NOTIFY changed)
    Q_PROPERTY(int added READ added NOTIFY changed)
    Q_PROPERTY(int removed READ removed NOTIFY changed)
    // True in the browser, where the page's own file chooser and downloads replace Qt dialogs.
    Q_PROPERTY(bool browserFiles READ browserFiles CONSTANT)
public:
    static constexpr int maxFiles = 24;
    static constexpr int maxFileBytes = 65536;
    static constexpr int maxTotalBytes = 262144;

    explicit DeveloperSession(QObject *parent = nullptr);
    // Null while signed out. Requests need it; the files themselves do not.
    void setGateway(mnaka::SupabaseData *gateway);

    QVariantList files() const;
    QVariantList proposal() const { return m_proposalView; }
    QVariantList steps() const;
    QString summary() const { return m_summary; }
    QString notice() const { return m_notice; }
    bool busy() const { return m_busy; }
    bool hasProposal() const { return !m_proposal.isEmpty(); }
    int added() const { return m_added; }
    int removed() const { return m_removed; }
    bool browserFiles() const;

    // Empty when added; otherwise why the file was refused.
    QString addFile(const QString &name, const QByteArray &bytes);
    // Checks a model reply against the project. Empty when usable; `changes` gets only real changes.
    static QString readProposal(const QJsonObject &reply, const QMap<QString, QString> &files,
        QMap<QString, QString> &changes, QString &summary);

    Q_INVOKABLE void addFiles(QList<QUrl> localFiles);
    Q_INVOKABLE void pickFiles();
    Q_INVOKABLE void removeFile(QString path);
    Q_INVOKABLE void clearProject();
    Q_INVOKABLE void request(QString text);
    Q_INVOKABLE void applyProposal();
    Q_INVOKABLE void discardProposal();
    // Browser: downloads the file. Desktop: writes it where the user's save dialog pointed.
    Q_INVOKABLE void saveFile(QString path, QUrl destination = QUrl());
signals:
    void changed();
private:
    void say(const QString &message) { m_notice = message; emit changed(); }
    void rebuildProposalView();
    qsizetype totalBytes() const;
    mnaka::SupabaseData *m_gateway = nullptr;
    QMap<QString, QString> m_files, m_proposal;
    QSet<QString> m_unsaved;
    QVariantList m_proposalView;
    QString m_summary, m_notice, m_lastRequest;
    bool m_busy = false, m_asked = false, m_applied = false;
    int m_added = 0, m_removed = 0;
    quint64 m_generation = 0;
};
