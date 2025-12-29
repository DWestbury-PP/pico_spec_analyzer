# Raspberry Pi Pico Development Environment
# Based on Ubuntu 22.04 with ARM toolchain, CMake, and Pico SDK

FROM ubuntu:22.04

# Prevent interactive prompts during package installation
ENV DEBIAN_FRONTEND=noninteractive

# Set working directory
WORKDIR /workspace

# Install system dependencies
RUN apt-get update && apt-get install -y \
    # Build essentials
    build-essential \
    cmake \
    gcc \
    g++ \
    git \
    # ARM toolchain
    gcc-arm-none-eabi \
    libnewlib-arm-none-eabi \
    libstdc++-arm-none-eabi-newlib \
    # Python for build scripts
    python3 \
    python3-pip \
    # USB tools for device detection
    usbutils \
    # Utilities
    wget \
    curl \
    vim \
    nano \
    # Cleanup
    && apt-get clean \
    && rm -rf /var/lib/apt/lists/*

# Install latest CMake (Ubuntu 22.04 has 3.22, but let's ensure we have a good version)
RUN cmake --version

# Clone Pico SDK and submodules
ENV PICO_SDK_PATH=/opt/pico-sdk
RUN git clone --depth 1 --branch master https://github.com/raspberrypi/pico-sdk.git ${PICO_SDK_PATH} \
    && cd ${PICO_SDK_PATH} \
    && git submodule update --init --depth 1

# Clone CMSIS-DSP (will also be added as submodule in project)
RUN git clone --depth 1 --branch main https://github.com/ARM-software/CMSIS-DSP.git /opt/CMSIS-DSP

# Verify ARM toolchain installation
RUN arm-none-eabi-gcc --version && \
    arm-none-eabi-g++ --version && \
    arm-none-eabi-objdump --version

# Set up environment variables
ENV PICO_SDK_PATH=${PICO_SDK_PATH}
ENV PATH="${PATH}:${PICO_SDK_PATH}/tools"

# Create a non-root user for development (optional but recommended)
ARG USERNAME=pico
ARG USER_UID=1000
ARG USER_GID=${USER_UID}

RUN groupadd --gid ${USER_GID} ${USERNAME} \
    && useradd --uid ${USER_UID} --gid ${USER_GID} -m ${USERNAME} \
    && apt-get update \
    && apt-get install -y sudo \
    && echo ${USERNAME} ALL=\(root\) NOPASSWD:ALL > /etc/sudoers.d/${USERNAME} \
    && chmod 0440 /etc/sudoers.d/${USERNAME} \
    && apt-get clean \
    && rm -rf /var/lib/apt/lists/*

# Switch to non-root user
USER ${USERNAME}

# Set working directory for project
WORKDIR /workspace

# Default command: print environment info
CMD ["bash", "-c", "echo '=== Pico Development Environment ==='; \
     echo 'ARM GCC:' && arm-none-eabi-gcc --version | head -n1; \
     echo 'CMake:' && cmake --version | head -n1; \
     echo 'Pico SDK:' && echo ${PICO_SDK_PATH}; \
     echo ''; \
     echo 'Ready to build! Run: mkdir -p build && cd build && cmake .. && make'; \
     echo 'Or start interactive shell: docker-compose run --rm dev bash'; \
     bash"]

