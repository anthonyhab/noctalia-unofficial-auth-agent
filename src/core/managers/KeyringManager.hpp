#pragma once

#include "RequestTypes.hpp"
#include "../RequestContext.hpp"

#include <QHash>
#include <QObject>

#include <optional>

namespace noctalia {

    class KeyringManager : public QObject {
        Q_OBJECT

      public:
        explicit KeyringManager(QObject* parent = nullptr);

        // Process an incoming keyring request
        void handleRequest(const QJsonObject& msg, QLocalSocket* socket, pid_t peerPid);

        // Process a response to a pending request
        // Returns responseJson to be sent to the socket
        QJsonObject handleResponse(const QString& cookie, const QString& response);

        // Process a cancellation
        QJsonObject handleCancel(const QString& cookie);

        // Check if a cookie belongs to this manager
        bool hasPendingRequest(const QString& cookie) const;

        // Get the socket for a pending request (for sending response)
        QLocalSocket* getSocketForRequest(const QString& cookie) const;

        // Clean up requests for a disconnected socket
        void cleanupForSocket(QLocalSocket* socket);

      private:
        QHash<QString, KeyringRequest> m_pendingRequests;
    };

} // namespace noctalia
