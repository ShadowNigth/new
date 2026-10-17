# Guía de Compilación y Ejecución en Linux

`ClassicFPS Engine` se compila de forma nativa desde una terminal en Linux utilizando **CMake** y un compilador compatible con **C++20** (`g++ >= 11` o `clang++ >= 14`).

---

## 1. Instalación de dependencias

### Debian 12 / Ubuntu 22.04+
```bash
sudo apt-get update
sudo apt-get install -y \
    build-essential \
    cmake \
    ninja-build \
    pkg-config \
    libsdl2-dev \
    libgl1-mesa-dev \
    libglm-dev \
    libopenal-dev \
    libenet-dev \
    gdb
```

### Arch Linux
```bash
sudo pacman -Syu --needed \
    base-devel \
    cmake \
    ninja \
    pkgconf \
    sdl2 \
    mesa \
    glm \
    openal \
    enet \
    gdb
```

*(Nota: En la Fase 1 el motor enlaza directamente contra `SDL2`, `pthread`, `dl` y opcionalmente `OpenGL::GL`, cargando los puntos de entrada de OpenGL 3.3 Core dinámicamente mediante `SDL_GL_GetProcAddress`).*

---

## 2. Compilación desde la terminal

### Compilación Release (optimizada)
```bash
cmake -S . -B build -GNinja -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
```

### Compilación Debug + Sanitizadores (AddressSanitizer y UBSan)
```bash
cmake -S . -B build-debug -GNinja \
    -DCMAKE_BUILD_TYPE=Debug \
    -DCLASSICFPS_ENABLE_SANITIZERS=ON
cmake --build build-debug -j$(nproc)
```

---

## 3. Ejecución de pruebas unitarias e integración (CTest)

```bash
ctest --test-dir build --output-on-failure -V
```

---

## 4. Ejecución del motor (`classicfps_demo`)

### Ejecución normal en escritorio Linux (X11 / Wayland + OpenGL 3.3 Core)
Desde la raíz del repositorio (carga automáticamente `game/configs/engine.cfg`):
```bash
./build/classicfps_demo
```
Pulsa `ESC` o cierra la ventana para salir limpiamente.

### Opciones de línea de comandos
```bash
# Especificar archivo de configuración y resolución personalizada
./build/classicfps_demo --config game/configs/engine.cfg --width 1024 --height 768

# Ejecutar un número fijo de fotogramas y salir (ideal para pruebas automatizadas)
./build/classicfps_demo --max-frames 60

# Exigir estrictamente contexto OpenGL 3.3 Core (fallar si no hay GPU/driver GL)
./build/classicfps_demo --require-gl

# Ejecutar en modo headless (servidores/contenedores sin X11/Wayland)
./build/classicfps_demo --headless --max-frames 30
```

---

## 5. Solución de problemas frecuentes (OpenGL, SDL2 y controladores)

1. **`OpenGL 3.3 Core required, but driver reported version ...` o fallo en `SDL_GL_CreateContext`**:
   - Verifica el soporte de tu driver con `glxinfo | grep "OpenGL version"`.
   - Si estás en una máquina virtual o deseas forzar el rasterizador por software de Mesa (`llvmpipe`), ejecuta:
     ```bash
     LIBGL_ALWAYS_SOFTWARE=1 ./build/classicfps_demo
     ```
2. **`x11 not available` o `DISPLAY` vacío en sesiones SSH / contenedores**:
   - Asegúrate de ejecutar desde una sesión gráfica X11/Wayland (`echo $DISPLAY` o `echo $WAYLAND_DISPLAY`).
   - En entornos de integración continua sin pantalla, usa `xvfb-run -a ./build/classicfps_demo --max-frames 60` o bien `./build/classicfps_demo --headless --max-frames 60`.
3. **Forzar backend de ventana Wayland o X11 en SDL2**:
   - Para forzar Wayland: `SDL_VIDEODRIVER=wayland ./build/classicfps_demo`
   - Para forzar X11/XWayland: `SDL_VIDEODRIVER=x11 ./build/classicfps_demo`
