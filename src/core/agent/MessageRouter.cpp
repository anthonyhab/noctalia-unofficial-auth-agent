#include "MessageRouter.hpp"

namespace bb::agent {

    void MessageRouter::registerHandler(const QString& type, HandlerFn handler) {
        m_handlers.insert(type, std::move(handler));
    }

    bool MessageRouter::dispatch(QLocalSocket* socket, const QString& type, const QJsonObject& msg) const {
        auto it = m_handlers.constFind(type);
        if (it == m_handlers.constEnd()) {
            return false;
        }

        it.value()(socket, msg);
        return true;
    }

} // namespace bb::agent
