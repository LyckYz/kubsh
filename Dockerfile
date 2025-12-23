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

# Создаем папку для тестов (даже если она пустая)
RUN mkdir -p /opt/tests

# Если есть папка tests локально - копируем ее содержимое
COPY tests/ /opt/tests/ 2>/dev/null || echo "No test files to copy"

# Компилируем проект
RUN echo "=== Building shell ===" && \
    make && \
    if make -n deb 2>/dev/null; then \
        echo "Building deb package..." && \
        make deb; \
    fi

# Устанавливаем пакет если он создан
RUN echo "=== Installing package ===" && \
    if [ -f "kubsh.deb" ]; then \
        apt-get update && apt-get install -y ./kubsh.deb; \
    elif [ -f "build/kubsh.deb" ]; then \
        apt-get install -y ./build/kubsh.deb; \
    else \
        echo "No deb package found"; \
    fi

# Запускаем тесты через CMD
CMD ["/usr/local/bin/run_tests.sh"]
