// #include <Arduino.h>
// // #include <SoftwareSerial.h>
// //Bluetooth firmware linvorV1.8

// #ifndef cbi
// #define cbi(sfr, bit) (_SFR_BYTE(sfr) &= ~_BV(bit))
// #endif
// #ifndef sbi
// #define sbi(sfr, bit) (_SFR_BYTE(sfr) |= _BV(bit))
// #endif


// //Осциллограф  ©RasyakRoman

// unsigned long currentTime;
// unsigned long loopTime;

// byte N = 0;
// // byte Rb = 255;
// uint8_t MyBuff[260];

// // SoftwareSerial mySerial(4, 2); // RX, TX
// // String command = ""; // Stores response of the HC-06 Bluetooth device

// void setup()
// {
//     pinMode(LED_BUILTIN, OUTPUT);
//     unsigned long baudRate = 230400;
//     // unsigned long baudRate = 460800;
//     // unsigned long baudRate = 115200;
//     // unsigned long baudRate = 38400;
//     // unsigned long baudRate = 1382400;
//     //  unsigned long baudRate = 57600;
//     // unsigned long baudRate = 9600;
//     // unsigned long baudRate = 115200;

//     Serial.begin(baudRate);
//     //mySerial.begin(baudRate);

//     ADCSRA = (1 << ADEN) | (1 << ADSC) | (1 << ADATE) | (0 << ADIF) | (1 << ADIE) | (0 << ADPS2) | (1 << ADPS1) | (0 << ADPS0);

//     //REFS1:REFS0
//     //00    AREF
//     //01    AVcc, с внешним конденсатором на AREF
//     //10    Резерв
//     //11    Внутренний 2.56В  источник, с внешним конденсатором на AREF
//     ADMUX = (0 << REFS1) | (0 << REFS0) 
//         | (1 << ADLAR) | (0 << MUX3) | (0 << MUX2) | (0 << MUX1) | (0 << MUX0);

//    //  These bits determine the division factor between the system clock
//    // frequency and the input clock to the ADC.
//    //	ADPS2	ADPS1	ADPS0	Division Factor
//    //	 0	     0	      0	        2
//    //	 0	     0	      1	        2
//    //	 0	     1	      0	        4
//    //	 0	     1	      1	        8
//    //	 1	     0	      0	        16
//    //	 1	     0	      1	        32
//    //	 1	     1	      0	        64
//    //	 1	     1	      1	        128
//     // cbi(ADCSRA, ADPS2); sbi(ADCSRA, ADPS1); cbi(ADCSRA, ADPS0); //4
//     cbi(ADCSRA, ADPS2); sbi(ADCSRA, ADPS1); sbi(ADCSRA, ADPS0); //8
// //    sbi(ADCSRA, ADPS2); cbi(ADCSRA, ADPS1); cbi(ADCSRA, ADPS0); //16
// //    sbi(ADCSRA, ADPS2); cbi(ADCSRA, ADPS1); sbi(ADCSRA, ADPS0); //32
// //    sbi(ADCSRA, ADPS2); sbi(ADCSRA, ADPS1); cbi(ADCSRA, ADPS0); //64
// //    sbi(ADCSRA, ADPS2); sbi(ADCSRA, ADPS1); sbi(ADCSRA, ADPS0); //128

// }

// unsigned long  counter = 0;
// // bool writeADCvalue = true;
// unsigned long prevMicros = 0;
// bool timeTaken = false;
// unsigned long time = 0;

// long moreThan200Counter = 0;
// long resetCounter = 0;
// long lastResetVal = 0;


// void loop()
// {
//     currentTime = millis();

//     if (currentTime >= (loopTime + 1000))  
//     {

//         // Serial.write("AT");
//         // Serial.flush();
//         //mySerial.write("AT+BAUD4");
//         // Serial.write("AT+BAUD8");//115200
//         // Serial.write("AT+BAUD9");//230400
//         //  Serial.write("AT+BAUDA");//460800
//         // mySerial.write("AT+BAUDB");//921600
//         // mySerial.write("AT+BAUDC");//1382400
//         // Serial.write("AT+VERSION");

//         // cbi(ADCSRA, ADIE);//Выключить прерывание АЦП
//         // sbi(ADCSRA, ADEN);//Disable ADC
//         ADCSRA &= ~(1 << ADIE); //Выключить прерывание АЦП
//         // mySerial.write(100); mySerial.write((byte)0); mySerial.write(100);

//         // for (int i = 0; i < 256; i++)
//         // {
//         //     MyBuff[i] = i;
//         // }

//         // MyBuff[256] = 1;
//         // MyBuff[257] = 100;
//         // MyBuff[258] = 1;
//         // Serial.write(MyBuff, 259);
//         //mySerial.write(MyBuff, 259);


//         // mySerial.write(OCR1A); //Не используются
//         // mySerial.write(OCR1B); //Не используются
//         // Serial.println(moreThan200Counter);
//         // Serial.println(resetCounter);
//         // Serial.println(lastResetVal);
//         // Serial.write(1); Serial.write(100); Serial.write(1);
//         // cbi(ADCSRA, ADEN);//Enable ADC
//         // sbi(ADCSRA, ADIE);// Включить прерывание АЦП
//         ADCSRA |= (1 << ADIE); // Включить прерывание АЦП
//         loopTime = currentTime;

//         // Serial.println((micros() - startMicros));
//     };



//     // if (Serial.available() > 0) {
//     //     // Rb = Serial.read();
//     //     // ADCSRA = (ADCSRA >> 3) << 3 | Rb;
//     //     digitalWrite(LED_BUILTIN, HIGH); 
//     // }



// }


// ISR(ADC_vect)
// {
//     MyBuff[N] = ADCH;
//     N++;
//     // counter++;
// }


/*
1200 AT+BAUD1

2400 AT+BAUD2

4800 AT+BAUD3

9600 AT+BAUD4

19200 AT+BAUD5

38400 AT+BAUD6

57600 AT+BAUD7

115200 AT+BAUD8

230400 AT+BAUD9

460800 AT+BAUDA

921600 AT+BAUDB

1382400 AT+BAUDC
*/

//unsigned long currentTime;
//unsigned long loopTime;
//
//// Defines for setting and clearing register bits
//#ifndef cbi
//#define cbi(sfr, bit) (_SFR_BYTE(sfr) &= ~_BV(bit))
//#endif
//#ifndef sbi
//#define sbi(sfr, bit) (_SFR_BYTE(sfr) |= _BV(bit))
//#endif
//
//volatile unsigned int N = 0;
//int Rb = 1024;
//uint8_t MyBuff[1024];
//
//void setup()
//{
//    //Serial.begin(115200);
//    //Serial.begin(9600);
//    Serial.begin(2000000);
//
//
//    sbi(ADCSRA, ADEN);//Disable ADC
//
//    //REFS1:REFS0
//    //00    AREF
//    //01    AVcc, с внешним конденсатором на AREF
//    //10    Резерв
//    //11    Внутренний 2.56В  источник, с внешним конденсатором на AREF
//    ADMUX = (0 << REFS1) | (1 << REFS0) | (1 << ADLAR) | (0 << MUX3) | (0 << MUX2) | (0 << MUX1) | (0 << MUX0);
//
//    //ADCSRA = (1 << ADEN) | (1 << ADSC) | (1 << ADATE) | (0 << ADIF) | (1 << ADIE);
//        //| (1 << ADPS2) | (0 << ADPS1) | (0 << ADPS0);
//
//    // These bits determine the division factor between the system clock
//    // frequency and the input clock to the ADC.
//    //	ADPS2	ADPS1	ADPS0	Division Factor
//    //	 0	     0	      0	        2
//    //	 0	     0	      1	        2
//    //	 0	     1	      0	        4
//    //	 0	     1	      1	        8
//    //	 1	     0	      0	        16
//    //	 1	     0	      1	        32
//    //	 1	     1	      0	        64
//    //	 1	     1	      1	        128
//    cbi(ADCSRA, ADPS2);
//    sbi(ADCSRA, ADPS1);
//    cbi(ADCSRA, ADPS0);
//
//    // If ADATE in ADCSRA is written to one, the value of these bits
//    // selects which source will trigger an ADC conversion. If ADATE is
//    // cleared, the ADTS2:0 settings will have no effect. A conversion will
//    // be triggered by the rising edge of the selected Interrupt Flag. Note
//    // that switching from a trigger source that is cleared to a trigger
//    // source that is set, will generate a positive edge on the trigger
//    // signal. If ADEN in ADCSRA is set, this will start a conversion.
//    // Switching to Free Running mode (ADTS[2:0]=0) will not cause a
//    // trigger event, even if the ADC Interrupt Flag is set.
//    //	ADTS2	ADTS1	ADTS0	Trigger source
//    //	0	0	0	Free Running mode
//    //	0	0	1	Analog Comparator
//    //	0	1	0	External Interrupt Request 0
//    //	0	1	1	Timer/Counter0 Compare Match A
//    //	1	0	0	Timer/Counter0 Overflow
//    //	1	0	1	Timer/Counter1 Compare Match B
//    //	1	1	0	Timer/Counter1 Overflow
//    //	1	1	1	Timer/Counter1 Capture Event
//    cbi(ADCSRB, ADTS2);
//    cbi(ADCSRB, ADTS1);
//    cbi(ADCSRB, ADTS0);
//
//    // In Single Conversion mode, write this bit to one to start each
//    // conversion. In Free Running mode, write this bit to one to start the
//    // first conversion. The first conversion after ADSC has been written
//    // after the ADC has been enabled, or if ADSC is written at the same
//    // time as the ADC is enabled, will take 25 ADC clock cycles instead of
//    // the normal 13. This first conversion performs initialization of the
//    // ADC. ADSC will read as one as long as a conversion is in progress.
//    // When the conversion is complete, it returns to zero. Writing zero to
//    // this bit has no effect.
//    sbi(ADCSRA, ADSC);
//
//    // When this bit is written to one, Auto Triggering of the ADC is
//    // enabled. The ADC will start a conversion on a positive edge of the
//    // selected trigger signal. The trigger source is selected by setting
//    // the ADC Trigger Select bits, ADTS in ADCSRB.
//    sbi(ADCSRA, ADATE);
//
//    // When this bit is written to one and the I-bit in SREG is set, the
//    // ADC Conversion Complete Interrupt is activated.
//    sbi(ADCSRA, ADIE);
//
//   
//
//    // Writing this bit to one enables the ADC. By writing it to zero, the
//    // ADC is turned off. Turning the ADC off while a conversion is in
//    // progress, will terminate this conversion.
//    //sbi(ADCSRA, ADEN);//Enable ADC
//}






// int numSamples=0;
// long t, t0;

// void setup()
// {
//   Serial.begin(115200);

//   ADCSRA = 0;             // clear ADCSRA register
//   ADCSRB = 0;             // clear ADCSRB register
//   ADMUX |= (0 & 0x07);    // set A0 analog input pin
//   ADMUX |= (1 << REFS0);  // set reference voltage
//   ADMUX |= (1 << ADLAR);  // left align ADC value to 8 bits from ADCH register

//   // sampling rate is [ADC clock] / [prescaler] / [conversion clock cycles]
//   // for Arduino Uno ADC clock is 16 MHz and a conversion takes 13 clock cycles
//   //ADCSRA |= (1 << ADPS2) | (1 << ADPS0);    // 32 prescaler for 38.5 KHz
//   ADCSRA |= (1 << ADPS2);                     // 16 prescaler for 76.9 KHz
//   //ADCSRA |= (1 << ADPS1) | (1 << ADPS0);    // 8 prescaler for 153.8 KHz

//   ADCSRA |= (1 << ADATE); // enable auto trigger
//   ADCSRA |= (1 << ADIE);  // enable interrupts when measurement complete
//   ADCSRA |= (1 << ADEN);  // enable ADC
//   ADCSRA |= (1 << ADSC);  // start ADC measurements
// }

// ISR(ADC_vect)
// {
//   byte x = ADCH;  // read 8 bit value from ADC
//   numSamples++;
// }
  
// void loop()
// {
//   if (numSamples>=1000)
//   {
//     t = micros()-t0;  // calculate elapsed time

//     Serial.print("Sampling frequency: ");
//     Serial.print((float)1000000/t);
//     Serial.println(" KHz");
//     delay(2000);
    
//     // restart
//     t0 = micros();
//     numSamples=0;
//   }
// }


// int numSamples=0;
// long t, t0;
// unsigned long test = 0;

// byte MyBuff[260];
// byte ADCBufferPos = 0;


// void setup()
// {
//   Serial.begin(230400);

//   ADCSRA = 0;             // clear ADCSRA register
//   ADCSRB = 0;             // clear ADCSRB register
//   ADMUX |= (0 & 0x07);    // set A0 analog input pin
//   ADMUX |= (1 << REFS0);  // set reference voltage
//   ADMUX |= (1 << ADLAR);  // left align ADC value to 8 bits from ADCH register

//   // sampling rate is [ADC clock] / [prescaler] / [conversion clock cycles]
//   // for Arduino Uno ADC clock is 16 MHz and a conversion takes 13 clock cycles
//   //ADCSRA |= (1 << ADPS2) | (1 << ADPS0);    // 32 prescaler for 38.5 KHz
// //   ADCSRA |= (1 << ADPS2);                     // 16 prescaler for 76.9 KHz
//   //ADCSRA |= (1 << ADPS1) | (1 << ADPS0);    // 8 prescaler for 153.8 KHz
//   ADCSRA |= (1 << ADPS1);    // 4 prescaler for  KHz

//   ADCSRA |= (1 << ADATE); // enable auto trigger
//   ADCSRA |= (1 << ADIE);  // enable interrupts when measurement complete
//   ADCSRA |= (1 << ADEN);  // enable ADC
//   ADCSRA |= (1 << ADSC);  // start ADC measurements
// }

// ISR(ADC_vect)
// {
// //   byte x = ADCH;  // read 8 bit value from ADC
//   numSamples++;
// //   if (numSamples > 1000)
// //   {
// //       test = ADCH;
// //   }

//     MyBuff[ADCBufferPos] = ADCH;
//     // ADCBufferPos++;
// }
  
// void loop()
// {
//   if (numSamples >= 10000)
//   {
//     t = micros() - t0;  // calculate elapsed time

//     Serial.print(MyBuff[ADCBufferPos]);
//     ADCBufferPos++;
//     // Serial.println(" time");

//     Serial.print("Sampling frequency: ");
//     Serial.print((float)10000000 / t);
//     Serial.println(" KHz");
//     delay(1000);
    
//     // restart
//     t0 = micros();
//     numSamples=0;
//   }
// }



// const byte bufSize = 60;
// byte buf[bufSize];

/* 				for (uint8_t i = 0; i < 255; i++)
        {
          Serial.println(adcBuf[i]);
          // Serial.print(", ");
          // Serial.println();
        }

        delay(50);
 */

// char res[CHAR_BUF_SIZE];

// sprintf(res, "%d", sensor0Readings);
// display.drawStr(57, SCREEN_TOP_MARGIN_PX, res);

/* 
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
  setBit(ACSR, ACIE);
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
  clearBit(ACSR, ACIC);
  // These bits determine which comparator events that trigger the Analog
  // Comparator interrupt.
  //	ACIS1	ACIS0	Mode
  //	0	0	Toggle
  //	0	1	Reserved
  //	1	0	Falling edge
  //	1	1	Rising edge
  setBit(ACSR, ACIS1);
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
 */