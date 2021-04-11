#pragma once
#include <array>
#include <cstdint>

#include "fastmath.h"
#include "ringbuffer.h"
#include "utils.h"

namespace dsp56k
{	
	constexpr float g_float2dspScale	= 8388608.0f;
	constexpr float g_dsp2FloatScale	= 0.00000011920928955078125f;
	constexpr float g_dspFloatMax		= 8388607.0f;
	constexpr float g_dspFloatMin		= -8388608.0f;

	static TWord float2Dsdp(float f)
	{
		f *= g_float2dspScale;
		f = clamp(f, g_dspFloatMin, g_dspFloatMax);

		return floor_int(f) & 0x00ffffff;
	}

	static float dsp2Float(TWord d)
	{
		return static_cast<float>(signextend<int32_t,24>(d)) * g_dsp2FloatScale;
	}

	class Audio
	{
	public:
		Audio() : m_pendingRXInterrupts(0) {}
		void processAudioInterleaved(float** _inputs, float** _outputs, size_t _sampleFrames, size_t _numDSPins, size_t _numDSPouts)
		{
			if (!_sampleFrames)
				return;

			// write input data
			for (size_t i = 0; i < _sampleFrames; ++i)
			{
				for (size_t c = 0; c < _numDSPins; ++c)
				{
					const auto in = c >> 1;
					m_audioInputs[in].waitNotFull();
					m_audioInputs[in].push_back(float2Dsdp(_inputs[c][i]));
				}

				m_pendingRXInterrupts += 2;
			}

			// read output
			for (size_t i = 0; i < _sampleFrames; ++i)
			{
				for (size_t c = 0; c < _numDSPouts; ++c)
				{
					const auto out = c >> 1;

					m_audioOutputs[out].waitNotEmpty();
					const auto v = m_audioOutputs[out].pop_front();

					_outputs[c][i] = dsp2Float(v);
				}
			}		
		}

		void processAudioInterleavedTX0(float** _inputs, float** _outputs, size_t _sampleFrames)
		{
			return processAudioInterleaved(_inputs, _outputs, _sampleFrames, 2, 2);
		}

	protected:
		TWord readRXimpl(size_t _index);
		void writeTXimpl(size_t _index, TWord _val);

		static void incFrameSync(uint32_t& _frameSync)
		{
			++_frameSync;
			_frameSync &= 1;
		}

		enum FrameSync
		{
			FrameSyncChannelLeft = 1,
			FrameSyncChannelRight = 0
		};

		std::array<RingBuffer<uint32_t, 8192, false>, 1> m_audioInputs;
		std::array<RingBuffer<uint32_t, 8192, false>, 3> m_audioOutputs;
		std::atomic<uint32_t> m_pendingRXInterrupts;
		
		uint32_t m_frameSyncDSPStatus = FrameSyncChannelLeft;
		uint32_t m_frameSyncDSPRead = FrameSyncChannelLeft;
		uint32_t m_frameSyncDSPWrite = FrameSyncChannelLeft;
		uint32_t m_frameSyncAudio = FrameSyncChannelLeft;
	};
}
