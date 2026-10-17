# BLACK (2006) — Port nativo a Linux

Ejecutable nativo de Linux x86_64 (CPU con AVX2). Sin Wine, sin binarios de Windows.

## 1. Instalar dependencias (Ubuntu/Debian)

```bash
sudo apt-get update
sudo apt-get install -y build-essential cmake ninja-build pkg-config git \
  libsdl2-dev libepoxy-dev libssl-dev python3-venv curl
curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh
```

En Arch: `sudo pacman -S base-devel cmake ninja sdl2 libepoxy openssl python rustup`
En Fedora: `sudo dnf install gcc cmake ninja pkgconf SDL2-devel libepoxy-devel openssl-devel python3 rustup`

## 2. Poner tus archivos del juego

Copia los archivos de **tu copia original de BLACK de Xbox** en la carpeta `game/`
(debe quedar `game/default.xbe` junto a `GlobData.bin`, `levels/`, `sound/` y `videos/`).
El build verifica el SHA-256 de la versión retail soportada.

## 3. Compilar

```bash
bash scripts/setup-native-linux.sh
```

La primera vez tarda varios minutos y necesita internet (descarga el runtime
XboxRecomp y el núcleo DSP fijados) y varios GB de disco.

## 4. Jugar

```bash
./'Play BLACK Native.sh'
```

F11 cambia a pantalla completa. Los ajustes de vídeo e idioma están en
`local/video.ini` y en las variables `RECOMP_*` (ver `docs/linux-native.md`).

> Este paquete contiene solo código fuente y herramientas. No incluye el juego,
> archivos retail, código generado ni ejecutables: todo eso se genera en tu
> máquina a partir de tu copia original.
