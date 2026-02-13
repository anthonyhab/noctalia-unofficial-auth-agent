#include "PromptExtractors.hpp"

#include "TextNormalize.hpp"

#include <QFileInfo>
#include <QRegularExpression>

namespace bb::fallback::prompt {

    namespace {

        QString captureFirst(const QString& text, const QRegularExpression& regex) {
            const QRegularExpressionMatch match = regex.match(text);
            if (!match.hasMatch()) {
                return QString();
            }

            return match.captured(1).trimmed();
        }

        bool isTemplateUnlockLine(const QString& line, const QString& target) {
            const QString normalized = normalizeCompareText(line);
            if (normalized.isEmpty()) {
                return true;
            }

            const QString normalizedTarget = normalizeCompareText(target);
            if (!normalizedTarget.isEmpty() && normalized == normalizedTarget) {
                return true;
            }

            if (!normalizedTarget.isEmpty() && normalized.contains("unlock") && normalized.contains(normalizedTarget)) {
                if (normalized.startsWith("authenticate to unlock") || normalized.startsWith("unlock") || normalized.startsWith("use your password to unlock") ||
                    normalized.startsWith("use your account password to unlock")) {
                    return true;
                }
            }

            return false;
        }

    } // namespace

    QString extractCommandName(const QString& message) {
        const QRegularExpression explicitRunRegex(R"(run\s+[`'"]([^`'"\s]+)[`'"])", QRegularExpression::CaseInsensitiveOption);
        QString                  command = captureFirst(message, explicitRunRegex);

        if (command.isEmpty()) {
            const QRegularExpression pathRegex(R"((/[A-Za-z0-9_\-\./]+))");
            command = captureFirst(message, pathRegex);
        }

        if (command.isEmpty()) {
            return QString();
        }

        const QString commandName = QFileInfo(command).fileName();
        return commandName.isEmpty() ? command : commandName;
    }

    QString extractUnlockTarget(const QString& text) {
        const QString normalized = normalizeDetailText(text);
        if (normalized.isEmpty()) {
            return QString();
        }

        const QRegularExpression unlockRegex(R"(unlock\s+([^\n]+))", QRegularExpression::CaseInsensitiveOption);
        QString                  target = captureFirst(normalized, unlockRegex);
        if (target.isEmpty()) {
            return QString();
        }

        target = target.trimmed();
        if (target.endsWith('.')) {
            target.chop(1);
        }

        return target.trimmed();
    }

    QString extractUnlockTargetFromContext(const QJsonObject& context) {
        QString target = extractUnlockTarget(context.value("keyringName").toString());
        if (target.isEmpty()) {
            target = extractUnlockTarget(context.value("message").toString());
        }
        if (target.isEmpty()) {
            target = extractUnlockTarget(context.value("description").toString());
        }
        return target;
    }

    QString buildUnlockDetails(const QJsonObject& context, const QString& target) {
        QStringList       details;

        const QStringList candidates = {normalizeDetailText(context.value("description").toString()), normalizeDetailText(context.value("message").toString()),
                                        normalizeDetailText(context.value("keyringName").toString())};

        for (const QString& candidate : candidates) {
            if (candidate.isEmpty()) {
                continue;
            }

            const QStringList lines = candidate.split('\n');
            for (const QString& rawLine : lines) {
                const QString line = rawLine.trimmed();
                if (line.isEmpty() || isTemplateUnlockLine(line, target)) {
                    continue;
                }
                details << line;
            }
        }

        return uniqueJoined(details);
    }

} // namespace bb::fallback::prompt
