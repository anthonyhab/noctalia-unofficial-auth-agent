#pragma once

#include <QJsonObject>
#include <QLocalSocket>
#include <QString>

namespace bb {

    // Base information common to all request types
    struct BaseRequest {
        QString       cookie;
        QLocalSocket* socket  = nullptr;
        pid_t         peerPid = -1;
    };

    struct KeyringRequest : BaseRequest {
        QString title;   // Replaces 'prompt' in plan to match client protocol
        QString message; // Replaces 'message' in plan (body text)
        QString choice;
        int     flags = 0;
    };

    struct PinentryRequest : BaseRequest {
        QString prompt;
        QString description;
        QString error;
        QString keyinfo;
        bool    repeat      = false;
        bool    confirmOnly = false;
    };

    // Retry tracking for pinentry
    struct PinentryRetryInfo {
        QString keyinfo;
        int     curRetry   = 0;
        int     maxRetries = 0;
    };

    // Event types for the UI queue
    struct RequestEvent {
        QString     type; // "request", "error", "complete"
        QJsonObject data;
    };

} // namespace bb
