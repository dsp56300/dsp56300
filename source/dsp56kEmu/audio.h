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

	template<typename T> TWord sample2dsp(T _src)
	{
		return _src;
	}

	template<> inline TWord sample2dsp(float _src)
	{
		_src *= g_float2dspScale;
		_src = clamp(_src, g_dspFloatMin, g_dspFloatMax);

		return floor_int(_src) & 0x00ffffff;
	}

	template<typename T> T dsp2sample(TWord d)
	{
		return d;
	}

	template<> inline float dsp2sample(const TWord d)
	{
		return static_cast<float>(signextend<int32_t,24>(d)) * g_dsp2FloatScale;
	}

	typedef void (*AudioCallback)(class Audio* audio);

	class Audio
	{
	public:
		Audio() : m_callback(0), m_pendingRXInterrupts(0) {}
		void setCallback(AudioCallback ac,int callbackSamples,int callbackChannels) {m_callback=ac;m_callbackSamples=callbackSamples;m_callbackChannels=callbackChannels;}
		void writeEmptyAudioIn(size_t len,size_t ins)
		{
			for (size_t i = 0; i < len; ++i) for (size_t c = 0; c < ins; ++c) m_audioInputs[c>>1].push_back(0);
		}

		template<typename T>
		void processAudioInterleaved(T** _inputs, T** _outputs, size_t _sampleFrames, size_t _numDSPins, size_t _numDSPouts, size_t _latency = 0)
		{
			if (!_sampleFrames)
				return;

			// write input data

			if(_latency > m_latency)
			{
				// write 0s to input to increase latency
				const auto len = std::min(_latency - m_latency, _sampleFrames);

				for (size_t i = 0; i < len; ++i)
				{
					for (size_t c = 0; c < _numDSPins; ++c)
					{
						const auto in = c >> 1;
						m_audioInputs[in].waitNotFull();
						m_audioInputs[in].push_back(0);
					}
				}
			}

			for (size_t i = 0; i < _sampleFrames; ++i)
			{
				for (size_t c = 0; c < _numDSPins; ++c)
				{
					const auto in = c >> 1;
					m_audioInputs[in].waitNotFull();
					m_audioInputs[in].push_back(sample2dsp<T>(_inputs[c][i]));
				}

				m_pendingRXInterrupts += 2;
			}

			// read output
			for (size_t i = 0; i < _sampleFrames; ++i)
			{
				if(_latency > m_latency)
				{
					for (size_t c = 0; c < _numDSPouts; ++c)
						_outputs[c][i] = 0;
					++m_latency;
				}

				for (size_t c = 0; c < _numDSPouts; ++c)
				{
					const auto out = c >> 1;

					TWord v = 0;
					
					if(out == 0)
					{
						m_audioOutputs[out].waitNotEmpty();						
						v = m_audioOutputs[out].pop_front();
					}
					else if(!m_audioOutputs[out].empty())
					{
						v = m_audioOutputs[out].pop_front();
					}

					_outputs[c][i] = dsp2sample<T>(v);
				}
			}
		}

		template<typename T>
		void processAudioInterleavedSingle(T* _inputs, T* _outputs, size_t _sampleFrames, size_t _numDSPins, size_t _numDSPouts, size_t _latency = 0)
		{
			if (!_sampleFrames)
				return;

			// write input data

			if(_latency > m_latency)
			{
				// write 0s to input to increase latency
				const auto len = std::min(_latency - m_latency, _sampleFrames);

				for (size_t i = 0; i < len; ++i)
				{
					for (size_t c = 0; c < _numDSPins; ++c)
					{
						const auto in = c >> 1;
						m_audioInputs[in].waitNotFull();
						m_audioInputs[in].push_back(0);
					}
				}
			}

			for (size_t i = 0; i < _sampleFrames; ++i)
			{
				for (size_t c = 0; c < _numDSPins; ++c)
				{
					const auto in = c >> 1;
					m_audioInputs[in].waitNotFull();
					m_audioInputs[in].push_back(sample2dsp<T>(*_inputs++));
				}

				m_pendingRXInterrupts += 2;
			}

			// read output
			for (size_t i = 0; i < _sampleFrames; ++i)
			{
				if(_latency > m_latency)
				{
					for (size_t c = 0; c < _numDSPouts; ++c)
						*_outputs++ = 0;
					++m_latency;
				}

				for (size_t c = 0; c < _numDSPouts; ++c)
				{
					const auto out = c >> 1;

					TWord v = 0;

					if(out == 0)
					{
						m_audioOutputs[out].waitNotEmpty();
						v = m_audioOutputs[out].pop_front();
					}
					else if(!m_audioOutputs[out].empty())
					{
						v = m_audioOutputs[out].pop_front();
					}

					*_outputs++ = dsp2sample<T>(v);
				}
			}
		}

		template<typename T>
		void processAudioInterleavedTX0(T** _inputs, T** _outputs, size_t _sampleFrames)
		{
			return processAudioInterleaved(_inputs, _outputs, _sampleFrames, 2, 2);
		}
		/*
		void setLatency(size_t _latency, size_t _numDSPins, size_t _numDSPouts, std::mutex& _dspLock)
		{
			if(m_latency == _latency)
				return;

			if(_latency > m_latency)
			{
				const auto len = _latency - m_latency;

				for (size_t i = 0; i < len; ++i)
				{
					for (size_t c = 0; c < _numDSPins; ++c)
					{
						const auto in = c >> 1;
						m_audioInputs[in].waitNotFull();
						m_audioInputs[in].push_back(0);
					}
					for (size_t c = 0; c < _numDSPouts; ++c)
					{
						const auto out = c >> 1;
						m_audioOutputs[out].waitNotFull();
						m_audioOutputs[out].push_back(0);
					}
				}
			}
			else
			{
				const auto len = m_latency - _latency;

				for (size_t i = 0; i < len; ++i)
				{
					for (size_t c = 0; c < _numDSPins; ++c)
					{
						const auto in = c >> 1;
						m_audioInputs[in].waitNotEmpty();
						m_audioInputs[in].pop_front();
					}
					for (size_t c = 0; c < _numDSPouts; ++c)
					{
						const auto out = c >> 1;
						m_audioOutputs[out].waitNotEmpty();
						m_audioOutputs[out].pop_front();
					}
				}
			}

			m_latency = _latency;
		}		
		*/
	protected:
		TWord readRXimpl(size_t _index);
		void writeTXimpl(size_t _index, TWord _val);
		AudioCallback m_callback;
		int m_callbackSamples,m_callbackChannels;

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

		std::array<RingBuffer<uint32_t, 4096, false>, 1> m_audioInputs;
		std::array<RingBuffer<uint32_t, 4096, false>, 3> m_audioOutputs;
		std::atomic<uint32_t> m_pendingRXInterrupts;

		uint32_t m_frameSyncDSPStatus = FrameSyncChannelLeft;
		uint32_t m_frameSyncDSPRead = FrameSyncChannelLeft;
		uint32_t m_frameSyncDSPWrite = FrameSyncChannelLeft;
		uint32_t m_frameSyncAudio = FrameSyncChannelLeft;
		size_t m_latency = 0;
	};
}
