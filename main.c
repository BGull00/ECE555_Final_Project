#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <FreeRTOS.h>
#include <task.h>


#include "UART.h"
#include "esp8266.h"


void DisableInterrupts(void); // Disable interrupts
void EnableInterrupts(void);  // Enable interrupts
long StartCritical (void);    // previous I bit, disable interrupts
void EndCritical(long sr);    // restore I bit to previous value
void WaitForInterrupt(void);  // low power mode

char Fetch[] = "GET /data/2.5/weather?q=Lexington&APPID=040bcc5b3b26a3d46897ea9cec5d76d0 HTTP/1.1\r\nHost:api.openweathermap.org\r\n\r\n";

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
int main(void)
{
	SystemInit();
	volatile uint32_t test=SystemCoreClock;	
	
	SYSCTL->RCGC2 |= 0x00000020;   // 1) F clock
	GPIOF->DIR |= 0x0E; 
	GPIOF->DEN |= 0x0E; 
	
	Output_Init(); 
	UART_OutString("UART0 Works!");
	

	
	xTaskCreate(BlinkerTask, "Blinker", 256, NULL, 1, NULL);
	//xTaskCreate(UARTTask, "Blinker", 256, NULL, 1, NULL);
	xTaskCreate(WifiTask, "Blinker", 256, NULL, 1, NULL);
	
	
	// Startup of the FreeRTOS scheduler.  The program should block here.  
	vTaskStartScheduler();
	
	// The following line should never be reached.  Failure to allocate enough
	//	memory from the heap would be one reason.
	for (;;);
	
}
