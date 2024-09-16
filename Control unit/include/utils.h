#include <Arduino.h>

#ifndef clearBit
#define clearBit(sfr, bit) (_SFR_BYTE(sfr) &= ~_BV(bit))
#endif
#ifndef setBit
#define setBit(sfr, bit) (_SFR_BYTE(sfr) |= _BV(bit))
#endif

/* void enableADCinterrupt()
{
  setBit(ADCSRA, ADIE);
} */

#define enableADCinterrupt() (setBit(ADCSRA, ADIE))

/* void disableADCinterrupt()
{
  clearBit(ADCSRA, ADIE);
} */

#define disableADCinterrupt() (clearBit(ADCSRA, ADIE))

enum class ADCPrescaler
{
  ADC_PRESCALER_128 = 128,
  ADC_PRESCALER_64 = 64,
  ADC_PRESCALER_32 = 32,
  ADC_PRESCALER_16 = 16,
  ADC_PRESCALER_8 = 8,
  ADC_PRESCALER_4 = 4,
  ADC_PRESCALER_2 = 2
};
#define startADCconversion() (ADCSRA |= (1 << ADSC));

void setADCprescaler(ADCPrescaler prescaler)
{
  // sampling rate is [ADC clock] / [prescaler] / [conversion clock cycles]
  // for Arduino Uno ADC clock is 16 MHz and a conversion takes 13 clock cycles
  switch (prescaler)
  {
  case ADCPrescaler::ADC_PRESCALER_2: // 2 prescaler for 615,2 KHz sampling rate
    setBit(ADCSRA, ADPS0);
    clearBit(ADCSRA, ADPS1);
    clearBit(ADCSRA, ADPS2);
    break;
  case ADCPrescaler::ADC_PRESCALER_4: // 4 prescaler for 307,6 KHz  sampling rate
    clearBit(ADCSRA, ADPS0);
    setBit(ADCSRA, ADPS1);
    clearBit(ADCSRA, ADPS2);
    break;
  case ADCPrescaler::ADC_PRESCALER_8: // 8 prescaler for 153.8 KHz  sampling rate
    setBit(ADCSRA, ADPS0);
    setBit(ADCSRA, ADPS1);
    clearBit(ADCSRA, ADPS2);
    break;
  case ADCPrescaler::ADC_PRESCALER_16: // 16 prescaler for 76.9 KHz  sampling rate
    clearBit(ADCSRA, ADPS0);
    clearBit(ADCSRA, ADPS1);
    setBit(ADCSRA, ADPS2);
    break;
  case ADCPrescaler::ADC_PRESCALER_32: // 32 prescaler for 38.5 KHz  sampling rate
    setBit(ADCSRA, ADPS0);
    clearBit(ADCSRA, ADPS1);
    setBit(ADCSRA, ADPS2);
    break;
  case ADCPrescaler::ADC_PRESCALER_64: // 64 prescaler for 19.25 KHz  sampling rate
    clearBit(ADCSRA, ADPS0);
    setBit(ADCSRA, ADPS1);
    setBit(ADCSRA, ADPS2);
    break;
  case ADCPrescaler::ADC_PRESCALER_128: // 128 prescaler for 9,625 KHz sampling rate
    setBit(ADCSRA, ADPS0);
    setBit(ADCSRA, ADPS1);
    setBit(ADCSRA, ADPS2);
    break;
  default:
    break;
  }
}

void setADCautoTriggerEnabled(bool isEnabled)
{
  if (isEnabled)
  {
    setBit(ADCSRA, ADATE);
  }
  else
  {
    clearBit(ADCSRA, ADATE);
  }
}

#define setADCInputPin(adcPinIndex) (ADMUX = (ADMUX & 0b11110000) | (adcPinIndex))

void SerialPrintf(char *fmt, ...)
{
  char buf[128]; // resulting string limited to 128 chars
  va_list args;
  va_start(args, fmt);
  vsnprintf(buf, 128, fmt, args);
  va_end(args);
  Serial.print(buf);
}