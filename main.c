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
#include "esp8266.h"


void DisableInterrupts(void); // Disable interrupts
void EnableInterrupts(void);  // Enable interrupts
long StartCritical (void);    // previous I bit, disable interrupts
void EndCritical(long sr);    // restore I bit to previous value
void WaitForInterrupt(void);  // low power mode

char Fetch[] = "GET /data/2.5/weather?q=Lexington&APPID=040bcc5b3b26a3d46897ea9cec5d76d0 HTTP/1.1\r\nHost:api.openweathermap.org\r\n\r\n";


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


void WifiTask(void *pvParameters)
{
	char data;
	
	const TickType_t xDelay = pdMS_TO_TICKS(5); //5ms delay
	TickType_t xLastWakeTime = xTaskGetTickCount();
	
	
  ESP8266_Init(115200);
  ESP8266_GetVersionNumber();
	while(1){
		
    while(1){// wait for touch
			vTaskDelayUntil(&xLastWakeTime, xDelay);
				data = UART_InCharNonBlock();
				if(data==0){
					continue;
				}
				
				ESP8266_GetStatus();
				if(ESP8266_MakeTCPConnection("api.openweathermap.org")){ // open socket in server
						ESP8266_SendTCP(Fetch);
					}
				ESP8266_CloseTCPConnection();
			}
}}


// Task used to read sensor values and put them on a shared queue
void AnalogSensorTask(void * pvParameters)
{
	uint8_t i;
	uint32_t val;
	
	const TickType_t delay = pdMS_TO_TICKS(1000);
	
	while(1)
	{
		if(pvParameters == 0)
		{
			val = ADC0_In();
		} else
		{
			val = ADC1_In();
		}
		
		// Put averaged sensor value on queue
		if(pvParameters == 0)
		{
			xQueueSendToBack(sensor_queue_0, &val, 0);
		} else
		{
			xQueueSendToBack(sensor_queue_1, &val, 0);
		}
		
		vTaskDelay(delay);
	}
}


// Task used to read sensor values from shared queue
void ConsumerTask(void * pvParameters)
{
	uint32_t val0;
	uint32_t val1;
	while(1)
	{
		// Get averaged sensor value from shared queue 0
		if(uxQueueMessagesWaiting(sensor_queue_0) > 0)
		{
			xQueueReceive(sensor_queue_0, &val0, 0);
			RTOS_Print_Int("Val From Queue 0 : ", val0);
		}
		
		// Get averaged sensor value from shared queue 1
		if(uxQueueMessagesWaiting(sensor_queue_1) > 0)
		{
			xQueueReceive(sensor_queue_1, &val1, 0);
			RTOS_Print_Int("Val From Queue 1 : ", val1);
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
	ADC1_Init();
	
	// Create one queue per sensor to place sensor data (allows another thread to read sensor data)
	sensor_queue_0 = xQueueCreate(10, sizeof(uint32_t));
	sensor_queue_1 = xQueueCreate(10, sizeof(uint32_t));
	
	// Create a thread for each task
	//xTaskCreate(BlinkerTask, "Blinker", 256, NULL, 1, NULL);
	//xTaskCreate(UARTTask, "Blinker", 256, NULL, 1, NULL);
	//xTaskCreate(WifiTask, "Blinker", 256, NULL, 1, NULL);
	xTaskCreate(AnalogSensorTask, "Sensor0", 256, (void *) 0, 1, NULL);
	xTaskCreate(AnalogSensorTask, "Sensor1", 256, (void *) 1, 1, NULL);
	xTaskCreate(ConsumerTask, "Consumer", 256, NULL, 1, NULL);
	
	// Startup of the FreeRTOS scheduler.  The program should block here.
	vTaskStartScheduler();
	
	// The following line should never be reached.  Failure to allocate enough
	//	memory from the heap would be one reason.
	for (;;);
}
