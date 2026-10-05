[sgcl](../../../README.md) › [net](../../README.md) › [imap](../README.md) › [sequence_set](README.md)

# sgcl::net::imap::sequence_set::sequence_set

```cpp
sequence_set() noexcept;                                   // (1)
sequence_set(uint32_t n) noexcept;                         // (2)
sequence_set(uint32_t first, uint32_t last) noexcept;      // (3)
sequence_set(const vector<uint32_t>& numbers) noexcept;    // (4)
explicit sequence_set(const string& text);                 // (5)
```

1. An empty set.
2. One number; `net::imap::last` is `*`.
3. The range `first:last`, either end `net::imap::last` for `*`; a reversed range is the same range.
4. Every number of the list, sorted, consecutive ones joined into ranges.
5. The text, `"1:4,7,10:*"` or `"$"`, as a program writes it.

## Parameters

| Parameter | Description |
|---|---|
| `n` | a number |
| `first`, `last` | the ends of the range |
| `numbers` | the numbers, in any order |
| `text` | the text |

## Return value

None.

## Complexity

- (1–3) Constant.
- (4) O(n log n) in the numbers.
- (5) Linear in the text.

## Exceptions

- (5) `std::invalid_argument` for a text that is no sequence set: [parse](parse.md) reads one that comes from outside.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net/imap.h"

using namespace sgcl;

int main() {
    println("{}", net::imap::sequence_set(42).to_string());
    println("{}", net::imap::sequence_set(10, net::imap::last).to_string());
    println("{}", net::imap::sequence_set(sgcl::vector<uint32_t>{5, 3, 4, 9}).to_string());
    println("{}", net::imap::sequence_set("2,4:6").contains(5));
}
```

Output:

```text
42
10:*
3:5,9
true
```

## See also

- [parse](parse.md)
- [sequence_set](README.md)
