#ifndef CTRL_BLOC_AMI
#define CTRL_BLOC_AMI
#include <stdint.h>
	//  prototypes initialsation des entrées
	void Init_PA0(void);
	void Init_PA1(void);
	void Init_PA4(void);
	void Init_PB0(void);
	void Init_PC1(void);

	// prototypes intialisation des sorties
	void Init_PB4(void);
	void Init_PB5(void);
	void Init_PB3(void);
	void Init_PA10(void);
	void Init_PA2(void);
	void Init_all_pins(void);

	// prototypes lecture des entrées
	uint8_t etat_PA0(void);
	uint8_t etat_PA1(void);
	uint8_t etat_PA4(void);
	uint8_t etat_PB0(void);
	uint8_t etat_PC1(void);

	// Prototype Action sur les sorties
	void etat_PB4(uint8_t etat_pin);
	void etat_PB5(uint8_t etat_pin);
	void etat_PB3(uint8_t etat_pin);
	void etat_PA10(uint8_t etat_pin);
	void etat_PC2(uint8_t etat_pin);

#endif
