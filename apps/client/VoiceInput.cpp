#include "VoiceInput.h"
#include <QPointer>
#ifdef Q_OS_WASM
#include <emscripten.h>
#include <emscripten/bind.h>
#endif

namespace {
QPointer<VoiceInput> activeInput;
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
#ifdef Q_OS_WASM
    m_available = EM_ASM_INT({ return (globalThis.SpeechRecognition || globalThis.webkitSpeechRecognition) ? 1 : 0; }) == 1;
#endif
}
VoiceInput::~VoiceInput() { stop(); }

void VoiceInput::start() {
    if (!m_available || m_listening) return;
    m_listening = true; m_partial.clear(); m_notice.clear();
    emit changed();
#ifdef Q_OS_WASM
    EM_ASM({
        const Recognition = globalThis.SpeechRecognition || globalThis.webkitSpeechRecognition;
        try {
            if (Module.mnakaRecognition) Module.mnakaRecognition.abort();
            const recognition = new Recognition();
            recognition.lang = navigator.language || "en-US";
            recognition.interimResults = true;
            recognition.continuous = false; // one utterance per press
            recognition.maxAlternatives = 1;
            recognition.onresult = event => {
                let text = ""; let final = false;
                for (let i = 0; i < event.results.length; ++i) { text += event.results[i][0].transcript; final = final || event.results[i].isFinal; }
                if (Module.mnakaRecognition === recognition) Module.mnakaVoiceEvent(final ? "final" : "partial", text);
            };
            recognition.onerror = event => { if (Module.mnakaRecognition === recognition) Module.mnakaVoiceEvent("error", String(event.error || "error")); };
            recognition.onend = () => { if (Module.mnakaRecognition === recognition) { Module.mnakaRecognition = null; Module.mnakaVoiceEvent("end", ""); } };
            Module.mnakaRecognition = recognition;
            recognition.start();
        } catch (error) {
            Module.mnakaRecognition = null;
            Module.mnakaVoiceEvent("error", "start-failed");
        }
    });
#endif
}
void VoiceInput::stop() {
    if (!m_listening) return;
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
        m_partial = text.left(4000);
    } else if (kind == "final") {
        m_partial.clear();
        const auto spoken = text.simplified().left(4000);
        if (!spoken.isEmpty()) emit recognized(spoken);
    } else if (kind == "end") {
        m_listening = false; m_partial.clear();
    } else if (kind == "error") {
        m_listening = false; m_partial.clear();
        m_notice = text == "not-allowed" || text == "service-not-allowed" ? "Microphone access was refused. Allow the microphone for this site in your browser, then try again."
            : text == "no-speech" ? "No speech was heard."
            : text == "audio-capture" ? "No microphone was found."
            : text == "network" ? "The browser's speech service could not be reached."
            : text == "aborted" ? QString()
            : "Voice input stopped (" + text.left(40) + ").";
    } else return;
    emit changed();
}
