[sgcl](../../README.md) › [core](../README.md) › [string](README.md)

# sgcl::string::repeat

```cpp
basic_string repeat(size_type count) const;
```

Returns the string `count` times over, as Go's `strings.Repeat` and Java's `String.repeat`: the empty string for 0
or for an empty string, the same object for 1. The result is written once, into a string of its size, after its size
is checked against `max_size()`.

## Parameters

| Parameter | Description |
|---|---|
| `count` | how many times the string stands in the result |

## Return value

The new string of `count` copies of the characters; the empty string when `count` is 0 or the string is empty; this
string's object when `count` is 1.

## Complexity

Linear in the length of the result.

## Exceptions

`length_error` when the result would pass `max_size()`; nothing is allocated then.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    string ab = "ab";
    println("{}", ab.repeat(3));
    println("{} {}", ab.repeat(0).empty(), ab.repeat(1).object() == ab.object());
    try {
        ab.repeat(3'000'000'000);
    } catch (const length_error& e) {
        println("length_error: {}", e.what());
    }
}
```

Output:

```text
ababab
true true
length_error: sgcl::basic_string::repeat
```

## See also

- [concat](concat.md): one string of a few known pieces
- [join](join.md): one string of a range of parts with a separator between each two
- [max_size](max_size.md): the largest number of characters a string holds
- [sgcl::string](README.md)
