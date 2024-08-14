#pragma once

#include <array>
#include <cstdint>
#include <functional>
#include <array>
#include <cstring> // memcpy

#include "fastmath.h"
#include "ringbuffer.h"
#include "utils.h"

namespace dsp56k
{	
	constexpr float g_float2dspScale	= 8388608.0f;
	constexpr float g_dsp2FloatScale	= 1.0f / g_float2dspScale;
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
		return static_cast<float>(signextend<int32_t,24>(static_cast<int32_t>(d))) * g_dsp2FloatScale;
	}

	class Audio;

	using AudioCallback = std::function<void(Audio*)>;

	class Audio
	{
	public:
		static constexpr uint32_t MaxSlotsPerFrame = 16;
		static constexpr uint32_t TxRegisterCount = 6;
		static constexpr uint32_t RxRegisterCount = 4;

		using TxSlot = std::array<TWord, TxRegisterCount>;
		using RxSlot = std::array<TWord, RxRegisterCount>;

		template<typename TSlot>
		class Frame
		{
		public:
			using Slot = TSlot;
			using Data = std::array<Slot, MaxSlotsPerFrame>;

			Frame() = default;
			~Frame() = default;

			Frame(const Frame&& _source) noexcept : m_slotCount(_source.m_slotCount)
			{
				_source.copyTo(*this);
			}

			Frame(const Frame& _source) : m_slotCount(_source.m_slotCount)
			{
				_source.copyTo(*this);
			}

			Frame& operator = (const Frame& _source)
			{
				_source.copyTo(*this);
				return *this;
			}

			Frame& operator = (Frame&& _source) noexcept
			{
				_source.copyTo(*this);
				return *this;
			}

			const Slot& operator[](size_t _index) const			{ return m_data[_index]; }
			Slot& operator[](size_t _index)						{ return m_data[_index]; }

			[[nodiscard]] uint32_t size() const					{ return m_slotCount; }
			bool empty() const									{ return 0 == size(); }
			void clear()										{ m_slotCount = 0; }
			void resize(const uint32_t _size)					{ m_slotCount = _size; }

			void copyTo(Frame& _target) const
			{
				_target.m_slotCount = m_slotCount;

				::memcpy(_target.m_data.data(), m_data.data(), sizeof(m_data[0]) * m_slotCount);
			}

		private:
			Data m_data;
			uint32_t m_slotCount = 0;
		};

		using TxFrame = Frame<TxSlot>;
		using RxFrame = Frame<RxSlot>;

		Audio() : m_callback([](Audio*) {}) {}

		void terminate();

		void setCallback(const AudioCallback& _ac, const int _callbackSamples)
		{
			m_callbackSamples = _callbackSamples;
			m_callback = _ac ? _ac : [](Audio*) {};
		}

		void writeEmptyAudioIn(const size_t _len)
		{
			for (size_t i = 0; i < _len; ++i)
				m_audioInputs.push_back({});
		}

		template<typename T>
		void processAudioInterleaved(const T** _inputs, T** _outputs, const uint32_t _sampleFrames, const size_t _latency = 0)
		{
			processAudioInputInterleaved<T>(_inputs, _sampleFrames, _latency);
			processAudioOutputInterleaved<T>(_outputs, _sampleFrames);
		}
		
		template<typename T, typename TFunc>
		void processAudioInput(const uint32_t _frames, const size_t _latency, const TFunc& _createRxFrame)
		{
			for (uint32_t s = 0; s < _frames; ++s)
			{
				// INPUT

				if(_latency > m_latency)
				{
					// a latency increase on the input means to feed additional zeroes into it
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
					m_audioInputs.emplace_back([&](RxFrame& _frame)
					{
						_createRxFrame(s, _frame);
					});
				}
			}
		}

		template<typename T>
		void processAudioInputInterleaved(const T** _ins, const uint32_t _frames, const size_t _latency = 0)
		{
			return processAudioInput<T>(_frames, _latency, [&](size_t _s, RxFrame& _f)
			{
				_f.resize(2);
				_f[0] = RxSlot{sample2dsp<T>(_ins[0][_s]), sample2dsp<T>(_ins[2][_s]), sample2dsp<T>(_ins[4][_s]), sample2dsp<T>(_ins[6][_s])};
				_f[1] = RxSlot{sample2dsp<T>(_ins[1][_s]), sample2dsp<T>(_ins[3][_s]), sample2dsp<T>(_ins[5][_s]), sample2dsp<T>(_ins[7][_s])};
			});
		}

		template<typename T>
		void processAudioInput(const T* _input, const uint32_t _frames, const uint32_t _slotsPerFrame, const size_t _latency = 0)
		{
			uint32_t readPos = 0;

			return processAudioInput<T>(_frames, _latency, [&](size_t _s, RxFrame& _f)
			{
				_f.resize(_slotsPerFrame);

				for(uint32_t s=0; s<_slotsPerFrame; ++s)
				{
					for(uint32_t i=0; i<_f[s].size(); ++i)
						_f[s][i] = sample2dsp<T>(_input[readPos++]);
				}
			});
		}

		template<typename T, typename TFunc>
		void processAudioOutput(const uint32_t _frames, const TFunc& _readOutputCbk)
		{
			for (uint32_t i = 0; i < _frames; ++i)
			{
				m_audioOutputs.waitNotEmpty();
				m_audioOutputs.pop_front([&](TxFrame& _frame)
				{
					_readOutputCbk(i, _frame);
				});
			}
		}

		template<typename T>
		void processAudioOutputInterleaved(T** _outputs, const uint32_t _sampleFrames)
		{
			processAudioOutput<T>(_sampleFrames, [&](size_t _frame, TxFrame& _tx)
			{
				if(_tx.empty())
					return;

				_outputs[0 ][_frame] = dsp2sample<T>(_tx[0][0]);
				_outputs[2 ][_frame] = dsp2sample<T>(_tx[0][1]);
				_outputs[4 ][_frame] = dsp2sample<T>(_tx[0][2]);
				_outputs[6 ][_frame] = dsp2sample<T>(_tx[0][3]);
				_outputs[8 ][_frame] = dsp2sample<T>(_tx[0][4]);
				_outputs[10][_frame] = dsp2sample<T>(_tx[0][5]);

				if(_tx.size() < 2)
					return;

				_outputs[ 1][_frame] = dsp2sample<T>(_tx[1][0]);
				_outputs[ 3][_frame] = dsp2sample<T>(_tx[1][1]);
				_outputs[ 5][_frame] = dsp2sample<T>(_tx[1][2]);
				_outputs[ 7][_frame] = dsp2sample<T>(_tx[1][3]);
				_outputs[ 9][_frame] = dsp2sample<T>(_tx[1][4]);
				_outputs[11][_frame] = dsp2sample<T>(_tx[1][5]);
			});
		}

		template<typename T>
		void processAudioOutput(T* _outputs, const uint32_t _sampleFrames)
		{
			size_t writePos = 0;
			processAudioOutput<T>(_sampleFrames, [&](size_t _frame, TxFrame& _tx)
			{
				for(size_t s=0; s<_tx.size(); ++s)
				{
					const auto& slot = _tx[s];
					for (const auto v : slot)
						_outputs[writePos++] = dsp2sample<T>(v);
				}
			});
		}

		const auto& getAudioInputs() const { return m_audioInputs; }
		const auto& getAudioOutputs() const { return m_audioOutputs; }

		auto& getAudioInputs() { return m_audioInputs; }
		auto& getAudioOutputs() { return m_audioOutputs; }

	public:
		static constexpr uint32_t RingBufferSize = 8192 * 4;

	protected:
		void readRXimpl(RxFrame& _values);
		void writeTXimpl(const TxFrame& _values);

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

		RingBuffer<RxFrame, RingBufferSize, true, false> m_audioInputs;
		RingBuffer<TxFrame, RingBufferSize, true, false> m_audioOutputs;
		size_t m_latency = 0;
	};
}
