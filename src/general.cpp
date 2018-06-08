#include "phylum/phylum.h"
#include "phylum/private.h"

namespace phylum {

#ifndef ARDUINO
std::ostream &sdebug = std::cout;
#endif

constexpr char BlockMagic::MagicKey[];

void BlockMagic::fill() {
    memcpy(key, MagicKey, sizeof(MagicKey));
}

bool BlockMagic::valid() const {
    return memcmp(key, MagicKey, sizeof(MagicKey)) == 0;
}

}
