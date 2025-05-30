//
//  safe_mem_reader.hpp
//  blocksci_parser
//
//  Created by Harry Kalodner on 9/26/17.
//

#ifndef safe_mem_reader_hpp
#define safe_mem_reader_hpp

#include <mio/mmap.hpp>
#include <array>
#include <vector>
#include <algorithm>
#include <cstring>
#include <stdexcept>
#include <cstdint>

inline unsigned int variableLengthIntSize(uint64_t nSize) {
    if (nSize < 253)             return sizeof(unsigned char);
    else if (nSize <= std::numeric_limits<unsigned short>::max()) return sizeof(unsigned char) + sizeof(unsigned short);
    else if (nSize <= std::numeric_limits<unsigned int>::max())  return sizeof(unsigned char) + sizeof(unsigned int);
    else                         return sizeof(unsigned char) + sizeof(uint64_t);
}

class SafeMemReader {
public:
    using iterator = mio::mmap_source::const_iterator;
    using size_type = mio::mmap_source::size_type;
    using difference_type = mio::mmap_source::difference_type;
    
    explicit SafeMemReader(std::string path_, const std::vector<uint8_t> &xorKeyArg = {})
        : path(std::move(path_)), useXor(xorKeyArg.size() == 8) {
        if (useXor) {
            std::copy_n(xorKeyArg.data(), 8, xorKey.begin());
        }
        std::error_code error;
        fileMap.map(path, 0, mio::map_entire_file, error);
        if (error) { throw error; }
        begin = fileMap.begin();
        end = fileMap.end();
        pos = begin;
    }
    
    std::string getPath() const {
        return path;
    }

    bool has(difference_type n) {
        return n <= std::distance(pos, end);
    }
    
    template<typename Type>
    Type readNext() {
        auto val = peakNext<Type>();
        pos += sizeof(val);
        return val;
    }
    
    template<typename Type>
    Type peakNext() {
        constexpr auto size = sizeof(Type);
        if (!has(size)) {
            throw std::out_of_range("Tried to read past end of file");
        }
        Type val;
        if (useXor) {
            auto offset0 = offset();
            uint8_t *p = reinterpret_cast<uint8_t *>(&val);
            for (size_t i = 0; i < size; ++i) {
                p[i] = static_cast<uint8_t>(pos[i]) ^ xorKey[(offset0 + i) % xorKey.size()];
            }
        } else {
            memcpy(&val, pos, size);
        }
        return val;
    }
    
    // reads a variable length integer.
    // See the documentation from here:  https://en.bitcoin.it/wiki/Protocol_specification#Variable_length_integer
    uint32_t readVariableLengthInteger() {
        auto v = readNext<uint8_t>();
        if ( v < 0xFD ) { // If it's less than 0xFD use this value as the unsigned integer
            return static_cast<uint32_t>(v);
        } else if (v == 0xFD) {
            return static_cast<uint32_t>(readNext<uint16_t>());
        } else if (v == 0xFE) {
            return readNext<uint32_t>();
        } else {
            return static_cast<uint32_t>(readNext<uint64_t>()); // TODO: maybe we should not support this here, we lose data
        }
    }
    
    void advance(difference_type n) {
        if (!has(n)) {
            throw std::out_of_range("Tried to advance past end of file");
        }
        pos += n;
    }
    
    void reset() {
        pos = begin;
    }
    
    void reset(difference_type n) {
        if (begin + n > end) {
            throw std::out_of_range("Tried to reset out of file");
        }
        pos = begin + n;
    }
    
    difference_type offset() {
        return std::distance(begin, pos);
    }
    
    iterator unsafePos() {
        return pos;
    }
    
protected:
    mio::mmap_source fileMap;
    std::string path;
    iterator pos;
    iterator begin;
    iterator end;
    bool useXor = false;
    std::array<uint8_t, 8> xorKey{};
};

#endif /* safe_mem_reader_hpp */
