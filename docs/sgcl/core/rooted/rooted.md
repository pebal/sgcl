[sgcl](../../README.md) › [core](../README.md) › [rooted](README.md)

# sgcl::rooted\<T\>::rooted

```cpp
template<class... A>
    requires std::is_constructible_v<T, A...>
explicit rooted(std::in_place_t, A&&... a) noexcept(/* see below */);    // (1)
template<class U = T>
rooted(U&& value) noexcept(/* see below */);                             // (2)
rooted(const rooted&) noexcept = default;                                // (3)
rooted(rooted&&) noexcept = default;                                     // (4)
```

Makes the value, or shares the value of another `rooted`.

1. The value made in place from the arguments, by `make_tracked<T>(std::forward<A>(a)...)`:
   `rooted<T> r(std::in_place, args...)`.
2. The value copied or moved in, by `make_tracked<T>(std::forward<U>(value))`; `rooted r(value)` deduces `T`. Takes
   part only when `T` is constructible from `U&&` and `U` is not a `rooted<T>`, which is the copy (3) or the move (4).
3. Shares the value of the other `rooted`: a cell of its own, the same object.
4. Takes the value of the other `rooted` over; the source holds nothing after, to be destroyed or assigned to.

- (1–2) are noexcept when the constructor of `T` they call is.

There is no default constructor: a `rooted` is never null but after a move.

## Parameters

| Parameter | Description |
|---|---|
| `a` | the arguments of the constructor of `T` |
| `value` | the value to copy or move in |
| The other `rooted` | (3) the `rooted` whose value is shared, (4) taken |

## Complexity

Constant: (1–2) one managed object and a root cell, (3–4) a cell.

## Exceptions

(1–2) What the constructor of `T` throws; none when it is noexcept.

## Notes

A handle of the library is kept by (2), a copy of the handle, the same object: `rooted<io::file> log(io::open(p));`.
An `expected` goes in through its value, as the conversion takes it, its error thrown as `bad_expected_access`. A
handle made of arguments is made in place by (1): `rooted<io::buffered_reader> in(std::in_place, f);`.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"
#include <vector>

using namespace sgcl;

struct Session {
    string user;
    int requests = 0;
};

int main() {
    std::vector<rooted<Session>> sessions;  // a std container: a tracked word may not lie here
    sessions.emplace_back(std::in_place, "ada");
    sessions.emplace_back(Session{"alan", 3});
    rooted shared = sessions[0];  // the same Session
    shared->requests = 5;
    println("{} {} {}", sessions[0]->requests, sessions[1]->user, sessions[1]->requests);

    rooted moved = std::move(shared);
    println("{} {}", shared.get() == nullptr, moved->user);
}
```

Output:

```text
5 alan 3
true ada
```

## See also

- [operator=](operator_assign.md): shares another `rooted`'s value
- [make_tracked](../make_tracked.md): creates a managed object
- [sgcl::rooted\<T\>](README.md)
