[sgcl](../README.md) › [compress](README.md) › [xz](xz.md)

# sgcl::compress::xz::filter

```cpp
#include "sgcl/compress/xz.h"   // or "sgcl/compress.h"

namespace sgcl::compress {
    class xz {
    public:
        enum class filter : uint8_t {
            x86,
            arm,
            armt,
            arm64,
            powerpc,
            sparc,
            ia64,
            riscv
        };
    };
}
```

The branch converters of xz: the processor the data's code is for. A converter turns the relative targets of calls
and jumps in machine code into absolute ones, so that calls to one function from many places look alike, before
LZMA2 codes the data; the reader turns them back. It keeps the length of the data, and data that is not code for the
processor passes through it as it is, a little worse compressed. The conversions are the formats' own, bit for bit
those of xz; each is a single pass over the bytes. The [options](xz-options.md)' `bcj` names one.

| Value | Description |
|---|---|
| `x86` | x86 and x86-64 (BCJ): `call` and `jmp` |
| `arm` | ARM, 32-bit instructions: `BL` |
| `armt` | ARM-Thumb: `BL` |
| `arm64` | ARM64: `BL` and `ADRP` |
| `powerpc` | PowerPC, big-endian: `B` with the link bit |
| `sparc` | SPARC: `CALL` |
| `ia64` | IA-64 (Itanium): the branches of a bundle |
| `riscv` | RISC-V: `JAL`, `AUIPC` pairs |

## Example

```cpp
#include "sgcl/compress.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    string text = "the same words, the same words, the same words again";
    io::buffer sink;
    compress::xz::writer w(sink, {.level = 9, .bcj = compress::xz::filter::arm64});
    w.write(text);
    if (auto done = w.close(); !done) {  // the first error of any write, kept
        println("{}", done.error().message());
        return 1;
    }
    compress::xz::reader r(sink);
    println("{}", r.read_all_text() == text);
}
```

Output:

```text
true
```

## See also

- [xz::options](xz-options.md)
- [benchmarks](benchmarks.md): the speed of each converter
- [sgcl::compress::xz](xz.md)
