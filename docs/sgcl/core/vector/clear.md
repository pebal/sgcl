[sgcl](../../README.md) › [core](../README.md) › [vector](README.md)

# sgcl::vector\<T\>::clear

```cpp
void clear() noexcept;
```

Destroys every element, from the last to the first; the size is 0 after. The buffer stays, with its capacity, for
the elements to come.

## Parameters

None.

## Return value

None.

## Complexity

Linear in `size()`: one destructor per element; constant for elements without a destructor.

## Exceptions

None.

## Notes

The elements are destroyed at once, here, as `std::vector` destroys them, not later by the collector. A vector
that should also give its buffer back calls [shrink_to_fit](shrink_to_fit.md) after, or is assigned an empty
vector.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

struct Noisy {
    int id;
    ~Noisy() {
        println("~{}", id);
    }
};

int main() {
    vector<Noisy> v;
    v.emplace_back(1);
    v.emplace_back(2);
    v.clear();
    println("size {}, capacity {}", v.size(), v.capacity());
}
```

Output:

```text
~2
~1
size 0, capacity 4
```

## See also

- [erase](erase.md): erases elements
- [shrink_to_fit](shrink_to_fit.md): replaces the buffer by one sized for the elements
- [sgcl::vector\<T\>](README.md)
