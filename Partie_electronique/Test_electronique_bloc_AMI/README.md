# Test électronique – Bloc AMI (PCB)

Ce dossier contient un code de test destiné à valider la partie électronique AMI du PCB qui communique avec :
- les **cartes SBI vers balises** ;
- la **carte SBI de maintenance**.

Le code cible un **STM32L476RG**.

## Principe du test

Afin de **simuler les commandes provenant de l’interface utilisateur du lecteur**, on utilise des **boutons poussoirs**.

- Les boutons poussoirs sont câblés **uniquement entre la masse (GND)** et les **entrées du STM32**.
- Les **résistances de pull-up sont configurées par le code** (voir `Control_bloc_AMI.c`) :  
  **aucune résistance externe n’est nécessaire** entre les boutons et les GPIO du STM32.
- Le STM32 lit l’état des boutons, puis **reproduit les signaux** correspondants sur des **sorties** reliées au PCB (bloc AMI).

## Câblage

### 1) Entrées STM32 (boutons poussoirs)

Boutons poussoirs connectés entre **GND** et les broches suivantes :

- `PA0` : Choix adresse MUX — **Adresse A**
- `PA1` : Choix adresse MUX — **Adresse B**
- `PA4` : Choix adresse MUX — **Adresse C**
- `PB0` : **Simuler le passage du train**
- `PC1` : **Synchronisation**

> Avec des pull-up internes activées, un bouton appuyé force l’entrée à **0 (niveau bas)**, et un bouton relâché laisse l’entrée à **1 (niveau haut)**.

### 2) Sorties STM32 (vers le PCB)

Relier les sorties suivantes du STM32 aux entrées correspondantes du PCB :

- `PB4` : **Adresse A** → entrée **A** du PCB
- `PB5` : **Adresse B** → entrée **B** du PCB
- `PB3` : **Adresse C** → entrée **C** du PCB
- `PA10` : **Passage train (PT)** → entrée **PT** du PCB
- `PC2` : **Synchronisation** → entrée **SYN** du PCB
- `GND` : Masse STM32 → **GND** du PCB (**masse commune obligatoire**)

## Choix des adresses (A/B/C)
### Adresse de base (aucun bouton appuyé)
- Sans appuyer sur aucun bouton, l’adresse est **000**.

### Attention : logique inversée sur le PCB
Même si le STM32 met certains niveaux en sortie, **le PCB inverse la logique via des transistors**.  
Donc l’adresse **effectivement reçue par le MUX** est l’inverse logique de ce que le STM32 sort sur A/B/C.

### Exemples
- Appui sur **A ** ⇒ adresse reçue par le MUX : **001**  
  (côté STM32 on peut observer **110** en sortie, mais après inversion sur le PCB on obtient bien **001** côté MUX)
- Adresse **5 (101)** ⇒ appuyer sur **A + C**
- Adresse **3 (011)** ⇒ appuyer sur **A + B**
- Adresse **6 (110)** ⇒ appuyer sur **B + C**
- Adresse **7 (111)** ⇒ appuyer sur **A + B + C**

> Rappel : A est le bit de poids faible (LSB), puis B, puis C (MSB) : **C B A**.
