#include "stm32l4xx.h"
#include "Initialisation_RS232.h"

// Initialiser le pin de transmission
void init_pin_TX(void)
{
	/*initialiser le pin PA9 pour TX*/
	RCC->AHB2ENR |= (1<<0); //enable GPIOA
	GPIOA->MODER &= ~(0x3<<18);//effacer les bits mode9
	GPIOA->MODER |=  (0x2<<18);// choisir le mode alternatif pour la broche PA9
	GPIOA->PUPDR &= ~(0x3<<18);//effacer les bits PUPD9
	GPIOA->OSPEEDR &= ~(0x3<<18);//effacer les bits OSPEED9
	GPIOA->OSPEEDR |= (0x2<<18);//high speed
	GPIOA->AFR[1] &= ~(0xF<<4);//effacer les bits 4,5,6,7
	GPIOA->AFR[1] |=(0x7<<4);//fonction AF7 (USART1_TX)
}

// Initialiser le pin de réception
void init_pin_RX(void)
{
	/*initialiser le pin PA10 pour RX*/
	GPIOA->MODER &= ~(0x3<<20);//effacer les bits mode10
	GPIOA->MODER |=  (0x2<<20);// choisir le mode alternatif pour la broche PA10
	GPIOA->PUPDR &= ~(0x3<<20);//effacer les bits PUPD10
	GPIOA->OSPEEDR &= ~(0x3<<20);//effacer les bits OSPEED10
	GPIOA->OSPEEDR |= (0x2<<20);//high speed
	GPIOA->AFR[1] &= ~(0xF<<8);//effacer les bits 8,9,10,11
	GPIOA->AFR[1] |=(0x7<<8);//fonction AF7 (USART1_RX)
}

// Initialiser le module USART
void USART1_init(void)
{
	RCC->APB2ENR |= (1<<14); // enable USART1 clock
	RCC->CCIPR |= (1UL << 2); // Sélectionner SYSCLK comme source d’horloge pour USART1
	USART1->CR1 &=~(1<<0);//désactiver l'USART avant le réglage (UE = 0)
	USART1->CR1 &=~(1<<28);// M1 = 0
	USART1->CR1 &=~(1<<12);// M0 = 0
	USART1->BRR = 417;//fréquence d'horloge 4Mhz, 9600 bauds
	USART1->CR2 &= ~(0x3<<12); // 1bit de STOP après les données
	USART1->CR1 |= (1<<0);//réactiver l'USART (UE = 1)
	//Transmission//
	USART1->CR1 |= (1<<3);  // activer la transmission TX (bit TE)
	while ((USART1->ISR & (1 << 21)) == 0) {}; // Attendre TEACK=1
	//Réception//
	USART1->CR1 |= (1<<2); // activer la réception RX (bit RE)
	while ((USART1->ISR & (1 << 22)) == 0) {};// attendre REACK = 1
}

// envoyer un octet par usart/
void USART1_send_char(uint8_t message)
{
	while (!(USART1->ISR & (1<<7)))
	{
		// Attendre que le registre TDR devienne libre (TXE=1)
	};
	USART1->TDR = message;
}

// recevoir un octet par USART
uint8_t USART1_recieved_char(void)
{
	while(!(USART1->ISR & (1<<5)))
	{
		// attendre la réception
	};
	return (USART1->RDR);//lire la valeur de l'octet recu
}
// Initialiser le Timer pour gérer les delay
void Tim5_init (void)
{

		 RCC->APB1ENR1 |= (1<<3);//Timer TIM5 enable
		 TIM5->PSC = 3999;//4Mhz/(3999 +1) => 1KHz (un tick par 1ms)
		 TIM5->ARR = 0xFFFFFFFF;  // Valeur maximale pour le registre 32 bits
		 TIM5->EGR |= TIM_EGR_UG;     // FORCER la prise en compte de PSC (et ARR)
		 TIM5->CR1 |=(1<<0);//enable timer bit CEN/
		 TIM5->SR &= ~(1<<0);//renitialiser le flag UIF*/
		 TIM5->CNT = 0; //renitialiser le compteur
}

//cette fonction permet de génerer des delay en ms
void delay_ms(uint32_t tms)
{
    uint32_t start = TIM5->CNT;                      // valeur de départ (en ms)
    // Attendre que tms ticks soient passés
    while ((uint32_t)(TIM5->CNT - start) < tms) {
        /* boucle bloquante */
    }
}

// initialiser l'émetteur RS-232
void init_emetteur_RS232 (void)
{
	init_pin_TX();
	init_pin_RX();
	USART1_init();
	Tim5_init();

}

