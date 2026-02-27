#include<stm32l4xx.h>
#include<stdint.h>
#include "Initialisation_pin_timer.h"

/***  prototypes ***/
void envoie_plus_0_logique(void);
void envoie_moins_0_logique(void);
void envoie_1_logique(void);
void ligne_etat_idle(void);
void envoie_octet(uint8_t  case_tableau);



int main(void)
{
	init_emetteur();
	uint8_t message = 143;
	while(1)
	{
		envoie_octet(message);
		delay_us(1000); // chaque 1sec
	}

	return 0;
}


// cette fonction envoie un +0 logique --> L1 = +5V (PA0 = 1 , PA1 = 0) , L2 = -5V (PA4 = 0 ,PB0 = 1)
void envoie_plus_0_logique(void)
{
	etat_pin_PA0(HIGH);
	etat_pin_PA1(LOW);
	etat_pin_PA4(LOW);
	etat_pin_PB0(HIGH);
}

// cette fonction envoie -0 logique --> L1 = -5V (PA0 = 0, PA1 = 1), L2 = 5V (PA4 = 1, PB0 = 0)
void envoie_moins_0_logique(void)
{
	etat_pin_PA0(LOW);
	etat_pin_PA1(HIGH);
	etat_pin_PA4(HIGH);
	etat_pin_PB0(LOW);
}

// cette fonction envoie 1 logique --> L1 = 0V (PA0 = 0, PA1 = 0), L2 = 0V (PA4 = 0, PB0 = 0)
void envoie_1_logique(void)
{
	etat_pin_PA0(LOW);
	etat_pin_PA1(LOW);
	etat_pin_PA4(LOW);
	etat_pin_PB0(LOW);
}

// met la ligne en état de repos --> 0V
void ligne_etat_idle(void)
{
	etat_pin_PA0(LOW);
	etat_pin_PA1(LOW);
	etat_pin_PA4(LOW);
	etat_pin_PB0(LOW);
}

// envoyer un octet su la ligne ( bit LSB envoyé en premier)
void envoie_octet(uint8_t  case_tableau)
{
	uint8_t i = 0;
	uint8_t valeur_bit;
	static uint8_t change_polarite = 0;
	for(i = 0 ; i < 8 ; i++)
	{
	    valeur_bit = case_tableau & (1 << i);
	    // envoie de +0 ou -0 logique
	    if(valeur_bit == 0)
	      {
	        	if(change_polarite)
	        	{
	        		envoie_moins_0_logique();
	        		change_polarite = 0;
	        	}
	        	else
	        	{
	        		envoie_plus_0_logique();
	        		change_polarite = 1;
	        	}
			   delay_us(9);// delay de 9us + 1us de temps d'écriture dans les registres
	   		   ligne_etat_idle();
	    	   delay_us(9);
	      }

	     // envoie 1 logique
	     else
	     {
	    	 envoie_1_logique();
			 delay_us(19);// delay de 19us + 1us de temps d'écriture dans les registres
	     }
	   
	 }
}

