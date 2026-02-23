# 🎙️ Proto Ready | Body Foley

> **[UE 5.4+]** **[C++]** **[Production-Ready]** **[Multiplayer]**

Proto Ready | Body Foley est un plugin de fonctionnalités clé en main. Il se divise en deux systèmes principaux :
1. **Un système de respiration réaliste** basé sur l'effort.
2. **Une gestion optimisée et production-ready des bruits de pas (footsteps)** incluant sons, particules, decals, détection par surface, types de déclencheurs, et mixage audio dynamique.

Utilisé en premier lieu par moi-même pour accélérer mon développement dans Unreal Engine, j'ai décidé de peaufiner son architecture pour l'offrir à la communauté.

**L'objectif de ce plugin est simple : le gain de temps.** Il fournit une architecture fiable et optimisée pour que vous puissiez étendre votre projet sereinement et vous concentrer sur ce qui différencie vraiment votre jeu.

---

## ✨ Features Core

- **Respiration Dynamique (MetaSounds) :** Utilisation des dernières fonctionnalités audio d'Unreal Engine pour créer un système de respiration réaliste et réactif (MS_Breathing), qui s'adapte à la vitesse et à l'effort.
- **Système de Footsteps Multi-Surfaces :** Support complet des paysages (Landscapes) avec blending de matériaux (multi-trace). Compatible avec les personnages bipèdes et n-legs (quadrupèdes, unijambistes, monstres arachnéens...). Ultra-configurable pour seoir à toutes les utilisations.
- **PR Foley Toolkit :** Un menu éditeur dédié pour créer, modifier et gérer facilement vos `Surface Types` (Project Settings) et générer automatiquement les `Physical Materials` associés à vos couches de Landscape en un clic.
- **Data-Driven & Modulaire :** Un système reposant sur des `DataAssets` hautement scalables. Activez à souhait n'importe quel calque : VFX (Niagara), Decals, Sons. Paramétrez-les individuellement pour chaque type de surface, ou laissez vide pour utiliser un Fallback global par défaut.

---

## 🏗️ Architecture — Les 5 Layers

Le composant `UPRFoleyComponent` fonctionne comme un chef d'orchestre à **5 couches** indépendantes, chacune activable/désactivable depuis l'éditeur ou en Blueprint :

| Layer        | Toggle                      | Rôle                                                   |
| ------------ | --------------------------- | ------------------------------------------------------ |
| **Footstep** | `bEnableFootstepLayer`      | Sons de pas per-surface (marche, course, sprint)       |
| **Voice**    | `bEnableVoiceLayer`         | Respiration dynamique + efforts vocaux (jump, land)    |
| **VFX**      | `bEnableVFXLayer`           | Particules Niagara (poussière, neige, éclaboussures)   |
| **Decal**    | `bEnableDecalLayer`         | Empreintes de pas projetées au sol (neige, boue, sang) |
| **Network**  | `bEnableNetworkReplication` | Réplication multiplayer via Multicast RPC              |

> **💡 Tip :** Chaque layer fonctionne de manière autonome. Vous pouvez n'utiliser que les sons (Voice + Footstep) sans VFX ni Decals, ou inversement. Le système adapte automatiquement sa consommation de ressources.

---

## 🚀 Quick Start — Setup en 3 étapes

### Étape 1 : Ajouter le composant

Ajoutez `PR Foley` (UPRFoleyComponent) à votre Character Blueprint, directement depuis le panneau `Add Component`.

### Étape 2 : Créer vos Data Assets

Le plugin utilise **deux Data Assets** séparés pour une modularité maximale :

- **PRFootstepData** — Configure les traces, les sons per-surface, les VFX, les decals, le landing, et l'optimisation LOD.
- **PRVoiceData** — Configure la respiration (MetaSound ou loops classiques), les efforts vocaux, et les seuils de vitesse.

Pour créer un Data Asset : *Clic droit > Miscellaneous > Data Asset > choisir `PR Footstep Data Asset` ou `PR Voice Data Asset`*.

Assignez-les ensuite aux champs `FootstepData` et `VoiceData` du composant.

### Étape 3 : Déclencher les pas

Le plugin supporte **deux modes de déclenchement** :

| Mode                    | Fonctionnement                                                                                                             | Idéal pour                                  |
| ----------------------- | -------------------------------------------------------------------------------------------------------------------------- | ------------------------------------------- |
| **AnimNotify** (défaut) | Placez les notifies `AnimNotify_PRFootstep` dans vos animations de marche/course. Le plugin gère le reste.                 | Bipèdes, humanoïdes                         |
| **Distance**            | Le composant mesure la distance parcourue et déclenche un pas tous les X unités automatiquement. Aucune animation requise. | Véhicules, robots, créatures sans squelette |

> **📌 Note :** En mode AnimNotify, le plugin auto-détecte les jumps (via `MovementModeChanged`) et les landings (via `LandedDelegate`). Vous pouvez activer/désactiver cette détection automatique dans le Data Asset (`bAutoTriggerJump`, `bAutoTriggerLand`).

---

## 🦶 Footstep Data Asset — Détail des catégories

Le `PRFootstepData` est le cœur du système. Il expose les paramètres suivants, organisés par catégorie dans l'éditeur :

### Trigger
- **TriggerMode** : `AnimNotify` ou `Distance`
- **FootIntervalDistance** : Intervalle en unités Unreal entre chaque pas (mode Distance uniquement)

### Trace
- **TraceType** : `Line`, `Sphere`, `Box`, ou `Multi` (sphere + line fallback)
- **TraceLength** : Longueur du rayon vers le bas
- **TraceChannel** : Canal de collision utilisé (défaut : Visibility)
- **bUseFootSockets** : Utilise les sockets de pieds du squelette comme point de départ du rayon
- **FootSockets** : Liste des noms de sockets (ex: `foot_l`, `foot_r`)
- **FootSocketZOffset** : Décalage vertical du point de départ (compense la latence AnimNotify en sprint)
- **SphereRadius / BoxHalfExtent** : Dimensions de la forme du sweep trace

### Surfaces
- **Surfaces** : Tableau de `FPRSurfaceFoleyConfig`, chaque entrée mappée à un `EPhysicalSurface`
- **GlobalJumpLaunch / GlobalLandImpact** : Sons globaux de jump/land (surchargeable per-surface)
- **SurfaceAudio** : Réglages audio partagés (volume, pitch, attenuation, effects chain, concurrency)
- **bEnableLandscapeBlending** : Active le blending multi-surface sur les Landscapes
- **bEnableLandscapeMultiTrace** : Tire 4 traces périphériques pour détecter les surfaces adjacentes
- **LandscapeBlendThreshold** : Seuil minimum de poids pour considérer une surface secondaire

### VFX
- **DefaultVFX** : Particules Niagara de fallback (footstep, jump, land)
- **VFXScale Walk/Jog/Sprint** : Multiplicateurs de taille des particules selon la vitesse

### Decals
Configuré par surface dans `FPRSurfaceDecalSet` :
- **DecalMaterial** : Matériau du decal projeté
- **DecalSize** : Dimensions du decal (largeur × hauteur)
- **DecalOffset** : Décalage visuel fin (ex: reculer l'empreinte de 5 cm)
- **LeftFootFrames / RightFootFrames** : Index de frames pour atlas de textures (alternance gauche/droit automatique)
- **LifeSpan** : Durée de vie avant disparition
- **MaxActiveDecals** : Limite du nombre de decals simultanés (pooling automatique)

### Landing
- **HeavyLandThresholdVelocity** : Seuil de vitesse Z pour déclencher un atterrissage "lourd"
- **bPlayFootstepAfterLandImpact** : Joue un son de pas supplémentaire après l'impact
- **FootstepDelayAfterLand** : Délai avant ce son additionnel

### Optimization
- **MaxLODDistance** : Distance caméra au-delà de laquelle le foley est entièrement coupé

---

## 🫁 Voice Data Asset — Respiration & Efforts

Le `PRVoiceData` pilote tout ce qui est vocal et respiratoire :

### Efforts
- **JumpEffort** : Son joué au saut
- **LandExhale** : Son joué à l'atterrissage
- **HeavyLandExhale** : Son joué lors d'un atterrissage lourd (vitesse Z > seuil)

### Breathing — Mode Simple (Loops)
Assignez un `SoundBase` (Sound Cue, Wave) par palier de vitesse :
- `Idle`, `Walk`, `Jog`, `Sprint`

Le système crossfade automatiquement entre les loops avec un temps configurable (`BreathingFadeTime`).

### Breathing — Mode Avancé (MetaSound)
Assignez un unique **MetaSound Source** (`MS_Breathing`) et le plugin pilote 4 paramètres en temps réel :

| Paramètre       | Plage | Description                                           |
| --------------- | ----- | ----------------------------------------------------- |
| `Intensity`     | 0 → 1 | Volume et profondeur globale                          |
| `BreathRate`    | 0 → 1 | Fréquence de la respiration                           |
| `EffortLevel`   | 0 → 1 | Intensité de l'effort (ramp exponentielle au sprint)  |
| `RecoveryPhase` | 0 → 1 | Phase de récupération après un sprint (souffle coupé) |

> **💡 Tip :** Le mode MetaSound est **recommandé** pour la qualité. En quelques nodes dans un MetaSound Source, vous obtenez une respiration **infiniment plus réaliste** que des loops prédécoupées, avec un contrôle total sur les transitions.

### Velocity Tiers
- **WalkSpeedThreshold** : Seuil vitesse marche (défaut: 150)
- **JogSpeedThreshold** : Seuil vitesse trot (défaut: 300)
- **SprintSpeedThreshold** : Seuil vitesse sprint (défaut: 500)
- **TierHysteresis** : Marge anti-oscillation entre les paliers (défaut: 15)

---

## 🔌 API Blueprint

### Fonctions Principales

| Fonction                                | Description                                                                                                  |
| --------------------------------------- | ------------------------------------------------------------------------------------------------------------ |
| `TriggerFootstep(SocketName)`           | Déclenche manuellement un pas. Appelé automatiquement par les AnimNotifies.                                  |
| `Landing()`                             | Déclenche un atterrissage. Auto-bindé au `LandedDelegate` si activé.                                         |
| `HandleJump()`                          | Déclenche un saut. Auto-détecté via le changement de mode de mouvement.                                      |
| `SetFootstepData(NewData)`              | Change le Data Asset de footstep à la volée (ex: changement de chaussures).                                  |
| `SetVoiceData(NewData)`                 | Change le Data Asset de voix à la volée (ex: transformation du personnage).                                  |
| `SetSpeedOverride(Speed)`               | Injecte une vitesse custom pour le système de respiration. Pass `-1` pour revenir à la vélocité automatique. |
| `SetSpeedThresholds(Walk, Jog, Sprint)` | Override les seuils de vitesse au runtime.                                                                   |
| `SetBreathingDrive(Intensity)`          | Override l'intensité de la respiration (0-1). Utile pour les cinématiques.                                   |

### Événements (Delegates)

| Événement                | Paramètres                       | Utilisation                            |
| ------------------------ | -------------------------------- | -------------------------------------- |
| `OnFootstepPlayed`       | Surface, Location, Volume, Sound | Réagir à chaque pas (UI, gameplay)     |
| `OnVoicePlayed`          | Tier, Volume, Sound              | Réagir aux sons vocaux                 |
| `OnBreathingTierChanged` | NewTier                          | Déclenchement de transitions visuelles |
| `OnVFXSpawned`           | Surface, Location, NiagaraSystem | Tracker les VFX actifs                 |
| `OnDecalSpawned`         | Surface, Location                | Logique custom sur les empreintes      |

### Getters

| Getter                           | Retour             | Description                                    |
| -------------------------------- | ------------------ | ---------------------------------------------- |
| `GetCurrentVelocityTier()`       | `EPRVelocityTier`  | Palier de vitesse actuel                       |
| `GetCurrentBreathingTier()`      | `EPRVelocityTier`  | Palier de respiration actuel (avec hystérésis) |
| `GetLastDetectedSurface()`       | `EPhysicalSurface` | Dernière surface détectée par le trace         |
| `GetSurfaceDisplayName(Surface)` | `FString`          | Nom affiché de la surface (pour debug/UI)      |
| `GetBreathingIntensity()`        | `float`            | Intensité actuelle de la respiration (0-1)     |

---

## 🛠️ PR Foley Toolkit

L'outil éditeur intégré accessible via le menu `Window > PR Foley Toolkit` offre deux fonctionnalités :

### Surface Type Manager
- Visualise et renomme les 64 `Physical Surface Types` du projet.
- Crée automatiquement les `Physical Materials` manquants en un clic (`Create All Missing PhysMats`).
- Filtre par statut : `Unconfigured`, `Missing PhysMat`.

### Landscape Helper
- Scanne tous les Landscapes du niveau ouvert.
- Vérifie que chaque couche de peinture possède un `LayerInfo` avec un `Physical Material` assigné.
- Diagnostic visuel : ✅ OK ou ❌ erreur avec message explicatif.

---

## ⚡ Optimisation & Performance

- **LOD Audio** : Tout le foley est coupé au-delà de `MaxLODDistance` (caméra).
- **Pooling de Decals** : `MaxActiveDecals` limite les decals actifs. Les plus anciens sont automatiquement recyclés.
- **Shuffle No-Repeat** : Évite la répétition consécutive du même son sans allocation supplémentaire.
- **Throttled MetaSound Parameters** : Les paramètres ne sont envoyés que si le delta dépasse un seuil (évite le spam audio).
- **Async-Free** : Tout s'exécute de manière **synchrone** sur le Game Thread. Pas de latence, pas de race condition.

---

## 🌐 Multijoueur

Le plugin supporte la réplication via **Multicast RPCs**. Activez `bEnableNetworkReplication` sur le composant et les événements suivants seront automatiquement broadcastés aux autres clients :
- Sons de footstep
- VFX Niagara
- Type de surface détecté

> **⚠️ Important :** Les Decals ne sont **pas** répliqués par défaut pour des raisons de performance. Utilisez le delegate `OnDecalSpawned` côté serveur pour implémenter votre propre logique de réplication si nécessaire.

---

## 🔧 Troubleshooting

| Problème                    | Cause probable                                                       | Solution                                                          |
| --------------------------- | -------------------------------------------------------------------- | ----------------------------------------------------------------- |
| Aucun son                   | `Surfaces` vide ou pas de fallback `Default`                         | Ajouter une entrée `SurfaceType_Default` dans le tableau Surfaces |
| Trace rate le sol           | `TraceLength` trop court ou `TraceType` = Line sur terrain accidenté | Augmenter `TraceLength`, passer en `Sphere` ou `Multi`            |
| Pas de variation de surface | Mesh sans Physical Material                                          | Assigner un PhysMat avec le bon SurfaceType à vos matériaux       |
| Decals invisibles           | Culling engine agressif                                              | Le plugin force `FadeScreenSize = 0.001` automatiquement          |
| Trace miss en sprint        | AnimNotify décalé d'une frame                                        | Augmenter `FootSocketZOffset` (ex: 30-40)                         |
| Landscape sans PhysMat      | Layer Info non configuré                                             | Utiliser le `Landscape Helper` du Toolkit                         |

---

## 📦 Contenu Inclus

| Asset                          | Description                                   |
| ------------------------------ | --------------------------------------------- |
| **DA_Footstep_Default**        | Data Asset préconfigured avec 13 surfaces     |
| **DA_Voice_Default**           | Data Asset voix avec MetaSound de respiration |
| **MS_Breathing**               | MetaSound Source de respiration dynamique     |
| **Sound Cues**                 | Bibliothèque de sons CC0 pour chaque surface  |
| **PR Foley Toolkit**           | Outil éditeur intégré                         |
| **AnimNotify_PRFootstep**      | Notify clé en main pour vos animations        |
| **AnimNotify_PRJump / PRLand** | Notifies pour jump et landing                 |

---

## 📋 Compatibilité

|                   |                                               |
| ----------------- | --------------------------------------------- |
| **Unreal Engine** | 5.4+ (optimisé pour 5.7)                      |
| **Plateformes**   | Windows, Linux, Mac, Consoles                 |
| **Réseau**        | Multijoueur (Listen Server, Dedicated Server) |
| **Langages**      | C++ & Blueprint                               |

---

## 📄 Changelog

### v1.0 — Initial Release
- 5-layer foley system (Footstep, Voice, VFX, Decal, Network)
- MetaSound breathing with 4 real-time parameters
- Multi-trace landscape blending
- Flipbook decal atlas support
- PR Foley Toolkit (Surface Manager + Landscape Helper)
- 13 surface types preconfigured
- Full Blueprint API with 5 delegates

---

## Support & Infos

- **License** : MIT (usage commercial autorisé après achat sur Fab).

*(c) 2026 ProtoReady Pack*
