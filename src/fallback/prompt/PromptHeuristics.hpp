#pragma once

#include <QString>

namespace bb::fallback::prompt {

    bool looksLikeFingerprintPrompt(const QString& text);
    bool looksLikeFidoPrompt(const QString& text);
    bool looksLikeTouchPrompt(const QString& text);

} // namespace bb::fallback::prompt
