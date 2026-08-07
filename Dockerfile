# Pinned to Ubuntu 26.04 LTS by digest (latest as of this pin) rather than
# the floating "latest" tag, so a base-image update can't silently change
# this build.
FROM ubuntu:26.04@sha256:678c6550cc43645e08669028bc177f50be4e7c5b8cca677067b1914d4afc7a03

RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential \
    make \
    zip \
    wget \
    git \
    ca-certificates \
    perl \
    file \
    libsdl-dev \
    libsdl2-dev \
    gcc-mingw-w64 \
    g++-mingw-w64 \
    bzip2 \
    xz-utils \
    patch \
    texinfo \
    automake \
    libtool-bin \
    autoconf \
    flex \
    bison \
    && rm -rf /var/lib/apt/lists/*

ENV SDL2_VERSION=2.32.10
RUN wget https://www.libsdl.org/release/SDL2-${SDL2_VERSION}.tar.gz \
    && tar xzf SDL2-${SDL2_VERSION}.tar.gz \
    && cd SDL2-${SDL2_VERSION} \
    && ./configure --host=i686-w64-mingw32 --prefix=/opt/mingw32-sdl \
    && make -j$(nproc) && make install \
    && cd .. \
    && rm -rf SDL2-${SDL2_VERSION} SDL2-${SDL2_VERSION}.tar.gz

ENV PATH="/opt/mingw32-sdl/bin:${PATH}"

# Build the mipsel-elf cross toolchain (used for native Ingenic X1000 device
# builds, e.g. AIGO Eros Q / K Native) via Rockbox's own rockboxdev.sh.
RUN git clone --depth=1 git://git.rockbox.org/rockbox /tmp/rockbox \
    && RBDEV_PREFIX=/opt/rbtoolchain RBDEV_TARGET=i /tmp/rockbox/tools/rockboxdev.sh \
    && rm -rf /tmp/rockbox /tmp/rbdev-dl /tmp/rbdev-build

ENV PATH="/opt/rbtoolchain/bin:${PATH}"

# Fetch and relocate the KNULLI aarch64 buildroot SDK for the H700 device
# family (RG35XX Pro/Plus/H), used for the Anbernic RG35XX Pro application
# build. Unlike the toolchains above, this isn't built via rockboxdev.sh --
# it's KNULLI's own published cross-toolchain + sysroot, which is what
# guarantees the resulting binary links against the same libc/SDL2 that
# actually ships on the device. libsdl2-dev above additionally lets this
# target's simulator build natively on the host. relocate-sdk.sh shells out
# to `file` to tell text files (which need their baked-in old path rewritten)
# from binaries (which must not be touched); without it (file is not part of
# Ubuntu's minimal image) it silently no-ops on every file instead of erroring,
# which only happens not to break tools that self-locate via $0 rather than a
# fixed path, such as sdl2-config -- hence `file` above.
ENV RG35XXPRO_SDK_RELEASE=rg35xx-plush-sdk-20240421
ENV RG35XXPRO_SDK_PATH=/opt/rg35xxpro-sdk/aarch64-buildroot-linux-gnu_sdk-buildroot
RUN mkdir -p /opt/rg35xxpro-sdk \
    && wget -q -O /tmp/rg35xxpro-sdk.tar.gz \
        "https://github.com/knulli-cfw/toolchains/releases/download/${RG35XXPRO_SDK_RELEASE}/aarch64-buildroot-linux-gnu_sdk-buildroot.tar.gz" \
    && tar xzf /tmp/rg35xxpro-sdk.tar.gz -C /opt/rg35xxpro-sdk \
    && rm /tmp/rg35xxpro-sdk.tar.gz \
    && "${RG35XXPRO_SDK_PATH}/relocate-sdk.sh"

ENV PATH="${RG35XXPRO_SDK_PATH}/bin:${PATH}"
