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

// A speech engine has never seen "Mnaka", so it writes whatever real word sounds closest:
// "Monica", "manuka", "Monaco", "manaka", "my naka". What those share is the sound m-n-k with a
// vowel on the end, so that is what is matched, not a spelling. Words with the same consonants
// but no final vowel ("monk", "manic", "mink") or a different ending ("monkey") do not match.
QString soundSkeleton(const QString &word) {
    QString skeleton;
    for (const QChar letter : word) {
        QChar sound = letter;
        if (QString("aeiouyhw").contains(letter)) continue;
        if (letter == 'c' || letter == 'q') sound = 'k'; // not g: "mango" is not the name
        if (skeleton.isEmpty() || skeleton.back() != sound) skeleton.append(sound);
    }
    return skeleton;
}
bool soundsLikeWakeWord(const QString &word) {
    if (word.size() < 4 || word.size() > 9) return false;
    if (!QString("aou").contains(word.back())) return false;
    const auto skeleton = soundSkeleton(word);
    // The leading "m" is often swallowed or heard as a separate "em"/"in": "naka", "inaka".
    return skeleton == "mnk" || (skeleton == "nk" && word.size() <= 6 && word.contains("nak"));
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
    if (enabled) { if (!m_sessionRunning) beginSession(); setOpenMicrophone(true); } // hands-free is always listening
    else { m_restart.stop(); setOpenMicrophone(false); awaitRequest(false); stop(); }
    emit changed();
}
void VoiceInput::setOpenMicrophone(bool enabled) {
    if (enabled == m_openMicrophone) return;
    if (enabled && !m_handsFree) setHandsFree(true);
    m_openMicrophone = enabled;
    m_notice.clear(); m_partial.clear();
#ifdef Q_OS_WASM
    QSettings().setValue("voice/openMicrophone", enabled);
#endif
    awaitRequest(enabled);
    emit changed();
}
void VoiceInput::awaitRequest(bool awaiting) {
    m_awaitingRequest = awaiting || m_openMicrophone; // an open microphone never stops awaiting
    if (awaiting && !m_openMicrophone) m_requestWindow.start(); else m_requestWindow.stop();
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
        } else {
            // Not addressed to MNAKA: dropped. Only its first word is shown, so that a name the
            // recogniser keeps mishearing can be seen and reported.
            m_notice = "Heard \"" + spoken.section(' ', 0, 0).left(24) + "…\", not \"Mnaka\". Start with \"Mnaka\".";
        }
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
            m_sessionRunning = false; m_listening = false; setOpenMicrophone(false); awaitRequest(false); m_restart.stop();
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
