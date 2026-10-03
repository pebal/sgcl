[sgcl](../../README.md) › [concurrent](../README.md) › [intern](README.md)

# sgcl::concurrent::intern\<T, Hash, KeyEqual\>::pool

```cpp
static intern& pool() noexcept;
```

Returns the default pool of the type, one for the program: a managed object under a
[root_ptr](../../core/root_ptr/README.md), made on the first call from any thread.

## Parameters

None.

## Return value

A reference to the default pool.

## Complexity

Constant.

## Exceptions

None.

## Notes

The pool is a function-local static, so the first call makes it once whatever the threads, and every thread gets
the same one. It lives for the program: the entries of its values stay until a sweep finds them dead, and its
objects die as soon as nobody holds them. [make](make.md) is `of` on it.

## Example

```cpp
#include "sgcl/concurrent.h"
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    auto& names = concurrent::intern<string>::pool();
    string mine = names.of("Ada");

    string theirs;
    thread other([&theirs] {
        theirs = concurrent::intern<string>::pool().of("Ada");  // the same pool
    });
    other.join();
    println("{}", mine.object() == theirs.object());
}
```

Output:

```text
true
```

## See also

- [make](make.md): `of` on the default pool
- [intern_string](../intern_string.md): the interned string from the default pool of strings
- [sgcl::concurrent::intern\<T, Hash, KeyEqual\>](README.md)
