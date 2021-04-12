#include "dsp.h"
#include "interrupts.h"
#include "hi08.h"

namespace dsp56k
{
	void HI08::exec()
	{
		if (m_hcr&1 && !m_data.empty())
		{
			
		}
	}
};
