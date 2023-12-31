#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <FreeRTOS.h>
#include <task.h>
#include <queue.h>

#include "Ports.h"
#include "UART.h"
#include "I2C.h""
#include "esp8266.h"


void DisableInterrupts(void); // Disable interrupts
void EnableInterrupts(void);  // Enable interrupts
long StartCritical (void);    // previous I bit, disable interrupts
void EndCritical(long sr);    // restore I bit to previous value
void WaitForInterrupt(void);  // low power mode

uint32_t Server_Port=8000;
char Server_Addr[] = "172.20.10.6";
char Req_Device_Log[] =  "POST /device/log?temperature=%i&humidity=%i HTTP/1.1\r\nHost:10.0.0.30\r\n\r\n";
char Req_Device_Alert[] =  "POST /device/alert?message=%s HTTP/1.1\r\nHost:10.0.0.30\r\n\r\n";


QueueHandle_t sensor_queue_0;
QueueHandle_t sensor_queue_1;


// RTOS-safe print string to UART
void RTOS_Print(const char * str)
{
	vTaskSuspendAll();
	{
		UART_OutString(str);
	}
	xTaskResumeAll();
}


// RTOS-safe print string followed by an integer to UART
void RTOS_Print_Int(const char * str, uint32_t val)
{
	vTaskSuspendAll();
	{
		UART_OutString(str);
		UART_OutUDec(val);
		UART_OutChar('\n');
	}
	xTaskResumeAll();
}


void BlinkerTask(void *pvParameters)
{
	
	// Establish the task's period.
	const TickType_t xDelay = pdMS_TO_TICKS(1000);
	TickType_t xLastWakeTime = xTaskGetTickCount();

	for (;;) {
		// Block until the next release time.
		vTaskDelayUntil(&xLastWakeTime, xDelay);
		GPIOF->DATA =0x02;
		vTaskDelayUntil(&xLastWakeTime, xDelay);
		GPIOF->DATA = 0x04;
		vTaskDelayUntil(&xLastWakeTime, xDelay);
		GPIOF->DATA = 0x08;
	}

}	


void UARTTask(void *pvParameters)
{
	const TickType_t xDelay = pdMS_TO_TICKS(5); //5ms delay
	TickType_t xLastWakeTime = xTaskGetTickCount();
	
	char data;
	
	for (;;) {
		vTaskDelayUntil(&xLastWakeTime, xDelay);

		data = UART_InCharNonBlock();
		if(data==0){continue;}
		UART_OutChar(data);
	}
}

void HTTP_Request(char * addr,uint32_t port, char * buffer){
	ESP8266_Init(115200);
  //ESP8266_GetVersionNumber();
	 
	if(ESP8266_MakeTCPConnection(addr,port)){ // open socket in server
		ESP8266_SendTCP(buffer);
	}
	ESP8266_CloseTCPConnection();
}

void SendSensorData(uint32_t temperature, uint32_t humidity)
{
	char temp_buff[200];
	sprintf(temp_buff, Req_Device_Log, temperature, humidity);
	HTTP_Request(Server_Addr, Server_Port, temp_buff);
}

void SendAlert(char * message)
{
	char temp_buff[200];
	sprintf(temp_buff, Req_Device_Alert, message);
	HTTP_Request(Server_Addr, Server_Port, temp_buff);
}

// Task used to read sensor values and put them on a shared queue
const uint32_t MAX_MOISTURE = 2840;
const uint32_t MIN_MOISTURE = 1280;
void AnalogSensorTask(void * pvParameters)
{
	uint8_t i;
	uint32_t reading;
	uint32_t val;
	
	const TickType_t delay = pdMS_TO_TICKS(10000);
	
	while(1)
	{
		reading = ADC0_In();
		if(reading < MIN_MOISTURE) val = 100;
		else if(reading > MAX_MOISTURE) val = 0;
		else
			val = 100 - 100 * (reading-MIN_MOISTURE) / (MAX_MOISTURE-MIN_MOISTURE);
		
		// Put averaged sensor value on queue
		xQueueSendToBack(sensor_queue_0, &val, 0);
		
		vTaskDelay(delay);
	}
}


// Task used to read sensor values and put them on a shared queue
void I2CTemperatureTask(void * pvParameters)
{
	int8_t error;
	uint8_t i;
	char data[2] = {0x00, 0x00};
	uint16_t temperature_unconverted;
	int32_t temperature;
	
	const TickType_t delay = pdMS_TO_TICKS(10000);
	
	while(1)
	{
		error = I2C2_Read_Bytes(0x18, 0x05, 2, data);
		
		if(error == 0)
		{
			temperature_unconverted = 0;
			temperature_unconverted |= data[1];
			temperature_unconverted |= data[0] << 8;
			
			if(temperature_unconverted != 0xFFFF)
			{
				temperature = temperature_unconverted & 0xFFF;
				temperature >>= 4;
				
				if(temperature_unconverted & 0x1000)
					temperature -= 256;
			
				if(temperature < 70 && temperature > -100)
				{
					xQueueSendToBack(sensor_queue_1, &temperature, 0);
					vTaskDelay(delay);
				}
			}
		}
	}
}


// Task used to read sensor values from shared queue
void ConsumerTask(void * pvParameters)
{
	uint32_t val_moisture;
	uint32_t val_temperature;
	while(1)
	{
		// Get averaged sensor value from shared queue 0
		if(uxQueueMessagesWaiting(sensor_queue_0) > 0)
		{
			xQueueReceive(sensor_queue_0, &val_moisture, 0);
			RTOS_Print_Int("Val From Queue 0 : ", val_moisture);
		}
		
		// Get averaged sensor value from shared queue 1
		if(uxQueueMessagesWaiting(sensor_queue_1) > 0)
		{
			xQueueReceive(sensor_queue_1, &val_temperature, 0);
			RTOS_Print_Int("Val From Queue 1 : ", val_temperature);
		}
		
		// Send sensor data to server using ESP
		SendSensorData(val_moisture, val_temperature);
		if(val_temperature>24){
			SendAlert("UT%20Smart%20Farm%20High%20Temperature%21Alert");
		}
		if(val_moisture<10){
			SendAlert("UT%20Smart%20Farm%20-%20Low%20Moisture%20Detected%21");
		}
		if(val_moisture>90){
			SendAlert("UT%20Smart%20Farm%20-%20High%20Moisture%20Detected%21");
		}		
		
	
	}
}


int main(void)
{
	SystemInit();
	volatile uint32_t test=SystemCoreClock;	
	
	// Setup on-board output LED
	SYSCTL->RCGC2 |= 0x00000020;
	GPIOF->DIR |= 0x0E; 
	GPIOF->DEN |= 0x0E; 
	
	// Init UART for test output
	Output_Init(); 
	UART_OutString("UART0 Works!\n");
	
	// Init PortE for ADC0 and ADC1 input sensors
	ADC0_Init();
	
	// Init I2C2
	I2C2_Init();
	
	// Create one queue per sensor to place sensor data (allows another thread to read sensor data)
	sensor_queue_0 = xQueueCreate(1, sizeof(uint32_t));
	sensor_queue_1 = xQueueCreate(1, sizeof(uint32_t));
	
	// Create a thread for each task
	//xTaskCreate(BlinkerTask, "Blinker", 256, NULL, 1, NULL);
	//xTaskCreate(UARTTask, "Blinker", 256, NULL, 1, NULL);
	//xTaskCreate(WifiTask, "Blinker", 256, NULL, 1, NULL);
	xTaskCreate(AnalogSensorTask, "Sensor0", 256, NULL, 1, NULL);
	xTaskCreate(I2CTemperatureTask, "Sensor1", 256, NULL, 1, NULL);
	xTaskCreate(ConsumerTask, "Consumer", 256, NULL, 1, NULL);
	
	// Startup of the FreeRTOS scheduler.  The program should block here.
	vTaskStartScheduler();
	
	// The following line should never be reached.  Failure to allocate enough
	//	memory from the heap would be one reason.
	for (;;);
}
