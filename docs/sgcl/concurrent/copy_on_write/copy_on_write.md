[sgcl](../../README.md) › [concurrent](../README.md) › [copy_on_write](README.md)

# sgcl::concurrent::copy_on_write\<T\>::copy_on_write

```cpp
copy_on_write()                                                                              // (1)
    noexcept(std::is_nothrow_default_constructible_v<T> &&
             std::is_nothrow_move_constructible_v<T>);
explicit copy_on_write(const T& value) noexcept(std::is_nothrow_copy_constructible_v<T>);    // (2)
explicit copy_on_write(T&& value) noexcept(std::is_nothrow_move_constructible_v<T>);         // (3)
template<class... A>
explicit copy_on_write(std::in_place_t, A&&... a)                                            // (4)
    noexcept(std::is_nothrow_constructible_v<T, A...>);
copy_on_write(const copy_on_write&) = delete;                                                // (5)
```

Constructs the first value, in a managed object of its own.

1. The value `T()`, value-initialized: zero for a number.
2. A copy of `value`.
3. `value`, moved.
4. The value constructed from `a...`, `T(std::forward<A>(a)...)`.
5. A `copy_on_write` is not copyable, and not movable: a shared value has one place.

## Parameters

| Parameter | Description |
|---|---|
| `value` | the first value |
| `a` | the arguments the first value is constructed from |

## Complexity

Constant: one allocation and the construction of one `T`.

## Exceptions

What the constructor of `T` throws; none when it is noexcept.

## Notes

The deduction guide `copy_on_write(T) -> copy_on_write<T>` lets (2) and (3) name the type of the value once:
`concurrent::copy_on_write table(vector<int>{1, 2})` is a `copy_on_write<vector<int>>`.

## Example

```cpp
#include "sgcl/concurrent.h"
#include "sgcl/core.h"
#include "sgcl/io.h"
#include <utility>

using namespace sgcl;

struct Config {
    string host;
    int port;
};

int main() {
    concurrent::copy_on_write<int> counter;
    concurrent::copy_on_write limits(vector<int>{10, 20});
    concurrent::copy_on_write<Config> config(std::in_place, "localhost", 8080);

    println("{} {}", *counter.load(), *limits.load());
    auto c = config.load();
    println("{}:{}", c->host, c->port);
}
```

Output:

```text
0 [10, 20]
localhost:8080
```

## See also

- [load](load.md): a snapshot of the value
- [store](store.md), [update](update.md): replace the value
- [sgcl::concurrent::copy_on_write\<T\>](README.md)
