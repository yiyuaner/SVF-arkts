# SVF Build Instructions

After the static-linking changes, all SVF executables (`saber`, `wpa`, `dvf`,
`ae`, `mta`, `cfl`, `llvm2svf`, `svf-ex`) statically link `libz3`, `libz`,
`libtinfo`, `libstdc++`, and `libgcc`. The resulting binaries depend only on
glibc-family shared libraries (`libc`, `libm`, `libdl`, `libpthread`, `librt`)
plus `linux-vdso` and the dynamic loader.

## Prerequisites

- CMake 3.23+
- A C/C++ toolchain (GCC 11+ tested) with the static archives available:
  - `/usr/lib/x86_64-linux-gnu/libz.a`
  - `/usr/lib/x86_64-linux-gnu/libtinfo.a`
  - `/usr/lib/gcc/x86_64-linux-gnu/<gcc-ver>/libstdc++.a`
  - `/usr/lib/gcc/x86_64-linux-gnu/<gcc-ver>/libgcc.a`
- An LLVM install (15+ tested) — directory pointed to by `LLVM_DIR` /
  `LLVM_HOME`
- A Z3 install with **both `libz3.a` and headers** — directory pointed to by
  `Z3_HOME`

For this checkout the following layout is used:

| Variable    | Value                                              |
|-------------|----------------------------------------------------|
| `LLVM_DIR`  | `/home/yiyuan/static_analysis/llvm-15-install/lib/cmake/llvm` |
| `Z3_HOME`   | `/home/yiyuan/static_analysis/z3.obj`              |

## Configure

From the repository root:

```bash
cmake -S . -B Debug-build \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
  -DLLVM_DIR=/home/yiyuan/static_analysis/llvm-15-install/lib/cmake/llvm \
  -DZ3_HOME=/home/yiyuan/static_analysis/z3.obj
```

For a release build replace `-DCMAKE_BUILD_TYPE=Debug` with
`-DCMAKE_BUILD_TYPE=Release` and use a different build directory
(e.g. `Release-build`).

### Important: use `Z3_HOME`, not `Z3_DIR`

CMake interprets `Z3_DIR` as the directory containing a `Z3Config.cmake`
file (consumed by `find_package(Z3 CONFIG)`). The Z3 install at
`/home/yiyuan/static_analysis/z3.obj` does not ship one, so
`find_package(Z3 CONFIG)` will fail and emit a confusing error. Pass
`Z3_HOME` instead — the custom `cmake/Modules/FindZ3.cmake` falls back to a
manual header+library search that prefers `libz3.a`.

### Reconfiguring an existing build directory

If `Debug-build/CMakeCache.txt` was created by a previous (dynamic) build,
clear the cached Z3 paths so the new static-search logic re-runs:

```bash
cmake -UZ3_LIBRARY_DIR -UZ3_INCLUDE_DIR -UZ3_DIR \
  -DZ3_HOME=/home/yiyuan/static_analysis/z3.obj Debug-build
```

## Build

```bash
cmake --build Debug-build -j$(nproc)
```

Or build a single tool:

```bash
cmake --build Debug-build --target saber -j$(nproc)
```

Binaries land in `Debug-build/bin/`.

## Verify

Confirm only glibc-family libraries are dynamically linked:

```bash
ldd Debug-build/bin/saber
```

Expected output:

```
linux-vdso.so.1
libm.so.6 => /lib/x86_64-linux-gnu/libm.so.6
libc.so.6 => /lib/x86_64-linux-gnu/libc.so.6
/lib64/ld-linux-x86-64.so.2
```

Any of `libz3.so`, `libz.so.1`, `libtinfo.so.6`, `libstdc++.so.6`, or
`libgcc_s.so.1` appearing in the output indicates the static linking did
not take effect — re-check that `Z3_HOME` points to a directory containing
`libz3.a`, and that the system static archives listed in **Prerequisites**
exist.

Smoke-test the binary runs:

```bash
./Debug-build/bin/saber --help
```

## What changed in the build system

- `cmake/Modules/FindZ3.cmake` — manual search prefers `libz3.a`, falling
  back to `libz3.so` only if no static archive is found.
- `svf-llvm/CMakeLists.txt` — overrides the `ZLIB::ZLIB` and
  `Terminfo::terminfo` imported targets that LLVM's `LLVMSupport` pulls in
  transitively, pointing them at the system static archives.
- `svf-llvm/tools/CMakeLists.txt` — adds `-static-libstdc++ -static-libgcc`
  to every executable in the `ALL_TOOLS` list.
