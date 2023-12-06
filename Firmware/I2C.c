#include "I2C.h"
#include <FreeRTOS.h>

// I2C2 init Routine
void I2C2_Init(void)
{
	SYSCTL->RCGCI2C |= 0x0004;   // 1) activate I2C2
  SYSCTL->RCGCGPIO |= 0x10;    // 2) activate clock for Port E
  while((SYSCTL->PRGPIO&0x10) != 0x10){};  // allow time for clock to stabilize
	GPIOE->DEN |= 0x30;    	// 3) enable digital I/O on PE4 and PE5
	GPIOE->AFSEL |= 0x30;   // 4) enable alternate function on PE4 and PE5
  GPIOE->PCTL |= 0x330000;    // 5) set function of PE4 and PE5 to I2C
	GPIOE->ODR |= 0x20;			// 6) select open drain operation for SDA pin (PE5)
	I2C2->MCR = 0x010;				// 7) Set I2C2 as I2C master
	I2C2->MTPR = 24;					// 8) Set I2C2 clock to int(50MHz /(2*(6+4)*100000))-1 = 24
}


// Wait until I2C2 master is not busy and return I2C error code
int8_t I2C2_Wait_Until_Done(void)
{
	while(I2C2->MCS & 1);
	return I2C2->MCS & 0xE;
}


// Read given number of bytes from I2C2
int8_t I2C2_Read_Bytes(uint32_t slave_addr, uint32_t slave_mem_addr, uint8_t num_bytes, char * data)
{
	int8_t error;
	uint8_t bytes_read;
	
	// Don't do anything if we don't want to ready any data
	if(num_bytes == 0)
	{
		return -1;
	}

	// Send slave address and slave memory address
	I2C2->MSA = slave_addr << 1;
	I2C2->MDR = slave_mem_addr;
	I2C2->MCS = 3;
	
	// Wait until I2C2 master is not busy and return -1 if there is any I2C error
	error = I2C2_Wait_Until_Done();
	if(error != 0) return -1;
	
	// Send restart to slave device
	I2C2->MSA = (slave_addr << 1) + 1;
	
	// Determine whether or not to ack (ack if reading more than one byte)
	if(num_bytes == 1)
		I2C2->MCS = 7;
	else
		I2C2->MCS = 0xB;
	
	// Wait until I2C2 master is not busy and return -1 if there is any I2C error
	error = I2C2_Wait_Until_Done();
	if(error != 0) return -1;
	
	// Get first byte of data from I2C2
	data[0] = I2C2->MDR;
	bytes_read = 1;
	
	// Return without error if only one byte needs to be read
	if(num_bytes == 1)
	{
		while(I2C2->MCS & 0x40);	// Wait until I2C2 bus is not busy
		return 0;
	}
	
	// Read all but last byte of data from I2C2
	while(bytes_read < num_bytes-1)
	{
		I2C2->MCS = 9;
		error = I2C2_Wait_Until_Done();
		if(error != 0) return -1;
		data[bytes_read] = I2C2->MDR;
		bytes_read++;
	}
	
	// Read last byte
	I2C2->MCS = 5;
	error = I2C2_Wait_Until_Done();
	if(error != 0) return -1;
	data[bytes_read] = I2C2->MDR;
	
	// Wait until I2C2 bus is not busy
	while(I2C2->MCS & 0x40);
	
	return 0;
}