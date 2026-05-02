# syntax=docker/dockerfile:1.6
FROM debian:bookworm-slim

ARG DEBIAN_FRONTEND=noninteractive
ARG TARGETARCH
ARG ARM_GCC_VERSION=13.3.rel1

# Install minimal runtime build deps (keep it small: no build-essential, no git, no python)
RUN apt-get update && apt-get install -y --no-install-recommends \
    ca-certificates bash cmake ninja-build \
 && rm -rf /var/lib/apt/lists/*

# Install ARM toolchain depending on container architecture
# - amd64/arm64: download official Arm tarball (glibc); remove curl/xz afterwards
# - arm (arm/v7): use Debian packages (no official tarball)
RUN set -eux; \
  case "${TARGETARCH}" in \
    amd64) \
      apt-get update && apt-get install -y --no-install-recommends curl xz-utils; \
      URL="https://developer.arm.com/-/media/Files/downloads/gnu/${ARM_GCC_VERSION}/binrel/arm-gnu-toolchain-${ARM_GCC_VERSION}-x86_64-arm-none-eabi.tar.xz"; \
      curl -L "$URL" -o /tmp/gcc.tar.xz; \
      mkdir -p /opt; tar -xJf /tmp/gcc.tar.xz -C /opt; rm -f /tmp/gcc.tar.xz; \
      ln -s /opt/arm-gnu-toolchain-${ARM_GCC_VERSION}-*/bin/* /usr/local/bin/; \
      apt-get purge -y curl xz-utils && apt-get autoremove -y; \
      rm -rf /var/lib/apt/lists/* /usr/share/doc /usr/share/man; \
      ;; \
    arm64) \
      apt-get update && apt-get install -y --no-install-recommends curl xz-utils; \
      URL="https://developer.arm.com/-/media/Files/downloads/gnu/${ARM_GCC_VERSION}/binrel/arm-gnu-toolchain-${ARM_GCC_VERSION}-aarch64-arm-none-eabi.tar.xz"; \
      curl -L "$URL" -o /tmp/gcc.tar.xz; \
      mkdir -p /opt; tar -xJf /tmp/gcc.tar.xz -C /opt; rm -f /tmp/gcc.tar.xz; \
      ln -s /opt/arm-gnu-toolchain-${ARM_GCC_VERSION}-*/bin/* /usr/local/bin/; \
      apt-get purge -y curl xz-utils && apt-get autoremove -y; \
      rm -rf /var/lib/apt/lists/* /usr/share/doc /usr/share/man; \
      ;; \
    arm) \
      apt-get update && apt-get install -y --no-install-recommends \
        gcc-arm-none-eabi binutils-arm-none-eabi gdb-multiarch \
      && rm -rf /var/lib/apt/lists/* /usr/share/doc /usr/share/man; \
      ;; \
    *) echo "Unsupported TARGETARCH=${TARGETARCH}"; exit 1; \
  esac

# Optional: trim docs/examples inside the Arm toolchain to save a few MB
RUN set -eux; \
  if ls /opt/arm-gnu-toolchain-* >/dev/null 2>&1; then \
    find /opt/arm-gnu-toolchain-* -type d \( -name doc -o -name docs -o -name man -o -name info -o -name share \) -prune -exec rm -rf {} + || true; \
  fi

WORKDIR /src
CMD ["bash"]
