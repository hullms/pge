# Récepteur de Trames Synchronisées — STM32L4

## Description du projet

Ce projet implémente un récepteur de trames synchronisées sur microcontrôleur **STM32L4**.  
Il acquiert un signal série sur la broche **PA0** (bit-banging à 20 µs/bit), détecte et valide les trames de 32 octets, puis les transmet via **PIN9 (PA9)** après un vote majoritaire sur un buffer circulaire de 10 trames.

---

## Architecture du système

| Composant       | Rôle                                                                 |
|-----------------|----------------------------------------------------------------------|
| **PA0**         | Acquisition du signal (bit-banging, 20 µs/bit)                       |
| **0xFE**        | Octet de synchronisation (verrouillage début de trame)               |
| **Trame 32 o.** | 1 octet sync + 31 octets de données                                  |
| **TIM5**        | Timer 32 bits — mesure la durée de chaque impulsion                  |
| **Buffer ×10**  | Tableau circulaire de 10 trames pour le vote majoritaire             |
| **PA9 / PIN9**  | Transmission : STX + 22 octets + ETX                                 |
| **PA10 / PIN10**| Réception des commandes MUX (USART1, interruption)                   |
| **PA5**         | LED d'alerte (10 s) en cas de perte de synchronisation               |
| **PA1**         | Signal logique de désynchronisation                                  |

### Pipeline de traitement

```
Signal PA0  →  Sync 0xFE  →  Trame 32 o.  →  Tampon ×10  →  TX PIN9
    (bit-banging)  (registre à décalage)   (lecture 31 o.)  (vote ≥3/10)  (STX+22o+ETX)
```

### Validation par vote majoritaire

Pour chaque trame `i` dans le buffer, on compte combien de trames `j` lui sont identiques (comparaison octet par octet sur 32 octets). La trame avec le score le plus élevé est sélectionnée.

- Score **≥ 3** → trame transmise sur PIN9  
- Score **< 3** → signal `[INSTABLE]`, trame rejetée

---

## ⚠️ Bug identifié — Pourquoi la démo n'a pas fonctionné

### Cause racine

Le code utilisait **30 µs** comme seuil de détection d'impulsion anormale dans TIM5.

Or, la durée d'un bit lors d'une **désynchronisation** est exactement **20 µs** — c'est-à-dire la même durée qu'un bit normal.

```
Bit normal        :  ┌────┐   durée = 20 µs  ✓ correct
                  ───┘    └───

Bit désynchronisé :  ┌────┐   durée = 20 µs  ← MÊME durée !
                  ───┘    └───
                             ✗ Non détecté par TIM5 (seuil était 30 µs)
```

### Conséquence en cascade

```
Seuil TIM5 = 30 µs
    → Anomalies de désync jamais détectées (20 µs < 30 µs)
    → Trames désynchronisées acceptées dans le buffer circulaire
    → Vote majoritaire faussé par des trames corrompues
    → Mauvaise trame transmise sur PIN9
    → Échec de la démo
```

### Correction

**Une seule ligne à modifier** dans le code de détection d'anomalie (TIM5) :

```c
// AVANT (incorrect)
if (pulse_duration > 30) {   // 30 µs — ne détecte jamais les désync
    anomaly_count++;
}

// APRÈS (correct)
if (pulse_duration > 20) {   // 20 µs — détecte les désync
    anomaly_count++;
}
```

Après cette correction :
- Les impulsions hors-sync (> 20 µs) sont comptées
- Au bout de 6 anomalies en 1 seconde → LED PA5 allumée 10 s + signal PA1
- Le buffer est protégé contre les trames corrompues
- Le vote majoritaire fonctionne correctement

---

## Sélection des canaux MUX (PIN10)

Les commandes reçues sur **PA10** via USART1 (interruption) sélectionnent l'un des 5 canaux :

| Canal      | Description              |
|------------|--------------------------|
| `MONO`     | Mode monomodal (défaut)  |
| `MAINT 1`  | Maintenance canal 1      |
| `MAINT 2`  | Maintenance canal 2      |
| `MAINT 3`  | Maintenance canal 3      |
| `MAINT 4`  | Maintenance canal 4      |

La boucle principale est **non-bloquante** : les interruptions USART1 capturent chaque commande sans interrompre l'acquisition en cours.

---

## Présentation (`recepteur_stm32_FINAL.pptx`)

| Slide | Contenu                                               |
|-------|-------------------------------------------------------|
| 1     | Titre — vue d'ensemble du système                     |
| 2     | Architecture — 3 blocs visuels (icônes + schémas)     |
| 3     | Pipeline — flux de traitement des données             |
| 4     | Validation — buffer circulaire + vote majoritaire     |
| 5     | Post-mortem — analyse du bug de la démo               |
