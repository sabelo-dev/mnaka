#pragma once
#include <QString>
#include <QStringList>

namespace mnaka {
inline bool isTerminal(const QString &state) {
    return state == "succeeded" || state == "failed" || state == "cancelled";
}
inline bool canTransition(const QString &from, const QString &to, bool verifiedResume = false) {
    if (from == "interrupted") return to == "queued" && verifiedResume;
    if (QStringList{"succeeded", "failed", "cancelled"}.contains(from)) return false;
    if (from == "queued") return QStringList{"planning", "cancelling", "failed"}.contains(to);
    if (from == "planning") return QStringList{"running", "awaiting_approval", "cancelling", "failed", "interrupted"}.contains(to);
    if (from == "running") return QStringList{"awaiting_approval", "cancelling", "succeeded", "failed", "interrupted"}.contains(to);
    if (from == "awaiting_approval") return QStringList{"running", "cancelling", "failed", "interrupted"}.contains(to);
    if (from == "cancelling") return QStringList{"cancelled", "interrupted"}.contains(to);
    return false;
}
}
