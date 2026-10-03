[sgcl](../../README.md) › [core](../README.md) › [bad_expected_access](README.md)

# sgcl::bad_expected_access\<E\>::what

```cpp
const char* what() const noexcept override;
```

The text of the exception: the error's `message()` when the error has one (an `io::error`, a struct of the
program with a `message()` member), else the general text of `bad_expected_access<void>`, "bad access to
sgcl::expected without a value".

The text is made on the first call, in plain memory, and published with a compare-exchange, so that threads that
read one exception at once (an `exception_ptr` rethrown on each) share one text; a thread that loses the exchange
drops its copy. A copy of the exception, or an assignment to it, makes the text again when asked. A `message()` that
throws leaves the general text.

## Parameters

None.

## Return value

A pointer to the text, valid while the exception lives and is not assigned to.

## Complexity

The call of `message()` and a copy of its text on the first call; constant after.

## Exceptions

None.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

struct Error {
    int code;
    string message() const {
        return code == 404 ? "not found" : "failed";
    }
};

int main() {
    expected<int, Error> described = unexpected(Error{404});
    try {
        described.value();
    } catch (const std::exception& e) {
        println("{}", e.what());
    }

    expected<int, int> plain = unexpected(1);
    try {
        plain.value();
    } catch (const std::exception& e) {
        println("{}", e.what());
    }
}
```

Output:

```text
not found
bad access to sgcl::expected without a value
```

## See also

- [error](error.md): the error itself
- [sgcl::bad_expected_access\<E\>](README.md)
