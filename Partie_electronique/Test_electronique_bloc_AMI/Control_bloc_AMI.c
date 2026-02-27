#include "Control_bloc_AMI.h"
#include "stm32l4xx.h"

/*********************     Entrées   **********************/
/*choix de l'adresse de MUX
 *PA0 ---> Adresse A
 *PA1 ---> Adresse B
 *PA4 ---> ADresse C
 *
 * PB0 ---> Simuler le passage du train
 * PC1 ---> Synchronisation
 * */

// Initialisation
void Init_PA0 (void)
{
	RCC->AHB2ENR |= (1 << 0);    // Enable GPIOA
	GPIOA->MODER &= ~(3 << 0);   // mode entrée
	GPIOA->OSPEEDR &= ~(3 << 0); // effacer les bits OSPEED
	GPIOA->OSPEEDR |= (2 << 0);  // speed High
	GPIOA->PUPDR &= ~(3 << 0);   // effacer les bits de pull
	GPIOA->PUPDR |= (1 << 0);    // pull-up
}
void Init_PA1(void)
{
	RCC->AHB2ENR |= (1 << 0);
	GPIOA->MODER &= ~(3 << 2);
	GPIOA->OSPEEDR &= ~(3 << 2);
	GPIOA->OSPEEDR |= (2 << 2);
	GPIOA->PUPDR &= ~(3 << 2);
	GPIOA->PUPDR |= (1 << 2);
}

void Init_PA4(void)
{
	RCC->AHB2ENR |= (1 << 0);
	GPIOA->MODER &= ~(3 << 8);
	GPIOA->OSPEEDR &= ~(3 << 8);
	GPIOA->OSPEEDR |= (2 << 8);
	GPIOA->PUPDR &= ~(3 << 8);
	GPIOA->PUPDR |= (1 << 8);
}

void Init_PB0(void)
{
	RCC->AHB2ENR |= (1 << 1);
	GPIOB->MODER &= ~(3 << 0);
	GPIOB->OSPEEDR &= ~(3 << 0);
	GPIOB->OSPEEDR |= (2 << 0);
	GPIOB->PUPDR &= ~(3 << 0);
	GPIOB->PUPDR |= (1 << 0);
}

void Init_PC1(void)
{
	RCC->AHB2ENR |= (1 << 2);
	GPIOC->MODER &= ~(3 << 2);
	GPIOC->OSPEEDR &= ~(3 << 2);
	GPIOC->OSPEEDR |= (2 << 2);
	GPIOC->PUPDR &= ~(3 << 2);
	GPIOC->PUPDR |= (1 << 2);
}

// Lecture de l'état des entrées

// renvoie 1 si pin = LOW (appuyer), renvoie 0 si pin = HIGH (non appuyer)
uint8_t etat_PA0(void)
{
	int i = 0;
	if(!(GPIOA->IDR & (1 << 0)))
	{	for(i = 0 ; i < 10000; i++){};// delay anti_rebond
		if(!(GPIOA->IDR & (1 << 0)))
		{
			return 1;
		}
	}
	return 0;
}

uint8_t etat_PA1(void)
{
	int i = 0;
	if(!(GPIOA->IDR & (1 << 1)))
	{	for(i = 0 ; i < 10000; i++){};
		if(!(GPIOA->IDR & (1 << 1)))
		{
			return 1;
		}
	}
	return 0;
}

uint8_t etat_PA4(void)
{
	int i = 0;
	if(!(GPIOA->IDR & (1 << 4)))
	{	for(i = 0 ; i < 10000; i++){};
		if(!(GPIOA->IDR & (1 << 4)))
		{
			return 1;
		}
	}
	return 0;
}

uint8_t etat_PB0(void)
{
	int i = 0;
	if(!(GPIOB->IDR & (1 << 0)))
	{	for(i = 0 ; i < 10000; i++){};
		if(!(GPIOB->IDR & (1 << 0)))
		{
			return 1;
		}
	}
	return 0;
}

uint8_t etat_PC1(void)
{
	int i = 0;
	if(!(GPIOC->IDR & (1 << 1)))
	{	for(i = 0 ; i < 10000; i++){};
		if(!(GPIOC->IDR & (1 << 1)))
		{
			return 1;
		}
	}
	return 0;
}


/*********************   Sorties   ******************/
// Pins sorties
/*
 * PB4 ---> Adresse A
 * PB5 ---> Adresse B
 * PB3 ---> Adresse C
 * PA10 --> Simuler le passage du train
 * PC2 ---> Synchronisation
 * */

// Initialisation
void Init_PB4(void)
{
	RCC->AHB2ENR |= (1 << 1);   // Enable GPIOB
	GPIOB->MODER &= ~ (3 << 8);
	GPIOB->MODER |= (1 << 8);   // mode sortie
	GPIOB->OTYPER &= ~(1 << 4); // push-pull
	GPIOB->OSPEEDR &= ~(3 << 8);
	GPIOB->OSPEEDR |= (2 << 8);
	GPIOB->PUPDR &= ~(3 << 8);  // no pull
}

void Init_PB5(void)
{
	RCC->AHB2ENR |= (1 << 1);
	GPIOB->MODER &= ~(3 << 10);
	GPIOB->MODER |= (1 << 10);
	GPIOB->OTYPER &= ~(1 << 5);
	GPIOB->OSPEEDR &= ~(3 << 10);
	GPIOB->OSPEEDR |= (2 << 10);
	GPIOB->PUPDR &= ~(3 << 10);
}

void Init_PB3(void)
{
	RCC->AHB2ENR |= (1 << 1);
	GPIOB->MODER &= ~(3 << 6);
	GPIOB->MODER |= (1 << 6);
	GPIOB->OTYPER &= ~(1 << 3);
	GPIOB->OSPEEDR &= ~(3 << 6);
	GPIOB->OSPEEDR |= (2 << 6);
	GPIOB->PUPDR &= ~(3 << 6);
}

void Init_PA10(void)
{
	RCC->AHB2ENR |= (1 << 0);
	GPIOA->MODER &= ~(3 << 20);
	GPIOA->MODER |= (1 << 20);
	GPIOA->OTYPER &= ~(1 << 10);
	GPIOA->OSPEEDR &= ~(3 << 20);
	GPIOA->OSPEEDR |= (2 << 20);
	GPIOA->PUPDR &= ~(3 << 20);
}

void Init_PC2(void)
{
	RCC->AHB2ENR |= (1 << 2);   // Enable GPIOC (bit 2)
	GPIOC->MODER &= ~(3 << 4);  // Clear bits pour pin 2
	GPIOC->MODER |= (1 << 4);   // Mode sortie (01)
	GPIOC->OTYPER &= ~(1 << 2);
	GPIOC->OSPEEDR &= ~(3 << 4);
	GPIOC->OSPEEDR |= (2 << 4);
	GPIOC->PUPDR &= ~(3 << 4);
}


void Init_all_pins(void)
{
	Init_PA0();
	Init_PA1();
	Init_PA4();
	Init_PB0();
	Init_PC1();
	Init_PB4();
	Init_PB5();
	Init_PB3();
	Init_PA10();
	Init_PC2();
}

// Action sur les sorties
// envoyer 1 -> Pin = High, envoie 0 -> Pin = LOW

void etat_PB4(uint8_t etat_pin)
{
	if(etat_pin)
	{
		GPIOB->ODR |= (1 << 4); // pin High (3.3V)
	}
	else
	{
		GPIOB->ODR &= ~(1 << 4); // pin LOW (0V)
	}
}

void etat_PB5(uint8_t etat_pin)
{
	if(etat_pin)
	{
		GPIOB->ODR |= (1 << 5);
	}
	else
	{
		GPIOB->ODR &= ~(1 << 5);
	}
}

void etat_PB3(uint8_t etat_pin)
{
	if(etat_pin)
	{
		GPIOB->ODR |= (1 << 3);
	}
	else
	{
		GPIOB->ODR &= ~(1 << 3);
	}
}

void etat_PA10(uint8_t etat_pin)
{
	if(etat_pin)
	{
		GPIOA->ODR |= (1 << 10);
	}
	else
	{
		GPIOA->ODR &= ~(1 << 10);
	}
}

void etat_PA2(uint8_t etat_pin)
{
	if(etat_pin)
	{
		GPIOA->ODR |= (1 << 2);
	}
	else
	{
		GPIOA->ODR &= ~(1 << 2);
	}
}

void etat_PC2(uint8_t etat_pin)
{
	if(etat_pin)
	{
		GPIOC->ODR |= (1 << 2);
	}
	else
	{
		GPIOC->ODR &= ~(1 << 2);
	}
}
