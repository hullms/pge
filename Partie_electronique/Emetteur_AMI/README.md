# Émetteur AMI 

## Objectif
Cette partie contient le code permettant de **piloter l'émetteur AMI** qu'on a fabriquer pour tester le décodage du signal AMI.

L’émetteur **imite le codeur numérique** : depuis le STM32, on commande les entrées A, B, C, D afin de **générer un code AMI en sortie**.

> Plateforme cible : **STM32L476RG**

---

## Principe de fonctionnement (vue générale)
- Le microcontrôleur STM32 pilote les **entrées de commande** de l’émetteur AMI via des GPIO.
- En appliquant des états logiques sur les lignes **A, B, C, D**, l’émetteur produit le **signal AMI** correspondant.
- Le signal AMI est récupéré sur les **deux sorties** de l’émetteur : **S1** et **S2**.

---

## Branchement matériel

### Alimentation
- `-VCC` → **-15 V**
- `+VCC` → **+15 V**

### Connexions STM32 ↔ Entrées émetteur AMI

| Pin STM32 | Entrée émetteur AMI |
|----------:|:---------------------|
| PA0       | A |
| PA1       | B |
| PA4       | C |
| PB0       | D |
| GND       | GND |

### Sorties
- La **sortie AMI** est récupérée sur : **S1** et **S2** (sorties de l’émetteur).
