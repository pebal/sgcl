[sgcl](../../../README.md) › [net](../../README.md) › [sftp](../README.md)

# sgcl::net::sftp::client

```cpp
#include "sgcl/net/sftp/client.h"   // or "sgcl/net/sftp.h"

namespace sgcl::net::sftp {
    class client;
}
```

**Requires [rooted](../../../core/rooted/README.md) outside a stack or a managed object.**

`net::sftp::client` is an SFTP session with a server, `sftp`'s and Go's `sftp.Client`: [connect](connect.md) opens the
`sftp` subsystem on an SSH connection and agrees the protocol's version 3. Over it, the server's files are worked on as
[io](../../../io/README.md)'s functions work on local ones, by the same names: [open](open.md) and
[create](create.md) give a [file](../file/README.md), [stat](stat.md), [read_dir](read_dir.md), [mkdir](mkdir.md),
[remove](remove.md), [rename](rename.md) and the rest do what io's do, and [read_file](read_file.md),
[write_file](write_file.md), [upload](upload.md) and [download](download.md) move a whole file in one line.

A client is a handle of one word: a copy is the same session, and its requests may come from any number of tasks and
threads at once, each answered by its id. Against Go, a client is made from an address in one call (the SSH
connection made with it), a rename replaces its target as `posix-rename@openssh.com` does where the server has it, and
the whole-file calls keep the server's reads and writes in flight together by themselves.

## Rules

- [close](close.md) ends the session at once, and the SSH connection when the client made it: the requests in
  progress, of the client and of its files, end with `io::errc::closed`, as does every call after it. The server's
  end or the connection's does the same, with its error.
- The blocking forms run the work on the scheduler and wait for it, so they are for a thread, as
  [task::wait](../../../async/task/wait.md) is; a task awaits the `async_` forms.

## Member functions

| Function | Description |
|---|---|
| [(constructor)](client.md) | no session |
| [connect, async_connect](connect.md) | an SFTP session over an SSH connection, or over one made to an address (static) |
| [close](close.md) | ends the session |

#### Files

| Function | Description |
|---|---|
| [open, async_open](open.md) | opens a remote file with io's flags |
| [create, async_create](create.md) | a remote file made empty for writing |
| [read_file, async_read_file](read_file.md) | a whole remote file |
| [read_text, async_read_text](read_text.md) | a whole remote file as text |
| [write_file, async_write_file](write_file.md) | a remote file made with the given bytes |
| [upload, async_upload](upload.md) | a local file sent to the server |
| [download, async_download](download.md) | a remote file fetched to a local path |

#### Attributes

| Function | Description |
|---|---|
| [stat, async_stat](stat.md) | what the server says about a path, a symlink followed |
| [lstat, async_lstat](lstat.md) | the same, a symlink not followed |
| [exists, async_exists](exists.md) | whether there is something at a path |
| [set_stat, async_set_stat](set_stat.md) | changes the attributes of a path |
| [chmod, async_chmod](chmod.md) | changes the mode bits |
| [set_modified, async_set_modified](set_modified.md) | changes the time of the last modification |
| [stat_fs, async_stat_fs](stat_fs.md) | what the file system of a path says of itself |

#### Directories

| Function | Description |
|---|---|
| [read_dir, async_read_dir](read_dir.md) | the entries of a directory with their attributes |
| [mkdir, async_mkdir](mkdir.md) | makes a directory |
| [mkdir_all, async_mkdir_all](mkdir_all.md) | makes a directory with its parents |
| [rmdir, async_rmdir](rmdir.md) | removes an empty directory |
| [remove, async_remove](remove.md) | removes a file or an empty directory |
| [remove_all, async_remove_all](remove_all.md) | removes a path with everything under it |

#### Names

| Function | Description |
|---|---|
| [rename, async_rename](rename.md) | renames a file or directory, a target replaced |
| [symlink, async_symlink](symlink.md) | makes a symlink |
| [hard_link, async_hard_link](hard_link.md) | makes a second name of a file |
| [read_link, async_read_link](read_link.md) | the text of a symlink |
| [real_path, async_real_path](real_path.md) | the canonical form of a path |
| [home_dir, async_home_dir](home_dir.md) | the user's home directory |

#### Observers

| Function | Description |
|---|---|
| [limits](limits.md) | the server's limits |
| [has_extension](has_extension.md) | whether the server offers an extension |
| [is_closed](is_closed.md) | whether the session ended |
| [operator bool](operator_bool.md) | whether there is a session |

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
    fs.mkdir("/docs");
    fs.write_file("/docs/readme.txt", "SFTP in one line");
    println("{}", fs.read_text("/docs/readme.txt").value());
    println("{}", fs.stat("/docs/readme.txt")->size);
    fs.close();
    srv.close();
}
```

Output:

```text
SFTP in one line
16
```

## See also

- [file](../file/README.md): a remote file open
- [serve](../serve.md): the other side
- [ssh::client](../../ssh/client/README.md): the connection under it
- [net::sftp](../README.md)
