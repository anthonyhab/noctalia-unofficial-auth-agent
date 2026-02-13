#include "EventRouter.hpp"

#include "EventQueue.hpp"
#include "ProviderRegistry.hpp"

namespace bb::agent {

    EventRouter::EventRouter(ProviderRegistry& providerRegistry, EventQueue& eventQueue) : m_providerRegistry(providerRegistry), m_eventQueue(eventQueue) {}

    bool EventRouter::isSessionEventForProviderRouting(const QJsonObject& event) const {
        const QString type = event.value("type").toString();
        return type.startsWith("session.");
    }

} // namespace bb::agent
