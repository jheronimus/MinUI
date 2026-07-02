#!/bin/bash
set -euo pipefail

echo "Running quality checks before push..."
just verify
