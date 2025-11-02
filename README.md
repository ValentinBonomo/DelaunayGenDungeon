# Génération Procédurale de Donjons par Triangulation

Projet étudiant de génération procédurale de donjons utilisant la triangulation de Delaunay et l'algorithme de Prim pour créer des niveaux rejouables.

## 🎯 Fonctionnalités

- **Génération aléatoire de pièces** : Création de multiples salles avec des tailles variables
- **Séparation physique** : Algorithme de relaxation pour éviter les chevauchements
- **Sélection de pièces principales** : Identification automatique des salles importantes
- **Triangulation de Delaunay** : Construction d'un graphe de connexions entre les pièces
- **Minimum Spanning Tree (Prim)** : Création d'un chemin optimal traversant toutes les pièces principales
- **Génération de couloirs** : Corridors en L ou lignes droites reliant les pièces
- **Culling intelligent** : Suppression des pièces résiduelles non connectées


## 📁 Structure du Projet
```
Triangulation_Based/
├── DungeonGenerator.h/cpp   # Classe principale de génération
├── Room.h/cpp                # Classe représentant une pièce
└── Triangulation_Based.Build.cs
```

## 🚀 Utilisation

### Dans l'éditeur Unreal

1. Glissez `ADungeonGenerator` dans votre niveau s'il n'y est pas déjà (la map de base contient déjà le DungeonGenerator)
2. Configurez les paramètres dans les détails :
   - **Rooms** : Nombre de pièces, tailles min/max
   - **Generation** : Rayon de spawn
   - **Relax** : Paramètres de séparation
   - **MainRooms** : Nombre de pièces principales, gap minimum
   - **Corridors** : Options de génération des couloirs

3. Lancez le jeu pour générer automatiquement le donjon

### Paramètres Principaux

| Paramètre | Description | Valeur par défaut |
|-----------|-------------|-------------------|
| `RoomsNbr` | Nombre total de pièces | 32 |
| `SpawnRadius` | Rayon de génération initiale | 1600 |
| `MainCount` | Nombre de pièces principales | 7 |
| `MaxRelaxIterations` | Itérations de séparation | 80 |
| `bBuildCorridors` | Activer les couloirs | true |

## 🔧 Algorithmes Implémentés

### 1. Génération Initiale
Les pièces sont générées aléatoirement dans un disque de rayon défini autour du centre du donjon et leurs tailles sont également générées aléatoirement.

### 2. Relaxation (Séparation des Pièces)
Utilise un algorithme de Minimum Translation Vector (MTV) pour séparer progressivement les pièces qui se chevauchent.

### 3. Triangulation de Delaunay

- Création d'un super-triangle englobant
- Insertion progressive des points
- Validation des cercles circonscrits

### 4. Minimum Spanning Tree (Prim)
Génère un arbre couvrant minimal pour connecter toutes les pièces principales avec un chemin optimal.

### 5. Génération de Couloirs
Crée des corridors en forme de L ou des lignes droites entre les pièces connectées par le MST.

### 6. Suppression des Salles Inutiles
Si des salles ne sont pas proches ou traversées par un couloir, elles sont supprimées automatiquement.

## 🎨 Visualisation Debug

Le projet inclut plusieurs outils de visualisation :
- **Centres des pièces principales** : Marqueurs cyan avec piliers
- **Triangulation de Delaunay** : Lignes bleues
- **MST** : Lignes vertes épaisses
- **Couloirs** : Lignes bleu foncé avec boîtes aux extrémités au centre des couloirs

---

**Astuce** : Pour de meilleurs résultats, augmentez `MaxRelaxIterations` si les pièces se chevauchent encore après génération.