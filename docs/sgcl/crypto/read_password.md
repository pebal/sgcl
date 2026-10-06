[sgcl](../README.md) › [crypto](README.md)

# sgcl::crypto::read_password

```cpp
#include "sgcl/crypto/read_secret.h"   // or "sgcl/crypto.h"

namespace sgcl::crypto {
    expected<secret_bytes, io::error> read_password(const string& prompt = {});
}
```

Reads a password typed on the process's own terminal into a [secret_bytes](secret_bytes/README.md): the terminal
`/dev/tty`, whatever the standard streams were redirected to (`prog < input.txt` still asks the person), `prompt`
written to it, the echo turned off ([io::disable_echo](../io/disable_echo.md)), one line read straight into the
secret, and the new line the hidden Enter did not show written after it. The bytes never pass through managed memory
or a buffer left behind: `getpass` and Python's `getpass` return a string, which the collector would free without
zeroing; every block the secret leaves as it grows is zeroed, as [read_secret](read_secret.md)'s are. A backspace (`BS`
or `DEL`) takes the last byte back, `^C` keeps its signal, and the terminal's modes are given back on every path,
the error's included. Go's `term.ReadPassword` reads a descriptor and writes nothing; this is the whole exchange.

## Parameters

| Parameter | Description |
|---|---|
| `prompt` | the text written to the terminal before the line is read |

## Return value

The password, without its `"\n"` (and a `"\r"` before it). Or the [io::error](../io/error/README.md), its operation
`read_password` and its path `/dev/tty`: `ENXIO` when the process has no terminal (a service, a job of a scheduler,
a program started in a session of its own), `io::errc::unexpected_eof` when the input ends (`^D`) before anything was
typed, the `errno` of a read otherwise.

## Complexity

Linear in the line; the wait is the person's.

## Exceptions

None.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    auto password = crypto::read_password("Password: ");
    if (!password) {
        eprintln("{}", password.error().message());
        return 1;
    }
    println("{} bytes", password->size());
}
```

## See also

- [read_secret](read_secret.md): a file's bytes as a secret
- [secret_bytes](secret_bytes/README.md)
- [io::disable_echo](../io/disable_echo.md), [io::make_raw](../io/make_raw.md): the terminal's modes
