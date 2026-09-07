# MimIR runtime plugin

This repository contains the `runtime` plugin for MimIR. It provides runtime
assertion and requirement operations used by higher-level plugins.

## Standalone build

Install MimIR so that its CMake package is discoverable, then configure this
repository:

```sh
cmake -S . -B build -DCMAKE_PREFIX_PATH=/path/to/mimir/install
cmake --build build -j14
```

When the repository is checked out under MimIR's `extra/` directory, MimIR
discovers and builds it automatically.
