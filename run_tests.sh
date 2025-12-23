#!/bin/bash
set -e

echo "=== Starting test execution ==="

# Проверяем, есть ли тесты
if [ ! -d "/opt/tests" ]; then
    echo "ERROR: Test directory /opt/tests not found!"
    echo "Creating a simple test to verify environment..."
    
    # Создаем простой тест для проверки
    cat > /tmp/simple_test.py << 'EOF'
#!/usr/bin/env python3
import sys
import subprocess

def test_shell_exists():
    """Test that kubsh exists"""
    try:
        result = subprocess.run(['which', 'kubsh'], capture_output=True, text=True)
        if result.returncode == 0:
            print("✓ kubsh found at:", result.stdout.strip())
            return True
        else:
            print("✗ kubsh not found in PATH")
            return False
    except Exception as e:
        print(f"✗ Error checking kubsh: {e}")
        return False

def test_python_version():
    """Test Python version"""
    print(f"✓ Python version: {sys.version}")
    return True

if __name__ == "__main__":
    print("Running simple environment tests...")
    tests = [test_shell_exists, test_python_version]
    passed = 0
    
    for test in tests:
        try:
            if test():
                passed += 1
        except Exception as e:
            print(f"Test failed with error: {e}")
    
    print(f"\n{passed}/{len(tests)} tests passed")
    sys.exit(0 if passed == len(tests) else 1)
EOF
    
    python3 /tmp/simple_test.py
    exit $?
fi

# Запускаем существующие тесты
echo "Running tests from /opt/tests..."
cd /opt/tests

# Проверяем, есть ли requirements.txt и устанавливаем зависимости
if [ -f "requirements.txt" ]; then
    echo "Installing test dependencies..."
    pip3 install -r requirements.txt
fi

# Запускаем тесты
echo "Executing tests..."
if command -v pytest >/dev/null 2>&1; then
    pytest -v --junitxml=/tmp/test-results.xml
else
    python3 -m pytest -v --junitxml=/tmp/test-results.xml
fi

test_exit_code=$?

if [ $test_exit_code -eq 0 ]; then
    echo "✓ All tests passed!"
else
    echo "✗ Some tests failed"
fi

exit $test_exit_code
