#include "Ports.h"
#include <FreeRTOS.h>

// ADC0 init Routine
void ADC0_Init(void)
{
	volatile unsigned long delay;
	
	SYSCTL->RCGCADC |= 0x0001;   // 1) activate ADC0
  SYSCTL->RCGCGPIO |= 0x10;    // 2) activate clock for Port E
  while((SYSCTL->PRGPIO&0x10) != 0x10){};  // allow time for clock to stabilize
  GPIOE->DIR &= ~0x04;    // 4) make PE2 input
  GPIOE->AFSEL |= 0x04;   // 5) enable alternate function on PE2
  GPIOE->DEN &= ~0x04;    // 6) disable digital I/O on PE2
  GPIOE->AMSEL |= 0x04;   // 7) enable analog function on PE2
		
  ADC0->PC &= ~0xF;
  ADC0->PC |= 0x1;             // 8) configure for 125K
  ADC0->SSPRI = 0x0123;        // 9) Sequencer 3 is highest priority
  ADC0->ACTSS &= ~0x0008;      // 10) disable sample sequencer 3
  ADC0->EMUX &= ~0xF000;       // 11) seq3 is software trigger
  ADC0->SSMUX3 &= ~0x000F;
  ADC0->SSMUX3 += 1;           // 12) channel Ain1 (PE2)
  ADC0->SSCTL3 = 0x0006;       // 13) no TS0 D0, yes IE0 END0
  ADC0->IM &= ~0x0008;         // 14) disable SS3 interrupts
  ADC0->ACTSS |= 0x0008;       // 15) enable sample sequencer 3
	ADC0->SAC = 5;							 // Oversample for higher precision readings
}


// Busy Wait ADC0 Read
uint32_t ADC0_In(void)
{
	uint32_t result;
	
	// 1) initiate SS3
	ADC0->PSSI = 0x0008;
	
  // 2) wait for conversion done
	while((ADC0->RIS&0x08)==0){};
		
  // 3) read result
	result = ADC0->SSFIFO3&0xFFF;
		
  // 4) acknowledge completion
	ADC0->ISC = 0x0008;

  return result;
}


// ADC1 init Routine
void ADC1_Init(void)
{
	volatile unsigned long delay;
	
	SYSCTL->RCGCADC |= 0x0002;   // 1) activate ADC1
  SYSCTL->RCGCGPIO |= 0x10;    // 2) activate clock for Port E
  while((SYSCTL->PRGPIO&0x10) != 0x10){};  // allow time for clock to stabilize
  GPIOE->DIR &= ~0x08;    // 4) make PE3 input
  GPIOE->AFSEL |= 0x08;   // 5) enable alternate function on PE3
  GPIOE->DEN &= ~0x08;    // 6) disable digital I/O on PE3
  GPIOE->AMSEL |= 0x08;   // 7) enable analog function on PE3
		
  ADC1->PC &= ~0xF;
  ADC1->PC |= 0x1;             // 8) configure for 125K
  ADC1->SSPRI = 0x0123;        // 9) Sequencer 3 is highest priority
  ADC1->ACTSS &= ~0x0008;      // 10) disable sample sequencer 3
  ADC1->EMUX &= ~0xF000;       // 11) seq3 is software trigger
  ADC1->SSMUX3 &= ~0x000F;		 // 12) channel Ain0 (PE3)
  ADC1->SSCTL3 = 0x0006;       // 13) no TS0 D0, yes IE0 END0
  ADC1->IM &= ~0x0008;         // 14) disable SS3 interrupts
  ADC1->ACTSS |= 0x0008;       // 15) enable sample sequencer 3
	ADC1->SAC = 5;							 // Oversample for higher precision readings
}


// Busy Wait ADC1 Read
uint32_t ADC1_In(void)
{
	uint32_t result;
	
	// 1) initiate SS3
	ADC1->PSSI = 0x0008;
	
  // 2) wait for conversion done
	while((ADC1->RIS&0x08)==0){};
		
  // 3) read result
	result = ADC1->SSFIFO3&0xFFF;
		
  // 4) acknowledge completion
	ADC1->ISC = 0x0008;

  return result;
}