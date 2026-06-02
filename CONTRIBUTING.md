# Contributing

Thanks for your interest in emcast.

## Building & testing

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

Dependencies are fetched automatically by CMake; no manual setup is needed.

## Ground rules

- **Every change keeps the test suite green.** If you add a feature, add tests.
- **New DSP/protocol behavior needs a round-trip test** that proves bit-exact
  recovery (see `tests/test_roundtrip.cpp`).
- Match the surrounding style (`.clang-format` is provided). Keep public headers
  documented.
- Keep generated artifacts (`build/`, `*.wav`, `*.exe`) out of commits — see
  `.gitignore`.

## Commit / PR

- Write focused commits with clear messages.
- Open PRs against `main`. CI must pass on Linux, macOS and Windows.
- For protocol changes, update `docs/protocol.md` and bump the version.
