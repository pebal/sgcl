[sgcl](../../README.md) › [txt](../README.md) › [regex](README.md)

# sgcl::txt::regex::program_size

```cpp
size_t program_size() const noexcept;
```

Returns the number of instructions the pattern was compiled into, for whoever wants to know what a `{n,m}` cost:
this engine spells a repetition out into instructions where a backtracking one keeps a counter, which is why
compiling a counted pattern costs more here than in `std::regex` ([Benchmarks: regex](../benchmarks.md#regex)). A
pattern may spell out to at most 20000 instructions.

## Parameters

None.

## Return value

The number of instructions.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    println("{}", txt::regex("a").program_size());
    println("{}", txt::regex("a{10}").program_size());
    println("{}", txt::regex("a{1000}").program_size());
}
```

Output:

```text
4
13
1003
```

## See also

- [compile](compile.md#limits): the limits of a pattern
- [sgcl::txt::regex](README.md)
