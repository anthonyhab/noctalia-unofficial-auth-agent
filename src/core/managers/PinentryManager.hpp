#pragma once

#include "RequestTypes.hpp"
#include "../RequestContext.hpp"

#include <QHash>
#include <QObject>
#include <QTimer>
#include <QSharedPointer>

#include <optional>

namespace noctalia {

    class PinentryManager : public QObject {
        Q_OBJECT

      public:
        explicit PinentryManager(QObject* parent = nullptr);
        ~PinentryManager() override;

        // Process incoming pinentry request
        void handleRequest(const QJsonObject& msg, QLocalSocket* socket, pid_t peerPid);

        // Process response - returns socket response
        struct ResponseResult {
            QJsonObject                 socketResponse;
            bool                        deferred = false;
        };
        ResponseResult handleResponse(const QString& cookie, const QString& response);

        // Process cancellation
        QJsonObject handleCancel(const QString& cookie);

        // Check for retry
        bool checkForRetry(const QString& keyinfo);

        // Check ownership
        bool          hasPendingRequest(const QString& cookie) const;
        QLocalSocket* getSocketForRequest(const QString& cookie) const;

        // Cleanup
        void cleanupForSocket(QLocalSocket* socket);

      signals:
        // Emitted when a deferred completion fires
        void deferredComplete(const QString& cookie, const QJsonObject& event);

      private:
        struct DeferredCompletion {
            QString                cookie;
            QString                keyinfo;
            QJsonObject            event;
            QSharedPointer<QTimer> timer;
        };

        void                               sendDeferredCompletion(const QString& cookie);

        QHash<QString, PinentryRequest>    m_pendingRequests;
        QHash<QString, DeferredCompletion> m_deferredCompletions; // keyed by keyinfo
        QHash<QString, PinentryRetryInfo>  m_retryInfo;           // keyed by keyinfo
        QHash<QLocalSocket*, QString>      m_socketKeyinfos;      // track keyinfo per socket
    };

} // namespace noctalia
