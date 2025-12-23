FROM ubuntu:22.04

# Установка системных зависимостей
RUN apt-get update && apt-get install -y \
    build-essential \
    g++ \
    cmake \
    make \
    autoconf \
    automake \
    libtool \
    pkg-config \
    python3 \
    python3-pip \
    git \
    && rm -rf /var/lib/apt/lists/*

# Устанавливаем pytest
RUN pip3 install pytest

# Создаем рабочую директорию
WORKDIR /workspace

# Копируем файлы проекта
COPY main.cpp /workspace/
COPY Makefile /workspace/

# Копируем тесты (если они есть)
COPY /tests /opt/tests/

# Компилируем проект
RUN set -e && \
    echo "=== Building shell ===" && \
    # Проверяем систему сборки и компилируем
    if [ -f "configure" ]; then \
        ./configure; \
    elif [ -f "configure.ac" ]; then \
        autoreconf -i && ./configure; \
    elif [ -f "CMakeLists.txt" ]; then \
        mkdir -p build && cd build && cmake ..; \
    else \
        echo "Using existing Makefile"; \
    fi && \
    # Компилируем
    make && \
    # Создаем deb-пакет если есть соответствующая цель
    if make -n deb 2>/dev/null; then \
        make deb; \
    fi && \
    echo "=== Installing package ===" && \
    # Устанавливаем пакет
    if [ -f "kubsh.deb" ]; then \
        apt-get update && apt-get install -y ./kubsh.deb; \
    elif [ -f "build/kubsh.deb" ]; then \
        apt-get install -y ./build/kubsh.deb; \
    fi

# Команда для запуска тестов через CMD
CMD ["bash", "-c", "cd /opt && if [ -d \"tests\" ]; then pytest -v --log-cli-level=10; else echo \"Tests not found in /opt/tests\"; find / -name \"*test*\" -type d | grep -E \"(test|tests)\" | head -5; fi"]
