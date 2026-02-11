#pragma once

#include "core/util/Types.h"
#include <vector>
#include <string>
#include <fstream>
#include <stdexcept>
#include <cstring>

namespace godsim {

/// BinaryWriter - writes primitive types to a byte buffer.
class BinaryWriter {
public:
    void write_u8(u8 v) { buf_.push_back(v); }

    void write_u32(u32 v) {
        write_bytes(&v, sizeof(v));
    }

    void write_u64(u64 v) {
        write_bytes(&v, sizeof(v));
    }

    void write_i64(i64 v) {
        write_bytes(&v, sizeof(v));
    }

    void write_f32(f32 v) {
        write_bytes(&v, sizeof(v));
    }

    void write_f64(f64 v) {
        write_bytes(&v, sizeof(v));
    }

    void write_string(const std::string& v) {
        write_u32(static_cast<u32>(v.size()));
        write_bytes(v.data(), v.size());
    }

    void write_bytes(const void* data, size_t size) {
        const auto* ptr = static_cast<const u8*>(data);
        buf_.insert(buf_.end(), ptr, ptr + size);
    }

    const std::vector<u8>& buffer() const { return buf_; }

    void save_to_file(const std::string& path) const {
        std::ofstream file(path, std::ios::binary);
        if (!file) throw std::runtime_error("Failed to open file: " + path);
        file.write(reinterpret_cast<const char*>(buf_.data()), buf_.size());
    }

    void clear() { buf_.clear(); }

private:
    std::vector<u8> buf_;
};

/// BinaryReader - reads primitive types from a byte buffer.
class BinaryReader {
public:
    explicit BinaryReader(const std::vector<u8>& data)
        : data_(data), pos_(0) {}

    static BinaryReader from_file(const std::string& path) {
        std::ifstream file(path, std::ios::binary | std::ios::ate);
        if (!file) throw std::runtime_error("Failed to open file: " + path);
        auto size = file.tellg();
        file.seekg(0);
        std::vector<u8> data(size);
        file.read(reinterpret_cast<char*>(data.data()), size);
        return BinaryReader(data);
    }

    u8 read_u8() {
        check_remaining(1);
        return data_[pos_++];
    }

    u32 read_u32() {
        u32 v;
        read_bytes(&v, sizeof(v));
        return v;
    }

    u64 read_u64() {
        u64 v;
        read_bytes(&v, sizeof(v));
        return v;
    }

    i64 read_i64() {
        i64 v;
        read_bytes(&v, sizeof(v));
        return v;
    }

    f32 read_f32() {
        f32 v;
        read_bytes(&v, sizeof(v));
        return v;
    }

    f64 read_f64() {
        f64 v;
        read_bytes(&v, sizeof(v));
        return v;
    }

    std::string read_string() {
        u32 len = read_u32();
        check_remaining(len);
        std::string result(reinterpret_cast<const char*>(&data_[pos_]), len);
        pos_ += len;
        return result;
    }

    void read_bytes(void* out, size_t size) {
        check_remaining(size);
        std::memcpy(out, &data_[pos_], size);
        pos_ += size;
    }

    bool at_end() const { return pos_ >= data_.size(); }
    size_t remaining() const { return data_.size() - pos_; }

private:
    void check_remaining(size_t needed) {
        if (pos_ + needed > data_.size()) {
            throw std::runtime_error("BinaryReader: attempted to read past end of buffer");
        }
    }

    std::vector<u8> data_;
    size_t pos_;
};

} // namespace godsim
