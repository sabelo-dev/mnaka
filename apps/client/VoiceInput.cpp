#include "VoiceInput.h"
#include <QPointer>
#include <QRegularExpression>
#include <QSettings>
#ifdef Q_OS_WASM
#include <emscripten.h>
#include <emscripten/bind.h>
#endif

namespace {
QPointer<VoiceInput> activeInput;

int editDistance(const QString &a, const QString &b) {
    QList<int> row(b.size() + 1);
    for (int j = 0; j <= b.size(); ++j) row[j] = j;
    for (int i = 1; i <= a.size(); ++i) {
        int diagonal = row[0];
        row[0] = i;
        for (int j = 1; j <= b.size(); ++j) {
            const int above = row[j];
            row[j] = qMin(qMin(row[j] + 1, row[j - 1] + 1), diagonal + (a[i - 1] == b[j - 1] ? 0 : 1));
            diagonal = above;
        }
    }
    return row[b.size()];
}
// "mnaka" as a recogniser might spell it. Kept tight: a loose match would wake on ordinary words.
bool soundsLikeWakeWord(const QString &word) {
    static const QStringList heard{"mnaka", "manaka", "monaka", "menaka", "minaka", "munaka", "monaca", "manaca", "mnaca",
        "emnaka", "imnaka", "amnaka", "umnaka", "mnakka", "manakka"};
    if (word.size() < 4 || word.size() > 8) return false;
    return heard.contains(word) || editDistance(word, "mnaka") <= 1;
}
}

#ifdef Q_OS_WASM
namespace {
// The page calls this from the recogniser's callbacks; hand over to the Qt event loop.
void voiceEvent(std::string kind, std::string text) {
    if (!activeInput) return;
    QMetaObject::invokeMethod(activeInput, [kind = QString::fromStdString(kind), text = QString::fromStdString(text)] {
        if (activeInput) activeInput->handleEvent(kind, text);
    }, Qt::QueuedConnection);
}
}
EMSCRIPTEN_BINDINGS(mnaka_voice) { emscripten::function("mnakaVoiceEvent", &voiceEvent); }
#endif

VoiceInput::VoiceInput(QObject *parent) : QObject(parent) {
    activeInput = this;
    m_restart.setSingleShot(true);
    connect(&m_restart, &QTimer::timeout, this, [this] { if (m_handsFree && !m_sessionRunning) beginSession(); });
    m_requestWindow.setSingleShot(true); m_requestWindow.setInterval(8000);
    connect(&m_requestWindow, &QTimer::timeout, this, [this] {
        awaitRequest(false);
        if (!m_handsFree) stop(); // a pressed microphone that heard nothing gives up
        emit changed();
    });
#ifdef Q_OS_WASM
    m_available = EM_ASM_INT({ return (globalThis.SpeechRecognition || globalThis.webkitSpeechRecognition) ? 1 : 0; }) == 1;
    // The choice to listen hands-free is remembered for this site; the browser still decides
    // whether the page may use the microphone at all.
    if (m_available && QSettings().value("voice/handsFree").toBool()) setHandsFree(true);
#endif
}
VoiceInput::~VoiceInput() { m_handsFree = false; stop(); }

QString VoiceInput::requestAfterWakeWord(const QString &spoken, bool *woke) {
    static const QRegularExpression edges("^[^\\p{L}\\p{N}]+|[^\\p{L}\\p{N}]+$");
    const auto words = spoken.simplified().split(' ', Qt::SkipEmptyParts);
    const auto bare = [&words](qsizetype i) { return QString(words[i]).remove(edges).remove('\'').toLower(); };
    *woke = false;
    qsizetype at = 0;
    // An opener people naturally add; anything else before the name means it was not a summons.
    if (!words.isEmpty() && QStringList{"hey", "hi", "ok", "okay", "yo"}.contains(bare(0))) at = 1;
    if (at >= words.size()) return {};
    qsizetype next = at + 1;
    if (!soundsLikeWakeWord(bare(at))) {
        // Split in two by the recogniser: "em naka", "m naka", "mana ka".
        if (at + 1 >= words.size() || !soundsLikeWakeWord(bare(at) + bare(at + 1))) return {};
        next = at + 2;
    }
    *woke = true;
    auto request = words.mid(next).join(' ');
    return request.remove(QRegularExpression("^[\\s,.:;!?-]+")).trimmed().left(4000);
}

void VoiceInput::setHandsFree(bool enabled) {
    if (enabled == m_handsFree) return;
    m_handsFree = enabled;
    m_notice.clear();
#ifdef Q_OS_WASM
    QSettings().setValue("voice/handsFree", enabled);
#endif
    if (enabled) { if (!m_sessionRunning) beginSession(); }
    else { m_restart.stop(); awaitRequest(false); stop(); }
    emit changed();
}
void VoiceInput::awaitRequest(bool awaiting) {
    m_awaitingRequest = awaiting;
    if (awaiting) m_requestWindow.start(); else m_requestWindow.stop();
}
void VoiceInput::start() {
    if (!m_available) return;
    m_notice.clear();
    awaitRequest(true); // the next utterance is the request, wake word or not
    if (!m_sessionRunning) beginSession();
    emit changed();
}
void VoiceInput::beginSession() {
    if (!m_available) return;
    m_sessionRunning = true; m_listening = true; m_partial.clear();
#ifdef Q_OS_WASM
    EM_ASM({
        const Recognition = globalThis.SpeechRecognition || globalThis.webkitSpeechRecognition;
        try {
            if (document.visibilityState !== "visible") { Module.mnakaVoiceEvent("end", "hidden"); return; }
            if (Module.mnakaRecognition) Module.mnakaRecognition.abort();
            const recognition = new Recognition();
            recognition.lang = navigator.language || "en-US";
            recognition.interimResults = true;
            recognition.continuous = $0 === 1; // hands-free keeps going between utterances
            recognition.maxAlternatives = 1;
            recognition.onresult = event => {
                if (Module.mnakaRecognition !== recognition) return;
                for (let i = event.resultIndex; i < event.results.length; ++i) {
                    const result = event.results[i];
                    Module.mnakaVoiceEvent(result.isFinal ? "final" : "partial", result[0].transcript);
                }
            };
            recognition.onerror = event => { if (Module.mnakaRecognition === recognition) Module.mnakaVoiceEvent("error", String(event.error || "error")); };
            recognition.onend = () => { if (Module.mnakaRecognition === recognition) { Module.mnakaRecognition = null; Module.mnakaVoiceEvent("end", ""); } };
            Module.mnakaRecognition = recognition;
            recognition.start();
        } catch (error) {
            Module.mnakaRecognition = null;
            Module.mnakaVoiceEvent("error", "start-failed");
        }
    }, m_handsFree ? 1 : 0);
#endif
}
void VoiceInput::stop() {
    if (m_handsFree) { // the microphone button only cancels a pending request in hands-free mode
        awaitRequest(false); m_partial.clear();
        emit changed();
        return;
    }
    if (!m_listening && !m_sessionRunning) return;
#ifdef Q_OS_WASM
    // stop() lets the recogniser deliver what it has heard so far, then end.
    EM_ASM({ try { if (Module.mnakaRecognition) Module.mnakaRecognition.stop(); } catch (error) {} });
#endif
    m_listening = false; m_partial.clear();
    emit changed();
}
void VoiceInput::handleEvent(const QString &kind, const QString &text) {
    if (kind == "partial") {
        if (!m_listening) return;
        bool woke = false;
        // Hands-free shows only speech addressed to MNAKA; the rest never reaches the screen.
        if (!m_handsFree || m_awaitingRequest) m_partial = text.left(4000);
        else { requestAfterWakeWord(text, &woke); m_partial = woke ? text.left(4000) : QString(); }
    } else if (kind == "final") {
        m_partial.clear();
        const auto spoken = text.simplified().left(4000);
        if (spoken.isEmpty()) return;
        bool woke = false;
        const auto request = requestAfterWakeWord(spoken, &woke);
        if (m_awaitingRequest) {
            // Already listening for a request: take it, with or without the name in front.
            awaitRequest(false);
            const auto wanted = woke ? request : spoken;
            if (!wanted.isEmpty()) emit recognized(wanted);
            else awaitRequest(true); // just the name again
        } else if (!m_handsFree) {
            emit recognized(woke && !request.isEmpty() ? request : spoken);
        } else if (woke) {
            if (request.isEmpty()) { awaitRequest(true); m_notice = "Yes? Say your request."; }
            else { m_notice.clear(); emit recognized(request); }
        } // otherwise: not addressed to MNAKA, dropped
    } else if (kind == "end") {
        m_sessionRunning = false; m_partial.clear();
        // Browsers end a session after a pause or a minute or so; hands-free starts the next one.
        if (m_handsFree) m_restart.start(text == "hidden" ? 2000 : 400);
        else { m_listening = false; awaitRequest(false); }
    } else if (kind == "error") {
        m_partial.clear();
        const bool refused = text == "not-allowed" || text == "service-not-allowed";
        if (refused || text == "audio-capture" || text == "start-failed") {
            // Nothing a retry would fix: stop asking until the user turns it on again.
            m_sessionRunning = false; m_listening = false; awaitRequest(false); m_restart.stop();
            if (m_handsFree) { m_handsFree = false;
#ifdef Q_OS_WASM
                QSettings().setValue("voice/handsFree", false);
#endif
            }
            m_notice = refused ? "Microphone access was refused. Allow the microphone for this site in your browser, then try again."
                : text == "audio-capture" ? "No microphone was found." : "Voice input could not start.";
        } else if (m_handsFree) {
            // Silence and dropped connections are routine while waiting; the session restarts on "end".
            if (text == "network") m_notice = "The browser's speech service could not be reached. Retrying.";
        } else {
            m_listening = false; awaitRequest(false);
            m_notice = text == "no-speech" ? "No speech was heard." : text == "network" ? "The browser's speech service could not be reached."
                : text == "aborted" ? QString() : "Voice input stopped (" + text.left(40) + ").";
        }
    } else return;
    emit changed();
}
