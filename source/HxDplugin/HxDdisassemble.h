#pragma once

#include "DataInspectorShared.h"
#include "DataInspectorPluginServer.h"

#include "dsp56kEmu/disasm.h"
#include "dsp56kEmu/peripherals.h"

class dsp56kConverter : public TExternalDataTypeConverter
{
public:
    // Class factory function / virtual constructor
    static TExternalDataTypeConverter* Create();

	dsp56kConverter();

    void ChangeByteOrder(uint8_t* Bytes, int ByteCount,
        TByteOrder TargetByteOrder) override;
    TBytesToStrError BytesToStr(uint8_t* Bytes, int ByteCount,
        TIntegerDisplayOption FormattingOptions, int& ConvertedByteCount,
        std::wstring& ConvertedStr) override;
    TStrToBytesError StrToBytes(std::wstring Str,
        TIntegerDisplayOption FormattingOptions,
        std::vector<uint8_t>& ConvertedBytes) override;
private:
    dsp56k::Peripherals56362 m_periphX;
    dsp56k::Peripherals56367 m_periphY;
    dsp56k::Opcodes m_opcodes;
    dsp56k::Disassembler m_disassembler;
};
