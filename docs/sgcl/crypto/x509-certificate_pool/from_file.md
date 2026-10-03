[sgcl](../../README.md) › [crypto](../README.md) › [x509](../x509.md) › [certificate_pool](README.md)

# sgcl::crypto::x509::certificate_pool::from_file, async_from_file

```cpp
[[nodiscard]] static expected<certificate_pool, io::error> from_file(const string& path);    // (1)
[[nodiscard]] static async::task<expected<certificate_pool, io::error>>
async_from_file(string path) noexcept;                                                       // (2)
```

Makes a pool of the certificates of a PEM file, read as [append_pem](append_pem.md) reads a text: a block that does
not read is passed over.

1. Reads the file on this thread.
2. The same, the file read on the blocking pool, for a task. `path` is taken by value: the task is lazy, and the
   caller's string may be gone before it runs.

## Parameters

| Parameter | Description |
|---|---|
| `path` | the path of the PEM file |

## Return value

The pool, or the file system's error as an [io::error](../../io/error/README.md) (`is_not_found()` for a file that is not
there).

## Complexity

Linear in the size of the file.

## Exceptions

- (1) None of its own: the failures of the file system are the result's error, the reading being
  [io](../../io/README.md)'s.
- (2) None.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    auto roots = crypto::x509::certificate_pool::from_file("tests/net/tls_testdata/ca.pem");
    println("{} roots", roots->size());
    auto missing = crypto::x509::certificate_pool::from_file("no/such/roots.pem");
    println("{}", missing.error().is_not_found() ? "no roots file" : missing.error().message());
}
```

Output:

```text
1 roots
no roots file
```

In a task:

```cpp
#include "sgcl/async.h"
#include "sgcl/crypto.h"
#include "sgcl/io.h"

using namespace sgcl;

async::task<int> program() {
    auto path = string("tests/net/tls_testdata/ca.pem");
    auto roots = co_await crypto::x509::certificate_pool::async_from_file(path);
    println("{} roots", roots->size());
    co_return 0;
}

int main() {
    return async::run(program());
}
```

Output:

```text
1 roots
```

## See also

- [from_pem](from_pem.md): a text the program has
- [system](system.md): the system's roots
- [sgcl::crypto::x509::certificate_pool](README.md)
