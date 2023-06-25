#pragma once

#include <array>
#include <cstdint>
#include <functional>

#include "fastmath.h"
#include "logging.h"
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

	class Audio;

	using AudioCallback = std::function<void(Audio*)>;

	class Audio
	{
	public:
		Audio() : m_callback(nullptr) {}

		void setCallback(const AudioCallback& _ac, const int _callbackSamples)
		{
			m_callbackSamples = _callbackSamples;
			m_callback = _ac;
		}

		void writeEmptyAudioIn(size_t len)
		{
			for (size_t i = 0; i < (len<<1); ++i)
				m_audioInputs.push_back({});
		}

		template<typename T>
		void processAudioInterleaved(const T** _inputs, T** _outputs, uint32_t _sampleFrames, const size_t _latency = 0)
		{
			processAudioInputInterleaved<T>(_inputs, _sampleFrames, _latency);
			processAudioOutputInterleaved<T>(_outputs, _sampleFrames);
		}

		template<typename T>
		void processAudioInputInterleaved(const T** _inputs, uint32_t _sampleFrames, const size_t _latency = 0)
		{
			if (!_sampleFrames)
				return;

			for (uint32_t i = 0; i < _sampleFrames; ++i)
			{
				// INPUT

				if(_latency > m_latency)
				{
					// a latency increase on the input means to feed additional zeroes into it
					m_audioInputs.waitNotFull();
					m_audioInputs.push_back({});
					m_audioInputs.waitNotFull();
					m_audioInputs.push_back({});

					++m_latency;
				}
				
				if(_latency < m_latency)
				{
					// a latency decrease on the input means to skip writing data
					--m_latency;
				}
				else
				{
					std::array<uint32_t, 4> inData;

					auto processInput = [&](uint32_t _offset)
					{
						for (uint32_t c = 0; c < inData.size(); ++c)
						{
							const uint32_t iSrc = (c<<1) + _offset;
							inData[c] = _inputs[iSrc] ? sample2dsp<T>(_inputs[iSrc][i]) : 0;
						}

						m_audioInputs.waitNotFull();
						m_audioInputs.push_back(inData);
					};

					processInput(0);
					processInput(1);
				}
			}
		}

		template<typename T>
		void processAudioOutputInterleaved(T** _outputs, uint32_t _sampleFrames)
		{
			if (!_sampleFrames)
				return;

			for (uint32_t i = 0; i < _sampleFrames; ++i)
			{
				// OUTPUT

				auto processOutput = [&](uint32_t _offset)
				{
					m_audioOutputs.waitNotEmpty();
					const auto& outData = m_audioOutputs.pop_front();

					for (uint32_t c = 0; c < outData.size(); ++c)
					{
						const uint32_t iDst = (c<<1) + _offset;

						if(_outputs[iDst])
						{
							_outputs[iDst][i] = dsp2sample<T>(outData[c]);
						}
					}
				};

				processOutput(0);
				processOutput(1);
			}
		}

		const auto& getAudioInputs() const { return m_audioInputs; }
		const auto& getAudioOutputs() const { return m_audioOutputs; }

		auto& getAudioInputs() { return m_audioInputs; }
		auto& getAudioOutputs() { return m_audioOutputs; }

	public:
		static constexpr uint32_t RingBufferSize = 8192;

	protected:
		void readRXimpl(std::array<TWord, 4>& _values);
		void writeTXimpl(const std::array<TWord, 6>& _values);

		AudioCallback m_callback;

		size_t m_callbackSamples = 0;

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

		RingBuffer<std::array<uint32_t, 4>, RingBufferSize, true> m_audioInputs;
		RingBuffer<std::array<uint32_t, 6>, RingBufferSize, true> m_audioOutputs;

		uint32_t m_frameSyncDSPStatus = FrameSyncChannelLeft;
		uint32_t m_frameSyncDSPRead = FrameSyncChannelLeft;
		uint32_t m_frameSyncDSPWrite = FrameSyncChannelLeft;
		uint32_t m_frameSyncAudio = FrameSyncChannelLeft;
		size_t m_latency = 0;
	};
}
