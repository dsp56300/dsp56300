#pragma once

namespace dsp56k
{
	class DSP;
	class Memory;

	class Essi
	{
		// _____________________________________________________________________________
		// members
		//

		DSP&		m_dsp;
		Memory&		m_memory;

		bool		m_toggleState;

		// _____________________________________________________________________________
		// implementation
		//
	public:
		Essi( DSP& _dsp, Memory& _memory ) : m_dsp(_dsp), m_memory( _memory ), m_toggleState(false)
		{
		}

		void exec();
	};
}
