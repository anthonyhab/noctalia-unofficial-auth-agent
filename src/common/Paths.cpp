#include "Paths.hpp"

#include <QStandardPaths>

namespace bb {

    QString socketPath() {
        const auto runtimeDir = QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation);
        return runtimeDir + QStringLiteral("/bb-auth.sock");
    }

} // namespace bb
