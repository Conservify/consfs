#ifndef __PHYLUM_SIMPLE_FILE_H_INCLUDED
#define __PHYLUM_SIMPLE_FILE_H_INCLUDED

#include "phylum/backend.h"
#include "phylum/file_descriptor.h"
#include "phylum/file_allocation.h"
#include "phylum/file_index.h"
#include "phylum/blocked_file.h"

namespace phylum {

class ExtentBlockedFile : public BlockedFile {
private:
    Extent data_;

public:
    ExtentBlockedFile() {
    }

    ExtentBlockedFile(StorageBackend *storage, OpenMode mode, Extent data) : BlockedFile(storage, mode), data_(data) {
    }

public:
    block_index_t allocate() override;
    void free(block_index_t block) override;

};

class SimpleFile : public File {
private:
    ExtentBlockedFile blocked_;
    FileDescriptor *fd_{ nullptr };
    FileAllocation *file_{ nullptr };
    uint32_t previous_index_block_{ 0 };
    FileIndex index_;

public:
    SimpleFile() {
    }

    SimpleFile(StorageBackend *storage, FileDescriptor *fd, FileAllocation *file, OpenMode mode) :
        blocked_(storage, mode, file->data), fd_(fd), file_(file), index_(storage, file) {
    }

    template<size_t SIZE>
    friend class FileLayout;

public:
    BlockedFile &blocked() {
        return blocked_;
    }

    operator bool() const {
        return file_ != nullptr;
    }

    uint32_t read_only() const {
        return blocked_.read_only();
    }

    uint32_t version() const override {
        return blocked_.version();
    }

    uint64_t maximum_size() const;

    uint64_t size() const override {
        return blocked_.size();
    }

    uint64_t tell() const override {
        return blocked_.tell();
    }

    BlockAddress head() const {
        return blocked_.head();
    }

    FileDescriptor &fd() const;

    bool in_final_block() const;

    FileAllocation &allocation() const {
        return *file_;
    }

    FileIndex &index();

    bool seek(uint64_t position) override;

    int32_t read(uint8_t *ptr, size_t size) override;

    int32_t write(uint8_t *ptr, size_t size, bool span_sectors = true, bool span_blocks = true) override;

    int32_t flush();

    bool erase();

    bool initialize();

    bool format();

    void close();

};

}

#endif
