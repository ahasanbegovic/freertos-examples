#include "FreeRTOS.h"
#include "task.h"
#include "led.h"
#include "board.h"
#include "compiler.h"
#include "gpio.h"
#include "pm.h"
#include "usart.h"

//Stack used for the scheduler
#define MINIMAL_STACK_SIZE configMINIMAL_STACK_SIZE

//Storing the periods for different LEDs
int periods[7];
//Counters for the LEDs that need to remain on because of a button press
int stayOn[3];
//LEDs from led.h
int LEDS[8] = {LED0, LED1, LED2, LED3, LED4, LED5, LED6, LED7};
bool was_pressed;
portTickType lastWakeTimes[7];

void init_usart(void){
	static const gpio_map_t USART_SERIAL_GPIO_MAP =
	{
		{ serialPORT_USART_RX_PIN, serialPORT_USART_RX_FUNCTION },
		{ serialPORT_USART_TX_PIN, serialPORT_USART_TX_FUNCTION }	
	};
	static const gpio_map_t USART_DEBUG_GPIO_MAP =
	{
		{ configDBG_USART_RX_PIN , configDBG_USART_RX_FUNCTION },
		{ configDBG_USART_TX_PIN , configDBG_USART_TX_FUNCTION }
	};
	static const usart_options_t USART_OPTIONS =
	{
		.baudrate = 115200 ,
		.charlength = 8,
		.paritytype = USART_NO_PARITY ,
		.stopbits = USART_1_STOPBIT ,
		.channelmode = USART_NORMAL_CHMODE
	};
	
	pm_switch_to_osc0(&AVR32_PM, FOSC0, OSC0_STARTUP);
	gpio_enable_module(USART_SERIAL_GPIO_MAP, 2);
	gpio_enable_module(USART_DEBUG_GPIO_MAP, 2);
	
	usart_init_rs232(serialPORT_USART, &USART_OPTIONS, FOSC0);
	usart_init_rs232(configDBG_USART, &USART_OPTIONS, FOSC0);
}


//Task for checking the buttons
void vButtonCheck(void *pvParameters){
	portTickType xLastWakeTime;
	xLastWakeTime = xTaskGetTickCount();
	//Period of 123 milliseconds, so it rarely interferes with the other tasks
	const portTickType xPeriodTime = 123;
	unsigned char BUTTON =  *(unsigned char*)pvParameters;
	uint32_t button_port = (GPIO_PUSH_BUTTON_0 >> 5);
	uint32_t button_pin;
	if (BUTTON == 0){
		button_pin =  (1 << (GPIO_PUSH_BUTTON_0 & 0x1f));
	}
	else if (BUTTON == 1){
		button_pin =  (1 << (GPIO_PUSH_BUTTON_1 & 0x1f));
	}
	else if (BUTTON == 2){
		button_pin =  (1 << (GPIO_PUSH_BUTTON_2 & 0x1f));
	}
	while(1){
		bool button_state = AVR32_GPIO.port[button_port].pvr & button_pin;
		if(!button_state && !was_pressed){
			stayOn[BUTTON] = 10 / periods[BUTTON];
			LED_On(LEDS[BUTTON]);
			char message[10] = {'0' + (int)BUTTON, 'p', 'r', 'e', 's', 's', 'e', 'd', '\n', '\r'};
			usart_write_line(serialPORT_USART, message);
			was_pressed = true;
		}
		else if(button_state && was_pressed){
			was_pressed = false;
		}
		vTaskDelayUntil(&xLastWakeTime, xPeriodTime);
	}
}

//Blinking different LEDs with one function
void vLedBlinkHandler(void *pvParameters){
	portTickType xLastWakeTime;
	xLastWakeTime = xTaskGetTickCount();
	unsigned char LED =  *(unsigned char*)pvParameters;
	portTickType xPeriodTime;
	if(LED == 7){
		xPeriodTime = periods[6] * configTICK_RATE_HZ;
		if(xLastWakeTime - lastWakeTimes[6] > xPeriodTime){
			usart_write_line(serialPORT_USART, "Deadline missed!\n\r");
		}
		lastWakeTimes[6] = xLastWakeTime;
	}
	else{
		 xPeriodTime = periods[LED] * configTICK_RATE_HZ;
		 if(xLastWakeTime - lastWakeTimes[LED] > xPeriodTime){
			 usart_write_line(serialPORT_USART, "Deadline missed!\n\r");
		 }
		 lastWakeTimes[LED] = xLastWakeTime;
	}
	//Deadline is set to 100ms
	portTickType xDeadline = 100;
	while(1){
		if(LED < 3){
			//Letting it stay on for as long as it should, because of the button being pressed
			if(stayOn[LED] > 0){
				stayOn[LED]--;
			}
			//not ELSE because it can just become 0
			if(stayOn[LED] == 0){
				LED_Toggle(LEDS[LED]);
				char message[10] = {'0' + (int)LED, 't', 'o', 'g', 'g', 'l', 'e', 'd', '\n', '\r'};
				usart_write_line(serialPORT_USART, message);
				
			}
		}
		else{
			LED_Toggle(LEDS[LED]);
			char message[10] = {'0' + (int)LED, 't', 'o', 'g', 'g', 'l', 'e', 'd', '\n', '\r'};
			usart_write_line(serialPORT_USART, message);
		}
		//Checking if the deadline has been missed
		if(xTaskGetTickCount() - xLastWakeTime > xDeadline){
			usart_write_line(serialPORT_USART, "Deadline missed!");
		}
		vTaskDelayUntil(&xLastWakeTime, xPeriodTime);
	}	
}

// Task that polls the USART port, waiting for a character. Because this will have a higher priority,
// it will block the other tasks from running. This will cause deadline misses. 
void vDummyPollUSART(void *pvParameters){
	portTickType xLastWakeTime;
	xLastWakeTime = xTaskGetTickCount();
	const portTickType xPeriodTime = 5000;
	
	while(1){
		int c;
		usart_write_line(serialPORT_USART, "NOW!");
		while (usart_read_char(serialPORT_USART, &c) == USART_RX_EMPTY);
		vTaskDelayUntil(&xLastWakeTime, xPeriodTime);
	}
}

void configure(){
	gpio_configure_pin(LED0_GPIO,GPIO_DIR_OUTPUT | GPIO_INIT_HIGH);
	gpio_configure_pin(LED1_GPIO,GPIO_DIR_OUTPUT | GPIO_INIT_HIGH);
	gpio_configure_pin(LED2_GPIO,GPIO_DIR_OUTPUT | GPIO_INIT_HIGH);
	gpio_configure_pin(LED3_GPIO,GPIO_DIR_OUTPUT | GPIO_INIT_HIGH);
	gpio_configure_pin(LED4_GPIO,GPIO_DIR_OUTPUT | GPIO_INIT_HIGH);
	gpio_configure_pin(LED5_GPIO,GPIO_DIR_OUTPUT | GPIO_INIT_HIGH);
	gpio_configure_pin(LED6_GPIO,GPIO_DIR_OUTPUT | GPIO_INIT_HIGH);
	gpio_configure_pin(LED7_GPIO,GPIO_DIR_OUTPUT | GPIO_INIT_HIGH);
	
	gpio_configure_pin(GPIO_PUSH_BUTTON_0,GPIO_DIR_INPUT);
	gpio_configure_pin(GPIO_PUSH_BUTTON_1,GPIO_DIR_INPUT);
	gpio_configure_pin(GPIO_PUSH_BUTTON_2,GPIO_DIR_INPUT);
	for (int i = 0; i < 3; i++){
		stayOn[i] = 0;
	}
	for (int i = 0; i < 7; i++){
		lastWakeTimes[i] = 0;
	}
	was_pressed = false;
	
	init_usart();
	int c;
	usart_write_line(configDBG_USART, " Serial initialized ");
	usart_write_line(serialPORT_USART, " Press a key ...");
	//while (usart_read_char(serialPORT_USART, &c) == USART_RX_EMPTY);
}

int main( void )
{
	configure();
	for(int i = 0; i < 6; i++){
		periods[i] = i + 1;
	}
	periods[5] = 5;
	periods[6] = 6;
	static unsigned char ucParametersToPass[6] = {0, 1, 2, 3, 5, 7};
	xTaskHandle xBlinkHandles[3];
	// Create the task , store the handle .
	signed portCHAR taskName[10];
	LED_Toggle(LED0 | LED1 | LED2 | LED3 | LED5 | LED7);
	taskName[0] = 'B';
	taskName[1] = 'l';
	taskName[2] = 'i';
	taskName[3] = 'n';
	taskName[4] = 'k';
	taskName[5] = 'L';
	taskName[6] = 'E';
	taskName[7] = 'D';
	taskName[9] = '\0';
	for(int i = 0; i < 6; i++){
		taskName[8] = '0' + i;
		xTaskCreate(vLedBlinkHandler, 
					taskName, 
					MINIMAL_STACK_SIZE, 
					(void*)(ucParametersToPass + i),
					tskIDLE_PRIORITY + 2, //+ i/2, //FOR DEADLINE MISS
					(xBlinkHandles + i));
	}
	static unsigned char ucButtonParametersToPass[3] = {0, 1, 2};
	xTaskHandle xButtonHandles[3];
	taskName[0] = 'P';
	taskName[1] = 'u';
	taskName[2] = 's';
	taskName[3] = 'h';
	taskName[4] = 'B';
	taskName[5] = 'U';
	taskName[6] = 'T';
	taskName[7] = 'N';
	for(int i = 0; i < 3; i++){
		taskName[8] = '0' + i;
		xTaskCreate(vButtonCheck,
					taskName,
					MINIMAL_STACK_SIZE,
					(void*)(ucButtonParametersToPass + i),
					tskIDLE_PRIORITY + 1,
					(xButtonHandles + i));
	}
	static unsigned char ucParameterToPass;
	xTaskHandle xPollHandle;
	xTaskCreate(vDummyPollUSART,
				"PollUSART",
				MINIMAL_STACK_SIZE,
				(void*)&ucParameterToPass,
				tskIDLE_PRIORITY + 3,
				&xPollHandle);
	vTaskStartScheduler();
	while(1);
}