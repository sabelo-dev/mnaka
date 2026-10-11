#include "LineDiff.h"
#include <QStringList>
#include <vector>

namespace mnaka {
namespace {
QStringList splitLines(const QString &text) {
    if (text.isEmpty()) return {};
    auto lines = text.split('\n');
    if (lines.last().isEmpty()) lines.removeLast(); // a trailing newline does not start a line
    for (auto &line : lines) if (line.endsWith('\r')) line.chop(1);
    return lines;
}
}
QString DiffHunk::header() const {
    return QString("@@ -%1,%2 +%3,%4 @@").arg(oldCount ? oldStart : 0).arg(oldCount).arg(newCount ? newStart : 0).arg(newCount);
}
FileDiff diffLines(const QString &before, const QString &after, int context) {
    const auto a = splitLines(before), b = splitLines(after);
    qsizetype head = 0;
    while (head < a.size() && head < b.size() && a[head] == b[head]) ++head;
    qsizetype tail = 0;
    while (tail < a.size() - head && tail < b.size() - head && a[a.size() - 1 - tail] == b[b.size() - 1 - tail]) ++tail;
    const qsizetype n = a.size() - head - tail, m = b.size() - head - tail;

    // One entry per line of the result, in order.
    struct Op { QChar sign; int oldNumber, newNumber; QString text; };
    QList<Op> ops;
    int oldNumber = 1, newNumber = 1;
    const auto keep = [&](const QString &text) { ops.append({' ', oldNumber++, newNumber++, text}); };
    const auto remove = [&](const QString &text) { ops.append({'-', oldNumber++, newNumber, text}); };
    const auto add = [&](const QString &text) { ops.append({'+', oldNumber, newNumber++, text}); };
    for (qsizetype i = 0; i < head; ++i) keep(a[i]);
    if (n * m > 4'000'000) {
        for (qsizetype i = 0; i < n; ++i) remove(a[head + i]);
        for (qsizetype j = 0; j < m; ++j) add(b[head + j]);
    } else {
        // Longest common subsequence over the part that differs.
        std::vector<int> table((n + 1) * (m + 1), 0);
        const auto at = [&](qsizetype i, qsizetype j) -> int & { return table[i * (m + 1) + j]; };
        for (qsizetype i = n - 1; i >= 0; --i)
            for (qsizetype j = m - 1; j >= 0; --j)
                at(i, j) = a[head + i] == b[head + j] ? at(i + 1, j + 1) + 1 : qMax(at(i + 1, j), at(i, j + 1));
        qsizetype i = 0, j = 0;
        while (i < n && j < m) {
            if (a[head + i] == b[head + j]) { keep(a[head + i]); ++i; ++j; }
            else if (at(i + 1, j) >= at(i, j + 1)) remove(a[head + i++]);
            else add(b[head + j++]);
        }
        while (i < n) remove(a[head + i++]);
        while (j < m) add(b[head + j++]);
    }
    for (qsizetype i = 0; i < tail; ++i) keep(a[a.size() - tail + i]);

    FileDiff result;
    qsizetype index = 0;
    while (index < ops.size()) {
        if (ops[index].sign == ' ') { ++index; continue; }
        // A hunk runs from `context` lines before this change to `context` lines after the last
        // change that is within 2 * context lines of the previous one.
        qsizetype start = qMax<qsizetype>(0, index - context), last = index, scan = index;
        while (scan < ops.size() && scan - last <= 2 * context) {
            if (ops[scan].sign != ' ') last = scan;
            ++scan;
        }
        const qsizetype end = qMin(ops.size(), last + context + 1);
        DiffHunk hunk;
        hunk.oldStart = ops[start].oldNumber; hunk.newStart = ops[start].newNumber;
        for (qsizetype k = start; k < end; ++k) {
            const auto &op = ops[k];
            if (op.sign != '+') ++hunk.oldCount;
            if (op.sign != '-') ++hunk.newCount;
            if (op.sign == '+') ++result.added;
            if (op.sign == '-') ++result.removed;
            hunk.lines.append({op.sign == '-' ? op.oldNumber : op.newNumber, op.sign, op.text});
        }
        result.hunks.append(hunk);
        index = end;
    }
    return result;
}
}
