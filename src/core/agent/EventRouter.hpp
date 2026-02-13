#pragma once

#include "EventQueue.hpp"
#include "ProviderRegistry.hpp"

#include <QJsonObject>
#include <QList>
#include <QLocalSocket>

namespace bb::agent {

    class EventRouter {
      public:
        EventRouter(ProviderRegistry& providerRegistry, EventQueue& eventQueue);

        template <typename SendFn>
        void route(const QJsonObject& event, const QList<QLocalSocket*>& subscribers, SendFn sendFn) {
            if (isSessionEventForProviderRouting(event) && m_providerRegistry.hasActiveProvider()) {
                QLocalSocket* activeProvider = m_providerRegistry.activeProvider();
                if (activeProvider && activeProvider->isValid()) {
                    sendFn(activeProvider, event);
                }
            } else {
                for (QLocalSocket* subscriber : subscribers) {
                    if (subscriber && subscriber->isValid()) {
                        sendFn(subscriber, event);
                    }
                }
            }

            m_eventQueue.enqueue(event);
            m_eventQueue.drainToWaiters(sendFn);
        }

      private:
        bool              isSessionEventForProviderRouting(const QJsonObject& event) const;

        ProviderRegistry& m_providerRegistry;
        EventQueue&       m_eventQueue;
    };

} // namespace bb::agent
