[sgcl](../../README.md) › [core](../README.md) › [root_ptr](../root_ptr.md)

# sgcl::root_ptr\<T\>::operator\*, operator-\>

```cpp
template<class U = element_type, std::enable_if_t<!std::is_void_v<U>, int> = 0>
U& operator*() const noexcept;                                                     // (1)
element_type* operator->() const noexcept;                                         // (2)
```

Access the object the root points at.

1. The object, by reference; not for `root_ptr<void>`.
2. The address of the object, for a member access.

Debug builds assert that the pointer is not null.

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

struct Settings {
    int width, height;
};

int main() {
    root_ptr<Settings> settings = make_tracked<Settings>(640, 480);
    settings->width = (*settings).height * 2;
    println("{}x{}", settings->width, settings->height);
}
```

Output:

```text
960x480
```

## See also

- [get](get.md): the raw pointer
- [operator bool](operator_bool.md): checks whether the pointer is not null
- [sgcl::root_ptr\<T\>](../root_ptr.md)
