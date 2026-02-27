#include "stm32l4xx.h"
#include "Initialisation_pin_timer.h"


/*********** Initialiser l'horloge du système (HSI 80MHz) *************/
void SystemClock_Config(void)
{
    RCC->APB1ENR1 |= RCC_APB1ENR1_PWREN; // 1️ Activer le PWR

    // Voltage scaling : Range 1
    PWR->CR1 &= ~PWR_CR1_VOS;
    PWR->CR1 |=  PWR_CR1_VOS_0;   // Range 1
    while (PWR->SR2 & PWR_SR2_VOSF); // attendre stabilité

    // Configurer la Flash : 4 wait states
    FLASH->ACR |= FLASH_ACR_LATENCY_4WS;
    while ((FLASH->ACR & FLASH_ACR_LATENCY) != FLASH_ACR_LATENCY_4WS);

    // Activer HSI (16 MHz)
    RCC->CR |= RCC_CR_HSION;
    while ((RCC->CR & RCC_CR_HSIRDY) == 0);

    // Stopper PLL
    RCC->CR &= ~RCC_CR_PLLON;
    while (RCC->CR & RCC_CR_PLLRDY);

    // Configurer PLL : HSI → 80 MHz
    RCC->PLLCFGR =
        RCC_PLLCFGR_PLLSRC_HSI |      // source = HSI
        (0 << RCC_PLLCFGR_PLLM_Pos) | // M = 1
        (10 << RCC_PLLCFGR_PLLN_Pos) |// N = 10
        (0 << RCC_PLLCFGR_PLLR_Pos) | // R = 2
        RCC_PLLCFGR_PLLREN;

    //Activer PLL
    RCC->CR |= RCC_CR_PLLON;
    while ((RCC->CR & RCC_CR_PLLRDY) == 0);

    // Prescalers = 1
    RCC->CFGR &= ~(RCC_CFGR_HPRE | RCC_CFGR_PPRE1 |RCC_CFGR_PPRE2);

    // PLL comme SYSCLK
    RCC->CFGR &= ~RCC_CFGR_SW;
    RCC->CFGR |= RCC_CFGR_SW_PLL;
    while ((RCC->CFGR & RCC_CFGR_SWS) != RCC_CFGR_SWS_PLL);
}

/**************** Initiliaser les pins de l'émetteur *************/
/*Connection STM vers PCB
 * PA0 ---> A
 * PA1 ---> B
 * PA4 ---> C
 * PB0 ---> D
 * */

// intialiser pin PA0
void init_pin_PA0(void)
{
	RCC->AHB2ENR |= (1 << 0);  // activer l'horloge du GPIOA
	GPIOA->MODER &= ~(3 << 0); // effacer les bits
	GPIOA->MODER |= (1 << 0);  // mode sortie
	GPIOA->OTYPER &= ~(1 << 0); // push-pull
	GPIOA->OSPEEDR &= ~(3 << 0); // effacer les bits
	GPIOA->OSPEEDR |= (2 << 0); // high speed
	GPIOA->PUPDR &= ~( 3 << 0); // no-pull
}

// init PA1
void init_pin_PA1(void)
{
	GPIOA->MODER &= ~(3 << 2);
	GPIOA->MODER |= (1 << 2);
	GPIOA->OTYPER &= ~(1 << 1);
	GPIOA->OSPEEDR &= ~(3 << 2);
	GPIOA->OSPEEDR |= (2 << 2);
	GPIOA->PUPDR &= ~( 3 << 2);
}

//init PA4
void init_pin_PA4(void)
{

	GPIOA->MODER &= ~(3 << 8);
	GPIOA->MODER |= (1 << 8);
	GPIOA->OTYPER &= ~(1 << 4);
	GPIOA->OSPEEDR &= ~(3 << 8);
	GPIOA->OSPEEDR |= (2 << 8);
	GPIOA->PUPDR &= ~( 3 << 8);

}

// inti PB0
void init_pin_PB0(void)
{
	RCC->AHB2ENR |= (1 << 1); // activer l'horloge du GPIOB
	GPIOB->MODER &= ~(3 << 0);
	GPIOB->MODER |= (1 << 0);
	GPIOB->OTYPER &= ~(1 << 0);
	GPIOB->OSPEEDR &= ~(3 << 0);
	GPIOB->OSPEEDR |= (2 << 0);
	GPIOB->PUPDR &= ~( 3 << 0);
}

// contrôler l'état du pin PA0
void etat_pin_PA0(etat_pin etat)
{
	if(etat == HIGH)
	{
		GPIOA->ODR |= ( 1 << 0);
	}
	else if(etat == LOW)
	{
		GPIOA->ODR &= ~( 1 << 0);
	}
}

void etat_pin_PA1(etat_pin etat)
{
	if(etat == HIGH)
	{
		GPIOA->ODR |= ( 1 << 1);
	}
	else if(etat == LOW)
	{
		GPIOA->ODR &= ~( 1 << 1);
	}
}

void etat_pin_PA4(etat_pin etat)
{
	if(etat == HIGH)
	{
		GPIOA->ODR |= ( 1 << 4);
	}
	else if(etat == LOW)
	{
		GPIOA->ODR &= ~( 1 << 4);
	}
}

void etat_pin_PB0(etat_pin etat)
{
	if(etat == HIGH)
	{
		GPIOB->ODR |= ( 1 << 0);
	}
	else if(etat == LOW)
	{
		GPIOB->ODR &= ~( 1 << 0);
	}
}

/*************************  Initialiser le Timer *********************/

// initialiser le timer
void init_TIM5 (void)
{

		 RCC->APB1ENR1 |= (1<<3);//Activer l'horloge de TIM5
		 TIM5->PSC = 79;//80Mhz/(79 +1) => 1MHz (un tick par 1us)
		 TIM5->ARR = 0xFFFFFFFF;  // Valeur maximale pour le registre 32 bits
		 TIM5->EGR |= (1 << 0); // FORCER la prise en compte de PSC et ARR
		 TIM5->CR1 |=(1<<0);//activer le timer (bit CEN)/
		 TIM5->SR &= ~(1<<0);//renitialiser le flag UIF*/
		 TIM5->CNT = 0; //renitialiser le compteur

}


// génerer des delay en µs
void delay_us(uint32_t delay)
{
	uint32_t start_time = TIM5->CNT; // capter la valeur du temps actuel
	while((uint32_t)(TIM5->CNT - start_time) < delay)
	{
		/*rester dans la boucle jusqu'à la fin du delay */
	}
}

// initialiser l'émetteur en complèt
void init_emetteur (void)
{
	SystemClock_Config();
	init_pin_PA0();
	init_pin_PA1();
	init_pin_PA4();
	init_pin_PB0();
	init_TIM5();

}


