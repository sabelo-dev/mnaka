#pragma once
#include <QObject>
#include <QString>
#include <QtQml/qqmlregistration.h>

// Push-to-talk speech input. In the browser this drives the page's own speech recognition
// (the Web Speech API); the audio is handled by the browser and, in Chrome and Edge, by the
// browser vendor's speech service. MNAKA and the user's model receive only the resulting text.
// Other platforms have no recogniser, so `available` is false there.
class VoiceInput : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Owned by WorkspaceModel")
    Q_PROPERTY(bool available READ available CONSTANT)
    Q_PROPERTY(bool listening READ listening NOTIFY changed)
    Q_PROPERTY(QString partial READ partial NOTIFY changed)
    Q_PROPERTY(QString notice READ notice NOTIFY changed)
public:
    explicit VoiceInput(QObject *parent = nullptr);
    ~VoiceInput() override;
    bool available() const { return m_available; }
    bool listening() const { return m_listening; }
    QString partial() const { return m_partial; }
    QString notice() const { return m_notice; }
    Q_INVOKABLE void start();
    Q_INVOKABLE void stop();
    // Called with what the recogniser reports: "partial", "final", "end" or "error".
    void handleEvent(const QString &kind, const QString &text);
signals:
    void changed();
    // One finished utterance, bounded and trimmed. Treated like typed text: never executed.
    void recognized(QString text);
private:
    bool m_available = false, m_listening = false;
    QString m_partial, m_notice;
};
