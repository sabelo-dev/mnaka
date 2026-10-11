#pragma once
#include <QString>
#include <QUrl>

// A file the user picked, as something QFile can open: a local path, or on Android the
// content:// URI the system picker returns. Empty for anything else (remote URLs included).
inline QString pickedFilePath(const QUrl &url) {
    if (url.isLocalFile()) return url.toLocalFile();
#ifdef Q_OS_ANDROID
    if (url.scheme() == "content") return url.toString();
#endif
    return {};
}
