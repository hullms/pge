# Schémas LTspice & modèles SPICE — Bloc AMI

Ce dossier regroupe les éléments de simulation sous **LTspice** utilisés pour la partie électronique du projet, notamment :
- le **schéma LTspice du récepteur AMI** ;
- le **schéma LTspice de l’émetteur AMI** ;
- les **modèles SPICE** des différents composants employés dans les simulations.

## Notes sur la simulation du récepteur AMI (important)

Dans le **récepteur AMI**, les éléments logiques (notamment les **bascules D** et les **portes XOR**) utilisés dans la simulation sont des composants **standards LTspice**.

- Cela implique que le **niveau de tension en sortie** du montage simulé **n’est pas exactement identique** à celui obtenu sur le **PCB** (notamment à cause des modèles logiques génériques).
- En revanche, **tous les composants situés avant les bascules** (ex. **comparateur**, **AOP**) sont basés sur des **modèles réels** des composants : les **niveaux de tension** observés à l’interface analogique → logique sont donc **corrects et représentatifs**.

## Ouvrir les schémas

1. Installer LTspice
2. Ouvrir les fichiers de schéma (`.asc`)
3. Vérifier que les bibliothèques/modèles SPICE fournis dans ce dossier sont bien dans le chemin de recherche LTspice (ou inclus via des directives `.include` / `.lib` dans les schémas).
