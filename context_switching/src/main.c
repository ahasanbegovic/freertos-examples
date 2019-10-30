#include "FreeRTOS.h"
#include "task.h"
#include "led.h"
#include "board.h"
#include "compiler.h"
#include "gpio.h"
#include "pm.h"
#include "usart.h"
#include "tc.h"

#define TC_CHANNEL0 0

//Stack used for the scheduler
#define MINIMAL_STACK_SIZE configMINIMAL_STACK_SIZE

int xLastBreakTime = 0;

//Waveform options: Channel0, RC Triggered UP Mode, Clock TC2
static tc_waveform_opt_t waveform_opt =
{
	.channel = TC_CHANNEL0,
	.wavsel = TC_WAVEFORM_SEL_UP_MODE_RC_TRIGGER,
	.enetrg = 0,
	.eevt = 0,
	.eevtedg = TC_SEL_NO_EDGE,
	.cpcdis = 0,
	.cpcstop = 0,
	.burst = 0,
	.clki = 0,
	.tcclks = TC_CLOCK_SOURCE_TC2
	//.tcclks = TC_CLOCK_SOURCE_XC0
	
};


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

void initialize_timer(){
	//Maybe
	pm_enable_osc0_ext_clock(&AVR32_PM);
	//pm_enable_osc0_crystal(&AVR32_PM, 12000000);
	
	// f = 115200/2 = 57600KHz => T = 1/57600 = 0.000017
	// one CV value is N = 0.000017 / 65535 = 2.65e-10s
	int RC = 65535;
	
	tc_write_rc(&AVR32_TC, TC_CHANNEL0, RC);
	
	tc_init_waveform(&AVR32_TC, &waveform_opt);
	
	tc_start(&AVR32_TC, TC_CHANNEL0);
	
	
	
	tc_read_sr(&AVR32_TC, TC_CHANNEL0);
}


void vTaskDoNothing(void *pvParameters){
	while(1){
		xLastBreakTime = tc_read_tc(&AVR32_TC, TC_CHANNEL0);
	}
}
void vMeasureTime(void *pvParameters){
	int time = tc_read_tc(&AVR32_TC, TC_CHANNEL0) - xLastBreakTime;
	portTickType wakeUp = xTaskGetTickCount();
	portTickType period = 1;
	portTickType zero = 0;
	char send_string[10];
	for(int i = 0; i < 10; i++){
		send_string[i] = '-';
	}
	
	int iterator = 0;
	bool sent = false;
	while(1){
		usart_write_line(serialPORT_USART, "Time:\n\r");
		while(time > zero){
			int c = '0' + time % 10;
			time /= 10;
			usart_write_char(serialPORT_USART, c);
		}
		//for(int i = 9; i >= 0; i--){
			//send_string[i] = '0' + time % 10;
			//time /= 10;
		//}
		//usart_write_line(serialPORT_USART, send_string);
		//usart_write_line(serialPORT_USART, "\r");
		vTaskDelayUntil(&wakeUp, period);
	}
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

	
	init_usart();
	int c;
	usart_write_line(configDBG_USART, " Serial initialized ");
	usart_write_line(serialPORT_USART, " Press a key ...");
	while (usart_read_char(serialPORT_USART, &c) == USART_RX_EMPTY);
}

int main( void )
{
	configure();
	xTaskHandle doNothing;
	xTaskHandle measure;
	unsigned char ucParameterToPass;
	int number_of_tasks = 5;
	for(int i = 0; i < number_of_tasks; i++){
		xTaskCreate(vTaskDoNothing,
					"DoNothing",
					MINIMAL_STACK_SIZE,
					(void*)(&ucParameterToPass),
					tskIDLE_PRIORITY + 1,
					(&doNothing));
	}
	xTaskCreate(vMeasureTime,
				"MeasureTime",
				MINIMAL_STACK_SIZE,
				(void*)(&ucParameterToPass),
				tskIDLE_PRIORITY + 1,
				(&measure));
	
	vTaskStartScheduler();
	while(1);
}