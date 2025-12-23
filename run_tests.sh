#!/bin/bash
set -e

echo "=== Running tests ==="
echo "Current directory: $(pwd)"
echo "Contents of /opt:"
ls -la /opt/ || true

# Проверяем наличие тестов
if [ -d "/opt/tests" ]; then
    echo "Found tests directory in /opt/tests"
    cd /opt/tests
    echo "Contents of tests directory:"
    ls -la
else
    echo "Tests directory not found in /opt/tests"
    
    # Ищем тесты в других местах
    echo "Searching for test files..."
    find / -type f -name "*test*.py" 2>/dev/null | head -10
    
    # Проверяем стандартные места
    for dir in /workspace /app /usr/local /home; do
        if [ -d "$dir" ]; then
            echo "Checking $dir for tests..."
            find "$dir" -type d -name "*test*" 2>/dev/null | head -5
        fi
    done
    
    exit 1
fi

# Проверяем наличие pytest
if command -v pytest >/dev/null 2>&1; then
    echo "pytest is available"
    echo "Running tests with pytest..."
    pytest -v --log-cli-level=INFO --tb=short || {
        echo "pytest failed with exit code $?"
        exit 1
    }
elif command -v python3 >/dev/null 2>&1; then
    echo "pytest not found, trying python3 -m pytest..."
    python3 -m pytest -v --log-cli-level=INFO --tb=short || {
        echo "python3 -m pytest failed with exit code $?"
        exit 1
    }
else
    echo "ERROR: Neither pytest nor python3 found!"
    exit 1
fi

echo "=== Tests completed successfully ==="
