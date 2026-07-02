# MinUI Code Quality Guidelines

The quality gate is intentionally small and high-signal. Run it locally and in CI with:

```sh
just verify
```

## Checks

| Tool | Purpose |
|---|---|
| gcc/clang | Compilation with strict warnings: `-std=c17 -Wall -Wextra -Werror -pedantic` |
| clang-format | Enforce consistent code formatting |
| clang-tidy | Static analysis and style check |
| test_runner | Unit tests verify core behavior |

## Local hook

`just setup` installs `scripts/pre_push_hook.sh` into `.git/hooks/pre-push`. Every push runs `just verify`.

## Continuous integration

GitHub Actions runs `just verify` for every push to `main`.
