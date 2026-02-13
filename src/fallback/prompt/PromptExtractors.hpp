#pragma once

#include <QJsonObject>
#include <QString>

namespace bb::fallback::prompt {

    QString extractCommandName(const QString& message);
    QString extractUnlockTarget(const QString& text);
    QString extractUnlockTargetFromContext(const QJsonObject& context);
    QString buildUnlockDetails(const QJsonObject& context, const QString& target);

} // namespace bb::fallback::prompt
