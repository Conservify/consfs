#include "phylum/phylum.h"
#include "phylum/private.h"
#include "phylum/super_block_manager.h"

using namespace alogging;

namespace phylum {

constexpr uint16_t SuperBlockStartSector = 0;
constexpr block_index_t SuperBlockManager::AnchorBlocks[];

SuperBlockManager::SuperBlockManager(StorageBackend &storage, ReusableBlockAllocator &blocks) :
    storage_(&storage), blocks_(&blocks) {
}

bool SuperBlockManager::walk(block_index_t desired, SuperBlockLink &link, SectorAddress &where, BlockVisitor *visitor) {
    link = { };
    where.invalid();

    // Find link in anchor block so we can follow the chain from there.
    for (auto block : AnchorBlocks) {
        if (!find_link(block, link, where)) {
            return false;
        }
    }

    // No link in the anchor area!
    if (!where.valid()) {
        sdebug() << "SuperBlockManager::walk: No link in anchor" << endl;
        return false;
    }

    // See if we're being asked for the child of an anchor block.
    if (desired != BLOCK_INDEX_INVALID && link.chained_block == desired) {
        return true;
    }

    for (auto i = 0; i < chain_length() + 1; ++i) {
        if (visitor != nullptr) {
            auto info = VisitInfo{ link.chained_block, 0 };
            visitor->block(info);
        }

        if (!find_link(link.chained_block, link, where)) {
            return false;
        }

        if (where.valid()) {
            if (link.chained_block == desired) {
                return true;
            }
        }
        else {
            break;
        }
    }

    sdebug() << "SuperBlockManager::walk: Failed to find" << endl;

    return false;
}

bool SuperBlockManager::locate(MinimumSuperBlock &sb, size_t size) {
    SuperBlockLink link;
    SectorAddress where;

    location_.invalid();

    if (!walk(BLOCK_INDEX_INVALID, link, where, nullptr)) {
        sdebug() << "SuperBlockManager::walk failed." << endl;
        return false;
    }

    location_ = where;

    if (!storage_->read({ storage_->geometry(), location_, 0 }, &sb, size)) {
        sdebug() << "SuperBlockManager::read_super failed." << endl;
        return false;
    }

    return true;
}

bool SuperBlockManager::walk(BlockVisitor *visitor) {
    SuperBlockLink link;
    SectorAddress where;

    if (!walk(BLOCK_INDEX_INVALID, link, where, visitor)) {
        sdebug() << "SuperBlockManager::walk failed." << endl;
        return false;
    }

    return true;
}

bool SuperBlockManager::find_link(block_index_t block, SuperBlockLink &found, SectorAddress &where) {
    for (auto s = SuperBlockStartSector; s < storage_->geometry().sectors_per_block(); ++s) {
        SuperBlockLink link;

        SectorAddress addr{ block, s };

        if (!read(addr, link)) {
            sdebug() << "Read failed: " << addr << endl;
            return false;
        }

        if (link.header.magic.valid()) {
            // NOTE: This first test serves two purposes. It ensures an
            // uninitialized found gets set to the first one we come across and
            // also makes the find_link work when a wrap around has occured. In
            // that case we automatically select a block that was prceeded by
            // the maximum value.
            if (found.header.timestamp == TIMESTAMP_INVALID || link.header.timestamp > found.header.timestamp) {
                found = link;
                where = addr;
            }
        }
        else {
            break;
        }
    }

    return true;
}

bool SuperBlockManager::create(MinimumSuperBlock &sb, size_t size) {
    return create(sb, size, [] {});
}

bool SuperBlockManager::create(MinimumSuperBlock &sb, size_t size, std::function<void()> update) {
    block_index_t super_block_block = BLOCK_INDEX_INVALID;
    SuperBlockLink link;
    link.chained_block = BLOCK_INDEX_INVALID;
    link.header.magic.fill();
    link.header.timestamp = chain_length() + 2 + 1;
    link.header.age = 0;

    for (auto i = 0; i < chain_length() + 1; ++i) {
        auto alloc = blocks_->allocate(i == 0 ? BlockType::SuperBlock : BlockType::SuperBlockLink);
        auto block = alloc.block;
        assert(block != BLOCK_INDEX_INVALID);

        if (!storage_->erase(block)) {
            sdebug() << "Erase failed: " << block << endl;
            return false;
        }

        // First of these blocks is actually where the super block goes.
        if (i == 0) {
            super_block_block = block;
            sb.link = link;
            sb.link.header.type = BlockType::SuperBlock;
        }
        else {
            if (!write({ block, SuperBlockStartSector }, link)) {
                sdebug() << "Write failed: " << block << endl;
                return false;
            }
        }

        link.chained_block = block;
        link.header.timestamp--;

        sdebug() << "Write block: " << block << endl;
    }

    // Overwrite both so an older one doesn't confuse us.
    for (auto anchor : AnchorBlocks) {
        link.header.type = BlockType::Anchor;

        if (!storage_->erase(anchor)) {
            sdebug() << "Erase failed: " << anchor << endl;
            return false;
        }

        if (!write({ anchor, SuperBlockStartSector }, link)) {
            sdebug() << "Write failed: " << anchor << endl;
            return false;
        }

        link.header.timestamp--;

        sdebug() << "Write anchor: " << anchor << endl;
    }

    update();

    SectorAddress addr = { super_block_block, SuperBlockStartSector };
    if (!storage_->write({ storage_->geometry(), addr, 0 }, &sb, size)) {
        sdebug() << "Write failed: " << addr << endl;
        return false;
    }

    sdebug() << "Create done, locating: " << addr << endl;
    if (locate(sb, size)) {
        return true;
    }

    sdebug() << "Yikes, erasing everything." << endl;
    storage_->eraseAll();
    sdebug() << "Done." << endl;
    return false;
}

bool SuperBlockManager::rollover(SectorAddress addr, SectorAddress &relocated, PendingWrite pending) {
    // Move to the following sector and see if we need to perform the rollover.
    addr.sector++;

    if (addr.sector < storage_->geometry().sectors_per_block()) {
        relocated = addr;
        return write(relocated, pending);
    }

    // We rollover the anchor blocks in a unique way.
    constexpr auto number_of_anchors = (int32_t)(sizeof(AnchorBlocks) / sizeof(AnchorBlocks[0]));
    for (auto i = 0; i < number_of_anchors; ++i) {
        if (AnchorBlocks[i] == addr.block) {
            relocated = {
                AnchorBlocks[(i + 1) % number_of_anchors],
                SuperBlockStartSector
            };

            if (!storage_->erase(relocated.block)) {
                return false;
            }

            return write(relocated, pending);
        }
    }

    auto alloc = blocks_->allocate(pending.type);
    auto block = alloc.block;
    relocated = { block, SuperBlockStartSector };
    if (!alloc.erased) {
        if (!storage_->erase(block)) {
            return false;
        }
    }

    pending.ptr->link.header.age = alloc.age + 1;

    if (!write(relocated, pending)) {
        return false;
    }

    // Find the chain link that references this now obsolete location.
    MinimumSuperBlock msb;
    SectorAddress previous;
    if (!walk(addr.block, msb.link, previous, nullptr)) {
        return false;
    }

    msb.link.header.timestamp++;
    msb.link.chained_block = block;

    auto link_write = PendingWrite {
        BlockType::SuperBlockLink,
        &msb,
        sizeof(SuperBlockLink)
    };

    SectorAddress actually_wrote;
    if (!rollover(previous, actually_wrote, link_write)) {
        return false;
    }

    blocks_->free(addr.block, msb.link.header.age);

    return true;
}

bool SuperBlockManager::save(MinimumSuperBlock &sb, size_t size) {
    sb.link.header.timestamp++;
    sb.link.header.fill();

    assert(location_.valid());

    auto write = PendingWrite{ BlockType::SuperBlock, &sb, size };

    SectorAddress actually_wrote;
    if (!rollover(location_, actually_wrote, write)) {
        return false;
    }

    location_ = actually_wrote;

    return true;
}

int32_t SuperBlockManager::chain_length() {
    return 2;
}

bool SuperBlockManager::read(SectorAddress addr, SuperBlockLink &link) {
    return storage_->read({ storage_->geometry(), addr, 0 }, &link, sizeof(SuperBlockLink));
}

bool SuperBlockManager::write(SectorAddress addr, SuperBlockLink &link) {
    return storage_->write({ storage_->geometry(), addr, 0 }, &link, sizeof(SuperBlockLink));
}

bool SuperBlockManager::write(SectorAddress addr, PendingWrite write) {
    return storage_->write({ storage_->geometry(), addr, 0 }, write.ptr, write.n);
}

}

namespace std {

void __throw_bad_function_call() {
    loginfof("Assert", "std::__throw_bad_function_call");
    while (true) {
    }
}

}
