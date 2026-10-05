[sgcl](../../../README.md) › [net](../../README.md) › [sftp](../README.md)

# sgcl::net::sftp::file

```cpp
#include "sgcl/net/sftp/client.h"   // or "sgcl/net/sftp.h"

namespace sgcl::net::sftp {
    class file final : public io::mixin::reader<file>, public io::mixin::writer<file>, public io::mixin::seeker<file>;
}
```

**Requires [rooted](../../../core/rooted/README.md) outside a stack or a managed object.**

`net::sftp::file` is a file open on an SFTP server, as [client::open](../client/open.md) and
[client::create](../client/create.md) give it: a handle of the server's and a position, read, written and seeked as
[io::file](../../../io/file/README.md) is, so that io's algorithms (`read_full`, `read_all`, `copy_to`,
`copy_from`) and every function that takes an [io::reader](../../../io/reader/README.md) or
[io::writer](../../../io/writer/README.md) take it too. Go's `sftp.File`.

A file is a handle of one word: a copy is the same file and the same position. The position is the client's, since
SFTP reads and writes at offsets: [read_at](read_at.md) and [write_at](write_at.md) leave it alone and may run at once
from many tasks.

## Rules

- [read](read.md) is one request, of at most the server's largest read; [write](write.md) sends the whole of its data
  in requests in flight together. A whole file moves faster by the client's [read_file](../client/read_file.md),
  [download](../client/download.md) and [upload](../client/upload.md).
- [close](close.md) gives the server's handle back; a file not closed holds it until the session ends, and the
  server's limit of open handles counts it. The session's end ends the file's calls with its error.
- The blocking forms are for a thread, as [task::wait](../../../async/task/wait.md) is; a task awaits the `async_`
  forms.

## Member functions

| Function | Description |
|---|---|
| [(constructor)](file.md) | no file |

#### Reading and writing

| Function | Description |
|---|---|
| [read, async_read](read.md) | reads at the position, one request |
| [write, async_write](write.md) | writes the whole of the data at the position |
| [read_at, async_read_at](read_at.md) | reads at an offset, the position untouched |
| [write_at, async_write_at](write_at.md) | writes at an offset, the position untouched |

#### Positioning

| Function | Description |
|---|---|
| [seek](seek.md) | moves the position |

#### File operations

| Function | Description |
|---|---|
| [stat, async_stat](stat.md) | what the server says about the open file |
| [set_stat, async_set_stat](set_stat.md) | changes its attributes |
| [truncate, async_truncate](truncate.md) | changes its size |
| [sync, async_sync](sync.md) | waits until what was written reaches the server's disk |
| [close, async_close](close.md) | gives the server's handle back |

#### Observers

| Function | Description |
|---|---|
| [is_closed](is_closed.md) | whether the file was closed |
| [path](path.md) | the path the file was opened by |
| [operator bool](operator_bool.md) | whether the handle holds a file |

#### From mixin::reader

[mixin::reader](../../../io/mixin/reader/README.md): the algorithms of io over this file, each with its `async_` form.

| Function | Description |
|---|---|
| [read_full, async_read_full](../../../io/mixin/reader/read_full.md) | fills the whole buffer, or says why not |
| [read_all, async_read_all](../../../io/mixin/reader/read_all.md) | reads to the end, into a `vector<byte>` |
| [read_all_text, async_read_all_text](../../../io/mixin/reader/read_all_text.md) | reads to the end, into a `string` |
| [copy_to, async_copy_to](../../../io/mixin/reader/copy_to.md) | copies everything to the end into a writer |

#### From mixin::writer

[mixin::writer](../../../io/mixin/writer/README.md): the overloads of `write` and `async_write` beside the file's own.

| Function | Description |
|---|---|
| [write, async_write](../../../io/mixin/writer/write.md) | writes a text or one byte |
| [copy_from, async_copy_from](../../../io/mixin/writer/copy_from.md) | copies everything from a reader to its end into the file |

#### From mixin::seeker

[mixin::seeker](../../../io/mixin/seeker/README.md), over [seek](seek.md).

| Function | Description |
|---|---|
| [tell](../../../io/mixin/seeker/tell.md) | the position |
| [size](../../../io/mixin/seeker/size.md) | the size, the position kept |
| [rewind](../../../io/mixin/seeker/rewind.md) | moves the position to the beginning |

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"
#include "sgcl/net/sftp.h"
#include "sgcl/net/ssh.h"
#include "sgcl/net.h"

using namespace sgcl;

int main() {
    (void)io::mkdir_all("files");  // the directory the server serves
    net::ssh::server srv;
    srv.host_keys = {net::ssh::private_key::generate()};
    srv.check_password = [](const string& user, const string& password) { return password == "secret"; };
    srv.handle([](net::ssh::server_session s) {
        if (s.subsystem() == "sftp") {
            (void)net::sftp::serve(s, "files");
        }
    });
    net::listener l = net::tcp::listen("127.0.0.1:0");
    async::go(srv.async_serve(l));

    net::ssh::client::options o;
    o.user = "ann";
    o.password = "secret";
    o.insecure_ignore_host_key = true;  // the server's key was made just now
    net::sftp::client fs = net::sftp::client::connect(l.local_endpoint().to_string(), o);
    auto rw = io::open_flags::read | io::open_flags::write | io::open_flags::create;
    net::sftp::file f = fs.open("/list.txt", rw);
    f.write("apples\npears\n");
    f.rewind();
    print("{}", f.read_all_text().value());
    f.close();
    srv.close();
}
```

Output:

```text
apples
pears
```

## See also

- [client::open](../client/open.md), [client::create](../client/create.md)
- [io::file](../../../io/file/README.md): a local file
- [net::sftp](../README.md)
