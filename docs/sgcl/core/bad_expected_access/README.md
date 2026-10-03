[sgcl](../../README.md) › [core](../README.md)

# sgcl::bad_expected_access\<E\>

```cpp
#include "sgcl/core/expected.h"   // or "sgcl/core.h"

namespace sgcl {
    template<class E>
    class bad_expected_access;   // : public bad_expected_access<void>

    template<>
    class bad_expected_access<void>;   // : public std::exception
}
```

`sgcl::bad_expected_access<E>` is the exception an [expected](../expected/README.md) throws when its value is asked for and
it holds an error: from `value()`, `*`, `->` and the conversion to the value. It carries a copy of the error, as
`std::bad_expected_access` (C++23) does, and its `what()` is the error's `message()` when the error has one ("open
log.gz: No such file or directory"), so that an exception nobody catches says what failed.

What differs from `std::bad_expected_access`: where the error lies. An exception object lives in unmanaged memory
(the runtime allocates it), where a tracked pointer may not ([The rules](../README.md#the-rules), 1), so the exception
holds its copy in a managed object of its own through a [rooted](../rooted/README.md): an error type with a `tracked_ptr` or a
`string` in it is thrown and caught like any other, the error alive for as long as the exception is, its copies
included.

## Rules

- The exception may live anywhere an exception lives: it holds its error by a root, not by a `tracked_ptr`.
- One managed allocation per throw: the error's managed object. The text of `what()` is made from `message()` when
  `what()` is first called, in plain memory, not at the throw; an exception that is caught and never asked costs
  nothing more.
- An exception read by several threads at once (an `exception_ptr` rethrown on each) makes the text once: a thread
  that loses the race for it drops its copy.

## Template parameters

| Parameter | Description |
|---|---|
| `E` | The type of the error, as the `expected` that throws has it. |

## Member functions

| Function | Description |
|---|---|
| [(constructor)](bad_expected_access.md) | constructs the exception with a copy of the error |
| `(destructor)` | frees the text of `what()`, if made; the error's managed object is left to the collector |
| `operator=` | assigns the error of another exception; the text of `what()` is made again when asked |
| [error](error.md) | the error |
| [what](what.md) | the error's `message()`, or a general text |

## Specializations

`bad_expected_access<void>` is the base of every `bad_expected_access<E>`, derived from `std::exception`:
`catch (const bad_expected_access<void>&)` catches the exception of any `expected`, and `catch (const
std::exception&)` catches it too. It holds no error; its `what()` is "bad access to sgcl::expected without a
value", and its constructors and assignments are protected.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

struct Error {
    tracked_ptr<string> path;  // a tracked pointer in the error: kept by the exception's root
    string message() const {
        return "cannot open " + *path;
    }
};

expected<int, Error> open_file(const char* path) {
    return unexpected(Error{make_tracked<string>(path)});
}

int main() {
    try {
        int fd = open_file("config.toml");
        println("{}", fd);
    } catch (const bad_expected_access<Error>& e) {
        collector::force_collect(true);  // optional: the error survives a cycle
        println("{} / {}", e.what(), *e.error().path);
    }
}
```

Output:

```text
cannot open config.toml / config.toml
```

## See also

- [expected](../expected/README.md): what throws it
- [rooted](../rooted/README.md): how the error is kept
- [unexpected](../unexpected/README.md): the error, wrapped
