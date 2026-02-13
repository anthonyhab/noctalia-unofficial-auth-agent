#pragma once

#include <QHash>
#include <QJsonObject>
#include <QString>

#include <functional>

class QLocalSocket;

namespace bb::agent {

    class MessageRouter {
      public:
        using HandlerFn = std::function<void(QLocalSocket*, const QJsonObject&)>;

        void registerHandler(const QString& type, HandlerFn handler);
        bool dispatch(QLocalSocket* socket, const QString& type, const QJsonObject& msg) const;

      private:
        QHash<QString, HandlerFn> m_handlers;
    };

} // namespace bb::agent
