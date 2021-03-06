#ifndef __PHYLUM_SUPER_BLOCK_MANAGER_H_INCLUDED
#define __PHYLUM_SUPER_BLOCK_MANAGER_H_INCLUDED

#undef min
#undef max
#include <functional>

#include "phylum/backend.h"
#include "phylum/private.h"
#include "phylum/block_alloc.h"
#include "phylum/visitor.h"

namespace phylum {

struct SuperBlockLink {
    BlockHead header;
    sector_index_t sector{ 0 };
    block_index_t chained_block{ 0 };

    SuperBlockLink(BlockType type = BlockType::SuperBlockLink) : header(type) {
    }
};

struct MinimumSuperBlock {
    SuperBlockLink link{ BlockType::SuperBlock };
};

class SuperBlockManager {
private:
    static constexpr block_index_t AnchorBlocks[] = { 1, 2 };
    // TODO: Store more than one super block in a sector?
    SectorAddress location_;
    StorageBackend *storage_;
    ReusableBlockAllocator *blocks_;

public:
    SuperBlockManager(StorageBackend &storage, ReusableBlockAllocator &blocks);

public:
    SectorAddress location() {
        return location_;
    }

public:
    bool locate(MinimumSuperBlock &sb, size_t size);
    bool create(MinimumSuperBlock &sb, size_t size);
    bool create(MinimumSuperBlock &sb, size_t size, std::function<void()> update);
    bool save(MinimumSuperBlock &sb, size_t size);
    bool walk(BlockVisitor *visitor);

private:
    struct PendingWrite {
        BlockType type;
        MinimumSuperBlock *ptr;
        size_t n;
    };

    int32_t chain_length();
    bool walk(block_index_t desired, SuperBlockLink &link, SectorAddress &where, BlockVisitor *visitor);
    bool find_link(block_index_t block, SuperBlockLink &found, SectorAddress &where);
    bool rollover(SectorAddress addr, SectorAddress &new_location, PendingWrite write);
    bool read(SectorAddress addr, SuperBlockLink &link);
    bool write(SectorAddress addr, SuperBlockLink &link);
    bool write(SectorAddress addr, PendingWrite write);

};

}

#endif
