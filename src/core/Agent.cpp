#include "Agent.hpp"
#include "../common/Constants.hpp"
#include "RequestContext.hpp"

#include <QCoreApplication>
#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusMetaType>
#include <QDBusReply>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QStandardPaths>
#include <QUuid>

#include <memory>
#include <pwd.h>
#include <sys/socket.h>
#include <unistd.h>

#include <print>

using namespace noctalia;

std::unique_ptr<CAgent> g_pAgent;

CAgent::CAgent(QObject* parent) : QObject(parent), m_listener(new CPolkitListener(this)) {
}

CAgent::~CAgent() {}

#include <PolkitQt1/Subject>

bool CAgent::start(QCoreApplication& app, const QString& socketPath) {
    PolkitQt1::UnixSessionSubject subject(getpid());
    if (!m_listener->registerListener(subject, "/org/kde/PolicyKit1/AuthenticationAgent")) {
        std::print(stderr, "Failed to register as Polkit agent listener\n");
        return false;
    }

    std::print("Polkit listener registered successfully\n");

    // Connect Polkit signals
    connect(m_listener.data(), &CPolkitListener::completed, this, &CAgent::onPolkitCompleted);

    // Connect Manager signals
    connect(&m_pinentryManager, &noctalia::PinentryManager::deferredComplete, this, [this](const QString&, const QJsonObject& event) {
        emitSessionEvent(event);
    });

    // Setup IPC server
    m_ipcServer.setMessageHandler([this](QLocalSocket* socket, const QString& type, const QJsonObject& msg) { handleMessage(socket, type, msg); });

    QObject::connect(&m_ipcServer, &noctalia::IpcServer::clientDisconnected, [this](QLocalSocket* socket) { onClientDisconnected(socket); });

    if (!m_ipcServer.start(socketPath)) {
        std::print(stderr, "Failed to start IPC server on {}\n", socketPath.toStdString());
        return false;
    }

    std::print("Agent started on {}\n", socketPath.toStdString());
    return app.exec() == 0;
}

void CAgent::onClientDisconnected(QLocalSocket* socket) {
    if (m_subscribers.removeOne(socket)) {
        qDebug() << "Subscriber removed, remaining:" << m_subscribers.size();
    }
    nextWaiters.removeAll(socket);
    m_keyringManager.cleanupForSocket(socket);
    m_pinentryManager.cleanupForSocket(socket);
}

void CAgent::handleMessage(QLocalSocket* socket, const QString& type, const QJsonObject& msg) {
    if (type == "ping") {
        m_ipcServer.sendJson(socket, QJsonObject{
            {"type", "pong"},
            {"version", "2.0"},
            {"capabilities", QJsonArray{"polkit", "keyring", "pinentry"}}
        });
    } else if (type == "subscribe") {
        handleSubscribe(socket);
    } else if (type == "next") {
        handleNext(socket);
    } else if (type == "keyring_request") {
        handleKeyringRequest(socket, msg);
    } else if (type == "pinentry_request") {
        handlePinentryRequest(socket, msg);
    } else if (type == "session.respond") {
        handleRespond(socket, msg);
    } else if (type == "session.cancel") {
        handleCancel(socket, msg);
    } else {
        m_ipcServer.sendJson(socket, QJsonObject{{"type", "error"}, {"message", "Unknown type"}});
    }
}

void CAgent::handleNext(QLocalSocket* socket) {
    if (eventQueue.isEmpty()) {
        nextWaiters.append(socket);
        return;
    }

    QJsonObject nextEvent = eventQueue.takeFirst();
    m_ipcServer.sendJson(socket, nextEvent);
}

void CAgent::handleSubscribe(QLocalSocket* socket) {
    if (!m_subscribers.contains(socket)) {
        m_subscribers.append(socket);
        qDebug() << "Subscriber added, total:" << m_subscribers.size();
    }

    // Sync active sessions
    for (const auto& [cookie, session] : m_sessions) {
        m_ipcServer.sendJson(socket, session->toCreatedEvent());
        m_ipcServer.sendJson(socket, session->toUpdatedEvent());
    }

    m_ipcServer.sendJson(socket, QJsonObject{
        {"type", "subscribed"},
        {"sessionCount", (int)m_sessions.size()}
    });
}

void CAgent::handleKeyringRequest(QLocalSocket* socket, const QJsonObject& msg) {
    pid_t peerPid = noctalia::IpcServer::getPeerPid(socket);
    m_keyringManager.handleRequest(msg, socket, peerPid);
}

void CAgent::handlePinentryRequest(QLocalSocket* socket, const QJsonObject& msg) {
    pid_t peerPid = noctalia::IpcServer::getPeerPid(socket);
    m_pinentryManager.handleRequest(msg, socket, peerPid);
}

void CAgent::handleRespond(QLocalSocket* socket, const QJsonObject& msg) {
    const QString cookie   = msg.value("id").toString();
    const QString response = msg.value("response").toString();

    if (m_keyringManager.hasPendingRequest(cookie)) {
        QLocalSocket* origSocket = m_keyringManager.getSocketForRequest(cookie);
        QJsonObject reply = m_keyringManager.handleResponse(cookie, response);
        if (origSocket) m_ipcServer.sendJson(origSocket, reply, true);
        m_ipcServer.sendJson(socket, QJsonObject{{"type", "ok"}});
        return;
    }

    if (m_pinentryManager.hasPendingRequest(cookie)) {
        QLocalSocket* origSocket = m_pinentryManager.getSocketForRequest(cookie);
        auto result = m_pinentryManager.handleResponse(cookie, response);
        if (origSocket) m_ipcServer.sendJson(origSocket, result.socketResponse, true);
        m_ipcServer.sendJson(socket, QJsonObject{{"type", "ok"}});
        return;
    }

    m_listener->submitPassword(cookie, response);
    m_ipcServer.sendJson(socket, QJsonObject{{"type", "ok"}});
}

void CAgent::handleCancel(QLocalSocket* socket, const QJsonObject& msg) {
    const QString cookie = msg.value("id").toString();

    if (m_keyringManager.hasPendingRequest(cookie)) {
        QLocalSocket* origSocket = m_keyringManager.getSocketForRequest(cookie);
        QJsonObject reply = m_keyringManager.handleCancel(cookie);
        if (origSocket) m_ipcServer.sendJson(origSocket, reply);
        m_ipcServer.sendJson(socket, QJsonObject{{"type", "ok"}});
        return;
    }

    if (m_pinentryManager.hasPendingRequest(cookie)) {
        QLocalSocket* origSocket = m_pinentryManager.getSocketForRequest(cookie);
        QJsonObject reply = m_pinentryManager.handleCancel(cookie);
        if (origSocket) m_ipcServer.sendJson(origSocket, reply);
        m_ipcServer.sendJson(socket, QJsonObject{{"type", "ok"}});
        return;
    }

    m_listener->cancelPending(cookie);
    m_ipcServer.sendJson(socket, QJsonObject{{"type", "ok"}});
}

void CAgent::emitSessionEvent(const QJsonObject& event) {
    qDebug() << "Broadcasting event:" << event["type"].toString()
             << "to" << m_subscribers.size() << "subscribers";

    for (QLocalSocket* subscriber : m_subscribers) {
        if (subscriber && subscriber->isValid()) {
            m_ipcServer.sendJson(subscriber, event);
        }
    }

    eventQueue.enqueue(event);
    processNextWaiter();
}

void CAgent::processNextWaiter() {
    while (!nextWaiters.isEmpty() && !eventQueue.isEmpty()) {
        QLocalSocket* socket = nextWaiters.takeFirst();
        handleNext(socket);
    }
}

void CAgent::onPolkitRequest(const QString& cookie, const QString& message,
                             const QString& iconName, const QString& actionId,
                             const QString& user, const PolkitQt1::Details& details) {
    qDebug() << "POLKIT REQUEST" << cookie;

    noctalia::Session::Context ctx;
    ctx.message = message;
    ctx.actionId = actionId;
    ctx.user = user;

    auto pid = RequestContextHelper::extractSubjectPid(details);
    if (pid) {
        auto proc = RequestContextHelper::readProc(*pid);
        if (proc) {
            auto actor = RequestContextHelper::resolveRequestorFromSubject(*proc, getuid());
            ctx.requestor.name = actor.displayName;
            ctx.requestor.icon = actor.iconName;
            ctx.requestor.fallbackLetter = actor.fallbackLetter;
            ctx.requestor.pid = *pid;
        }
    }

    if (ctx.requestor.name.isEmpty()) {
        ctx.requestor.name = "Unknown";
        ctx.requestor.fallbackLetter = "?";
    }

    createSession(cookie, noctalia::Session::Source::Polkit, ctx);
}

void CAgent::onSessionRequest(const QString& cookie, const QString& prompt, bool echo) {
    auto it = m_sessions.find(cookie);
    if (it == m_sessions.end()) {
        qWarning() << "Session not found:" << cookie;
        return;
    }
    
    it->second->setPrompt(prompt, echo);
    emitSessionEvent(it->second->toUpdatedEvent());
}

void CAgent::onSessionComplete(const QString& cookie, bool success) {
    auto it = m_sessions.find(cookie);
    if (it == m_sessions.end()) {
        qWarning() << "Session not found:" << cookie;
        return;
    }
    
    it->second->close(success ? noctalia::Session::Result::Success 
                              : noctalia::Session::Result::Cancelled);
    emitSessionEvent(it->second->toClosedEvent());
    m_sessions.erase(it);
}

void CAgent::onSessionRetry(const QString& cookie, const QString& error) {
    auto it = m_sessions.find(cookie);
    if (it == m_sessions.end()) return;
    
    it->second->setError(error);
    emitSessionEvent(it->second->toUpdatedEvent());
}

void CAgent::onPolkitCompleted(bool gainedAuthorization) {
}

// Centralized session management
void CAgent::createSession(const QString& id, Session::Source source, Session::Context ctx) {
    auto session = std::make_unique<noctalia::Session>(id, source, ctx);
    emitSessionEvent(session->toCreatedEvent());
    m_sessions[id] = std::move(session);
}

void CAgent::updateSessionPrompt(const QString& id, const QString& prompt, bool echo) {
    auto it = m_sessions.find(id);
    if (it == m_sessions.end()) {
        qWarning() << "updateSessionPrompt: Session not found:" << id;
        return;
    }
    it->second->setPrompt(prompt, echo);
    emitSessionEvent(it->second->toUpdatedEvent());
}

void CAgent::updateSessionError(const QString& id, const QString& error) {
    auto it = m_sessions.find(id);
    if (it == m_sessions.end()) {
        qWarning() << "updateSessionError: Session not found:" << id;
        return;
    }
    it->second->setError(error);
    emitSessionEvent(it->second->toUpdatedEvent());
}

QJsonObject CAgent::closeSession(const QString& id, Session::Result result, bool deferred) {
    auto it = m_sessions.find(id);
    if (it == m_sessions.end()) {
        qWarning() << "closeSession: Session not found:" << id;
        return QJsonObject{};
    }

    it->second->close(result);
    QJsonObject event = it->second->toClosedEvent();
    m_sessions.erase(it);

    if (!deferred) {
        emitSessionEvent(event);
        return QJsonObject{};
    }
    return event;
}

Session* CAgent::getSession(const QString& id) {
    auto it = m_sessions.find(id);
    return (it != m_sessions.end()) ? it->second.get() : nullptr;
}
