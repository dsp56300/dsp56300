#include "HxDdisassemble.h"

TExternalDataTypeConverter* dsp56kConverter::Create()
{
    return new dsp56kConverter();
}

dsp56kConverter::dsp56kConverter() : m_disassembler(m_opcodes)
{
    FTypeName = L"{s:Disassembly(\"DSP 56300\")}";
    FFriendlyTypeName = FTypeName;
//    FCategory = tcSignedInteger;
    FWidth = dtwVariable;
    FMaxTypeSize = sizeof(uint8_t) * 3 * 2; // one or two 24 bit words
    FSupportedByteOrders = 1 << boLittleEndian | 1 << boBigEndian;
    FSupportsStrToBytes = FALSE;

    m_periphX.setSymbols(m_disassembler);
    m_periphY.setSymbols(m_disassembler);
}

void dsp56kConverter::ChangeByteOrder(uint8_t* Bytes, int ByteCount, TByteOrder TargetByteOrder)
{
    static_assert(sizeof(unsigned long) == sizeof(int32_t),
        "types must match in size to ensure a valid byte swapping");

    if (TargetByteOrder == boBigEndian && ByteCount >= sizeof(int32_t))
       *(int32_t*)Bytes = _byteswap_ulong(*(int32_t*)Bytes);
}

TBytesToStrError dsp56kConverter::BytesToStr(uint8_t* Bytes, int ByteCount, TIntegerDisplayOption FormattingOptions, int& ConvertedByteCount, std::wstring& ConvertedStr)
{
    if(ByteCount < 3)
        return btseBytesTooShort;
    /*
    std::vector<dsp56k::TWord> data;
	data.reserve((ByteCount+2)/3);

    for(size_t i=0; i<ByteCount/3; i += 3)
    {
	    dsp56k::TWord word = 0;

        if(i   < ByteCount)   word = static_cast<dsp56k::TWord>(Bytes[i]) << 16;
        if(i+1 < ByteCount)   word |= static_cast<dsp56k::TWord>(Bytes[i + 1]) << 8;
        if(i+2 < ByteCount)   word |= static_cast<dsp56k::TWord>(Bytes[i + 2]);

        data.push_back(word);
    }

    std::string output;
    m_disassembler.disassembleMemoryBlock(output, data, 0, false, false, false);
    */

    dsp56k::TWord opA = 0, opB = 0;

	opA = static_cast<dsp56k::TWord>(Bytes[0]) << 16;
	opA |= static_cast<dsp56k::TWord>(Bytes[1]) << 8;
	opA |= static_cast<dsp56k::TWord>(Bytes[2]);

    if(ByteCount >= 6)
    {
        opB = static_cast<dsp56k::TWord>(Bytes[3]) << 16;
        opB |= static_cast<dsp56k::TWord>(Bytes[4]) << 8;
        opB |= static_cast<dsp56k::TWord>(Bytes[5]);
    }

    dsp56k::Disassembler::Line line;
    m_disassembler.disassemble(line, opA, opB, 0, 0, 0);
    const auto output = dsp56k::Disassembler::formatLine(line, false);
    ConvertedStr.clear();
    ConvertedStr.insert(ConvertedStr.begin(), output.begin(), output.end());

    return btseNone;
}

TStrToBytesError dsp56kConverter::StrToBytes(std::wstring Str, TIntegerDisplayOption FormattingOptions, std::vector<uint8_t>& ConvertedBytes)
{
    // We do not have an assembler yet
    return stbeOutOfRange;
}
