# Build reference

## Toolchain

- C11 compiler (gcc 8+ or clang 10+)
- GNU make
- POSIX threads

## Libraries

| Library | Debian/Ubuntu/RPi OS | Fedora | Used for |
|---|---|---|---|
| OpenSSL 1.1.1 or 3.x | `libssl-dev` | `openssl-devel` | DTLS 1.2, certificates, HMAC |
| libsrtp2 2.x | `libsrtp2-dev` | `libsrtp-devel` | SRTP protect and unprotect |
| libx264 (optional) | `libx264-dev` | `x264-devel` | software encoder fallback |

Mongoose 7.23 is vendored in `third_party/`, nothing to install.

Install everything on Debian family:

```bash
sudo apt install build-essential libssl-dev libsrtp2-dev libx264-dev
```

## Targets

```bash
make                # build/camstream, the WebRTC server (default)
make legacy         # build/http_server, the stage 4 WebSocket prototype
make clean          # remove build/
make help           # quick reference
```

## Make variables

| Variable | Default | Meaning |
|---|---|---|
| `OPENSSL_DIR` | system | prefix with OpenSSL headers and libs |
| `SRTP_DIR` | system | prefix with `include/srtp2` and lib |
| `X264_DIR` | system | prefix with `x264.h` and lib |
| `HAVE_X264` | autodetected | force `0` to build without the software encoder |
| `CC`, `CFLAGS` | gcc, -O2 | usual overrides |

Example with custom prefixes:

```bash
make OPENSSL_DIR=/opt/ssl SRTP_DIR=/opt/srtp X264_DIR=/opt/x264
```

x264 autodetection checks `$(X264_DIR)/include`, `/usr/include` and `/usr/local/include`. Without x264 the binary still builds and runs wherever a hardware V4L2 M2M encoder exists (for example Raspberry Pi); otherwise startup fails with instructions.

## Static or offline builds (optional)

The same variables work against static libraries:

```bash
# build static deps into /tmp/deps, then
make OPENSSL_DIR=/tmp/deps SRTP_DIR=/tmp/deps X264_DIR=/tmp/deps HAVE_X264=1
```

libsrtp2 can be compiled directly from source without autotools: compile `srtp/srtp.c` plus `crypto/{cipher,hash,kernel,math,replay}/*.c` (minus test files) with `-DHAVE_CONFIG_H -DOPENSSL`, a minimal `config.h` and `crypto/include` on the include path, then archive with `ar rcs`.

## Compile time switches

`-DHAVE_X264=1` is the only project switch, set by the Makefile. Feature macros (`_DEFAULT_SOURCE`, `_POSIX_C_SOURCE=200809L`) come from the Makefile as well, so sources stay plain C11.

## Cross compiling

Set `CC` to the cross prefix and point the three `*_DIR` variables at the sysroot:

```bash
make CC=aarch64-linux-gnu-gcc \
     OPENSSL_DIR=$SYSROOT/usr SRTP_DIR=$SYSROOT/usr X264_DIR=$SYSROOT/usr
```

## Platform support

| Platform | Status |
|---|---|
| x86_64 and arm64 Linux (any distro) | fully supported |
| Raspberry Pi OS Bullseye and Bookworm | fully supported, hardware encoder auto-detected |
| 32 bit ARM | supported (compile time checks pass), hardware encoder recommended |
| macOS and Windows | not supported (V4L2 is Linux only) |
