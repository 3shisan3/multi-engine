#ifndef ME_COMMUNICATION_MESSAGE_CODEC_IMPL_H
#define ME_COMMUNICATION_MESSAGE_CODEC_IMPL_H

#include "MessageCodec.h"

#include <cstring>
#include <string>
#include <vector>

namespace CommunicationModule
{

class BinaryWriterImpl : public IBinaryWriter
{
public:
    void WriteBool(bool value) override;
    void WriteInt8(int8_t value) override;
    void WriteUInt8(uint8_t value) override;
    void WriteInt16(int16_t value) override;
    void WriteUInt16(uint16_t value) override;
    void WriteInt32(int32_t value) override;
    void WriteUInt32(uint32_t value) override;
    void WriteInt64(int64_t value) override;
    void WriteUInt64(uint64_t value) override;
    void WriteFloat(float value) override;
    void WriteDouble(double value) override;
    void WriteString(const std::string& value) override;
    void WriteBytes(const std::vector<uint8_t>& value) override;

    const std::vector<uint8_t>& Data() const override { return buffer_; }
    size_t Size() const override { return buffer_.size(); }
    void Clear() override;
    bool HasError() const override { return error_; }

private:
    template <typename T>
    void Append(const T& value);

    std::vector<uint8_t> buffer_;
    bool error_ = false;
};

class BinaryReaderImpl : public IBinaryReader
{
public:
    explicit BinaryReaderImpl(std::vector<uint8_t> data);

    bool ReadBool() override;
    int8_t ReadInt8() override;
    uint8_t ReadUInt8() override;
    int16_t ReadInt16() override;
    uint16_t ReadUInt16() override;
    int32_t ReadInt32() override;
    uint32_t ReadUInt32() override;
    int64_t ReadInt64() override;
    uint64_t ReadUInt64() override;
    float ReadFloat() override;
    double ReadDouble() override;
    std::string ReadString() override;
    std::vector<uint8_t> ReadBytes() override;

    size_t Remaining() const override;
    size_t Position() const override { return position_; }
    bool HasError() const override { return error_; }

private:
    template <typename T>
    bool ReadRaw(T& value);

    bool ReadBytesRaw(uint8_t* out, size_t length);

    std::vector<uint8_t> data_;
    size_t position_ = 0;
    bool error_ = false;
};

} // namespace CommunicationModule

#endif // ME_COMMUNICATION_MESSAGE_CODEC_IMPL_H
