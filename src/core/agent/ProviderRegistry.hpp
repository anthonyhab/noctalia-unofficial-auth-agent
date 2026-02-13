#pragma once

#include <QHash>
#include <QJsonObject>
#include <QPointer>
#include <functional>

class QLocalSocket;

namespace bb::agent {

    struct UIProvider {
        QString id;
        QString name;
        QString kind;
        int     priority        = 0;
        qint64  lastHeartbeatMs = 0;
    };

    class ProviderRegistry {
      public:
        using NowFn = std::function<qint64()>;

        ProviderRegistry();
        explicit ProviderRegistry(NowFn nowFn);

        UIProvider           registerProvider(QLocalSocket* socket, const QJsonObject& msg);
        bool                 heartbeat(QLocalSocket* socket);
        bool                 unregisterProvider(QLocalSocket* socket);
        bool                 removeSocket(QLocalSocket* socket);
        bool                 recomputeActiveProvider();
        bool                 pruneStale();

        bool                 isAuthorized(QLocalSocket* socket) const;
        bool                 hasActiveProvider() const;

        QLocalSocket*        activeProvider() const;
        const UIProvider*    activeProviderInfo() const;
        const UIProvider*    provider(QLocalSocket* socket) const;
        bool                 contains(QLocalSocket* socket) const;
        QList<QLocalSocket*> sockets() const;

      private:
        NowFn                            m_nowFn;

        QHash<QLocalSocket*, UIProvider> m_uiProviders;
        QPointer<QLocalSocket>           m_activeProvider;
    };

} // namespace bb::agent
