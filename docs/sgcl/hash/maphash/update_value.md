[sgcl](../../README.md) › [hash](../README.md) › [maphash](../maphash.md)

# sgcl::hash::maphash::update_value

```cpp
template<class T>
requires std::has_unique_object_representations_v<T>
void update_value(const T& value) noexcept;
```

Hashes the bytes of `value` as they lie in memory, in the machine's order: the parts of a key that are not text (a
number, a pair of coordinates), with nothing written out first. Go's `h.WriteComparable(v)`.

It is here and nowhere else in the module, because a value of `maphash` never leaves the process, where the order
of the bytes would matter. It takes only a type every byte of which is its value
(`std::has_unique_object_representations`): not a struct with padding, whose padding bytes are anything, and not a
`float` or a `double`, whose `+0` and `−0` are equal and differ in their bytes. Such a key goes in field by field.

## Parameters

| Parameter | Description |
|---|---|
| `value` | the value whose bytes to hash |

## Return value

None.

## Complexity

Linear in `sizeof(T)`.

## Exceptions

None.

## Example

```cpp
#include "sgcl/hash.h"
#include "sgcl/io.h"

#include <type_traits>

using namespace sgcl;

struct point {
    int32_t x;
    int32_t y;
};

struct padded {
    char tag;
    int32_t size;
};

int main() {
    hash::maphash a(42);
    a.update_value(point{3, 4});
    hash::maphash b(42);
    b.update_value(int32_t(3));
    b.update_value(int32_t(4));
    println("{}", a.value() == b.value());

    println("{} {} {}", std::has_unique_object_representations_v<point>,
            std::has_unique_object_representations_v<padded>, std::has_unique_object_representations_v<double>);
}
```

Output:

```text
true
true false false
```

## See also

- [update](update.md): bytes
- [mixin::hasher::update](../mixin/hasher/update.md): a text
- [sgcl::hash::maphash](../maphash.md)
