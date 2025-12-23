FROM ubuntu:22.04

# Установка зависимостей
RUN apt-get update && apt-get install -y \
    build-essential \
    python3 \
    python3-pip \
    && rm -rf /var/lib/apt/lists/*

# Устанавливаем pytest
RUN pip3 install pytest

# Рабочая директория
WORKDIR /workspace

# Копируем все файлы
COPY . /workspace/

# Делаем скрипт исполняемым
RUN chmod +x run_tests.sh && \
    mv run_tests.sh /usr/local/bin/

# Компилируем проект
RUN make

# Создаем папку для тестов
RUN mkdir -p /opt/tests

CMD ["/usr/local/bin/run_tests.sh"]
