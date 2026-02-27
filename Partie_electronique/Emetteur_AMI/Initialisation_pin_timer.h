#ifndef INIT_PIN_TIMER
#define INIT_PIN_TIMER
typedef enum etat_pin{HIGH,LOW} etat_pin;

void init_pin_PA0(void);
void init_pin_PA1(void);
void init_pin_PA4(void);
void init_pin_PB0(void);
void init_emetteur (void);
void etat_pin_PA0(etat_pin etat);
void etat_pin_PA1(etat_pin etat);
void etat_pin_PA4(etat_pin etat);
void etat_pin_PB0(etat_pin etat);
void init_TIM5(void);
void delay_us(uint32_t delay);
void SystemClock_Config(void);



#endif
