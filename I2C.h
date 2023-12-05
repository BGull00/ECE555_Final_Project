#include <stdint.h>

// I2C2 init Routine
void I2C2_Init(void);

// Read given number of bytes from I2C2
int8_t I2C2_Read_Bytes(uint32_t slave_addr, uint32_t slave_mem_addr, uint8_t num_bytes, char * data);