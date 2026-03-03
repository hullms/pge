# Simulateur_interface — Arduino Mega 2560 (simulateur commandes IHM KVB → STM32)

## Contexte / Objectif
Ce dossier contient un **sketch Arduino Mega 2560** permettant de **simuler les commandes** que l’interface utilisateur du lecteur **KVB (Raspberry Pi)** envoie à une **carte STM32**.

La carte STM32 communique avec le **codeur**  et renvoie ensuite différentes données.  
Ce simulateur permet de **déclencher** ces échanges en envoyant des **octets de commande** au STM32, puis en **affichant les réponses** reçues.

## Communication UART
- Lien concerné : **Arduino Mega 2560 ↔ STM32**
- **Débit** : `115200` bauds
- Ports série utilisés :
  - `Serial` (USB) : interface de commande / affichage (menu, logs)
  - `Serial1` : UART vers STM32 (émission des commandes + réception des réponses)
- Réception : **non-bloquante** avec détection de fin de trame par **timeout**
  - Taille max paquet : `MAX_PACKET_SIZE = 24` octets
  - Fin de paquet si aucun octet reçu pendant `TIMEOUT_MS = 10 ms`

## Câblage (à adapter selon la carte STM32)
- Mega `TX1` (pin 18) → STM32 `RX`
- Mega `RX1` (pin 19) ← STM32 `TX`
- **GND commun obligatoire**

## Utilisation
1. Flasher le sketch sur l’**Arduino Mega 2560**
2. Ouvrir le **Moniteur Série** (ou Serial Plotter) sur `115200` bauds (port USB de l’Arduino)
3. Le menu s’affiche :
   - Taper un **numéro** (`0` à `9`) puis Entrée
4. L’Arduino envoie **1 octet** de commande sur `Serial1` vers la STM32
5. La réponse de la STM32 est reçue sur `Serial1` et affichée en **hexadécimal** sur `Serial`

## Commandes disponibles (Menu → Octet envoyé au STM32)
Le programme propose un menu utilisateur (0..9). Chaque numéro correspond à **un code de commande (1 octet)** envoyé au STM32.

| Entrée menu | Nom commande | Code envoyé (hex) | Description |
|------------:|-------------|-------------------|-------------|
| 0 | `CMD_SBIB` | `0xC0` | Demander les trames de la sortie **SBI vers balise** |
| 1 | `CMD_SBIM1` | `0xD0` | Demander les trames de la sortie **SBI maintenance 1** |
| 2 | `CMD_SBIM2` | `0xD4` | Demander les trames de la sortie **SBI maintenance 2** |
| 3 | `CMD_SBIM3` | `0xD8` | Demander les trames de la sortie **SBI maintenance 3** |
| 4 | `CMD_SBIM4` | `0xDC` | Demander les trames de la sortie **SBI maintenance 4** |
| 5 | `CMD_AFF_CONFIG` | `0xE0` | **Affichage configuration** (UCS) |
| 6 | `CMD_INCIDENTS_T` | `0xE4` | **Lecture des incidents** (UCS) |
| 7 | `CMD_ETAT_ENTREES` | `0xE8` | **État des entrées** (UCS) |
| 8 | `CMD_EFF_MEMOIRE` | `0xEC` | **Effacement mémoire** (UCS) |
| 9 | `CMD_test_train` | `0xF0` | Simulation du **passage du train** |

## Format des échanges
- **Commande** : 1 octet (valeurs ci-dessus)
- **Réponse STM32** : paquet binaire de longueur variable (jusqu’à 24 octets dans ce sketch)
- Fin de paquet : considérée lorsque plus aucun octet n’arrive pendant **10 ms**
- Affichage : dump hexadécimal sur le port USB, ex. :
  - `Paquet recu (N octets) : 0xAA 0xBB 0x01 ...`

## Notes / Limitations
- La détection de fin de trame par timeout (`10 ms`) suppose que la STM32 envoie les octets avec un intervalle inférieur à ce délai.  
  Si des trames sont plus longues ou hachées, augmenter `TIMEOUT_MS` et/ou `MAX_PACKET_SIZE`.
- En cas de dépassement (`index_rx >= 24`), les octets supplémentaires sont ignorés pour éviter l’overflow.


