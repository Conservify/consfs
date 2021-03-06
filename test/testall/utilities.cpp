#include "utilities.h"

#include "phylum/persisted_tree.h"
#include "phylum/node_serializer.h"
#include "phylum/backend_nodes.h"
#include "phylum/layout.h"

using namespace alogging;

std::map<uint64_t, uint64_t> random_data() {
    std::map<uint64_t, uint64_t> data;

    for (auto i = 0; i < 32; ++i) {
        while (true) {
            auto raw = random() % 1024;
            if (raw > 0) {
                data[raw] = raw * 1024;
                break;
            }
        }
    }

    return data;
}

namespace phylum {

bool BlockHelper::is_type(block_index_t block, BlockType type) {
    BlockHead head;

    if (!storage_->read({ block, 0 }, &head, sizeof(BlockHead))) {
        return false;
    }

    if (!head.valid()) {
        return false;
    }

    return head.type == type;
}

void BlockHelper::dump(block_index_t first, block_index_t last) {
    for (auto block = first; block < last; ++block) {
        dump(block);
    }
}

void BlockHelper::live(std::map<block_index_t, std::vector<BlockAddress>> &live) {
    for (auto &p : live) {
        if (blocks.find(p.first) == blocks.end()) {
            blocks[p.first] = {};
        }
        for (auto &a : p.second) {
            blocks[p.first].live.push_back(a);
        }
    }
}

int32_t BlockHelper::number_of_chains(BlockType type, block_index_t first, block_index_t last) {
    auto &g = storage_->geometry();
    size_t c = 0;

    if (last == BLOCK_INDEX_INVALID) {
        last = g.number_of_blocks - 1;
    }

    for (auto block = first; block < last; ++block) {
        BlockHead head;
        BlockTail tail;

        auto head_addr = BlockAddress{ block, 0 };
        auto tail_addr = BlockAddress::tail_data_of(block, g, sizeof(BlockTail));

        storage_->read(head_addr, &head, sizeof(BlockHead));
        storage_->read(tail_addr, &tail, sizeof(BlockTail));

        if (head.type == type) {
            if (head.linked_block == BLOCK_INDEX_INVALID) {
                c++;
            }
        }
    }

    return c;
}

int32_t BlockHelper::number_of_blocks(BlockType type, block_index_t first, block_index_t last) {
    auto &g = storage_->geometry();
    size_t c = 0;

    if (last == BLOCK_INDEX_INVALID) {
        last = g.number_of_blocks - 1;
    }

    for (auto block = first; block < last; ++block) {
        BlockHead head;
        BlockTail tail;

        auto head_addr = BlockAddress{ block, 0 };
        auto tail_addr = BlockAddress::tail_data_of(block, g, sizeof(BlockTail));

        storage_->read(head_addr, &head, sizeof(BlockHead));
        storage_->read(tail_addr, &tail, sizeof(BlockTail));

        if (head.valid()) {
            if (head.type == type) {
                c++;
            }
        }
    }

    return c;
}

static inline BlockLayout<TreeBlockHead, TreeBlockTail> get_journal_layout(StorageBackend &storage,
              BlockAllocator &allocator, BlockAddress address) {
    return { storage, allocator, address, BlockType::Error };
}

static inline BlockLayout<TreeBlockHead, TreeBlockTail> get_tree_layout(StorageBackend &storage,
              BlockAllocator &allocator, BlockAddress address) {
    return { storage, allocator, address, BlockType::Error };
}

void BlockHelper::dump(block_index_t block) {
    using NodeType = Node<uint64_t, int32_t, BlockAddress, 6, 6>;
    using SerializerType = NodeSerializer<NodeType>;

    auto &g = storage_->geometry();

    BlockHead head;
    BlockTail tail;

    auto head_addr = BlockAddress{ block, 0 };
    auto tail_addr = BlockAddress::tail_data_of(block, g, sizeof(BlockTail));

    storage_->read(head_addr, &head, sizeof(BlockHead));
    storage_->read(tail_addr, &tail, sizeof(BlockTail));

    auto &info = blocks[block];
    auto &live = info.live;

    sdebug() << block << ": " << " " << head.type
             << " (p=" << head.linked_block << ")"
             << " (n=" << tail.linked_block << ")"
             << " (live=" << live.size() << ")"
             << " ";

    switch (head.type) {
    case BlockType::Anchor: {
        break;
    }
    case BlockType::SuperBlockLink: {
        break;
    }
    case BlockType::SuperBlock: {
        break;
    }
    case BlockType::Leaf: {
        auto layout = get_tree_layout(*storage_, empty_allocator, BlockAddress{ block, SectorSize });
        while (layout.walk_single_block(SerializerType::HeadNodeSize)) {
            SerializerType::serialized_node_t node;
            storage_->read(layout.address(), &node, sizeof(node));
            if (!node.valid()) {
                break;
            }

            auto live = std::find(info.live.begin(), info.live.end(), layout.address()) != info.live.end();
            sdebug() << "  " << (live ? "L" : " ") << (int32_t)node.level;

            layout.add(SerializerType::HeadNodeSize);
        }
        break;
    }
    case BlockType::Index: {
        auto layout = get_tree_layout(*storage_, empty_allocator, BlockAddress{ block, SectorSize });
        while (layout.walk_single_block(SerializerType::HeadNodeSize)) {
            SerializerType::serialized_node_t node;
            storage_->read(layout.address(), &node, sizeof(node));
            if (!node.valid()) {
                break;
            }

            auto live = std::find(info.live.begin(), info.live.end(), layout.address()) != info.live.end();
            sdebug() << "  " << (live ? "L" : " ") << (int32_t)node.level;

            layout.add(SerializerType::HeadNodeSize);
        }
        break;
    }
    case BlockType::File: {
        break;
    }
    default: {
        break;
    }
    }

    sdebug() << endl;
}

DataHelper::DataHelper(FileSystem &fs) : fs_(&fs) {
}

bool DataHelper::write_file(const char *name, size_t size) {
    uint8_t pattern[] = { 'd', 'e', 't', 'u', 'g', 'o', 'l', 'p' };

    auto file = fs_->open(name);
    if (!file) {
        return false;
    }

    auto written = 0;
    while (written < (int32_t)size) {
        if (file.write(pattern, sizeof(pattern)) != sizeof(pattern)) {
            break;
        }

        written += sizeof(pattern);
    }

    file.close();

    return true;
}

}
