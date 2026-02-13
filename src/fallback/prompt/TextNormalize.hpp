#pragma once

#include <QString>
#include <QStringList>

namespace bb::fallback::prompt {

    QString normalizeDetailText(const QString& text);
    QString normalizeCompareText(const QString& text);
    bool    textEquivalent(const QString& left, const QString& right);
    QString firstMeaningfulLine(const QString& text);
    QString trimToLength(const QString& text, int maxChars);
    QString uniqueJoined(const QStringList& values);

} // namespace bb::fallback::prompt
