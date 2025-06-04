FROM devkitpro/devkita64

# Install latest cmake
RUN echo "Removing older version of cmake" && \
  apt remove --purge --auto-remove -y cmake
RUN echo "Installing latest cmake" 
RUN apt-get update && apt-get install -y curl
RUN CMAKE_LATEST_URL=$(curl -s https://api.github.com/repos/Kitware/CMake/releases/latest | grep "browser_download_url.*linux-x86_64.sh" | cut -d '"' -f 4) && \
  curl -L "$CMAKE_LATEST_URL" -o /cmake-latest-linux-x86_64.sh && \
  mkdir /opt/cmake && \
  sh /cmake-latest-linux-x86_64.sh --prefix=/opt/cmake --skip-license
RUN ln -s /opt/cmake/bin/cmake /usr/local/bin/cmake
RUN cmake --version

COPY . /workspace
WORKDIR /workspace
