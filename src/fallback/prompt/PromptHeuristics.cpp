#include "PromptHeuristics.hpp"

namespace bb::fallback::prompt {

    namespace {

        bool containsAnyTerm(const QString& text, std::initializer_list<const char*> terms) {
            const QString lower = text.toLower();
            for (const char* term : terms) {
                if (lower.contains(QString::fromLatin1(term))) {
                    return true;
                }
            }
            return false;
        }

    } // namespace

    bool looksLikeFingerprintPrompt(const QString& text) {
        return containsAnyTerm(text, {"fingerprint", "finger print", "fprint", "swipe", "scan your finger"});
    }

    bool looksLikeFidoPrompt(const QString& text) {
        return containsAnyTerm(text, {"fido", "fido2", "webauthn", "security key", "yubikey", "hardware token", "user presence"});
    }

    bool looksLikeTouchPrompt(const QString& text) {
        return containsAnyTerm(text, {"touch", "tap", "insert", "use your security key", "verify your identity"});
    }

} // namespace bb::fallback::prompt
