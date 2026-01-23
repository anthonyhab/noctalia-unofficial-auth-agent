#include "PinentryManager.hpp"
#include "../Agent.hpp"
#include "../../common/Constants.hpp"

#include <QJsonDocument>
#include <QRegularExpression>
#include <QUuid>

namespace noctalia {

    PinentryManager::PinentryManager(QObject* parent) : QObject(parent) {}

    PinentryManager::~PinentryManager() = default;

    void PinentryManager::handleRequest(const QJsonObject& msg, QLocalSocket* socket, pid_t peerPid) {
        QString cookie = msg.value("cookie").toString();
        if (cookie.isEmpty()) {
            cookie = QUuid::createUuid().toString(QUuid::WithoutBraces);
        }

        PinentryRequest request;
        request.cookie      = cookie;
        request.socket      = socket;
        request.peerPid     = peerPid;
        request.prompt      = msg.value("prompt").toString();
        request.description = msg.value("description").toString();
        request.error       = msg.value("error").toString();
        request.keyinfo     = msg.value("keyinfo").toString();
        request.repeat      = msg.value("repeat").toBool();
        request.confirmOnly = msg.value("confirm_only").toBool();

        // Track keyinfo for this socket
        if (!request.keyinfo.isEmpty()) {
            m_socketKeyinfos[socket] = request.keyinfo;

            // Check if this is a retry
            checkForRetry(request.keyinfo);
        }

        m_pendingRequests[cookie] = request;

        // Resolve requestor
        std::optional<ProcInfo> proc = RequestContextHelper::readProc(peerPid);
        ActorInfo               actor;
        if (proc) {
            actor = RequestContextHelper::resolveRequestorFromSubject(*proc, getuid());
        }

        // Parse retry info from description if present
        int                             curRetry = 0, maxRetries = 0;
        static const QRegularExpression retryRe(R"(\((\d+)\s+of\s+(\d+)\s+attempts\))");
        auto                            match = retryRe.match(request.description);
        if (match.hasMatch()) {
            curRetry   = match.captured(1).toInt();
            maxRetries = match.captured(2).toInt();

            // Update retry info
            if (!request.keyinfo.isEmpty()) {
                auto& info      = m_retryInfo[request.keyinfo];
                info.keyinfo    = request.keyinfo;
                info.curRetry   = curRetry;
                info.maxRetries = maxRetries;
            }
        }

        noctalia::Session::Context ctx;
        ctx.message = request.prompt;
        ctx.description = request.description;
        ctx.keyinfo = request.keyinfo;
        ctx.curRetry = curRetry > 0 ? curRetry : (request.keyinfo.isEmpty() ? 0 : m_retryInfo[request.keyinfo].curRetry);
        ctx.maxRetries = maxRetries > 0 ? maxRetries : 3;
        ctx.confirmOnly = request.confirmOnly;
        ctx.repeat = request.repeat;
        ctx.requestor.name = actor.displayName;
        ctx.requestor.icon = actor.iconName;
        ctx.requestor.fallbackLetter = actor.fallbackLetter;
        ctx.requestor.pid = peerPid;

        // Use centralized session management
        g_pAgent->createSession(cookie, noctalia::Session::Source::Pinentry, ctx);
        g_pAgent->updateSessionPrompt(cookie, request.prompt, false);
        if (!request.error.isEmpty()) {
            g_pAgent->updateSessionError(cookie, request.error);
        }
    }

    PinentryManager::ResponseResult PinentryManager::handleResponse(const QString& cookie, const QString& response) {
        auto it = m_pendingRequests.find(cookie);
        if (it == m_pendingRequests.end()) {
            return {QJsonObject{{"type", "error"}, {"message", "Unknown cookie"}}, false};
        }

        PinentryRequest request = it.value();
        m_pendingRequests.erase(it);

        // Build response in format expected by pinentry.cpp
        QJsonObject socketResponse;
        socketResponse["type"] = "pinentry_response";
        socketResponse["id"] = cookie;
        if (request.confirmOnly) {
            socketResponse["result"] = "confirmed";
        } else {
            socketResponse["result"] = "ok";
            socketResponse["password"] = response;
        }
        
        // Defer completion if we have keyinfo (to detect retries)
        if (!request.keyinfo.isEmpty()) {
            auto& deferred   = m_deferredCompletions[request.keyinfo];
            deferred.cookie  = cookie;
            deferred.keyinfo = request.keyinfo;

            // Close session but defer emitting the event
            deferred.event = g_pAgent->closeSession(cookie, noctalia::Session::Result::Success, true);

            deferred.timer   = QSharedPointer<QTimer>::create();
            deferred.timer->setSingleShot(true);

            // Determine delay based on retry status
            int  delay   = PINENTRY_DEFERRED_DELAY_MS;
            auto retryIt = m_retryInfo.find(request.keyinfo);
            if (retryIt != m_retryInfo.end() && retryIt->curRetry > 0) {
                delay = PINENTRY_DEFERRED_DELAY_RETRY_MS;
            }

            connect(deferred.timer.get(), &QTimer::timeout, this, [this, cookie]() { sendDeferredCompletion(cookie); });
            deferred.timer->start(delay);

            return {socketResponse, true};
        }

        // No keyinfo - close immediately
        g_pAgent->closeSession(cookie, noctalia::Session::Result::Success);

        return {socketResponse, false};
    }

    QJsonObject PinentryManager::handleCancel(const QString& cookie) {
        auto it = m_pendingRequests.find(cookie);
        if (it == m_pendingRequests.end()) {
            return QJsonObject{{"type", "error"}, {"message", "Unknown cookie"}};
        }

        m_pendingRequests.erase(it);

        // Close session via Agent
        g_pAgent->closeSession(cookie, noctalia::Session::Result::Cancelled);

        return QJsonObject{{"type", "pinentry_response"}, {"id", cookie}, {"result", "cancelled"}};
    }

    bool PinentryManager::checkForRetry(const QString& keyinfo) {
        auto it = m_deferredCompletions.find(keyinfo);
        if (it == m_deferredCompletions.end()) {
            return false;
        }

        // Cancel the deferred completion - this is a retry
        it->timer->stop();
        m_deferredCompletions.erase(it);

        // Update retry counter
        auto& retry   = m_retryInfo[keyinfo];
        retry.keyinfo = keyinfo;
        retry.curRetry++;

        return true;
    }

    bool PinentryManager::hasPendingRequest(const QString& cookie) const {
        return m_pendingRequests.contains(cookie);
    }

    QLocalSocket* PinentryManager::getSocketForRequest(const QString& cookie) const {
        auto it = m_pendingRequests.find(cookie);
        return (it != m_pendingRequests.end()) ? it->socket : nullptr;
    }

    void PinentryManager::cleanupForSocket(QLocalSocket* socket) {
        // Clean pending requests
        for (auto it = m_pendingRequests.begin(); it != m_pendingRequests.end();) {
            if (it->socket == socket) {
                QString cookie = it->cookie;
                it = m_pendingRequests.erase(it);

                // Close session via Agent
                g_pAgent->closeSession(cookie, noctalia::Session::Result::Cancelled);
            } else {
                ++it;
            }
        }

        // Clean socket keyinfo tracking
        auto keyinfoIt = m_socketKeyinfos.find(socket);
        if (keyinfoIt != m_socketKeyinfos.end()) {
            const QString keyinfo = keyinfoIt.value();
            m_socketKeyinfos.erase(keyinfoIt);

            // If there's a deferred completion for this keyinfo, emit session.closed now
            // (the pinentry client disconnected before the deferred timer fired)
            auto deferredIt = m_deferredCompletions.find(keyinfo);
            if (deferredIt != m_deferredCompletions.end()) {
                deferredIt->timer->stop();
                g_pAgent->emitSessionEvent(deferredIt->event);
                m_deferredCompletions.erase(deferredIt);
            }
        }
    }

    void PinentryManager::sendDeferredCompletion(const QString& cookie) {
        // Find by cookie in deferred completions
        for (auto it = m_deferredCompletions.begin(); it != m_deferredCompletions.end(); ++it) {
            if (it->cookie == cookie) {
                g_pAgent->emitSessionEvent(it->event);
                m_deferredCompletions.erase(it);
                return;
            }
        }
    }

} // namespace noctalia
