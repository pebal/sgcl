[sgcl](../../README.md) › [txt](../README.md) › [regex](README.md)

# sgcl::txt::regex::regex

```cpp
regex(const /* a pattern read by the compiler */& pattern) noexcept;
```

Constructs a regex from a pattern written into the program: a string literal, or any constant expression that
converts to `std::string_view`. The pattern is read where the program is compiled, as the pattern of
[format](../format.md) is, so `txt::regex re("(?<n>\\d+)\\s*(?i:kg)");` has no optional and no error to handle, and
a pattern that cannot be one does not build. The compiler's message points at one of three sentences, which says
why: a backreference or a lookaround, a pattern past one of the engine's limits, or a malformed pattern —
[compile](compile.md) gives the same reason with the place in the pattern, for a pattern that only arrives while
the program runs.

Nothing the compiler read is kept: the program the machine runs is built again where the regex is constructed,
which costs a few hundred nanoseconds once.

## Parameters

| Parameter | Description |
|---|---|
| `pattern` | the pattern, a constant expression; its syntax is on the page of [compile](compile.md#notes) |

## Complexity

Linear in the length of the pattern once a `{n,m}` is spelled out into instructions.

## Exceptions

None.

## Notes

The type of the parameter is the library's own: it reads the pattern in its `consteval` constructor, and a pattern
that is not a constant expression does not convert to it. Such a pattern goes through [compile](compile.md).

A regex is copied as one word: the copy shares the compiled program, which nothing changes.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    txt::regex weight("(?<n>\\d+)\\s*(?i:kg)");
    auto m = weight.find("paczka: 12 KG, list: 1 kg");
    println("{} -> {}", m->text(), *m->group("n"));
    // txt::regex bad("(a)\\1");  does not build: a backreference
}
```

Output:

```text
12 KG -> 12
```

## See also

- [compile](compile.md): a pattern that arrives while the program runs, and the syntax
- [sgcl::txt::regex](README.md)
