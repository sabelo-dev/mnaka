#pragma once
#include <QList>
#include <QString>

namespace mnaka {
struct DiffLine {
    int number;   // line number in the old file for '-', in the new file otherwise
    QChar sign;   // '+', '-' or ' '
    QString text;
};
struct DiffHunk {
    int oldStart = 0, oldCount = 0, newStart = 0, newCount = 0;
    QList<DiffLine> lines;
    QString header() const;
};
struct FileDiff {
    int added = 0, removed = 0;
    QList<DiffHunk> hunks;
};
// Line-by-line difference with `context` unchanged lines around each change. Very large inputs
// fall back to "everything between the common start and end was replaced", which is still correct.
FileDiff diffLines(const QString &before, const QString &after, int context = 3);
}
