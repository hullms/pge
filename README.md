=========================================================================================
Page 0 — Intro vidéo (VideoIntroPage)
=========================================================================================

But : jouer une vidéo d’intro plein écran.
Fichier vidéo attendu : ~/pge/intro_lecteur.mp4
Si le fichier n’existe pas → passe directement à la page suivante.
Quand la vidéo se termine → passe automatiquement au Menu (page 1).
Actions / touches :
EsC : skip l’intro → va au menu.
Audio : volume forcé à 0.0 (muet).

=========================================================================================
Page 1 — Menu principal (MenuPage)
=========================================================================================

But : page d’accueil, choix d’action.
Affiche :
Titre “PGE - Interface Qt”
“Choisis une action (↑/↓ + Entrée) :”
Boutons :
“Lecture de données (UART)”
Ouvre la page 2 (Commandes UART)
Appelle refreshUartState() pour ouvrir l’UART si nécessaire.
“Logs (lire)”
Ouvre la page 5 (Logs)
Recharge les logs avant d’afficher.
“Quitter”
Ferme l’application (app.quit()).

Navigation :
↑ / ↓ : change le bouton sélectionné (highlight via propriété sel="true").
Entrée : clique sur le bouton sélectionné.

=========================================================================================
Page 2 — Commandes UART (UartCommandPage) “PAGE 2 - COMMANDES UART”
=========================================================================================

But : envoyer des commandes UART “principales” et accéder aux sous-menus.
Affiche :
Titre + aide : “↑/↓ choisir | Entrée valider | ESC menu”
Un label status (état UART + état communication)
Boutons (5 choix) :
“Lire Balise SBIB”
Envoie une commande UART : cmd2=0, param2=0
Message affiché du type : TX SBIB (...) -> 0x..
Attend une réponse jusqu’à 4000 ms
Si réponse : payload loggé dans pgelogs.txt (code 25 car cmd2 != 1).
“Sortie SBIM”
Ne transmet pas directement : va à la page 3 (choix 1..4).
“Menu UCS”
Va à la page 4 (menu UCS).
“Test passage train”
Envoie une commande UART : cmd2=3, param2=0
Même mécanique (timeout, log si réponse).
“Retour”
Revient au Menu (page 1).

Touches :
↑ / ↓ : naviguer dans la liste des 5 actions.
Entrée : exécuter l’action sélectionnée.
ESC : retour menu (page 1).

Remarque :
refreshUartState() tente d’ouvrir /dev/serial0 si pas déjà ouvert et met à jour le status (“UART: pret …” ou erreur).

=========================================================================================
Page 3 — Sortie SBIM (SbimSelectPage) “PAGE 2B - SORTIE SBIM”
=========================================================================================

But : choisir un paramètre SBIM (1 à 4) et l’envoyer.
Boutons (5 choix) :
“1” → envoie cmd2=1, param2=0
“2” → envoie cmd2=1, param2=1
“3” → envoie cmd2=1, param2=2
“4” → envoie cmd2=1, param2=3
“Retour” → revient à la page 2

Résultat affiché :
“Communication en cours…” pendant l’échange,
puis “Communication terminée …”
et si OK : affiche aussi la taille du payload reçu.

Logs :
Pour SBIM (cmd2=1), les réponses sont loggées avec le code 15 (règle logCodeFromCmdByte).

Touches :
↑ / ↓ : sélection
Entrée : envoyer
ESC : retour page 2

=========================================================================================
Page 4 — Menu UCS (UcsSelectPage) “PAGE 2C - MENU UCS”
=========================================================================================

But : envoyer une commande UCS (4 options) et afficher le retour.
Boutons (5 choix) :
“Affichage de la configuration” → cmd2=2, param2=0 (UCS-CONFIG)
“Lecture incident” → cmd2=2, param2=1 (UCS-INCIDENT)
“Etat des entrées” → cmd2=2, param2=2 (UCS-ETAT)
“Effacement mémoire” → cmd2=2, param2=3 (UCS-ERASE)
“Retour” → revient à la page 2

Affichage :
statut “en cours” puis “terminée”, avec taille payload si OK.
Logs :
cmd2=2 ⇒ log code 25.

Touches :
↑ / ↓ : sélection
Entrée : envoyer
ESC : retour page 2

=========================================================================================
Page 5 — Logs (LogsPage) “PAGE 3 - LOGS”
=========================================================================================

But : lire les dernières lignes du fichier ~/pge/pgelogs.txt avec pagination + filtres.
Contenu affiché
Une liste (QListWidget) avec les lignes de log.
Barre de navigation :
Retour, ◀, ▶, Rafraichir, info “page x/y”
Barre de filtre :
Tous, 15, 25, Autres, + info “filtre: … | lignes: …”
Pagination
pageSize = 18 lignes affichées par page.
Limite : MAX_PAGES = 5, donc max ~90 lignes conservées en mémoire.
Les logs sont affichés du plus récent vers l’ancien (le code inverse les index pour montrer les dernières lignes).
Filtres
Basé sur la présence de (15) ou (25) dans la ligne :
Tous
15 : seulement les lignes contenant (15)
25 : seulement (25)
Autres : ni (15) ni (25)

Actions / touches
◀ / → : page précédente / suivante
R : rafraîchir (recharge depuis le fichier)
A : filtre “Tous”
1 : filtre “15”
2 : filtre “25”
O : filtre “Autres”
Entrée (quand la liste a le focus) : fait défiler les filtres (Tous → 15 → 25 → Autres)
Raccourcis globaux + GPIO
ESC global (sur toute l’appli)
Si tu es sur :
Intro (0) → va au menu (1)
UART cmd (2) ou Logs (5) → va au menu (1)
SBIM (3) ou UCS (4) → revient à UART cmd (2)
Boutons physiques GPIO → touches clavier

Via /dev/gpiochip4 :
BCM 17 → ↑
BCM 27 → ↓
BCM 23 → ←
BCM 24 → →
BCM 22 → Entrée
BCM 25 → ESC
(avec un debounce ~40 ms)
Logs UART : ce qui est enregistré
À chaque réponse UART reçue (payload entre STX/ETX) :
Si payload “imprimable” ASCII → écrit le texte.
Sinon → écrit une version hexadécimale.

Format :
[date heure] (15|25) contenu...
Fichier : ~/pge/pgelogs.txt
