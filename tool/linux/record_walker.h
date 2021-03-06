#pragma once

#include <string>

#include <google/protobuf/descriptor.h>
#include <google/protobuf/descriptor.pb.h>
#include <google/protobuf/dynamic_message.h>
#include <google/protobuf/io/zero_copy_stream_impl.h>

#include "phylum_input_stream.h"

class RecordVisitor {
public:
    virtual void message(PhylumInputStream &stream, google::protobuf::Message *message, size_t serialized_size) = 0;

};

class NoopVisitor : public RecordVisitor {
public:
    void message(PhylumInputStream &stream, google::protobuf::Message *message, size_t serialized_size) override;

};

class PrintDetailsVisitor : public RecordVisitor {
public:
    void message(PhylumInputStream &stream, google::protobuf::Message *message, size_t serialized_size) override;

};

class LogsVisitor : public RecordVisitor {
public:
    void message(PhylumInputStream &stream, google::protobuf::Message *message, size_t serialized_size) override;

};

class DataVisitor : public RecordVisitor {
private:
    bool header_{ false };
    std::vector<std::string> sensor_names_;
    std::vector<float> sensor_values_;
    int64_t time_;
    int32_t reading_;

public:
    void message(PhylumInputStream &stream, google::protobuf::Message *message, size_t serialized_size) override;

private:
    void dump_header();
    void dump_readings();

};

class StreamVisitor : public RecordVisitor {
private:
    std::ostream &stream_;

public:
    StreamVisitor(std::ostream &stream) : stream_(stream) {
    }

public:
    void message(PhylumInputStream &stream, google::protobuf::Message *message, size_t serialized_size) override;

};

class RecordWalker {
private:
    google::protobuf::DescriptorPool pool;
    google::protobuf::DynamicMessageFactory factory;
    std::string proto_file_;
    std::string message_name_;
    std::string full_message_name_;
    uint64_t message_at_{ 0 };
    uint64_t corruption_at_{ 0 };

public:
    struct Read {
        bool is_eos;
        bool is_failed;
        bool is_incomplete;
        size_t size;

        operator bool() const {
            return !is_failed && !is_incomplete && !is_eos;
        }

        static Read eos() { return { true, false, false, 0 }; };
        static Read failed(size_t size) { return { false, true, false, size }; };
        static Read incomplete(size_t size) { return { false, false, true, size }; };
        static Read success(size_t size) { return { false, false, false, size }; };
    };

public:
    RecordWalker(std::string proto_file, std::string message_name);

public:
    bool walk(PhylumInputStream &stream, RecordVisitor &visitor, bool verbose = false);
    Read read(google::protobuf::io::ZeroCopyInputStream *ri, google::protobuf::MessageLite *message);
    uint32_t single(uint8_t *ptr);

private:
    google::protobuf::Message *get_message();

};
