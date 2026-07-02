set shell := ["bash", "-uc"]

# Show available commands
default:
    @just --list

# Format check using clang-format
fmt-check:
    find src tests -name "*.c" -o -name "*.h" | xargs clang-format --dry-run --Werror

# Lint check using clang-tidy
lint:
    clang-tidy --checks='bugprone-*,cert-*,clang-analyzer-*,misc-*,performance-*,-misc-use-internal-linkage,-clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling' --warnings-as-errors='*' tests/*.c -- -std=c17 -Isrc

# Build code
build:
    make

# Run tests
test: build
    ./test_runner

# Run all verification checks in sequence
verify: fmt-check lint test
    @echo "All quality checks passed successfully!"

# Setup local hooks
setup:
    @mkdir -p .git/hooks
    @cp scripts/pre_push_hook.sh .git/hooks/pre-push
    @chmod +x .git/hooks/pre-push
    @echo "Git pre-push hook installed successfully!"

# Clean build artifacts
clean:
    make clean
