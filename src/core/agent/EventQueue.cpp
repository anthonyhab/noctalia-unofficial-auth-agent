#include "EventQueue.hpp"

namespace bb::agent {

    EventQueue::EventQueue(int maxSize) : m_maxSize(maxSize) {}

    bool EventQueue::isEmpty() const {
        return m_eventQueue.isEmpty();
    }

    bool EventQueue::hasEvents() const {
        return !m_eventQueue.isEmpty();
    }

    QJsonObject EventQueue::takeNext() {
        return m_eventQueue.isEmpty() ? QJsonObject{} : m_eventQueue.takeFirst();
    }

    void EventQueue::enqueue(const QJsonObject& event) {
        if (m_eventQueue.size() >= m_maxSize) {
            m_eventQueue.dequeue();
        }
        m_eventQueue.enqueue(event);
    }

    void EventQueue::subscribeNext(QLocalSocket* socket) {
        m_nextWaiters.append(socket);
    }

    void EventQueue::removeWaiter(QLocalSocket* socket) {
        m_nextWaiters.removeAll(socket);
    }

} // namespace bb::agent
