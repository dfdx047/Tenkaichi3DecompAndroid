# The system the releases are built on: Ubuntu 22.04 (glibc 2.35), so that the Linux programs run on any distribution
# of the last few years and on any x86-64 CPU; the Windows programs are cross-compiled in it too. Building on the developer's own machine is not safe for that: a
# distribution compiled for a newer CPU level stamps its start-up code into every program, and its libraries really
# do use those instructions.
#   docker build -t bt3-port-release:jammy port/release        (port/release/build.sh does this)
FROM ubuntu:22.04
ENV DEBIAN_FRONTEND=noninteractive TZ=UTC LANG=C.UTF-8
RUN apt-get update && apt-get install -y --no-install-recommends \
        ca-certificates curl gnupg build-essential binutils python3 cmake ninja-build pkg-config git p7zip-full \
        libwayland-dev libxkbcommon-dev wayland-protocols libdecor-0-dev libx11-dev libxext-dev libxrandr-dev \
        libxcursor-dev libxi-dev libxfixes-dev libxss-dev libxtst-dev libpulse-dev libpipewire-0.3-dev libasound2-dev \
        libdrm-dev libgbm-dev libegl1-mesa-dev libgl1-mesa-dev libgles2-mesa-dev libvulkan-dev libudev-dev libdbus-1-dev \
        libibus-1.0-dev libusb-1.0-0-dev libssl-dev libopus-dev \
    && rm -rf /var/lib/apt/lists/*
# clang / LLVM 23 (the version ptr32.py and irfix.py are written against), with its Python bindings
RUN curl -fsSL https://apt.llvm.org/llvm-snapshot.gpg.key | gpg --dearmor -o /usr/share/keyrings/llvm.gpg \
    && echo "deb [signed-by=/usr/share/keyrings/llvm.gpg] http://apt.llvm.org/jammy/ llvm-toolchain-jammy-23 main" > /etc/apt/sources.list.d/llvm.list \
    && apt-get update && apt-get install -y --no-install-recommends clang-23 llvm-23 libclang-23-dev python3-clang-23 \
    && rm -rf /var/lib/apt/lists/* \
    && ln -s /usr/bin/clang-23 /usr/local/bin/clang && ln -s /usr/bin/llc-23 /usr/local/bin/llc
# SDL3 from source, for any x86-64 CPU; this is also the copy the release carries in lib/
ARG SDL_VERSION=3.4.12
RUN curl -fsSL -o /tmp/sdl.tar.gz https://github.com/libsdl-org/SDL/releases/download/release-${SDL_VERSION}/SDL3-${SDL_VERSION}.tar.gz \
    && tar -xzf /tmp/sdl.tar.gz -C /tmp \
    && cmake -S /tmp/SDL3-${SDL_VERSION} -B /tmp/sdl-build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr/local \
             -DSDL_SHARED=ON -DSDL_STATIC=OFF -DSDL_TEST_LIBRARY=OFF -DSDL_TESTS=OFF -DSDL_EXAMPLES=OFF \
    && cmake --build /tmp/sdl-build && cmake --install /tmp/sdl-build && ldconfig \
    && mkdir -p /usr/local/share/licenses/SDL3 && cp /tmp/SDL3-${SDL_VERSION}/LICENSE.txt /usr/local/share/licenses/SDL3/LICENSE.txt \
    && rm -rf /tmp/sdl.tar.gz /tmp/SDL3-${SDL_VERSION} /tmp/sdl-build
# The shader compiler (glslangValidator), and for the Windows release: the mingw-w64 cross-compiler (the variant with
# POSIX threads) and SDL3's development package for it
RUN apt-get update && apt-get install -y --no-install-recommends glslang-tools mingw-w64 libz-mingw-w64-dev zlib1g-dev zip unzip patchelf file \
    && rm -rf /var/lib/apt/lists/* \
    && update-alternatives --set x86_64-w64-mingw32-gcc /usr/bin/x86_64-w64-mingw32-gcc-posix \
    && update-alternatives --set x86_64-w64-mingw32-g++ /usr/bin/x86_64-w64-mingw32-g++-posix \
    && curl -fsSL -o /tmp/sdl-mingw.tar.gz https://github.com/libsdl-org/SDL/releases/download/release-${SDL_VERSION}/SDL3-devel-${SDL_VERSION}-mingw.tar.gz \
    && mkdir -p /opt/sdl3-mingw && tar -xzf /tmp/sdl-mingw.tar.gz -C /opt/sdl3-mingw && rm /tmp/sdl-mingw.tar.gz
ENV BT3_SDL3=/opt/sdl3-mingw/SDL3-${SDL_VERSION}/x86_64-w64-mingw32
# Tag Team on native Unix uses libcurl's TLS WebSocket implementation.
# Jammy's system libcurl predates that API, so build a pinned native copy.
ARG CURL_VERSION=8.23.0
RUN curl -fsSL -o /tmp/curl.tar.xz https://curl.se/download/curl-${CURL_VERSION}.tar.xz \
    && echo '8dadd7d2536a314f982ad05a651b713f426bf7d02cc2c76cffb9796f2218e6c5  /tmp/curl.tar.xz' | sha256sum -c - \
    && tar -xJf /tmp/curl.tar.xz -C /tmp \
    && cd /tmp/curl-${CURL_VERSION} \
    && ./configure --prefix=/usr/local --with-openssl --enable-websockets \
         --without-libpsl --without-libidn2 --without-libssh2 --without-brotli --without-zstd \
         --disable-static --disable-ldap --disable-ldaps \
    && make -j2 && make install && ldconfig \
    && mkdir -p /usr/local/share/licenses/curl && cp /tmp/curl-${CURL_VERSION}/COPYING /usr/local/share/licenses/curl/COPYING \
    && rm -rf /tmp/curl.tar.xz /tmp/curl-${CURL_VERSION}
# The RTC C API needs SCTP DataChannels as well as media. Use the same release
# and exact submodule sources as the Windows runtime, without floating git refs.
COPY build_rtc.py rtc-sources.lock.json /tmp/bt4-rtc-build/
RUN python3 /tmp/bt4-rtc-build/build_rtc.py --prefix /usr/local --jobs 2 \
    && ldconfig && rm -rf /tmp/bt4-native-rtc /tmp/bt4-rtc-build
WORKDIR /src
