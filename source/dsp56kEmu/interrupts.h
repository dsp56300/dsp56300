#pragma once

namespace dsp56k
{
	enum InterruptVectorAddress		// Register VBA + InterruptVectorAddress = effective address
	{
		// Level 3 (non-maskable)
		Vba_HardwareRESET							= 0x00,			// Hardware RESET							
		Vba_Stackerror								= 0x02,			// Stack error								
		Vba_Illegalinstruction						= 0x04,			// Illegal instruction						
		Vba_DebugRequest							= 0x06,			// Debug request interrupt					
		Vba_Trap									= 0x08,			// Trap										
		Vba_NMI										= 0x0A,			// Nonmaskable interrupt (NMI)				
		Vba_Reserved0C								= 0x0C,			// Reserved									
		Vba_Reserved0E								= 0x0E,			// Reserved									

		// Levels 0-2 (maskable)
		Vba_IRQA									= 0x10,			// IRQA											
		Vba_IRQB									= 0x12,			// IRQB											
		Vba_IRQC									= 0x14,			// IRQC											
		Vba_IRQD									= 0x16,			// IRQD											

		Vba_DMAchannel0								= 0x18,			// DMA channel 0								
		Vba_DMAchannel1								= 0x1A,			// DMA channel 1								
		Vba_DMAchannel2								= 0x1C,			// DMA channel 2								
		Vba_DMAchannel3								= 0x1E,			// DMA channel 3								
		Vba_DMAchannel4								= 0x20,			// DMA channel 4								
		Vba_DMAchannel5								= 0x22,			// DMA channel 5							
	};

	enum InterruptVectorAddress56303
	{
		Vba_TIMER0compare							= 0x24,			// TIMER 0 compare								
		Vba_TIMER0overflow							= 0x26,			// TIMER 0 overflow								
		Vba_TIMER1compare							= 0x28,			// TIMER 1 compare								
		Vba_TIMER1overflow							= 0x2A,			// TIMER 1 overflow								
		Vba_TIMER2compare							= 0x2C,			// TIMER 2 compare								
		Vba_TIMER2overflow							= 0x2E,			// TIMER 2 overflow								

		Vba_ESSI0receivedata						= 0x30,			// ESSI0 receive data							
		Vba_ESSI0receivedatawithexceptionstatus		= 0x32,			// ESSI0 receive data with exception status		
		Vba_ESSI0receivelastslot					= 0x34,			// ESSI0 receive last slot						
		Vba_ESSI0transmitdata						= 0x36,			// ESSI0 transmit data							
		Vba_ESSI0transmitdatawithexceptionstatus	= 0x38,			// ESSI0 transmit data with exception status	
		Vba_ESSI0transmitlastslot					= 0x3A,			// ESSI0 transmit last slot						
		Vba_Reserved3C								= 0x3C,			// Reserved										
		Vba_Reserved3E								= 0x3E,			// Reserved										

		Vba_ESSI1receivedata						= 0x40,			// ESSI1 receive data							
		Vba_ESSI1receivedatawithexceptionstatus		= 0x42,			// ESSI1 receive data with exception status		
		Vba_ESSI1receivelastslot					= 0x44,			// ESSI1 receive last slot						
		Vba_ESSI1transmitdata						= 0x46,			// ESSI1 transmit data							
		Vba_ESSI1transmitdatawithexceptionstatus	= 0x48,			// ESSI1 transmit data with exception status	
		Vba_ESSI1transmitlastslot					= 0x4A,			// ESSI1 transmit last slot						
		Vba_Reserved4C								= 0x4C,			// Reserved										
		Vba_Reserved4E								= 0x4E,			// Reserved										

		Vba_SCIReceiveData							= 0x50,			// SCI receive data								
		Vba_SCIReceiveDataWithExceptionStatus		= 0x52,			// SCI receive data with exception status		
		Vba_SCITransmitData							= 0x54,			// SCI transmit data							
		Vba_SCIIdleLine								= 0x56,			// SCI idle line								
		Vba_SCITimer								= 0x58,			// SCI timer									
		Vba_Reserved5A								= 0x5A,			// Reserved										
		Vba_Reserved5C								= 0x5C,			// Reserved										
		Vba_Reserved5E								= 0x5E,			// Reserved										

		Vba_HostReceiveDataFull						= 0x60,			// Host receive data full						
		Vba_HostTransmitDataEmpty					= 0x62,			// Host transmit data empty						
		Vba_HostCommandDefault						= 0x64,			// Host command (default)						
		Vba_Reserved66								= 0x66,			// Reserved
		// ....
		Vba_ReservedFE								= 0xFE,			// Reserved
	};
	
	enum InterruptVectorAddress56362
	{
		Vba_Reserved24									= 0x24,		// Reserved
		Vba_Reserved26									= 0x26,		// Reserved
		Vba_DAX_Underrun_Error							= 0x28,		// DAX Underrun Error
		Vba_DAX_Block_Transferred						= 0x2A,		// DAX Block Transferred
		Vba_Reserved2C									= 0x2C,		// Reserved
		Vba_DAX_Audio_Data_Empty						= 0x2E,		// DAX Audio Data Empty
		Vba_ESAI_Receive_Data							= 0x30,		// ESAI Receive Data
		Vba_ESAI_Receive_Even_Data						= 0x32,		// ESAI Receive Even Data
		Vba_ESAI_Receive_Data_With_Exception_Status		= 0x34,		// ESAI Receive Data With Exception Status
		Vba_ESAI_Receive_Last_Slot						= 0x36,		// ESAI Receive Last Slot
		Vba_ESAI_Transmit_Data							= 0x38,		// ESAI Transmit Data
		Vba_ESAI_Transmit_Even_Data						= 0x3A,		// ESAI Transmit Even Data
		Vba_ESAI_Transmit_Data_with_Exception_Status	= 0x3C,		// ESAI Transmit Data with Exception Status
		Vba_ESAI_Transmit_Last_Slot						= 0x3E,		// ESAI Transmit Last Slot
		Vba_SHI_Transmit_Data							= 0x40,		// SHI Transmit Data
		Vba_SHI_Transmit_Underrun_Error					= 0x42,		// SHI Transmit Underrun Error
		Vba_SHI_Receive_FIFO_Not_Empty					= 0x44,		// SHI Receive FIFO Not Empty
		Vba_Reserved									= 0x46,		// Reserved
		Vba_SHI_Receive_FIFO_Full						= 0x48,		// SHI Receive FIFO Full
		Vba_SHI_Receive_Overrun_Error					= 0x4A,		// SHI Receive Overrun Error
		Vba_SHI_Bus_Error								= 0x4C,		// SHI Bus Error
//		Vba_Reserved4E									= 0x4E,		// Reserved
		Vba_Reserved50									= 0x50,		// Reserved
		Vba_Reserved52									= 0x52,		// Reserved
		Vba_TIMER0_Compare								= 0x54,		// TIMER0 Compare
		Vba_TIMER0_Overflow								= 0x56,		// TIMER0 Overflow
		Vba_TIMER1_Compare								= 0x58,		// TIMER1 Compare
		Vba_TIMER1_Overflow								= 0x5A,		// TIMER1 Overflow
		Vba_TIMER2_Compare								= 0x5C,		// TIMER2 Compare
		Vba_TIMER2_Overflow								= 0x5E,		// TIMER2 Overflow
		Vba_Host_Receive_Data_Full						= 0x60,		// Host Receive Data Full
		Vba_Host_Transmit_Data_Empty					= 0x62,		// Host Transmit Data Empty
		Vba_Host_Command								= 0x64,		// Host Command (Default)
		Vba_Reserved_066								= 0x66,		// Reserved
		// ...
//		Vba_ReservedFE									= 0xFE,		// Reserved
	};
}
