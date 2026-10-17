# Arquitectura de ClassicFPS Engine

## 1. Filosofía de diseño

`ClassicFPS Engine` es un motor FPS ligero, modular y nativo para Linux escrito en **C++20**, inspirado en la arquitectura técnica, el determinismo y la baja sobrecarga de **GoldSrc / Counter-Strike 1.6**.

Principios clave:
- **Separación estricta entre simulación y presentación**: la lógica de juego y la física avanzan con un reloj de paso fijo (`sys_ticrate`, por defecto 100 Hz) desacoplado de la frecuencia de fotogramas (`fps_max` / VSync).
- **Abstracción de bajo acoplamiento**: las bibliotecas externas (`SDL2`, `OpenGL 3.3 Core`, `GLM`, `OpenAL Soft`, `ENet`) se encapsulan detrás de interfaces propias del motor (`platform::Window`, `renderer::IGraphicsDevice`, etc.).
- **Gestión determinista de memoria y recursos (RAII)**: cada subsistema se inicializa en orden explícito y libera sus recursos en orden inverso al cerrar o ante una señal del sistema (`SIGINT`, `SIGTERM`).

---

## 2. Diagrama de capas y módulos

```text
+-------------------------------------------------------------------+
|                     Ejecutable / Juego (game/)                    |
|              main.cpp, configuración CVars (engine.cfg)           |
+-------------------------------------------------------------------+
                                 |
                                 v
+-------------------------------------------------------------------+
|                      Núcleo del Motor (engine/core/)              |
|   Engine (Bucle principal) | EngineClock (Tick fijo + Render dt)  |
|   Config (.cfg CVars)      | ResourceManager (RAII + IDs)         |
|   Logger (Consola/Archivo) | ErrorHandler (Señales + Reportes)    |
+-------------------------------------------------------------------+
        |                        |                        |
        v                        v                        v
+-------------------+  +-----------------------+  +-----------------+
| Plataforma        |  | Renderizador 3D       |  | Depuración      |
| (engine/platform) |  | (engine/renderer)     |  | (engine/debug)  |
| - Window (SDL2)   |  | - IGraphicsDevice     |  | - FrameStats    |
| - Eventos Linux   |  | - OpenGLGraphicsDevice|  |   (FPS, ms/f,   |
|   (X11/Wayland/   |  |   (OpenGL 3.3 Core)   |  |    ticks/frame) |
|    Dummy)         |  | - GL33Loader          |  +-----------------+
+-------------------+  | - SoftwareGraphicsDev |
                       +-----------------------+
```

---

## 3. Subsistemas implementados en la Fase 1

1. **`engine/core/Version`**: versión semántica empaquetada (`0.1.0-phase1-core`) y comprobación de compatibilidad binaria/protocolo.
2. **`engine/core/Logger`**: registro thread-safe con niveles (`TRACE`, `DEBUG`, `INFO`, `WARN`, `ERROR`, `FATAL`), marcas de tiempo UTC con milisegundos, salida opcional a archivo y historial circular en memoria para consola de diagnóstico.
3. **`engine/core/Error`**: categorización de errores críticos (`Config`, `Platform`, `Graphics`, `Resource`, `Runtime`, `Signal`) y captura de señales POSIX (`SIGINT`, `SIGTERM`) para apagado seguro.
4. **`engine/core/Clock`**: reloj dual que combina:
   - Acumulador de paso fijo (`fixedDeltaTime = 1.0 / sys_ticrate`) con protección contra *espiral de la muerte* (`maxFrameDelta = 0.25s`).
   - Reloj de renderizado y factor de interpolación `alpha` en `[0.0, 1.0]`.
5. **`engine/core/Config`**: parser y serializador de configuración inspirado en los archivos `.cfg` de GoldSrc (`clave valor`, `clave "valor"`, `clave = valor`, comentarios `#` y `//`).
6. **`engine/core/ResourceManager`**: registro centralizado de recursos `IResource` con identificadores `ResourceId` de 64 bits, búsqueda por nombre/ID, contabilidad de bytes en memoria y liberación garantizada al apagar el motor.
7. **`engine/platform/Window`**: ventana nativa Linux mediante SDL2 con configuración de atributos OpenGL 3.3 Core Profile (24-bit depth, 8-bit stencil, double buffering), manejo de eventos (`SDL_QUIT`, `SDL_WINDOWEVENT_CLOSE`, redimensionado, tecla `ESC`) y soporte transparente para entornos sin servidor gráfico (`SDL_VIDEODRIVER=dummy`).
8. **`engine/renderer/IGraphicsDevice` + `OpenGLGraphicsDevice` + `GL33Loader` + `SoftwareGraphicsDevice`**:
   - **`OpenGLGraphicsDevice`**: crea y valida un contexto **OpenGL 3.3 Core Profile**, carga dinámicamente los puntos de entrada mediante `SDL_GL_GetProcAddress` (`GL33Loader`), configura `glViewport`, prueba de profundidad (`GL_DEPTH_TEST`), descarte de caras traseras (`GL_CULL_FACE`), `glClearColor`, `glClear`, verificación de errores (`glGetError`) e intercambio de buffers (`SDL_GL_SwapWindow`).
   - **`SoftwareGraphicsDevice`**: implementación de respaldo con framebuffer `ARGB8888` y `Z-buffer` en CPU sobre `SDL_Surface`, útil para validación automatizada en contenedores/terminales Linux sin servidor X11/Wayland ni ICD de GPU.
9. **`engine/debug/FrameStats`**: ventana deslizante de 120 muestras sin asignaciones dinámicas en el bucle crítico para calcular FPS, tiempo medio/mínimo/máximo de fotograma en milisegundos y total de ticks simulados.

---

## 4. Subsistemas implementados en las Fases 2 a 8

- **Fase 2 (`engine/renderer/`, `engine/world/`)**:
  - `FirstPersonCamera` y `Frustum`: matrices de vista/proyección con GLM y extracción de los 6 planos del frustum para *frustum culling* (`intersectsAABB`).
  - `Mesh`, `Texture2D` y `ShaderProgram`: mallas 3D (`Vertex3D`), texturas procedurales `64x64` (`GL_NEAREST`, `GL_REPEAT`, serialización `.ppm`) y shaders GLSL `#version 330 core` con iluminación direccional + puntual + niebla lineal.
- **Fase 3 (`engine/input/`, `engine/physics/`)**:
  - `InputManager`: captura de teclado y ratón, sensibilidad configurable y generación determinista de `PlayerCommand`.
  - `CollisionWorld` y `FPSController`: colisión continua por barrido de AABB (`sweepPlayerHull`), rampas inclinadas (`RampNegZ`), gravedad, salto, agachado con comprobación de techo y subida de escalones.
- **Fase 4 (`engine/weapons/`)**:
  - `WeaponDef` y `WeaponLibrary`: definición genérica de armas cargada desde `game/configs/weapons.cfg` sin necesidad de recompilar el motor.
  - `WeaponSystem`: máquina de estados determinista (`Idle`, `Firing`, `Reloading`, `Switching`, `Empty`) totalmente desacoplada del renderizado; soporta disparos instantáneos (`HitscanRaycast`) y proyectiles físicos (`PhysicalProjectile`), dispersión dinámica, retroceso con recuperación y multiplicadores de daño por zona (`head` `4x`, `torso` `1x`).
  - `ViewModelRenderer`: modelo low-poly en primera persona compuesto por 11 piezas articuladas (carabina de 7 partes + antebrazos y manos enguantadas de 4 partes), animaciones procedurales por estado, fogonazo en boca de cañón (*muzzle flash*) y marcadores 3D de impacto en el escenario.
- **Fase 5 (`engine/animation/`, `engine/resources/`, `engine/audio/`)**:
  - `Skeleton` y `AnimationPlayer`: jerarquía de huesos (`Bone`), interpolación lineal/esférica (`slerp`) de keyframes (`TransformTRS`), cálculo de matrices globales y de *skinning* (`globalMatrix * inverseBindMatrix`), y mezcla suave (*cross-fade*) entre animaciones.
  - `ModelLoader`: carga y serialización de modelos articulados en **glTF 2.0** (`.gltf` con buffer binario embebido en Base64) y **Wavefront OBJ** (`.obj`), generando los activos `assets/models/tactical_carbine.gltf` y `assets/models/target_dummy.gltf`.
  - `AudioEngine` y `SoundClip`: carga/guardado de archivos PCM 16-bit `RIFF/WAVE` (`.wav`), síntesis procedural de sonidos clásicos FPS (`assets/sounds/*.wav`) y mezclador estéreo espacial 3D con atenuación por distancia y panoramización izquierda/derecha.
- **Fase 6 (`engine/world/EntityManager`, `engine/world/MapFormat`, `engine/ui/`, `tools/`)**:
  - `EntityManager`: ciclo de vida de entidades (`PlayerSpawn`, `TargetDummy`, `WeaponPickup`, `AmmoCrate`, `PointLight`, `TriggerZone`), salud, destrucción, animación de impacto (`hit_flinch`) y reaparición automática.
  - `MapFormat`: serialización y carga de mapas en texto legible (`.cfmap`) y formato binario versionado (`.cbsp`, magia `"CFBM"` v1).
  - `HudRenderer` y `DiagnosticConsole`: HUD clásico estilo CS 1.6 con mira dinámica sensible a la dispersión, fuente bitmap 5x7 e intérprete de comandos en tiempo real.
  - Herramientas independientes: `classicfps_asset_converter` y `classicfps_map_viewer`.
- **Fase 7 (`engine/networking/`)**:
  - `UdpSocket`, `NetServer` y `NetClient`: servidor autoritativo y cliente sobre sockets UDP POSIX no bloqueantes (`"CFSP"` v1), envío de `ClientUserCommand`, difusión de `ServerWorldSnapshot`, **predicción en el cliente** e historial circular de comandos para **reconciliación y re-simulación** ante discrepancias con el servidor.
- **Fase 8 (`engine/debug/Profiler`)**:
  - `Profiler` y `ScopedProfileTimer`: instrumentación de tiempos de CPU por subsistema en microsegundos (`Render3D`, `HudOverlay`, `Physics`, `Weapons`, `EntitiesAndAudio`) y verificación integral con `ASan` + `UBSan`.
