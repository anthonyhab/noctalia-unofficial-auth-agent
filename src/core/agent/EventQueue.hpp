#pragma once

#include <QJsonObject>
#include <QList>
#include <QQueue>

class QLocalSocket;

namespace bb::agent {

    class EventQueue {
      public:
        explicit EventQueue(int maxSize = 256);

        bool        isEmpty() const;
        bool        hasEvents() const;
        QJsonObject takeNext();

        void        enqueue(const QJsonObject& event);
        void        subscribeNext(QLocalSocket* socket);
        void        removeWaiter(QLocalSocket* socket);

        template <typename SendFn>
        void drainToWaiters(SendFn sendFn) {
            while (!m_nextWaiters.isEmpty() && !m_eventQueue.isEmpty()) {
                QLocalSocket* socket = m_nextWaiters.takeFirst();
                sendFn(socket, m_eventQueue.takeFirst());
            }
        }

      private:
        int                  m_maxSize;
        QQueue<QJsonObject>  m_eventQueue;
        QList<QLocalSocket*> m_nextWaiters;
    };

} // namespace bb::agent
