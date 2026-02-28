#include"stm32l4xx.h"
#include"Initialisation_RS232.h"
#define TRAME 0xA9
int main(void)
{
	init_emetteur_RS232();
	while(1)
	{

		USART1_send_char(TRAME);
		delay_ms(100);
	}
	return 0;
}
