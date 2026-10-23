/***************************************************************
Copyright (c) 2022-2030, shisan233@sszc.live.
SPDX-License-Identifier: MIT
File:        message_codec_test.cpp
Description: 通信消息编解码测试

*****************************************************************/

#include "MessageCodec.h"

#include <cmath>
#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

using namespace CommunicationModule;

namespace
{

bool NearlyEqual(double a, double b)
{
    return std::fabs(a - b) < 1e-9;
}

} // namespace

int main()
{
    // 1. 基本类型往返
    auto writer = CreateBinaryWriter();
    if (!writer)
    {
        return 1;
    }

    writer->WriteBool(true);
    writer->WriteBool(false);
    writer->WriteInt8(-12);
    writer->WriteUInt8(200);
    writer->WriteInt16(-3000);
    writer->WriteUInt16(60000);
    writer->WriteInt32(-70000);
    writer->WriteUInt32(4000000000u);
    writer->WriteInt64(-9000000000000ll);
    writer->WriteUInt64(18000000000000ull);
    writer->WriteFloat(3.5f);
    writer->WriteDouble(2.718281828459045);

    auto reader = CreateBinaryReader(writer->Data());
    if (!reader)
    {
        return 2;
    }

    if (reader->ReadBool() != true || reader->ReadBool() != false) return 3;
    if (reader->ReadInt8() != -12) return 4;
    if (reader->ReadUInt8() != 200) return 5;
    if (reader->ReadInt16() != -3000) return 6;
    if (reader->ReadUInt16() != 60000) return 7;
    if (reader->ReadInt32() != -70000) return 8;
    if (reader->ReadUInt32() != 4000000000u) return 9;
    if (reader->ReadInt64() != -9000000000000ll) return 10;
    if (reader->ReadUInt64() != 18000000000000ull) return 11;
    if (!NearlyEqual(reader->ReadFloat(), 3.5f)) return 12;
    if (!NearlyEqual(reader->ReadDouble(), 2.718281828459045)) return 13;

    if (reader->HasError() || reader->Remaining() != 0)
    {
        std::cerr << "reader should be fully consumed" << std::endl;
        return 14;
    }

    // 2. 字符串（含中文）与字节数组
    std::string text = u8"hello 世界";
    std::vector<uint8_t> bytes = {0, 1, 2, 253, 254, 255};

    auto writer2 = CreateBinaryWriter();
    writer2->WriteString(text);
    writer2->WriteBytes(bytes);

    auto reader2 = CreateBinaryReader(writer2->Data());
    if (reader2->ReadString() != text) return 15;
    if (reader2->ReadBytes() != bytes) return 16;

    // 3. 空字符串 / 空字节
    auto writer3 = CreateBinaryWriter();
    writer3->WriteString("");
    writer3->WriteBytes({});
    auto reader3 = CreateBinaryReader(writer3->Data());
    if (!reader3->ReadString().empty() || !reader3->ReadBytes().empty()) return 17;

    // 4. 越界读取应置错误并返回默认值
    auto reader4 = CreateBinaryReader(std::vector<uint8_t>{0x01, 0x02});
    reader4->ReadUInt32();
    if (!reader4->HasError()) return 18;
    if (reader4->ReadUInt8() != 0) return 19;

    // 5. 字符串长度前缀被截断应报错
    std::vector<uint8_t> truncated = {0x10, 0x00, 0x00, 0x00, 'a', 'b'};
    auto reader5 = CreateBinaryReader(truncated);
    reader5->ReadString();
    if (!reader5->HasError()) return 20;

    // 6. 小端序验证
    auto writer6 = CreateBinaryWriter();
    writer6->WriteUInt32(0x01020304u);
    const std::vector<uint8_t>& raw = writer6->Data();
    if (raw.size() != 4) return 21;
    if (raw[0] != 0x04 || raw[1] != 0x03 || raw[2] != 0x02 || raw[3] != 0x01)
    {
        std::cerr << "not little-endian" << std::endl;
        return 22;
    }

    // 7. Clear 后可复用
    writer->Clear();
    if (writer->Size() != 0) return 23;

    std::cout << "message codec test passed" << std::endl;
    return 0;
}
