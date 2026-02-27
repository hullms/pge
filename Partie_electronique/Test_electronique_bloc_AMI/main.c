#include"stm32l4xx.h"
#include "Control_bloc_AMI.h"

int main(void)
{
	Init_all_pins();

	while(1)
	{
		// --- Adresse A (Entrée PA0 -> Sortie PB4) ---
		if(etat_PA0() == 1)
		{
			etat_PB4(0);
		}
		else
		{
			etat_PB4(1);
		}

		// --- Adresse B (Entrée PA1 -> Sortie PB5) ---
		if(etat_PA1() == 1)
		{
			etat_PB5(0);
		}
		else
		{
			etat_PB5(1);
		}

		// --- Adresse C (Entrée PA4 -> Sortie PB3) ---
		if(etat_PA4() == 1)
		{
			etat_PB3(0);
		}
		else
		{
			etat_PB3(1);
		}

		// --- Simuler le passage du train (Entrée PB0 -> Sortie PA10) ---
		if(etat_PB0() == 1)
		{
			etat_PA10(1);
		}
		else
		{
			etat_PA10(0);
		}

		// --- Synchronisation (Entrée PC1 -> Sortie PA2) ---
		if(etat_PC1() == 1)
		{
			etat_PC2(1);
		}
		else
		{
			etat_PC2(0);
		}
	}
	return 0;
}
