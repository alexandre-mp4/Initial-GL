# Initial GL

Un jeu de drift sur une route de montagne, fait en C++ avec OpenGL Core.

## Le jeu

Il y a deux modes de jeu :

- **Time attack** : on part de la ligne de départ, et il faut atteindre la ligne d'arrivée le plus vite possible. Le meilleur temps est gardé pour chaque voiture.
- **Drift libre** : pas de ligne d'arrivée, on cherche juste à faire le plus gros score de drift.

Trois voitures sont disponibles (une AE86, une R34 et une RX-7). Elles n'ont pas les mêmes stats (puissance, poids, adhérence), donc elles ne se conduisent pas pareil. Le menu de choix affiche les valeurs de chacune.

### Le score de drift

On est en drift quand l'angle entre la direction de la voiture et sa trajectoire dépasse 12° et que la vitesse est au-dessus de 25 km/h. Pendant ce temps une chaîne de points se remplit. Elle dépend de l'angle et de la vitesse, et un multiplicateur monte jusqu'à x5 si on tient longtemps.

La chaîne est ajoutée au score après 0,8 s sans drift. Par contre, si on tape un mur ou un rail, la chaîne en cours est perdue.

### Ce qu'il y a dans la scène

- une montagne générée à partir d'une heightmap, avec la route qui serpente dessus
- des arbres et des rochers
- des lampadaires, des rails de sécurité, des panneaux chevron dans les virages
- une voiture avec une physique à elle (pneus, motricité, régime moteur)
- ombres, ciel, bloom et post-traitement
- une interface en 2D (compteur, chrono, mini-carte, menus) et de la musique avec des bruitages

## Ce qu'il faut installer

Il faut un compilateur C++17, CMake (3.16 minimum), et les bibliothèques GLFW, GLEW et GLM.

### Linux (Ubuntu / Debian)

    sudo apt install build-essential cmake libglfw3-dev libglew-dev libglm-dev libgl-dev

### macOS

Il faut Homebrew et les outils de ligne de commande Xcode (`xcode-select --install`), puis :

    brew install cmake glfw glew glm

## Compiler et lancer

Dans le dossier du projet :

    ./build_and_run.sh

Ça compile puis ça lance le jeu. Le script est écrit pour marcher sous Linux et sous macOS.

Il y a aussi deux options :

    ./build_and_run.sh --build    # compile seulement
    ./build_and_run.sh --clean    # supprime build/ et recompile tout

Sans le script, à la main :

    cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
    cmake --build build -j
    cd build
    ./DriftGame

Il faut bien lancer le jeu depuis le dossier `build/`, parce qu'il charge ses fichiers (shaders, modèles, textures) avec des chemins relatifs. CMake les copie à côté de l'exécutable après la compilation.

La compilation est en Release par défaut. En Debug ce serait trop lent avec le nombre d'arbres.

Les réglages (`settings.cfg`) et les records (`scores.txt`) sont créés dans `build/`.

Sur macOS, OpenGL est marqué comme déprécié par Apple, mais ça marche quand même (les warnings sont désactivés dans le CMakeLists).

## Commandes

Pendant la partie :

| Touche | Action |
|---|---|
| Flèche haut / bas | accélérer / freiner puis marche arrière |
| Flèche gauche / droite | tourner |
| Espace | frein à main (pour lancer le drift) |
| C | changer de caméra |
| V | alterner caméra orbite / caméra poursuite |
| L | phares |
| N | néons |
| M | couleur des néons |
| F | lampe torche |
| R | remettre la voiture sur la route |
| Q | qualité graphique |
| 1 / 2 | aide au contre-braquage - / + |
| 3 / 4 | adhérence des pneus - / + |
| 5 / 6 | puissance moteur - / + |
| 7 / 8 | aide à la motricité - / + |
| F1 | mode fil de fer |
| F3 | afficher les forces (debug de la physique) |
| Échap | pause |

Dans les menus : flèches (ou W / S) pour naviguer, gauche / droite pour changer une valeur, Entrée ou Espace pour valider, Échap pour revenir.

Avec la caméra libre, on se déplace avec W A S D, et Z / X pour monter et descendre.

Les touches 1 à 8 sont aussi accessibles dans le menu Réglages.

## Organisation du code

    src/core       boucle du jeu, menus, session (chrono, score), chargement des ressources
    src/render     rendu (shaders, meshes, caméra, lumières, ombres, post-traitement)
    src/world      scène, terrain, route, forêt, décor de la route
    src/physics    la voiture
    src/ui         interface 2D et affichage pendant la course
    src/audio      musique et sons
    shaders/       programmes GLSL
    assets/        modèles, textures, monde généré, polices, musiques
    models/        lampadaire et lampe (modèles OBJ vus en cours)
    tools/         scripts Python qui génèrent le monde et certaines textures
    third_party/   bibliothèques en un seul en-tête (cgltf, miniaudio, stb_truetype)
    stb_image/     chargement des images

Les scripts de `tools/` servent seulement à régénérer le monde (`python3 tools/gen_world.py`, avec numpy, scipy et Pillow). Le résultat est déjà dans `assets/world`, donc ce n'est pas nécessaire pour lancer le jeu.
