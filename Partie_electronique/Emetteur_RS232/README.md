# Emetteur_RS232 — Test de la communication RS‑232 du PCB

## Objectif
Cette partie sert à **tester la chaîne de communication RS‑232 du PCB**, la partie du lecteur utilisée pour communiquer avec la  **sortie UCS du codeur numérique**.

Elle permet :
- **d’émettre** des messages série depuis un **microcontrôleur (STM32, niveaux 0–3,3 V)** et de vérifier que le PCB les **convertit correctement en niveaux RS‑232** ;
- **de recevoir** un message RS‑232 (généré via un MAX232 pour simuler l’UCS) et de vérifier que le PCB le **transforme** puis le **renvoie vers sa sortie RX avec des niveau 0–3,3V** (côté logique microcontrôleur).

---

## Matériel / éléments utilisés
- STM32 (UART sur **PA9 = TX**)
- PCB à tester (connecteurs **TX / TXD / RXD / RX** selon la configuration)
- MAX232 (uniquement pour le **test de réception RS‑232**)
---

## 1) Test de l’envoi RS‑232 (TX)
### But
Vérifier que le PCB transforme correctement un signal série issu du microcontrôleur (**0 V / 3,3 V**) en un signal aux **niveaux RS‑232**.

Le signal RS‑232 généré est observé sur le connecteur **TXD** du PCB.

### Connexions
| Connexion (source) | Vers (destination) | Rôle |
|---|---|---|
| **PA9 (TX STM32)** | **TX (PCB)** | Envoi du message du microcontrôleur vers le PCB |
| **GND** | **GND (PCB)** | Masse commune |
| **TXD (PCB)** | **Oscilloscope** | Visualisation du signal RS‑232 en sortie du PCB |

### Validation attendue
- Sur **TXD**, on doit observer un signal **RS‑232** (niveaux et forme conformes).
- Le contenu (trame UART) doit correspondre au message envoyé par le STM32.

---

## 2) Test de la réception RS‑232 (RX)
### But
Vérifier la capacité du PCB à **recevoir** un signal RS‑232 (comme celui provenant de la sortie UCS), à le **transformer**, puis à le renvoyer vers la **sortie RX** du PCB en niveaux **0–3,3 V** (côté logique microcontrôleur).

Pour simuler la sortie UCS, on utilise un **MAX232** : le STM32 génère un signal série **0–3,3 V**, converti ensuite en **RS‑232** par le MAX232.

### Connexions
| Connexion (source) | Vers (destination) | Rôle |
|---|---|---|
| **PA9 (TX STM32)** | **T1IN (MAX232)** | Générer un signal série 0–3,3 V (à convertir en RS‑232) |
| **T1OUT (MAX232)** | **RXD (PCB)** | Injection du signal RS‑232 vers le PCB |
| **GND** | **GND (STM32 / MAX232 / PCB)** | Masse commune entre STM32, MAX232 et PCB |

### Principe de fonctionnement (résumé)
1. Le **STM32** émet une trame UART en **0–3,3 V**.
2. Le **MAX232** convertit cette trame en **RS‑232**.
3. Le signal RS‑232 est injecté dans le PCB via **RXD**.
4. Le PCB traite le signal et le **renvoie vers la sortie RX du PCB**.
5. La visualisation de cette sortie permet d’observer la forme du signal que le microcontrôleur va recevoir.

### Validation attendue
- Le signal injecté sur **RXD** est bien au format RS‑232.
- La sortie **RX (PCB)** présente une trame exploitable côté microcontrôleur (forme correcte, niveau logique attendu (0 -3.3V).
