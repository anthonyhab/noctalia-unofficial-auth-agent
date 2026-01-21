#pragma once

#include <QLocalServer>
#include <QLocalSocket>
#include <QJsonObject>
#include <QObject>

#include <functional>

namespace noctalia {

// Callback type for handling parsed JSON messages
// Parameters: socket, message type, full JSON object
using MessageHandler = std::function<void(QLocalSocket*, const QString&, const QJsonObject&)>;

class IpcServer : public QObject {
    Q_OBJECT

public:
    explicit IpcServer(QObject* parent = nullptr);
    ~IpcServer() override;

    // Start listening on the given socket path
    // Returns false if binding fails
    bool start(const QString& socketPath);

    // Stop the server and disconnect all clients
    void stop();

    // Set the handler for incoming messages
    void setMessageHandler(MessageHandler handler);

    // Send a JSON response to a specific socket
    // If secureWipe is true, zeros the buffer after sending
    void sendJson(QLocalSocket* socket, const QJsonObject& json, bool secureWipe = false);

    // Get peer process ID for a connected socket
    // Returns -1 on failure
    static pid_t getPeerPid(QLocalSocket* socket);

signals:
    void clientConnected(QLocalSocket* socket);
    void clientDisconnected(QLocalSocket* socket);

private slots:
    void onNewConnection();
    void onReadyRead();
    void onDisconnected();

private:
    void handleLine(QLocalSocket* socket, const QByteArray& line);

    QLocalServer* m_server = nullptr;
    MessageHandler m_handler;
    QHash<QLocalSocket*, QByteArray> m_buffers;
};

} // namespace noctalia
