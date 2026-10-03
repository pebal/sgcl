[sgcl](../../README.md) › [core](../README.md) › [slice](README.md)

# sgcl::slice\<T\>::operator=

```cpp
slice& operator=(const slice& o) noexcept;    // (1)
slice& operator=(slice&& o) noexcept;         // (2)
```

Makes the slice view the elements of `o`, with its owner.

1. The elements and the owner of `o`. A non-null owner arriving on this thread's stack has the thread registered
   first, as a constructor would; a null owner costs nothing more than the three stores.
2. The same as (1): the source keeps its elements and its owner.

The owner the slice held before is released as a `tracked_ptr` releases its object: nothing is destroyed at once,
the object is the collector's once nothing else refers to it. The elements are not copied.

## Parameters

| Parameter | Description |
|---|---|
| `o` | the slice to view the elements of |

## Return value

`*this`.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    string_slice word;
    {
        string text = "first second";
        word = text.as_slice(6);  // the slice holds the text from now on
    }
    collector::force_collect();  // optional: the text survives, held by the slice
    println("{} {}", word, word.owned());
}
```

Output:

```text
second true
```

## See also

- [(constructor)](slice.md): constructs a slice
- [swap](swap.md): swaps two slices
- [sgcl::slice\<T\>](README.md)
