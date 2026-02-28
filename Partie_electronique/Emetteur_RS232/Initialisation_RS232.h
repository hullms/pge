#ifndef INIT_RS232
#define INIT_RS232
	void init_pin_TX(void);
	void init_pin_RX(void);
	void USART1_init(void);
	void init_emetteur_RS232 (void);
	void USART1_send_char(uint8_t message);
	void Tim5_init (void);
	void delay_ms(uint32_t tms);
	uint8_t USART1_recieved_char(void);

#endif
