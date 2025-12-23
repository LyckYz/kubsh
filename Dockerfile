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

# Копируем скрипт для запуска тестов
COPY run_tests.sh /usr/local/bin/run_tests.sh
RUN chmod +x /usr/local/bin/run_tests.sh

# Копируем файлы проекта
COPY main.cpp /workspace/
COPY Makefile /workspace/

# Копируем тесты (если папка tests существует)
# Используем . чтобы игнорировать ошибку если папки нет
COPY ./tests /opt/tests/ 2>/dev/null || echo "Note: No tests directory found, continuing..."

# Компилируем проект
RUN set -e && \
    echo "=== Building shell ===" && \
    if [ -f "configure" ]; then \
        ./configure; \
    elif [ -f "configure.ac" ]; then \
        autoreconf -i && ./configure; \
    elif [ -f "CMakeLists.txt" ]; then \
        mkdir -p build && cd build && cmake ..; \
    else \
        echo "Using existing Makefile"; \
    fi && \
    make && \
    if make -n deb 2>/dev/null; then \
        make deb; \
    fi && \
    echo "=== Installing package ===" && \
    if [ -f "kubsh.deb" ]; then \
        apt-get update && apt-get install -y ./kubsh.deb; \
    elif [ -f "build/kubsh.deb" ]; then \
        apt-get install -y ./build/kubsh.deb; \
    fi

# Запускаем тесты через CMD
CMD ["/usr/local/bin/run_tests.sh"]
