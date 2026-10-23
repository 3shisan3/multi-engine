/***************************************************************
Copyright (c) 2022-2030, shisan233@sszc.live.
SPDX-License-Identifier: MIT
File:        message_codec.cpp
Description: 通信消息编解码实现（小端序二进制协议）

*****************************************************************/

#include "protocol/message_codec_impl.h"

#include <cstring>
#include <type_traits>

namespace CommunicationModule
{

// ==================== 写入器 ====================

template <typename T>
void BinaryWriterImpl::Append(const T& value)
{
    // 显式按小端序写入，保证跨字节序平台的一致性
    uint8_t bytes[sizeof(T)];
    for (size_t i = 0; i < sizeof(T); ++i)
    {
        bytes[i] = static_cast<uint8_t>(
            (static_cast<uint64_t>(value) >> (i * 8)) & 0xFFu);
    }

    buffer_.insert(buffer_.end(), bytes, bytes + sizeof(T));
}

void BinaryWriterImpl::WriteBool(bool value)
{
    buffer_.push_back(value ? 1 : 0);
}

void BinaryWriterImpl::WriteInt8(int8_t value) { Append(value); }
void BinaryWriterImpl::WriteUInt8(uint8_t value) { Append(value); }
void BinaryWriterImpl::WriteInt16(int16_t value) { Append(value); }
void BinaryWriterImpl::WriteUInt16(uint16_t value) { Append(value); }
void BinaryWriterImpl::WriteInt32(int32_t value) { Append(value); }
void BinaryWriterImpl::WriteUInt32(uint32_t value) { Append(value); }
void BinaryWriterImpl::WriteInt64(int64_t value) { Append(value); }
void BinaryWriterImpl::WriteUInt64(uint64_t value) { Append(value); }

void BinaryWriterImpl::WriteFloat(float value)
{
    uint32_t bits = 0;
    std::memcpy(&bits, &value, sizeof(bits));
    Append(bits);
}

void BinaryWriterImpl::WriteDouble(double value)
{
    uint64_t bits = 0;
    std::memcpy(&bits, &value, sizeof(bits));
    Append(bits);
}

void BinaryWriterImpl::WriteString(const std::string& value)
{
    WriteUInt32(static_cast<uint32_t>(value.size()));
    buffer_.insert(buffer_.end(), value.begin(), value.end());
}

void BinaryWriterImpl::WriteBytes(const std::vector<uint8_t>& value)
{
    WriteUInt32(static_cast<uint32_t>(value.size()));
    buffer_.insert(buffer_.end(), value.begin(), value.end());
}

void BinaryWriterImpl::Clear()
{
    buffer_.clear();
    error_ = false;
}

// ==================== 读取器 ====================

BinaryReaderImpl::BinaryReaderImpl(std::vector<uint8_t> data)
    : data_(std::move(data))
{
}

bool BinaryReaderImpl::ReadBytesRaw(uint8_t* out, size_t length)
{
    if (error_)
    {
        return false;
    }
    if (position_ + length > data_.size())
    {
        error_ = true;
        return false;
    }

    if (length > 0 && out != nullptr)
    {
        std::memcpy(out, data_.data() + position_, length);
    }
    position_ += length;
    return true;
}

template <typename T>
bool BinaryReaderImpl::ReadRaw(T& value)
{
    if (error_)
    {
        return false;
    }

    if (position_ + sizeof(T) > data_.size())
    {
        error_ = true;
        return false;
    }

    uint64_t raw = 0;
    for (size_t i = 0; i < sizeof(T); ++i)
    {
        raw |= static_cast<uint64_t>(data_[position_ + i]) << (i * 8);
    }
    position_ += sizeof(T);

    if constexpr (std::is_floating_point<T>::value)
    {
        if (sizeof(T) == sizeof(uint32_t))
        {
            uint32_t bits = static_cast<uint32_t>(raw);
            float result = 0.0f;
            std::memcpy(&result, &bits, sizeof(result));
            value = result;
        }
        else
        {
            uint64_t bits = raw;
            double result = 0.0;
            std::memcpy(&result, &bits, sizeof(result));
            value = result;
        }
    }
    else
    {
        value = static_cast<T>(raw);
    }

    return true;
}

bool BinaryReaderImpl::ReadBool()
{
    uint8_t value = 0;
    if (!ReadRaw(value))
    {
        return false;
    }
    return value != 0;
}

int8_t BinaryReaderImpl::ReadInt8() { int8_t v = 0; ReadRaw(v); return v; }
uint8_t BinaryReaderImpl::ReadUInt8() { uint8_t v = 0; ReadRaw(v); return v; }
int16_t BinaryReaderImpl::ReadInt16() { int16_t v = 0; ReadRaw(v); return v; }
uint16_t BinaryReaderImpl::ReadUInt16() { uint16_t v = 0; ReadRaw(v); return v; }
int32_t BinaryReaderImpl::ReadInt32() { int32_t v = 0; ReadRaw(v); return v; }
uint32_t BinaryReaderImpl::ReadUInt32() { uint32_t v = 0; ReadRaw(v); return v; }
int64_t BinaryReaderImpl::ReadInt64() { int64_t v = 0; ReadRaw(v); return v; }
uint64_t BinaryReaderImpl::ReadUInt64() { uint64_t v = 0; ReadRaw(v); return v; }
float BinaryReaderImpl::ReadFloat() { float v = 0.0f; ReadRaw(v); return v; }
double BinaryReaderImpl::ReadDouble() { double v = 0.0; ReadRaw(v); return v; }

std::string BinaryReaderImpl::ReadString()
{
    uint32_t length = 0;
    if (!ReadRaw(length))
    {
        return "";
    }

    if (position_ + length > data_.size())
    {
        error_ = true;
        return "";
    }

    std::string result(reinterpret_cast<const char*>(data_.data() + position_), length);
    position_ += length;
    return result;
}

std::vector<uint8_t> BinaryReaderImpl::ReadBytes()
{
    uint32_t length = 0;
    if (!ReadRaw(length))
    {
        return {};
    }

    if (position_ + length > data_.size())
    {
        error_ = true;
        return {};
    }

    std::vector<uint8_t> result(data_.begin() + static_cast<std::ptrdiff_t>(position_),
                                data_.begin() + static_cast<std::ptrdiff_t>(position_ + length));
    position_ += length;
    return result;
}

size_t BinaryReaderImpl::Remaining() const
{
    if (position_ >= data_.size())
    {
        return 0;
    }
    return data_.size() - position_;
}

// ==================== 工厂函数 ====================

std::shared_ptr<IBinaryWriter> CreateBinaryWriter()
{
    return std::make_shared<BinaryWriterImpl>();
}

std::shared_ptr<IBinaryReader> CreateBinaryReader(const std::vector<uint8_t>& data)
{
    return std::make_shared<BinaryReaderImpl>(data);
}

} // namespace CommunicationModule
