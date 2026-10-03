[sgcl](../../README.md) › [core](../README.md) › [utf8](../utf8.md)

# sgcl::utf8::ascii_run

```cpp
static constexpr size_t ascii_run(std::string_view s, size_t at = 0) noexcept;
```

Returns how many bytes of `s` from the position `at` are plain ASCII, below 128. The bytes are read eight at a time,
one mask over a word of eight testing them all, and the last few one by one. A caller walks such a run in one step
rather than one step a character: [count](count.md), [valid](valid.md), a string's `to_lower()` and `to_upper()`.

## Parameters

| Parameter | Description |
|---|---|
| `s` | the bytes of the text |
| `at` | the position the run starts at |

## Return value

The length of the run in bytes; 0 when the byte at `at` is not ASCII or `at` is at or past the end, `npos`
included.

## Complexity

Linear in the length of the run.

## Exceptions

None.

## Notes

In a constant expression the bytes are read one by one.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    std::string_view s = "plain text, then żółw";
    size_t run = utf8::ascii_run(s);
    println("{} [{}]", run, s.substr(0, run));
    println("{}", utf8::ascii_run(s, run));
}
```

Output:

```text
17 [plain text, then ]
0
```

## See also

- [all_ascii](all_ascii.md): whether the whole text is ASCII
- [sgcl::utf8](../utf8.md)
