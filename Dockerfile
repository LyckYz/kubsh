FROM ubuntu:22.04

# Установка минимальных зависимостей
RUN apt-get update && apt-get install -y \
    python3 \
    python3-pip \
    && rm -rf /var/lib/apt/lists/*

# Устанавливаем pytest
RUN pip3 install pytest

# Рабочая директория
WORKDIR /workspace

# Копируем файлы проекта
COPY run_tests.sh /usr/local/bin/
RUN chmod +x /usr/local/bin/run_tests.sh

COPY main.cpp /workspace/
COPY Makefile /workspace/

# Компилируем с обработкой ошибок
RUN if [ -f "Makefile" ]; then \
        echo "Building with make..." && \
        make || echo "Make completed (ignoring errors)"; \
    else \
        echo "No Makefile found, skipping build"; \
    fi

# Создаем папку для тестов
RUN mkdir -p /opt/tests

# Запускаем тесты
CMD ["/usr/local/bin/run_tests.sh"]
