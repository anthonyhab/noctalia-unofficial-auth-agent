#pragma once

#include <QString>

namespace bb::fallback::prompt {

    enum class PromptIntent {
        Generic,
        Unlock,
        RunCommand,
        OpenPgp,
        Fingerprint,
        Fido2
    };

    struct PromptDisplayModel {
        PromptIntent intent = PromptIntent::Generic;
        QString      title;
        QString      summary;
        QString      requestor;
        QString      details;
        QString      prompt;
        bool         passphrasePrompt   = false;
        bool         allowEmptyResponse = false;
    };

} // namespace bb::fallback::prompt
