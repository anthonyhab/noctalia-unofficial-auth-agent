#pragma once

#include <QString>

namespace bb {

    // Returns the default socket path: $XDG_RUNTIME_DIR/bb-auth.sock
    QString socketPath();

} // namespace bb
