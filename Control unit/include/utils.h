#include <Arduino.h>

#ifndef clearBit
#define clearBit(sfr, bit) (_SFR_BYTE(sfr) &= ~_BV(bit))
#endif
#ifndef setBit
#define setBit(sfr, bit) (_SFR_BYTE(sfr) |= _BV(bit))
#endif

//-----------------------------------------------------------------------------
// initAnalogComparator()
//-----------------------------------------------------------------------------
void initAnalogComparator(void)
{
	//---------------------------------------------------------------------
	// ACSR settings
	//---------------------------------------------------------------------
	// When this bit is written logic one, the power to the Analog
	// Comparator is switched off. This bit can be set at any time to turn
	// off the Analog Comparator. This will reduce power consumption in
	// Active and Idle mode. When changing the ACD bit, the Analog
	// Comparator Interrupt must be disabled by clearing the ACIE bit in
	// ACSR. Otherwise an interrupt can occur when the bit is changed.
	clearBit(ACSR, ACD);
	// When this bit is set, a fixed bandgap reference voltage replaces the
	// positive input to the Analog Comparator. When this bit is cleared,
	// AIN0 is applied to the positive input of the Analog Comparator. When
	// the bandgap referance is used as input to the Analog Comparator, it
	// will take a certain time for the voltage to stabilize. If not
	// stabilized, the first conversion may give a wrong value.
	clearBit(ACSR, ACBG);
	// When the ACIE bit is written logic one and the I-bit in the Status
	// Register is set, the Analog Comparator interrupt is activated.
	// When written logic zero, the interrupt is disabled.
	setBit(ACSR,ACIE);
	// When written logic one, this bit enables the input capture function
	// in Timer/Counter1 to be triggered by the Analog Comparator. The
	// comparator output is in this case directly connected to the input
	// capture front-end logic, making the comparator utilize the noise
	// canceler and edge select features of the Timer/Counter1 Input
	// Capture interrupt. When written logic zero, no connection between
	// the Analog Comparator and the input capture function exists. To
	// make the comparator trigger the Timer/Counter1 Input Capture
	// interrupt, the ICIE1 bit in the Timer Interrupt Mask Register
	// (TIMSK1) must be set.
	clearBit(ACSR,ACIC);
	// These bits determine which comparator events that trigger the Analog
	// Comparator interrupt.
	//	ACIS1	ACIS0	Mode
	//	0	0	Toggle
	//	0	1	Reserved
	//	1	0	Falling edge
	//	1	1	Rising edge
	setBit(ACSR,ACIS1);
	setBit(ACSR, ACIS0);

	//---------------------------------------------------------------------
	// DIDR1 settings
	//---------------------------------------------------------------------
	// When this bit is written logic one, the digital input buffer on the
	// AIN1/0 pin is disabled. The corresponding PIN Register bit will
	// always read as zero when this bit is set. When an analog signal is
	// applied to the AIN1/0 pin and the digital input from this pin is not
	// needed, this bit should be written logic one to reduce power
	// consumption in the digital input buffer.
	setBit(DIDR1, AIN1D);
	setBit(DIDR1, AIN0D);
}
