#include "IpcServer.hpp"
#include "../../common/Constants.hpp"

#include <QFile>
#include <QJsonDocument>
#include <QJsonParseError>

#include <sys/socket.h>
#include <cstring>

namespace noctalia {

namespace {

void secureZero(void* ptr, std::size_t size) {
    volatile unsigned char* p = static_cast<volatile unsigned char*>(ptr);
    while (size--) {
        *p++ = 0;
    }
}

} // namespace

IpcServer::IpcServer(QObject* parent)
    : QObject(parent)
{}

IpcServer::~IpcServer() {
    stop();
}

bool IpcServer::start(const QString& socketPath) {
    if (m_server)
        return false;

    // Remove stale socket file
    if (QFile::exists(socketPath)) {
        QFile::remove(socketPath);
    }

    m_server = new QLocalServer(this);
    m_server->setSocketOptions(QLocalServer::UserAccessOption);

    if (!m_server->listen(socketPath)) {
        delete m_server;
        m_server = nullptr;
        return false;
    }

    connect(m_server, &QLocalServer::newConnection, this, &IpcServer::onNewConnection);
    return true;
}

void IpcServer::stop() {
    if (!m_server)
        return;

    // Disconnect all clients
    for (auto* socket : m_buffers.keys()) {
        socket->disconnectFromServer();
    }
    m_buffers.clear();

    m_server->close();
    delete m_server;
    m_server = nullptr;
}

void IpcServer::setMessageHandler(MessageHandler handler) {
    m_handler = std::move(handler);
}

void IpcServer::sendJson(QLocalSocket* socket, const QJsonObject& json, bool secureWipe) {
    if (!socket || socket->state() != QLocalSocket::ConnectedState)
        return;

    QByteArray data = QJsonDocument(json).toJson(QJsonDocument::Compact);
    data.append('\n');

    socket->write(data);
    socket->flush();

    if (secureWipe) {
        secureZero(data.data(), static_cast<std::size_t>(data.size()));
    }
}

pid_t IpcServer::getPeerPid(QLocalSocket* socket) {
    if (!socket)
        return -1;

    struct ucred cred;
    socklen_t len = sizeof(cred);

    if (getsockopt(socket->socketDescriptor(), SOL_SOCKET, SO_PEERCRED, &cred, &len) == -1) {
        return -1;
    }

    return cred.pid;
}

void IpcServer::onNewConnection() {
    while (m_server->hasPendingConnections()) {
        QLocalSocket* socket = m_server->nextPendingConnection();
        if (!socket)
            continue;

        m_buffers[socket] = QByteArray();

        connect(socket, &QLocalSocket::readyRead, this, &IpcServer::onReadyRead);
        connect(socket, &QLocalSocket::disconnected, this, &IpcServer::onDisconnected);

        emit clientConnected(socket);
    }
}

void IpcServer::onReadyRead() {
    auto* socket = qobject_cast<QLocalSocket*>(sender());
    if (!socket)
        return;

    QByteArray& buffer = m_buffers[socket];
    buffer.append(socket->readAll());

    // Enforce max message size
    if (buffer.size() > static_cast<qsizetype>(MAX_MESSAGE_SIZE)) {
        socket->disconnectFromServer();
        return;
    }

    // Process complete lines
    int idx;
    while ((idx = buffer.indexOf('\n')) != -1) {
        QByteArray line = buffer.left(idx).trimmed();
        buffer.remove(0, idx + 1);

        if (!line.isEmpty()) {
            handleLine(socket, line);
        }
    }
}

void IpcServer::onDisconnected() {
    auto* socket = qobject_cast<QLocalSocket*>(sender());
    if (!socket)
        return;

    m_buffers.remove(socket);
    emit clientDisconnected(socket);

    socket->deleteLater();
}

void IpcServer::handleLine(QLocalSocket* socket, const QByteArray& line) {
    if (!m_handler)
        return;

    QJsonParseError parseError;
    const auto doc = QJsonDocument::fromJson(line, &parseError);

    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        sendJson(socket, QJsonObject{{"type", "error"}, {"message", "Invalid JSON"}});
        return;
    }

    const QJsonObject obj = doc.object();
    const QString type = obj.value("type").toString();

    if (type.isEmpty()) {
        sendJson(socket, QJsonObject{{"type", "error"}, {"message", "Missing type field"}});
        return;
    }

    m_handler(socket, type, obj);
}

} // namespace noctalia
