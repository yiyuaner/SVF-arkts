# SVF Typed Pointer Support for LLVM 15

## Problem

When running SVF's saber tool on LLVM 15 bitcode compiled with typed pointers (using `-Xclang -no-opaque-pointers`), the tool crashed with errors:

1. **extapi.bc mismatch**: The `extapi.bc` file was compiled with opaque pointers (LLVM 15 default), but the input bitcode used typed pointers.
2. **Context mode mismatch**: SVF's `LLVMContext` defaulted to opaque pointer mode in LLVM 15+, causing assertion failures when loading typed-pointer bitcode.
3. **PointerType API incompatibility**: Calls to `PointerType::getUnqual(Context)` in LLVM 15+ create opaque pointers, which fail when the context is in typed-pointer mode.

## Solution

### 1. Rebuild extapi.bc with typed pointers

Modified the compilation command to add `-Xclang -no-opaque-pointers`:

```bash
/home/yiyuan/static_analysis/llvm-15-install/bin/clang \
  -w -S -c -fPIC -std=gnu11 -emit-llvm \
  -Xclang -no-opaque-pointers \
  -o Debug-build/lib/extapi.bc \
  svf-llvm/lib/extapi.c
```

### 2. Configure LLVMContext for typed pointers

**File**: `svf-llvm/lib/LLVMModule.cpp:360`

Added code to disable opaque pointers immediately after creating the context:

```cpp
owned_ctx = std::make_unique<LLVMContext>();
#if LLVM_VERSION_MAJOR >= 15
    // Disable opaque pointers for LLVM 15+ to support typed pointer bitcode
    owned_ctx->setOpaquePointers(false);
#endif
```

### 3. Fix PointerType creation calls

Replaced all `PointerType::getUnqual(Context)` calls with typed pointer creation for LLVM 15+:

**Files modified**:
- `svf-llvm/lib/LLVMModule.cpp:558`
- `svf-llvm/lib/SVFIRBuilder.cpp:1827`
- `svf-llvm/include/SVF-LLVM/ObjTypeInference.h:85`
- `svf-llvm/include/SVF-LLVM/SVFIRBuilder.h:284`

**Pattern**:
```cpp
// Old (fails in LLVM 15+ typed-pointer mode)
Type* ptr = PointerType::getUnqual(Context);

// New (works in both modes)
#if LLVM_VERSION_MAJOR >= 15
    Type* ptr = PointerType::get(Type::getInt8Ty(Context), 0);
#else
    Type* ptr = PointerType::getUnqual(Context);
#endif
```

## Usage

After applying these fixes and rebuilding SVF, the saber tool works correctly with typed-pointer bitcode:

```bash
LD_LIBRARY_PATH=/path/to/z3/lib \
  /path/to/SVF/Debug-build/bin/saber \
  -leak your_program.ll \
  -extapi=/path/to/SVF/Debug-build/lib/extapi.bc
```

## Why This Matters

LLVM 15 introduced opaque pointers as the default but still supports typed pointers via `-no-opaque-pointers`. Many existing codebases and tools still use typed pointers. This fix allows SVF to work with both:

- **Opaque pointer bitcode** (LLVM 15+ default)
- **Typed pointer bitcode** (LLVM 15 with `-no-opaque-pointers`, or LLVM 14 and earlier)

## Compatibility

These changes are backward compatible:
- LLVM 14 and earlier: Uses the original `PointerType::getUnqual()` API
- LLVM 15+: Uses typed pointer creation when context is in typed-pointer mode
- The `#if LLVM_VERSION_MAJOR >= 15` guards ensure the code compiles on all LLVM versions

## Files Changed

1. `svf-llvm/lib/LLVMModule.cpp`
2. `svf-llvm/lib/SVFIRBuilder.cpp`
3. `svf-llvm/include/SVF-LLVM/ObjTypeInference.h`
4. `svf-llvm/include/SVF-LLVM/SVFIRBuilder.h`
5. `Debug-build/lib/extapi.bc` (rebuilt with typed pointers)
