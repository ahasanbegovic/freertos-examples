#include "FreeRTOS.h"
#include "task.h"
#include "FreeRTOSConfig.h"
#include "led.h"
#include "board.h"
#include "compiler.h"
#include "gpio.h"
#include "pm.h"
#include "usart.h"
#include "semphr.h"
#include "display_init.h"

#define DATALEN 20

#define MINIMAL_STACK_SIZE configMINIMAL_STACK_SIZE //256

xTaskHandle xProducerHandle;
xTaskHandle xConsumerHandle;
xTaskHandle xStatusHandle;
xTaskHandle xCountHandle;
xTaskHandle xPotProducerHandle;
xTaskHandle xPotConsumerHandle;
xTaskHandle xTempProducerHandle;
xTaskHandle xTempConsumerHandle;
xTaskHandle xLightProducerHandle;
xTaskHandle xLightConsumerHandle;

xQueueHandle queueHandle;
xQueueHandle potQueue;
xQueueHandle tempQueue;
xQueueHandle lightQueue;

xSemaphoreHandle xSemaphorePot = NULL;
xSemaphoreHandle xSemaphoreTemp = NULL;
xSemaphoreHandle xSemaphoreLight = NULL;
xSemaphoreHandle xSemaphoreDisp = NULL;

int potCount = 0;
int tempCount = 0;
int lightCount = 0;




struct mesg{
	int id;
	unsigned char data[DATALEN];
	portTickType timestamp;
} potMessage, tempMessage, lightMessage;


void addPotToQueue(int number)
{
	struct mesg * pMessage;
	for(int i = 3; i >= 0; i--){
		potMessage.data[i] = '0' + number % 10;
		number /= 10;
	}
	pMessage = &potMessage;
	xQueueSendToBack(potQueue, (void *) &pMessage, 10);
}

void addTempToQueue(int number)
{
	struct mesg * pMessage;
	for(int i = 3; i >= 0; i--){
		tempMessage.data[i] = '0' + number % 10;
		number /= 10;
	}
	pMessage = &tempMessage;
	xQueueSendToBack(tempQueue, (void *) &pMessage, 10);
}

void addLightToQueue(int number)
{
	struct mesg * pMessage;
	for(int i = 3; i >= 0; i--){
		lightMessage.data[i] = '0' + number % 10;
		number /= 10;
	}
	pMessage = &lightMessage;
	xQueueSendToBack(lightQueue, (void *) &pMessage, 10);
}


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

	display_init();
}


void configure(){
	//gpio_configure_pin(LED0_GPIO,GPIO_DIR_OUTPUT | GPIO_INIT_HIGH);
	//gpio_configure_pin(LED1_GPIO,GPIO_DIR_OUTPUT | GPIO_INIT_HIGH);
	//gpio_configure_pin(LED2_GPIO,GPIO_DIR_OUTPUT | GPIO_INIT_HIGH);
	//gpio_configure_pin(LED3_GPIO,GPIO_DIR_OUTPUT | GPIO_INIT_HIGH);
	//gpio_configure_pin(LED4_GPIO,GPIO_DIR_OUTPUT | GPIO_INIT_HIGH);
	//gpio_configure_pin(LED5_GPIO,GPIO_DIR_OUTPUT | GPIO_INIT_HIGH);
	//gpio_configure_pin(LED6_GPIO,GPIO_DIR_OUTPUT | GPIO_INIT_HIGH);
	//gpio_configure_pin(LED7_GPIO,GPIO_DIR_OUTPUT | GPIO_INIT_HIGH);
	//
	//gpio_configure_pin(GPIO_PUSH_BUTTON_0,GPIO_DIR_INPUT);
	//gpio_configure_pin(GPIO_PUSH_BUTTON_1,GPIO_DIR_INPUT);
	//gpio_configure_pin(GPIO_PUSH_BUTTON_2,GPIO_DIR_INPUT);
	
	adc_configure(&AVR32_ADC);
	adc_enable(&AVR32_ADC, ADC_POTENTIOMETER_CHANNEL);
	adc_enable(&AVR32_ADC, ADC_TEMPERATURE_CHANNEL);
	adc_enable(&AVR32_ADC, ADC_LIGHT_CHANNEL);

	 
	init_usart();
	int c;
	usart_write_line(configDBG_USART, " Serial initialized ");
	usart_write_line(serialPORT_USART, " Press a key ...");
	while (usart_read_char(serialPORT_USART, &c) == USART_RX_EMPTY);
}

void displayByteCount(int number)
{


	char text[4];
	int i = 3;
	while(i >= 0)
	{
		text[i] = '0' + number % 10;
		number = number / 10;
		i--;
	}
	
	dip204_set_cursor_position(1,3);
	dip204_write_string(text);
	dip204_set_cursor_position(5,3);
	dip204_write_string("             ");
}

void vPotProducer(void *pvParameters)
{
	//struct mesg * pMessage;
	while(1)
	{
		adc_start(&AVR32_ADC);
		int pot_value = adc_get_value(&AVR32_ADC, ADC_POTENTIOMETER_CHANNEL);

		usart_write_line(configDBG_USART, "Pot Read\r\n");
		portBASE_TYPE success = xSemaphoreTake(xSemaphorePot, 0);
		if (success == pdTRUE)
		{
			if(potCount == DATALEN){
				xSemaphoreGive(xSemaphorePot);
				vTaskSuspend(xPotProducerHandle);
			}
			addPotToQueue(pot_value);
			potCount++;
			if (potCount >= DATALEN)
			{
				xSemaphoreGive(xSemaphorePot);
				vTaskSuspend(xPotProducerHandle);
			}
			if(potCount == 1){
				vTaskResume(xPotConsumerHandle);
			}
			xSemaphoreGive(xSemaphorePot);
		}
		vTaskDelay(100);
	}
}

void vPotConsumer(void *pvParameters)
{
	struct mesg * pMessage;
	while(1)
	{
		portBASE_TYPE success = xSemaphoreTake(xSemaphorePot, 0);
		if (success == pdTRUE)
		{
			if(potCount == 0)
			{
				xSemaphoreGive(xSemaphorePot);
				vTaskSuspend(xPotConsumerHandle);
			}

			success = xQueueReceive(potQueue, &(pMessage), 10);
			usart_write_line(configDBG_USART, pMessage->data);
			usart_write_line(configDBG_USART, " Pot Received\r\n");
			potCount--;
			success = xSemaphoreTake(xSemaphoreDisp, 10);
			if(success){
				dip204_set_cursor_position(1,1);
				dip204_write_string("POT = ");
				dip204_write_string(pMessage->data);
				xSemaphoreGive(xSemaphoreDisp);
			}
			
			if (potCount == DATALEN - 1)
			{
				vTaskResume(xPotProducerHandle);
			}
			xSemaphoreGive(xSemaphorePot);
		}
		vTaskDelay(100);
	}
}

void vTempProducer(void *pvParameters)
{
	//struct mesg * pMessage;
	while(1)
	{
		adc_start(&AVR32_ADC);
		int temp_value = adc_get_value(&AVR32_ADC, ADC_TEMPERATURE_CHANNEL);

		usart_write_line(configDBG_USART, "Temp Read\r\n");
		portBASE_TYPE success = xSemaphoreTake(xSemaphoreTemp, 10);
		if (success == pdTRUE)
		{
			if(tempCount == DATALEN){
				xSemaphoreGive(xSemaphoreTemp);
				vTaskSuspend(xTempProducerHandle);
			}
			addTempToQueue(temp_value);
			tempCount++;
			if (tempCount >= DATALEN)
			{
				xSemaphoreGive(xSemaphoreTemp);
				vTaskSuspend(xTempProducerHandle);
			}
			if(tempCount == 1){
				vTaskResume(xTempConsumerHandle);
			}
			xSemaphoreGive(xSemaphoreTemp);
		}
		vTaskDelay(300);
	}
}

void vTempConsumer(void *pvParameters)
{
	struct mesg * pMessage;
	while(1)
	{
		portBASE_TYPE success = xSemaphoreTake(xSemaphoreTemp, 10);
		if (success == pdTRUE)
		{
			if(tempCount == 0)
			{
				xSemaphoreGive(xSemaphoreTemp);
				vTaskSuspend(xTempConsumerHandle);
			}

			success = xQueueReceive(tempQueue, &(pMessage), 10);
			usart_write_line(configDBG_USART, pMessage->data);
			usart_write_line(configDBG_USART, " Temp Received\r\n");
			tempCount--;
			
			success = xSemaphoreTake(xSemaphoreDisp, 10);
			if(success){
				dip204_set_cursor_position(1,2);
				dip204_write_string("TMP = ");
				dip204_write_string(pMessage->data);
				xSemaphoreGive(xSemaphoreDisp);
			}
			if (tempCount == DATALEN - 1)
			{
				vTaskResume(xTempProducerHandle);
			}
			xSemaphoreGive(xSemaphoreTemp);
		}
		vTaskDelay(300);
	}
}

void vLightProducer(void *pvParameters)
{
	//struct mesg * pMessage;
	while(1)
	{
		adc_start(&AVR32_ADC);
		int light_value = adc_get_value(&AVR32_ADC, ADC_LIGHT_CHANNEL);

		usart_write_line(configDBG_USART, "Light Read\r\n");
		portBASE_TYPE success = xSemaphoreTake(xSemaphoreLight, 100);
		if (success == pdTRUE)
		{
			if(lightCount == DATALEN){
				xSemaphoreGive(xSemaphoreLight);
				vTaskSuspend(xLightProducerHandle);
			}
			addLightToQueue(light_value);
			lightCount++;
			if (lightCount >= DATALEN)
			{
				xSemaphoreGive(xSemaphoreLight);
				vTaskSuspend(xLightProducerHandle);
			}
			if(lightCount == 1){
				vTaskResume(xLightConsumerHandle);
			}
			xSemaphoreGive(xSemaphoreLight);
		}
		vTaskDelay(300);
	}
}

void vLightConsumer(void *pvParameters)
{
	struct mesg * pMessage;
	while(1)
	{
		portBASE_TYPE success = xSemaphoreTake(xSemaphoreLight, 10);
		if (success == pdTRUE)
		{
			if(lightCount == 0)
			{
				xSemaphoreGive(xSemaphoreLight);
				vTaskSuspend(xLightConsumerHandle);
			}

			success = xQueueReceive(lightQueue, &(pMessage), 10);
			usart_write_line(configDBG_USART, pMessage->data);
			usart_write_line(configDBG_USART, " Light Received\r\n");
			lightCount--;
			
			success = xSemaphoreTake(xSemaphoreDisp, 10);
			if(success){
				dip204_set_cursor_position(1,3);
				dip204_write_string("LGHT= ");
				dip204_write_string(pMessage->data);
				xSemaphoreGive(xSemaphoreDisp);
			}
			if (lightCount == DATALEN - 1)
			{
				vTaskResume(xLightProducerHandle);
			}
			xSemaphoreGive(xSemaphoreLight);
		}
		vTaskDelay(300);
	}
}


void main (void){
	configure();
	static unsigned char ucParameterToPass;
	potQueue = xQueueCreate(DATALEN, sizeof(struct mesg *));
	tempQueue = xQueueCreate(DATALEN, sizeof(struct mesg *));
	lightQueue = xQueueCreate(DATALEN, sizeof(struct mesg *));
	dip204_clear_display();

	vSemaphoreCreateBinary(xSemaphorePot);
	vSemaphoreCreateBinary(xSemaphoreTemp);
	vSemaphoreCreateBinary(xSemaphoreLight);
	vSemaphoreCreateBinary(xSemaphoreDisp);

	xTaskCreate(vPotProducer,
				"PotProducerTask",
				MINIMAL_STACK_SIZE,
				(void*)(&ucParameterToPass),
				tskIDLE_PRIORITY + 1,
				(&xPotProducerHandle));
	 
	xTaskCreate(vPotConsumer,
				"PotConsumerTask",
				MINIMAL_STACK_SIZE,
				(void*)(&ucParameterToPass),
				tskIDLE_PRIORITY + 1,
				(&xPotConsumerHandle));
				
	xTaskCreate(vTempProducer,
				"TempProducerTask",
				MINIMAL_STACK_SIZE,
				(void*)(&ucParameterToPass),
				tskIDLE_PRIORITY + 1,
				(&xTempProducerHandle));
	
	xTaskCreate(vTempConsumer,
				"TempConsumerTask",
				MINIMAL_STACK_SIZE,
				(void*)(&ucParameterToPass),
				tskIDLE_PRIORITY + 1,
				(&xTempConsumerHandle));
	xTaskCreate(vLightProducer,
				"LightProducerTask",
				MINIMAL_STACK_SIZE,
				(void*)(&ucParameterToPass),
				tskIDLE_PRIORITY + 1,
				(&xLightProducerHandle));
	
	xTaskCreate(vLightConsumer,
				"LightConsumerTask",
				MINIMAL_STACK_SIZE,
				(void*)(&ucParameterToPass),
				tskIDLE_PRIORITY + 1,
				(&xLightConsumerHandle));
	 
	vTaskStartScheduler();
	while(1);
}
