[sgcl](../README.md) › [crypto](README.md)

# sgcl::crypto::digest_file, async_digest_file

```cpp
#include "sgcl/crypto/hash_id.h"   // or "sgcl/crypto.h"

namespace sgcl::crypto {
    /*(1)*/ expected<vector<byte>, io::error> digest_file(hash_id id, const string& path);
    /*(2)*/ async::task<expected<vector<byte>, io::error>> async_digest_file(hash_id id,
                                                                            const string& path) noexcept;
}
```

The digest of the whole file at `path` by the algorithm `id` names: what [digest](digest.md) gives of the file's
bytes, `digest(id, io::read_file(path))`, without the file held in memory. It is `T::of_file(path)` for the type `T`
that `id` names ([of_file](../hash/mixin/hasher/of_file.md)): the file is opened, read to its end a block at a time
and closed. Where the digest is known in the code, `sha256::of_file(path)` is the same and gives an `array`.

1. On the calling thread; the block of the reads on its stack.
2. The same in a task, for `co_await crypto::async_digest_file(id, path)`; the block managed, since its reads may
   run on the blocking pool. The task keeps its own copy of `path`, so the caller's string may go before the task
   runs.

## Parameters

| Parameter | Description |
|---|---|
| `id` | the digest, one of the values of [hash_id](hash_id.md) |
| `path` | the path of the file |

## Return value

- (1) The digest, `digest_size(id)` bytes, or the error of the open or of a read; a path that is not there is an
  error with `is_not_found()`, a directory an error of its read.
- (2) `async::task` of the same.

## Complexity

Linear in the size of the file.

## Exceptions

- (1) `std::invalid_argument` when `id` is none of the values of `hash_id` (a number cast to it). A FIFO or a device
  is read through io's reactor, and a read there throws `std::system_error` when it has to start the reactor's
  thread and cannot.
- (2) None from the call; the same exceptions come out of the `co_await` of the task.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/crypto.h"
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    (void)io::write_file("release.tar", "first line\nsecond line\n");

    auto id = crypto::hash_id::sha256;  // as a manifest names it
    auto d = crypto::digest_file(id, "release.tar");
    println("{}", encoding::hex::encode(*d));

    // in a task
    auto d3 = async::run(crypto::async_digest_file(crypto::hash_id::sha3_256, "release.tar"));
    println("{}", encoding::hex::encode(*d3));

    auto missing = crypto::digest_file(id, "missing.tar");
    println("{} {}", missing.has_value(), missing.error().is_not_found());
}
```

Output:

```text
c2097f55f01fc297fc7f4acf21438123e06e4d409a818524428534e850642f4f
2e95b053665e94de05ccbc14cba8b38a183896238b3023e4e6ba40f1d86a8019
false true
```

## See also

- [digest](digest.md): bytes already in memory
- [of_file](../hash/mixin/hasher/of_file.md): the same by the digest's type, as an `array`
- [digest_size](digest_size.md): the length of what it gives
- [sgcl::crypto::hash_id](hash_id.md)
