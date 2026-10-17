# ClassicFPS Engine v1.0

Motor FPS 3D ligero, modular y nativo para **Linux** escrito en **C++20**, inspirado en la arquitectura técnica, el determinismo de simulación y la estética clásica de **GoldSrc / Counter-Strike 1.6**.

Todo el código fuente, los subsistemas del motor, las texturas procedurales, los modelos 3D `glTF 2.0`, los sonidos `WAV` y los mapas son completamente originales y libres de código o recursos propietarios.

---

## Estado del Proyecto por Fases (8/8 Fases Completadas)

- [x] **Fase 1: Núcleo ejecutable** *(Completada)*
  - Sistema de compilación CMake + C++20 con advertencias estrictas y soporte para sanitizadores (`ASan`/`UBSan`).
  - Control de versiones (`engine/core/Version`), registro thread-safe (`engine/core/Logger`), manejo de errores y señales POSIX (`engine/core/Error`).
  - Reloj dual de paso fijo (`sys_ticrate`) + interpolación `alpha` (`engine/core/Clock`), configuración estilo CVars `.cfg` (`engine/core/Config`), gestor RAII de recursos (`engine/core/ResourceManager`), ventana SDL2 (`engine/platform/Window`) y telemetría (`engine/debug/FrameStats`).
- [x] **Fase 2: Escena 3D** *(Completada)*
  - Cámara en primera persona (`FirstPersonCamera`) con matrices de vista/proyección (GLM) y **frustum culling** (`Frustum::intersectsAABB`).
  - Geometría 3D (`Mesh`, `Vertex3D`, `AABB`), shaders GLSL `#version 330 core` (`ShaderProgram`, `assets/shaders/`), texturas pixeladas `64x64` (`GL_NEAREST`, `GL_REPEAT`, carga/guardado `.ppm`) e iluminación direccional + puntual + niebla lineal en `OpenGLGraphicsDevice` y `SoftwareGraphicsDevice`.
- [x] **Fase 3: Movimiento FPS** *(Completada)*
  - Gestor de entrada desacoplado (`engine/input/InputManager`) con soporte de teclado y ratón, sensibilidad configurable (`sensitivity`), inversión vertical (`m_invert_pitch`), bloqueo de cursor (`Tab`/`M`) y generación determinista de `PlayerCommand`.
  - Mundo de colisiones independiente de los modelos visuales (`engine/physics/CollisionWorld`) con volúmenes sólidos (`SolidBox`), rampas inclinadas (`RampNegZ`), barrido continuo (`sweepPlayerHull`), detección de solapamiento para techos bajos (`checkHullOverlap`), superficie de soporte (`querySupportSurface`) y trazado de rayos (`raycast`).
  - Controlador cinemático FPS estilo GoldSrc (`engine/physics/FPSController`) con paso fijo (`sys_ticrate`), fricción, aceleración en suelo/aire, correr/caminar/agacharse, gravedad, salto, *collide-and-slide*, subida de escalones y rampas.
- [x] **Fase 4: Arma y combate** *(Completada)*
  - Sistema de armas genérico y desacoplado del jugador (`engine/weapons/WeaponDef`, `engine/weapons/WeaponSystem`) configurable mediante archivo externo editable (`game/configs/weapons.cfg`) sin recompilar el motor (`carbine_m4`, `tactical_usp`, `launcher_gl`).
  - Máquina de estados explícita (`Idle`, `Firing`, `Reloading`, `Switching`, `Empty`) con simulación separada del renderizado y daño zonal (`4x` en cabeza `head`, `1x` en torso `torso`).
  - Modelo visual 3D low-poly reconocible en primera persona (`engine/weapons/ViewModelRenderer`) compuesto por 11 piezas articuladas (carabina de 7 piezas + antebrazos y manos enguantadas de 4 piezas), fogonazo en boca de cañón (*muzzle flash*) y marcadores 3D de impacto en el escenario.
- [x] **Fase 5: Recursos, modelos glTF 2.0, animaciones esqueléticas y audio 3D** *(Completada)*
  - Jerarquía de huesos (`Bone`, `Skeleton`), pistas de keyframes (`TransformTRS` con interpolación `slerp` de cuaterniones) y reproductor de animaciones con *cross-fade blending* (`AnimationPlayer` en `engine/animation/Skeleton.*`).
  - Cargador y serializador de modelos **glTF 2.0** (`.gltf` con buffer binario embebido en Base64) y **Wavefront OBJ** (`.obj`) en `engine/resources/ModelLoader.*`, junto con los modelos originales `assets/models/tactical_carbine.gltf` y `assets/models/target_dummy.gltf`.
  - Subsistema de **Audio Espacial 3D** (`engine/audio/AudioEngine.*`) con carga/guardado de archivos PCM 16-bit `RIFF/WAVE` (`.wav`), síntesis procedural de sonidos FPS (`assets/sounds/m4_fire.wav`, `m4_reload.wav`, `dryfire.wav`) y mezclador estéreo 3D con atenuación por distancia y panoramización izquierda/derecha según la orientación del oyente.
- [x] **Fase 6: Entidades, formato de mapas, HUD clásico, consola y herramientas** *(Completada)*
  - Gestor de entidades (`engine/world/EntityManager.*`) con ciclo de vida, salud, destrucción, animación de impacto (`hit_flinch`) y reaparición temporizada.
  - Formato de mapas propio en versión legible de texto (`.cfmap`, `assets/maps/de_arena.cfmap`) y versión binaria versionada (`.cbsp` con cabecera mágica `"CFBM"`, `assets/maps/de_arena.cbsp`) en `engine/world/MapFormat.*`.
  - **HUD 2D clásico estilo CS 1.6** y **Consola de Diagnóstico interactiva** (`engine/ui/HudAndConsole.*`) con fuente bitmap 5x7 integrada, mira dinámica que se expande según la dispersión actual del arma, paneles ámbar de salud/armadura/munición/FPS e intérprete de comandos en vivo (`help`, `status`, `get`, `set`, `give_ammo`, `slot`, `reset_entities`, `clear`).
  - Herramientas independientes de línea de comandos: `classicfps_asset_converter` y `classicfps_map_viewer` (`tools/`).
- [x] **Fase 7: Multijugador básico cliente-servidor sobre UDP** *(Completada)*
  - Arquitectura cliente-servidor autoritativa sobre sockets UDP POSIX no bloqueantes (`engine/networking/NetSystem.*`), protocolo binario versionado (`"CFSP"` v1), envío de `ClientUserCommand`, difusión de `ServerWorldSnapshot`, **predicción en cliente** y **reconciliación con re-simulación** cuando el estado autoritativo del servidor diverge del predicho.
- [x] **Fase 8: Optimización, perfilado y estabilidad** *(Completada)*
  - Perfilador por subsistema con temporizadores RAII (`engine/debug/Profiler.*`) que mide tiempos medios y máximos en microsegundos (`Render3D`, `HudOverlay`, `Physics`, `Weapons`, `EntitiesAndAudio`).
  - Suite completa de **22 pruebas automatizadas en CTest** verificadas tanto en modo `Release (-O3)` como en modo `Debug` con **AddressSanitizer (`ASan`) y UndefinedBehaviorSanitizer (`UBSan`)** con 0 fugas de memoria y 0 errores.

---

## Compilación, Pruebas y Ejecución (Linux)

Consulta [`docs/building-linux.md`](docs/building-linux.md) para las instrucciones detalladas de instalación de paquetes en **Debian/Ubuntu** y **Arch Linux**, y [`docs/architecture.md`](docs/architecture.md) para el diseño técnico.

```bash
# 1. Compilar todo el motor, herramientas y ejecutar las 22 pruebas de CTest
./scripts/build_and_test.sh build Release

# 2. Ejecutar el motor FPS completo (escena 3D, física, armas, audio 3D y HUD)
./build/classicfps_demo

# 3. Ejecutar 30 fotogramas y exportar una captura PPM con el arma y el HUD
./build/classicfps_demo --max-frames 30 --screenshot frame.ppm

# 4. Usar las herramientas independientes del motor
./build/classicfps_asset_converter --generate-defaults
./build/classicfps_asset_converter --inspect-gltf assets/models/tactical_carbine.gltf
./build/classicfps_map_viewer assets/maps/de_arena.cbsp
```

---

## Estructura completa del repositorio

```text
ClassicFPS/
├── CMakeLists.txt
├── LICENSE
├── README.md
├── assets/
│   ├── maps/
│   │   ├── de_arena.cbsp
│   │   └── de_arena.cfmap
│   ├── models/
│   │   ├── tactical_carbine.gltf
│   │   └── target_dummy.gltf
│   ├── shaders/
│   │   ├── classic_lit.frag
│   │   └── classic_lit.vert
│   └── sounds/
│       ├── dryfire.wav
│       ├── m4_fire.wav
│       └── m4_reload.wav
├── docs/
│   ├── architecture.md
│   └── building-linux.md
├── engine/
│   ├── animation/
│   │   └── Skeleton.cpp / Skeleton.hpp
│   ├── audio/
│   │   └── AudioEngine.cpp / AudioEngine.hpp
│   ├── core/
│   ├── debug/
│   │   ├── FrameStats.cpp / FrameStats.hpp
│   │   └── Profiler.cpp / Profiler.hpp
│   ├── input/
│   │   └── InputManager.cpp / InputManager.hpp
│   ├── networking/
│   │   └── NetSystem.cpp / NetSystem.hpp
│   ├── physics/
│   │   ├── CollisionWorld.cpp / CollisionWorld.hpp
│   │   └── FPSController.cpp / FPSController.hpp
│   ├── platform/
│   ├── renderer/
│   ├── resources/
│   │   └── ModelLoader.cpp / ModelLoader.hpp
│   ├── ui/
│   │   └── HudAndConsole.cpp / HudAndConsole.hpp
│   ├── weapons/
│   │   ├── ViewModelRenderer.cpp / ViewModelRenderer.hpp
│   │   ├── WeaponDef.cpp / WeaponDef.hpp
│   │   └── WeaponSystem.cpp / WeaponSystem.hpp
│   └── world/
│       ├── EntityManager.cpp / EntityManager.hpp
│       ├── MapFormat.cpp / MapFormat.hpp
│       └── TestMap.cpp / TestMap.hpp
├── game/
│   ├── configs/
│   │   ├── engine.cfg
│   │   └── weapons.cfg
│   └── src/
│       └── main.cpp
├── scripts/
│   └── build_and_test.sh
├── tests/
│   └── test_main.cpp
└── tools/
    ├── asset_converter.cpp
    └── map_viewer.cpp
```
