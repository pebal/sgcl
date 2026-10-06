[sgcl](../../README.md) › [io](../README.md) › [file_lock](README.md)

# sgcl::io::file_lock::unlock

```cpp
expected<void, error> unlock() noexcept;
```

Gives the lock back now (`flock`'s `LOCK_UN`, or the range unlocked), and lets go of it: a second call, or one of a
guard that holds none, does nothing and succeeds. The destructor calls it when nothing did; `unlock()` is how its
error is seen. The file stays open, a handle the guard keeps; close it to give the descriptor back.

## Parameters

None.

## Return value

Nothing, or the [error](../error/README.md), its operation `unlock` and its path the file's: `errc::closed` when the
file was closed meanwhile (which gave the lock back with the descriptor), the `errno` of `flock` or `fcntl`
otherwise.

## Complexity

Constant: one system call.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    io::file_lock held = io::lock_file("unlock.lock").value();
    println("{}", held.unlock().has_value());
    println("{} {}", static_cast<bool>(held), held.unlock().has_value());
    println("{}", io::lock_file("unlock.lock", {.timeout = duration::zero()}).has_value());
}
```

Output:

```text
true
false true
true
```

## See also

- [operator bool](operator_bool.md): whether it holds a lock
- [sgcl::io::file_lock](README.md)
