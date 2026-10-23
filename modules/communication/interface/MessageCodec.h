/***************************************************************
Copyright (c) 2022-2030, shisan233@sszc.live.
SPDX-License-Identifier: MIT
File:        MessageCodec.h
Version:     1.0
Author:      cjx
start date: 2026-10-23
Description: 通信消息编解码接口
Version history

[序号]    |   [修改日期]  |   [修改者]   |   [修改内容]
1             2026-10-23      cjx        create

*****************************************************************/

#ifndef ME_COMMUNICATION_MESSAGE_CODEC_H
#define ME_COMMUNICATION_MESSAGE_CODEC_H

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace CommunicationModule
{

/**
 * @brief 二进制写入器
 *
 * 按小端序将基本类型、字符串与字节数组写入缓冲区。
 * 字符串与字节数组均带 4 字节长度前缀，便于读取方界定边界。
 * 线程不安全，请勿跨线程共享同一实例。
 */
class IBinaryWriter
{
public:
    virtual ~IBinaryWriter() = default;

    virtual void WriteBool(bool value) = 0;
    virtual void WriteInt8(int8_t value) = 0;
    virtual void WriteUInt8(uint8_t value) = 0;
    virtual void WriteInt16(int16_t value) = 0;
    virtual void WriteUInt16(uint16_t value) = 0;
    virtual void WriteInt32(int32_t value) = 0;
    virtual void WriteUInt32(uint32_t value) = 0;
    virtual void WriteInt64(int64_t value) = 0;
    virtual void WriteUInt64(uint64_t value) = 0;
    virtual void WriteFloat(float value) = 0;
    virtual void WriteDouble(double value) = 0;

    /** @brief 写入字符串（4 字节长度前缀 + UTF-8 字节） */
    virtual void WriteString(const std::string& value) = 0;

    /** @brief 写入原始字节（4 字节长度前缀 + 数据） */
    virtual void WriteBytes(const std::vector<uint8_t>& value) = 0;

    /** @brief 获取已写入的字节流 */
    virtual const std::vector<uint8_t>& Data() const = 0;

    /** @brief 当前缓冲区大小（字节） */
    virtual size_t Size() const = 0;

    /** @brief 清空缓冲区 */
    virtual void Clear() = 0;

    /** @brief 是否发生过写入错误 */
    virtual bool HasError() const = 0;
};

/**
 * @brief 二进制读取器
 *
 * 从小端序字节流中依次读取基本类型、字符串与字节数组。
 * 读取越界或数据不足时置错误标志，后续读取返回默认值。
 * 线程不安全，请勿跨线程共享同一实例。
 */
class IBinaryReader
{
public:
    virtual ~IBinaryReader() = default;

    virtual bool ReadBool() = 0;
    virtual int8_t ReadInt8() = 0;
    virtual uint8_t ReadUInt8() = 0;
    virtual int16_t ReadInt16() = 0;
    virtual uint16_t ReadUInt16() = 0;
    virtual int32_t ReadInt32() = 0;
    virtual uint32_t ReadUInt32() = 0;
    virtual int64_t ReadInt64() = 0;
    virtual uint64_t ReadUInt64() = 0;
    virtual float ReadFloat() = 0;
    virtual double ReadDouble() = 0;

    /** @brief 读取字符串 */
    virtual std::string ReadString() = 0;

    /** @brief 读取原始字节 */
    virtual std::vector<uint8_t> ReadBytes() = 0;

    /** @brief 剩余可读字节数 */
    virtual size_t Remaining() const = 0;

    /** @brief 读取位置（字节偏移） */
    virtual size_t Position() const = 0;

    /** @brief 是否发生过读取错误 */
    virtual bool HasError() const = 0;
};

/**
 * @brief 创建二进制写入器
 */
std::shared_ptr<IBinaryWriter> CreateBinaryWriter();

/**
 * @brief 基于已有字节流创建二进制读取器
 */
std::shared_ptr<IBinaryReader> CreateBinaryReader(const std::vector<uint8_t>& data);

} // namespace CommunicationModule

#endif // ME_COMMUNICATION_MESSAGE_CODEC_H
