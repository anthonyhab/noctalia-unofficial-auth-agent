#include "keyring.hpp"

// C entry point from keyring-prompter code
extern "C" {
    int bb_auth_keyring_main(int argc, char* argv[]);
}

namespace modes {

int runKeyring(int argc, char* argv[]) {
    return bb_auth_keyring_main(argc, argv);
}

} // namespace modes
