#pragma once
#include "adapters/models/ModelProvider.h"
#include <QHash>
#include <QMutex>
#include <QUrl>
#include <memory>

class QNetworkAccessManager;
class QNetworkReply;

namespace mnaka {
// The signed-in user's access token, shared with the provider on the planning thread.
class SessionToken {
public:
    void set(const QString &token) { QMutexLocker lock(&m_mutex); m_token = token; }
    QString get() const { QMutexLocker lock(&m_mutex); return m_token; }
private:
    mutable QMutex m_mutex;
    QString m_token;
};

// Sends the user's message to the project's `assistant` function, which forwards it to the
// owner's own model. The model endpoint and its key live server-side; this class never sees them.
class GatewayModelProvider : public ModelProvider {
    Q_OBJECT
public:
    GatewayModelProvider(QUrl projectUrl, QString publishableKey, std::shared_ptr<SessionToken> token);
    // Shared with image interpretation, which is not a task. Empty URL when the pair is unusable.
    static QUrl endpoint(const QUrl &projectUrl);
public slots:
    bool start(QUuid requestId, QJsonObject request) override;
    void cancel(QUuid requestId) override;
private:
    QUrl m_projectUrl;
    QString m_key;
    std::shared_ptr<SessionToken> m_token;
    QNetworkAccessManager *m_network = nullptr;
    QHash<QUuid, QNetworkReply *> m_replies;
};
}
