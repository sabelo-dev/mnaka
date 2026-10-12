#pragma once
#include <QObject>
#include <QString>
#include <QTimer>
#include <QtQml/qqmlregistration.h>

// Speech input. In the browser this drives the page's own speech recognition (the Web Speech
// API); the audio is handled by the browser and, in Chrome and Edge, by the browser vendor's
// speech service. MNAKA and the model receive only the resulting text. Other platforms have no
// recogniser, so `available` is false there.
//
// Two ways to talk: press the microphone for one utterance, or turn on hands-free, where the
// recogniser stays on and only speech that starts with the wake word "Mnaka" is acted on.
// Everything else heard in hands-free mode is dropped here and never shown or sent.
class VoiceInput : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Owned by WorkspaceModel")
    Q_PROPERTY(bool available READ available CONSTANT)
    Q_PROPERTY(bool listening READ listening NOTIFY changed)
    Q_PROPERTY(bool handsFree READ handsFree WRITE setHandsFree NOTIFY changed)
    // True for a few seconds after the wake word alone, or after the microphone is pressed:
    // the next thing said is the request.
    Q_PROPERTY(bool awaitingRequest READ awaitingRequest NOTIFY changed)
    Q_PROPERTY(QString partial READ partial NOTIFY changed)
    Q_PROPERTY(QString notice READ notice NOTIFY changed)
public:
    explicit VoiceInput(QObject *parent = nullptr);
    ~VoiceInput() override;
    bool available() const { return m_available; }
    bool listening() const { return m_listening; }
    bool handsFree() const { return m_handsFree; }
    void setHandsFree(bool enabled);
    bool awaitingRequest() const { return m_awaitingRequest; }
    QString partial() const { return m_partial; }
    QString notice() const { return m_notice; }
    Q_INVOKABLE void start();
    Q_INVOKABLE void stop();
    // Called with what the recogniser reports: "partial", "final", "end" or "error".
    void handleEvent(const QString &kind, const QString &text);
    // The request that follows the wake word, when `spoken` starts with it (after an optional
    // "hey", "ok" or "hi"). `woke` says whether the wake word was there at all; the result is
    // empty when it was said on its own. Speech engines have never heard the name, so common
    // mishearings ("manaka", "monaka", "em naka") count too.
    static QString requestAfterWakeWord(const QString &spoken, bool *woke);
signals:
    void changed();
    // One finished request, bounded and trimmed, without the wake word. Treated like typed
    // text: never executed.
    void recognized(QString text);
private:
    void beginSession();
    void awaitRequest(bool awaiting);
    bool m_available = false, m_listening = false, m_handsFree = false, m_awaitingRequest = false, m_sessionRunning = false;
    QString m_partial, m_notice;
    QTimer m_restart, m_requestWindow;
};
