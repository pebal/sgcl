[sgcl](../../README.md) › [core](../README.md) › [rooted](../rooted.md)

# sgcl::rooted\<T\>::operator\*, operator-\>

```cpp
T& operator*() const noexcept;     // (1)
T* operator->() const noexcept;    // (2)
```

Access the value.

1. The value, by reference: a handle kept in a `rooted` is passed on as `*r`.
2. The address of the value, for a member access: `r->write(...)` on a kept handle.

Debug builds assert that the `rooted` was not moved from.

## Parameters

None.

## Return value

1. `*get()`.
2. `get()`.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

void greet(const string& name) {
    println("hello, {}", name);
}

int main() {
    rooted<string> name(string("ada"));
    greet(*name);  // the handle itself
    println("{} characters", name->size());
}
```

Output:

```text
hello, ada
3 characters
```

## See also

- [get](get.md): the address of the value
- [sgcl::rooted\<T\>](../rooted.md)
