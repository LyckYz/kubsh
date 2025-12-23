#!/bin/bash
set -e

echo "=== Running tests ==="
cd /opt

if [ -d "tests" ]; then
    pytest -v --log-cli-level=10
else
    echo "Tests not found in /opt/tests"
    # Ищем тесты в других местах
    find / -name "*test*" -type d | grep -E "(test|tests)" | head -5
    exit 1
fi
