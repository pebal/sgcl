//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "types.h"
#include "detail/protocol.h"
#include "../error.h"
#include "../ssh/client.h"
#include "../../async/channel.h"
#include "../../async/coroutine.h"
#include "../../core/detail/handle_word.h"
#include "../../core/make_tracked.h"
#include "../../core/deque.h"
#include "../../core/map.h"
#include "../../core/string.h"
#include "../../core/tracked_ptr.h"
#include "../../core/vector.h"
#include "../../io/file.h"
#include "../../io/fs.h"
#include "../../io/mixin/reader.h"
#include "../../io/mixin/seeker.h"
#include "../../io/mixin/writer.h"
#include "../../io/path.h"
#include "../../io/stream.h"

#include <algorithm>
#include <atomic>
#include <deque>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

// An SFTP client (version 3 with OpenSSH's extensions) over an SSH
// connection's "sftp" subsystem, in the manner of the io module: files
// opened, read, written and seeked as io streams, stat, the directory
// listings, the changes of the file system; and whole files in one call
// (read_file, write_file, upload, download) with many requests in flight,
// as OpenSSH's sftp sends them.
//
//   auto fs = net::sftp::client::connect("host:22", {.user = "me"});
//   fs->upload("report.pdf", "/incoming/report.pdf");
namespace sgcl::net::sftp {
    class client;
    class file;

    namespace detail {
        using Reply = async::detail::ChannelState<Bytes>;

        // An SFTP session: its streams, the requests waiting for their
        // answers by id, what the server's VERSION and limits said
        class SftpConn {
        public:
            ssh::client ssh;            // the connection when the client made it (closed with it)
            bool owns_ssh = false;
            ssh::session session;
            io::reader out;             // the server's answers
            io::writer in;              // the requests
            std::mutex m;
            map<uint32_t, tracked_ptr<Reply>> pending;   // under m
            uint32_t next_id = 1;                        // under m
            std::atomic<bool> failed{false};
            optional<io::error> error;                   // under m
            std::vector<std::string> extensions;         // the server's, with their versions after a '='
            sftp::limits lim;
            uint32_t max_read = 32768;
            uint32_t max_write = 32768;

            bool has(std::string_view name) const noexcept {
                for (const auto& e : extensions) {
                    if (std::string_view(e).substr(0, e.find('=')) == name) {
                        return true;
                    }
                }
                return false;
            }

            io::error current_error() {
                std::lock_guard<std::mutex> g(m);
                return error ? *error : io::error(io::errc::closed, "sftp");
            }

            void fail_with(const io::error& e) {
                vector<tracked_ptr<Reply>> all;
                {
                    std::lock_guard<std::mutex> g(m);
                    if (!error) {
                        error = e;
                    }
                    failed.store(true);
                    for (auto& [id, r] : pending) {
                        all.push_back(r);
                    }
                    pending.clear();
                }
                for (auto& r : all) {
                    r->close();
                }
            }
        };

        inline io::error protocol_error(std::string_view what) noexcept {
            return io::error(net::make_error_code(net::errc::sftp_protocol), "sftp", string(what));
        }

        // The answers read and handed to their requests, until the session
        // ends
        inline async::task<void> read_loop(tracked_ptr<SftpConn> c) noexcept {
            for (;;) {
                uint8_t len[4];
                auto h = co_await c->out.async_read_full(ssh::detail::mutable_bytes_of(len, 4));
                if (!h || *h == 0) {
                    c->fail_with(h ? io::error(io::errc::closed, "sftp", string("the server ended the session")) : h.error());
                    co_return;
                }
                const uint32_t n = load32(len);
                if (n < 5 || n > MaxPacket + 1024) {
                    c->fail_with(protocol_error("an answer of a length out of range"));
                    co_return;
                }
                Bytes p(n);
                auto b = co_await c->out.async_read_full(ssh::detail::mutable_bytes_of(p.data(), n));
                if (!b || *b != n) {
                    c->fail_with(b ? io::error(io::errc::unexpected_eof, "sftp") : b.error());
                    co_return;
                }
                const uint32_t id = load32(p.data() + 1);
                tracked_ptr<Reply> r;
                {
                    std::lock_guard<std::mutex> g(c->m);
                    auto it = c->pending.find(id);
                    if (it != c->pending.end()) {
                        r = it->second;
                        c->pending.erase(it);
                    }
                }
                if (!r) {
                    c->fail_with(protocol_error("an answer to no request"));
                    co_return;
                }
                r->try_send(std::move(p));
            }
        }

        // A request sent (its type, a fresh id, its fields): the channel its
        // answer comes on (the answer's type and payload)
        inline async::task<expected<tracked_ptr<Reply>, io::error>> co_send(tracked_ptr<SftpConn> c, uint8_t type, Bytes fields) noexcept {
            tracked_ptr reply = make_tracked<Reply>(1);
            uint32_t id;
            {
                std::lock_guard<std::mutex> g(c->m);
                if (c->failed.load()) {
                    auto e = c->error ? *c->error : io::error(io::errc::closed, "sftp");
                    co_return unexpected(e);
                }
                id = c->next_id++;
                c->pending[id] = reply;
            }
            Bytes p;
            Writer w(p);
            size_t at = begin_packet(w, type);
            w.u32(id);
            w.raw(fields.data(), fields.size());
            end_packet(w, at);
            auto s = co_await c->in.async_write(ssh::detail::bytes_of(p));
            if (!s) {
                c->fail_with(s.error());
                co_return unexpected(s.error());
            }
            co_return reply;
        }

        inline async::task<expected<Bytes, io::error>> co_await_reply(tracked_ptr<SftpConn> c, tracked_ptr<Reply> r) noexcept {
            auto got = co_await r->receive();
            if (!got) {
                co_return unexpected(c->current_error());
            }
            co_return std::move(*got);
        }

        inline async::task<expected<Bytes, io::error>> co_call(tracked_ptr<SftpConn> c, uint8_t type, Bytes fields) noexcept {
            auto r = co_await co_send(c, type, std::move(fields));
            if (!r) {
                co_return unexpected(r.error());
            }
            co_return co_await co_await_reply(c, *r);
        }

        // A STATUS answer as the error of op on path (OK: none)
        inline optional<io::error> status_of(const Bytes& reply, const char* op, std::string_view path) {
            Reader r(reply.data() + 5, reply.size() - 5);
            uint32_t code = r.u32();
            Span msg = r.string();
            if (!r.ok()) {
                return protocol_error("a malformed status");
            }
            if (code == FxOk) {
                return nullopt;
            }
            std::string where(path);
            if (status_error(code) == net::errc::sftp_failure && msg.n) {
                where += " (" + ssh::detail::printable(msg.view()) + ")";
            }
            return io::error(status_error(code), op, string(where));
        }

        // An answer that should be a STATUS OK
        inline expected<void, io::error> expect_ok(const expected<Bytes, io::error>& reply, const char* op, std::string_view path) {
            if (!reply) {
                return unexpected(reply.error());
            }
            if ((*reply)[0] != FxpStatus) {
                return unexpected(protocol_error("an unexpected answer"));
            }
            if (auto e = status_of(*reply, op, path)) {
                return unexpected(*e);
            }
            return {};
        }

        // An answer of a type, or the STATUS's error; its fields after the id
        inline expected<Reader, io::error> expect(const expected<Bytes, io::error>& reply, uint8_t type, const char* op, std::string_view path) {
            if (!reply) {
                return unexpected(reply.error());
            }
            const uint8_t t = (*reply)[0];
            if (t == FxpStatus) {
                auto e = status_of(*reply, op, path);
                return unexpected(e ? *e : protocol_error("a status where an answer was expected"));
            }
            if (t != type) {
                return unexpected(protocol_error("an unexpected answer"));
            }
            return Reader(reply->data() + 5, reply->size() - 5);
        }

        SGCL_INLINE_HOT Bytes fields() {
            return Bytes();
        }

        inline Bytes path_fields(std::string_view p) {
            Bytes b;
            Writer w(b);
            w.string(p);
            return b;
        }

        // The answer of a request that names one entry (REALPATH, READLINK,
        // expand-path, home-directory): its name
        inline expected<string, io::error> one_name(const expected<Bytes, io::error>& reply, const char* op, std::string_view path) {
            auto r = expect(reply, FxpName, op, path);
            if (!r) {
                return unexpected(r.error());
            }
            uint32_t count = r->u32();
            Span name = r->string();
            if (!r->ok() || count != 1) {
                return unexpected(protocol_error("a NAME answer of no one entry"));
            }
            return string(name.view());
        }

        struct FileState {
            tracked_ptr<SftpConn> conn;
            std::string handle;
            string path;
            std::atomic<uint64_t> pos{0};
            std::atomic<bool> closed{false};
        };

        struct ClientAccess;
        struct FileAccess;
    }

    // A file open on the server: a handle of one word over its SFTP handle,
    // read and written as an io stream from its position, or at offsets.
    // close() lets the server's handle go; a file not closed holds it until
    // the connection ends
    class file final : public io::mixin::reader<file>, public io::mixin::writer<file>, public io::mixin::seeker<file> {
    public:
        using io::mixin::writer<file>::write;
        using io::mixin::writer<file>::async_write;

        file() noexcept = default;

        // Up to buffer.size() bytes from the position (one request, at most
        // the server's largest read), 0 at the end
        // `read(...)` on this thread, `co_await async_read(...)` in a task
        expected<size_t, io::error> read(const slice<byte>& buffer) const {
            return async_read(buffer).wait();
        }

        async::task<expected<size_t, io::error>> async_read(const slice<byte>& buffer) const noexcept {
            return _co_read(_s, buffer, true, 0);
        }

        // All of data at the position, which moves past it (in requests of
        // the server's largest write, all in flight at once)
        // `write(...)` on this thread, `co_await async_write(...)` in a task
        expected<size_t, io::error> write(const slice<const byte>& data) const {
            return async_write(data).wait();
        }

        async::task<expected<size_t, io::error>> async_write(const slice<const byte>& data) const noexcept {
            return _co_write(_s, data, true, 0);
        }

        // At an offset, the position untouched
        // `read_at(...)` on this thread, `co_await async_read_at(...)` in a task
        expected<size_t, io::error> read_at(const slice<byte>& buffer, uint64_t offset) const {
            return _co_read(_s, buffer, false, offset).wait();
        }

        async::task<expected<size_t, io::error>> async_read_at(const slice<byte>& buffer, uint64_t offset) const noexcept {
            return _co_read(_s, buffer, false, offset);
        }

        // `write_at(...)` on this thread, `co_await async_write_at(...)` in a task
        expected<size_t, io::error> write_at(const slice<const byte>& data, uint64_t offset) const {
            return _co_write(_s, data, false, offset).wait();
        }

        async::task<expected<size_t, io::error>> async_write_at(const slice<const byte>& data, uint64_t offset) const noexcept {
            return _co_write(_s, data, false, offset);
        }

        // The position moved (seek_from::end asks the server for the size):
        // the new position
        expected<uint64_t, io::error> seek(int64_t offset, io::seek_from from = io::seek_from::begin) const {
            int64_t base = 0;
            if (from == io::seek_from::current) {
                base = int64_t(_s->pos.load());
            } else if (from == io::seek_from::end) {
                auto st = stat();
                if (!st) {
                    return unexpected(st.error());
                }
                base = int64_t(st->size);
            }
            if (base + offset < 0) {
                return unexpected(io::error(std::make_error_code(std::errc::invalid_argument), "seek", _s->path));
            }
            _s->pos.store(uint64_t(base + offset));
            return uint64_t(base + offset);
        }

        // What the server says about the open file (FSTAT)
        // `stat(...)` on this thread, `co_await async_stat(...)` in a task
        expected<file_info, io::error> stat() const {
            return async_stat().wait();
        }

        async::task<expected<file_info, io::error>> async_stat() const noexcept {
            return _co_stat(_s);
        }

        // The attributes set (FSETSTAT)
        // `set_stat(...)` on this thread, `co_await async_set_stat(...)` in a task
        expected<void, io::error> set_stat(const attributes& a) const {
            return async_set_stat(a).wait();
        }

        async::task<expected<void, io::error>> async_set_stat(const attributes& a) const noexcept {
            return _co_set_stat(_s, a);
        }

        // The size changed to `size` (FSETSTAT of the size)
        // `truncate(...)` on this thread, `co_await async_truncate(...)` in a task
        expected<void, io::error> truncate(uint64_t size) const {
            return async_truncate(size).wait();
        }

        async::task<expected<void, io::error>> async_truncate(uint64_t size) const noexcept {
            attributes a;
            a.size = size;
            return _co_set_stat(_s, a);
        }

        // The file's data on the server's disk (fsync@openssh.com);
        // ENOTSUP from a server without it
        // `sync(...)` on this thread, `co_await async_sync(...)` in a task
        expected<void, io::error> sync() const {
            return async_sync().wait();
        }

        async::task<expected<void, io::error>> async_sync() const noexcept {
            return _co_sync(_s);
        }

        // The server's handle let go (CLOSE); a second close does nothing
        // `close(...)` on this thread, `co_await async_close(...)` in a task
        expected<void, io::error> close() const {
            return async_close().wait();
        }

        async::task<expected<void, io::error>> async_close() const noexcept {
            return _co_close(_s);
        }

        SGCL_INLINE_HOT bool is_closed() const noexcept {
            return _s->closed.load();
        }

        // The path the file was opened by
        SGCL_INLINE_HOT const string& path() const noexcept {
            return _s->path;
        }

        SGCL_INLINE_HOT explicit operator bool() const noexcept {
            return (bool)_s;
        }

    private:
        friend struct detail::FileAccess;
        friend struct sgcl::detail::HandleWord;
        friend struct io::detail::HandleAccess;

        SGCL_INLINE_HOT explicit file(tracked_ptr<detail::FileState> s) noexcept
        : _s(std::move(s)) {
        }

        SGCL_INLINE_HOT file(sgcl::detail::FromWord, const tracked_ptr<detail::FileState>& w) noexcept
        : _s(w) {
        }

        SGCL_INLINE_HOT tracked_ptr<detail::FileState>& _handle_word() noexcept {
            return _s;
        }

        SGCL_INLINE_HOT const tracked_ptr<detail::FileState>& _handle_word() const noexcept {
            return _s;
        }

        static detail::Bytes _handle_fields(const detail::FileState& s) {
            detail::Bytes b;
            detail::Writer w(b);
            w.string(s.handle);
            return b;
        }

        static io::error _closed_error(const detail::FileState& s, const char* op) noexcept {
            return io::error(io::errc::closed, op, s.path);
        }

        static async::task<expected<size_t, io::error>> _co_read(tracked_ptr<detail::FileState> s, slice<byte> buffer, bool at_pos, uint64_t offset) noexcept {
            if (buffer.empty()) {
                co_return size_t(0);
            }
            if (s->closed.load()) {
                co_return unexpected(_closed_error(*s, "read"));
            }
            const uint64_t at = at_pos ? s->pos.load() : offset;
            const uint32_t n = uint32_t(std::min<size_t>(buffer.size(), s->conn->max_read));
            detail::Bytes f = _handle_fields(*s);
            detail::Writer w(f);
            w.u64(at).u32(n);
            auto reply = co_await detail::co_call(s->conn, detail::FxpRead, std::move(f));
            if (reply && !reply->empty() && (*reply)[0] == detail::FxpStatus) {
                detail::Reader sr(reply->data() + 5, reply->size() - 5);
                if (sr.u32() == detail::FxEof) {
                    co_return size_t(0);
                }
            }
            auto r = detail::expect(reply, detail::FxpData, "read", s->path.view());
            if (!r) {
                co_return unexpected(r.error());
            }
            detail::Span data = r->string();
            if (!r->ok() || data.n > n) {
                co_return unexpected(detail::protocol_error("a DATA answer longer than asked for"));
            }
            sgcl::detail::copy_bytes(buffer.data(), data.p, data.n);
            if (at_pos) {
                s->pos.store(at + data.n);
            }
            co_return data.n;
        }

        static async::task<expected<size_t, io::error>> _co_write(tracked_ptr<detail::FileState> s, slice<const byte> data, bool at_pos, uint64_t offset) noexcept {
            if (s->closed.load()) {
                co_return unexpected(_closed_error(*s, "write"));
            }
            const uint64_t start = at_pos ? s->pos.load() : offset;
            deque<tracked_ptr<detail::Reply>> flight;
            size_t done = 0;
            optional<io::error> err;
            const size_t chunk = s->conn->max_write;
            while (done < data.size() || !flight.empty()) {
                while (done < data.size() && flight.size() < 64 && !err) {
                    const size_t n = std::min(chunk, data.size() - done);
                    detail::Bytes f = _handle_fields(*s);
                    detail::Writer w(f);
                    w.u64(start + done).string(data.data() + done, n);
                    auto r = co_await detail::co_send(s->conn, detail::FxpWrite, std::move(f));
                    if (!r) {
                        err = r.error();
                        break;
                    }
                    flight.push_back(*r);
                    done += n;
                }
                if (flight.empty()) {
                    break;
                }
                auto reply = co_await detail::co_await_reply(s->conn, flight.front());
                flight.pop_front();
                auto ok = detail::expect_ok(reply, "write", s->path.view());
                if (!ok && !err) {
                    err = ok.error();
                }
                if (err && done < data.size()) {
                    done = data.size();   // nothing more sent; the answers in flight drained
                }
            }
            if (err) {
                co_return unexpected(*err);
            }
            if (at_pos) {
                s->pos.store(start + data.size());
            }
            co_return data.size();
        }

        static async::task<expected<file_info, io::error>> _co_stat(tracked_ptr<detail::FileState> s) noexcept {
            if (s->closed.load()) {
                co_return unexpected(_closed_error(*s, "stat"));
            }
            auto reply = co_await detail::co_call(s->conn, detail::FxpFstat, _handle_fields(*s));
            auto r = detail::expect(reply, detail::FxpAttrs, "stat", s->path.view());
            if (!r) {
                co_return unexpected(r.error());
            }
            detail::Attrs a;
            if (!detail::read_attrs(*r, a)) {
                co_return unexpected(detail::protocol_error("malformed attributes"));
            }
            co_return detail::info_of(a, io::path::base(s->path).view());
        }

        static async::task<expected<void, io::error>> _co_set_stat(tracked_ptr<detail::FileState> s, attributes at) noexcept {
            if (s->closed.load()) {
                co_return unexpected(_closed_error(*s, "set_stat"));
            }
            optional<file_info> cur;
            if (bool(at.accessed) != bool(at.modified) || bool(at.uid) != bool(at.gid)) {
                auto st = co_await _co_stat(s);
                if (!st) {
                    co_return unexpected(st.error());
                }
                cur = *st;
            }
            detail::Bytes f = _handle_fields(*s);
            detail::Writer w(f);
            detail::write_attrs(w, detail::attrs_of(at, cur ? &*cur : nullptr));
            co_return detail::expect_ok(co_await detail::co_call(s->conn, detail::FxpFsetstat, std::move(f)), "set_stat", s->path.view());
        }

        static async::task<expected<void, io::error>> _co_sync(tracked_ptr<detail::FileState> s) noexcept {
            if (s->closed.load()) {
                co_return unexpected(_closed_error(*s, "sync"));
            }
            if (!s->conn->has("fsync@openssh.com")) {
                co_return unexpected(io::error(std::make_error_code(std::errc::not_supported), "sync", s->path));
            }
            detail::Bytes f;
            detail::Writer w(f);
            w.string("fsync@openssh.com").string(s->handle);
            co_return detail::expect_ok(co_await detail::co_call(s->conn, detail::FxpExtended, std::move(f)), "sync", s->path.view());
        }

        static async::task<expected<void, io::error>> _co_close(tracked_ptr<detail::FileState> s) noexcept {
            if (s->closed.exchange(true)) {
                co_return expected<void, io::error>();
            }
            co_return detail::expect_ok(co_await detail::co_call(s->conn, detail::FxpClose, _handle_fields(*s)), "close", s->path.view());
        }

        // What an io::reader or io::writer made of the handle binds
        SGCL_INLINE_HOT const tracked_ptr<detail::FileState>& _stream_state() const noexcept {
            return _s;
        }

        tracked_ptr<detail::FileState> _s;
    };

    namespace detail {
        struct FileAccess {
            SGCL_INLINE_HOT static file make(const tracked_ptr<FileState>& s) noexcept {
                return file(s);
            }

            SGCL_INLINE_HOT static tracked_ptr<FileState> make_state(const file& f) noexcept {
                return f._s;
            }
        };

        SGCL_INLINE_HOT uint32_t pflags_of(io::open_flags f) noexcept {
            uint32_t p = 0;
            if (f & io::open_flags::read) {
                p |= FxfRead;
            }
            if (f & io::open_flags::write) {
                p |= FxfWrite;
            }
            if (f & io::open_flags::append) {
                p |= FxfAppend | FxfWrite;
            }
            if (f & io::open_flags::create) {
                p |= FxfCreat;
            }
            if (f & io::open_flags::truncate) {
                p |= FxfTrunc;
            }
            if (f & io::open_flags::exclusive) {
                p |= FxfExcl;
            }
            if (!(p & (FxfRead | FxfWrite))) {
                p |= FxfRead;
            }
            return p;
        }

        // How many requests a whole-file transfer keeps in flight: about 8 MB
        // of them, 4 to 64
        SGCL_INLINE_HOT size_t window_of(size_t chunk) noexcept {
            return std::clamp<size_t>((size_t(8) << 20) / std::max<size_t>(chunk, 1), 4, 64);
        }
    }

    // An SFTP session: a handle of one word, its copies the same session,
    // safe from many tasks and threads (their requests in flight together,
    // each answer to its own). close() ends it, and the SSH connection with
    // it when the client made that too
    class client {
    public:
        client() noexcept = default;

        // The "sftp" subsystem of a connection there is, its VERSION and
        // limits read
        // `connect(...)` on this thread, `co_await async_connect(...)` in a task
        static expected<client, io::error> connect(const ssh::client& c) {
            return async_connect(c).wait();
        }

        static async::task<expected<client, io::error>> async_connect(ssh::client c) noexcept {
            return _co_connect(std::move(c), false);
        }

        // An SSH connection to "host:port" made (ssh::client::connect) and
        // the subsystem over it; close() closes both
        // `connect(...)` on this thread, `co_await async_connect(...)` in a task
        static expected<client, io::error> connect(const string& address, const ssh::client::options& o = {}) {
            return async_connect(address, o).wait();
        }

        static async::task<expected<client, io::error>> async_connect(string address, ssh::client::options o = {}) noexcept {
            return _co_connect_address(std::move(address), std::move(o));
        }

        // A file opened with io's flags (read by default; write, append,
        // create, truncate, exclusive), a new one made with the permissions
        // (the server's umask applies)
        // `open(...)` on this thread, `co_await async_open(...)` in a task
        expected<file, io::error> open(const string& path, io::open_flags flags = io::open_flags::read, io::permissions p = io::permissions(0666)) const {
            return async_open(path, flags, p).wait();
        }

        async::task<expected<file, io::error>> async_open(const string& path, io::open_flags flags = io::open_flags::read,
                                                          io::permissions p = io::permissions(0666)) const noexcept {
            return _co_open(_c, path, detail::pflags_of(flags), p);
        }

        // A file made empty for writing, as io::create: write, create, truncate
        // `create(...)` on this thread, `co_await async_create(...)` in a task
        expected<file, io::error> create(const string& path, io::permissions p = io::permissions(0666)) const {
            return async_create(path, p).wait();
        }

        async::task<expected<file, io::error>> async_create(const string& path, io::permissions p = io::permissions(0666)) const noexcept {
            return _co_open(_c, path, detail::FxfWrite | detail::FxfCreat | detail::FxfTrunc, p);
        }

        // What the server says about a path, a symlink followed (stat) or
        // not (lstat)
        // `stat(...)` on this thread, `co_await async_stat(...)` in a task
        expected<file_info, io::error> stat(const string& path) const {
            return async_stat(path).wait();
        }

        async::task<expected<file_info, io::error>> async_stat(const string& path) const noexcept {
            return _co_stat(_c, path, detail::FxpStat);
        }

        // `lstat(...)` on this thread, `co_await async_lstat(...)` in a task
        expected<file_info, io::error> lstat(const string& path) const {
            return async_lstat(path).wait();
        }

        async::task<expected<file_info, io::error>> async_lstat(const string& path) const noexcept {
            return _co_stat(_c, path, detail::FxpLstat);
        }

        // Whether a path is there (a stat that succeeds)
        // `exists(...)` on this thread, `co_await async_exists(...)` in a task
        expected<bool, io::error> exists(const string& path) const {
            return async_exists(path).wait();
        }

        async::task<expected<bool, io::error>> async_exists(const string& path) const noexcept {
            return _co_exists(_c, path);
        }

        // The attributes set (SETSTAT): what `a` holds, the rest left
        // `set_stat(...)` on this thread, `co_await async_set_stat(...)` in a task
        expected<void, io::error> set_stat(const string& path, const attributes& a) const {
            return async_set_stat(path, a).wait();
        }

        async::task<expected<void, io::error>> async_set_stat(const string& path, const attributes& a) const noexcept {
            return _co_set_stat(_c, path, a);
        }

        // The mode bits set, as io::chmod
        // `chmod(...)` on this thread, `co_await async_chmod(...)` in a task
        expected<void, io::error> chmod(const string& path, io::permissions p) const {
            return async_chmod(path, p).wait();
        }

        async::task<expected<void, io::error>> async_chmod(const string& path, io::permissions p) const noexcept {
            attributes a;
            a.mode = p;
            return _co_set_stat(_c, path, a);
        }

        // The modification time set (whole seconds), as io::set_modified
        // `set_modified(...)` on this thread, `co_await async_set_modified(...)` in a task
        expected<void, io::error> set_modified(const string& path, io::file_time t) const {
            return async_set_modified(path, t).wait();
        }

        async::task<expected<void, io::error>> async_set_modified(const string& path, io::file_time t) const noexcept {
            attributes a;
            a.modified = t;
            return _co_set_stat(_c, path, a);
        }

        // The entries of a directory (without "." and ".."), each with its
        // attributes, in the server's order
        // `read_dir(...)` on this thread, `co_await async_read_dir(...)` in a task
        expected<vector<file_info>, io::error> read_dir(const string& path) const {
            return async_read_dir(path).wait();
        }

        async::task<expected<vector<file_info>, io::error>> async_read_dir(const string& path) const noexcept {
            return _co_read_dir(_c, path);
        }

        // A directory made (MKDIR); as io::mkdir, the parent must be there
        // `mkdir(...)` on this thread, `co_await async_mkdir(...)` in a task
        expected<void, io::error> mkdir(const string& path, io::permissions p = io::permissions(0777)) const {
            return async_mkdir(path, p).wait();
        }

        async::task<expected<void, io::error>> async_mkdir(const string& path, io::permissions p = io::permissions(0777)) const noexcept {
            return _co_mkdir(_c, path, p);
        }

        // A directory made with every parent that is not there; one that is
        // there is no error
        // `mkdir_all(...)` on this thread, `co_await async_mkdir_all(...)` in a task
        expected<void, io::error> mkdir_all(const string& path, io::permissions p = io::permissions(0777)) const {
            return async_mkdir_all(path, p).wait();
        }

        async::task<expected<void, io::error>> async_mkdir_all(const string& path, io::permissions p = io::permissions(0777)) const noexcept {
            return _co_mkdir_all(_c, path, p);
        }

        // An empty directory removed (RMDIR)
        // `rmdir(...)` on this thread, `co_await async_rmdir(...)` in a task
        expected<void, io::error> rmdir(const string& path) const {
            return async_rmdir(path).wait();
        }

        async::task<expected<void, io::error>> async_rmdir(const string& path) const noexcept {
            return _co_path_op(_c, detail::FxpRmdir, path, "rmdir");
        }

        // A file removed (REMOVE), or an empty directory (then RMDIR), as
        // io::remove
        // `remove(...)` on this thread, `co_await async_remove(...)` in a task
        expected<void, io::error> remove(const string& path) const {
            return async_remove(path).wait();
        }

        async::task<expected<void, io::error>> async_remove(const string& path) const noexcept {
            return _co_remove(_c, path);
        }

        // A path removed with everything under it; one that is not there is
        // no error
        // `remove_all(...)` on this thread, `co_await async_remove_all(...)` in a task
        expected<void, io::error> remove_all(const string& path) const {
            return async_remove_all(path).wait();
        }

        async::task<expected<void, io::error>> async_remove_all(const string& path) const noexcept {
            return _co_remove_all(_c, path, 0);
        }

        // A file or directory renamed, as io::rename: one at `to` replaced
        // (posix-rename@openssh.com; a server without it is sent RENAME,
        // which refuses a `to` that is there)
        // `rename(...)` on this thread, `co_await async_rename(...)` in a task
        expected<void, io::error> rename(const string& from, const string& to) const {
            return async_rename(from, to).wait();
        }

        async::task<expected<void, io::error>> async_rename(const string& from, const string& to) const noexcept {
            return _co_rename(_c, from, to);
        }

        // A symlink at `link` to `target` (the target's text kept as it is)
        // `symlink(...)` on this thread, `co_await async_symlink(...)` in a task
        expected<void, io::error> symlink(const string& target, const string& link) const {
            return async_symlink(target, link).wait();
        }

        async::task<expected<void, io::error>> async_symlink(const string& target, const string& link) const noexcept {
            return _co_symlink(_c, target, link);
        }

        // A hard link at `link` to the file `target` (hardlink@openssh.com)
        // `hard_link(...)` on this thread, `co_await async_hard_link(...)` in a task
        expected<void, io::error> hard_link(const string& target, const string& link) const {
            return async_hard_link(target, link).wait();
        }

        async::task<expected<void, io::error>> async_hard_link(const string& target, const string& link) const noexcept {
            return _co_two_paths_ext(_c, "hardlink@openssh.com", target, link, "hard_link");
        }

        // The text of a symlink
        // `read_link(...)` on this thread, `co_await async_read_link(...)` in a task
        expected<string, io::error> read_link(const string& path) const {
            return async_read_link(path).wait();
        }

        async::task<expected<string, io::error>> async_read_link(const string& path) const noexcept {
            return _co_name(_c, detail::FxpReadlink, path, "read_link");
        }

        // The server's absolute, canonical form of a path (REALPATH): "." is
        // the session's directory
        // `real_path(...)` on this thread, `co_await async_real_path(...)` in a task
        expected<string, io::error> real_path(const string& path) const {
            return async_real_path(path).wait();
        }

        async::task<expected<string, io::error>> async_real_path(const string& path) const noexcept {
            return _co_name(_c, detail::FxpRealpath, path, "real_path");
        }

        // The user's home on the server (home-directory, or the canonical
        // "." of a server without it)
        // `home_dir(...)` on this thread, `co_await async_home_dir(...)` in a task
        expected<string, io::error> home_dir() const {
            return async_home_dir().wait();
        }

        async::task<expected<string, io::error>> async_home_dir() const noexcept {
            return _co_home(_c);
        }

        // What the file system of a path says (statvfs@openssh.com)
        // `stat_fs(...)` on this thread, `co_await async_stat_fs(...)` in a task
        expected<file_system_info, io::error> stat_fs(const string& path) const {
            return async_stat_fs(path).wait();
        }

        async::task<expected<file_system_info, io::error>> async_stat_fs(const string& path) const noexcept {
            return _co_stat_fs(_c, path);
        }

        // A whole remote file, its reads in flight together
        // `read_file(...)` on this thread, `co_await async_read_file(...)` in a task
        expected<vector<byte>, io::error> read_file(const string& path) const {
            return async_read_file(path).wait();
        }

        async::task<expected<vector<byte>, io::error>> async_read_file(const string& path) const noexcept {
            return _co_read_file(_c, path);
        }

        // The same as text
        // `read_text(...)` on this thread, `co_await async_read_text(...)` in a task
        expected<string, io::error> read_text(const string& path) const {
            return async_read_text(path).wait();
        }

        async::task<expected<string, io::error>> async_read_text(const string& path) const noexcept {
            return _co_read_text(_c, path);
        }

        // A remote file made (or emptied) with data, its writes in flight
        // together
        // `write_file(...)` on this thread, `co_await async_write_file(...)` in a task
        expected<void, io::error> write_file(const string& path, const slice<const byte>& data, io::permissions p = io::permissions(0666)) const {
            return async_write_file(path, data, p).wait();
        }

        async::task<expected<void, io::error>> async_write_file(const string& path, const slice<const byte>& data, io::permissions p = io::permissions(0666)) const noexcept {
            return _co_write_file(_c, path, data, p);
        }

        // A local file sent to a remote path (made or emptied), its writes in
        // flight together: what sftp's put does. The bytes sent
        // `upload(...)` on this thread, `co_await async_upload(...)` in a task
        expected<uint64_t, io::error> upload(const string& local, const string& remote) const {
            return async_upload(local, remote).wait();
        }

        async::task<expected<uint64_t, io::error>> async_upload(const string& local, const string& remote) const noexcept {
            return _co_upload(_c, local, remote);
        }

        // A remote file fetched to a local path (made or emptied), its reads
        // in flight together: what sftp's get does. The bytes fetched
        // `download(...)` on this thread, `co_await async_download(...)` in a task
        expected<uint64_t, io::error> download(const string& remote, const string& local) const {
            return async_download(remote, local).wait();
        }

        async::task<expected<uint64_t, io::error>> async_download(const string& remote, const string& local) const noexcept {
            return _co_download(_c, remote, local);
        }

        // The server's limits (limits@openssh.com; zeros from a server
        // without it)
        SGCL_INLINE_HOT sftp::limits limits() const noexcept {
            return _c->lim;
        }

        // Whether the server offers an extension by its name
        // ("posix-rename@openssh.com" …)
        SGCL_INLINE_HOT bool has_extension(const string& name) const noexcept {
            return _c->has(name.view());
        }

        // The session ended (and the SSH connection, when the client made
        // it): the requests in progress end with io::errc::closed
        expected<void, io::error> close() const noexcept {
            _c->fail_with(io::error(io::errc::closed, "sftp"));
            (void)_c->session.close();
            if (_c->owns_ssh) {
                (void)_c->ssh.close();
            }
            return {};
        }

        SGCL_INLINE_HOT bool is_closed() const noexcept {
            return _c->failed.load();
        }

        SGCL_INLINE_HOT explicit operator bool() const noexcept {
            return (bool)_c;
        }

    private:
        friend struct detail::ClientAccess;
        friend struct sgcl::detail::HandleWord;

        SGCL_INLINE_HOT explicit client(tracked_ptr<detail::SftpConn> c) noexcept
        : _c(std::move(c)) {
        }

        SGCL_INLINE_HOT client(sgcl::detail::FromWord, const tracked_ptr<detail::SftpConn>& w) noexcept
        : _c(w) {
        }

        SGCL_INLINE_HOT tracked_ptr<detail::SftpConn>& _handle_word() noexcept {
            return _c;
        }

        SGCL_INLINE_HOT const tracked_ptr<detail::SftpConn>& _handle_word() const noexcept {
            return _c;
        }

        using C = tracked_ptr<detail::SftpConn>;

        static async::task<expected<client, io::error>> _co_connect_address(string address, ssh::client::options o) noexcept {
            auto s = co_await ssh::client::async_connect(address, o);
            if (!s) {
                co_return unexpected(s.error());
            }
            auto c = co_await _co_connect(*s, true);
            if (!c) {
                (void)s->close();
            }
            co_return c;
        }

        static async::task<expected<client, io::error>> _co_connect(ssh::client ssh, bool owns) noexcept {
            auto sess = co_await ssh.async_open_session();
            if (!sess) {
                co_return unexpected(sess.error());
            }
            auto sub = co_await sess->async_subsystem(string("sftp"));
            if (!sub) {
                (void)sess->close();
                co_return unexpected(sub.error());
            }
            tracked_ptr c = make_tracked<detail::SftpConn>();
            c->ssh = ssh;
            c->owns_ssh = owns;
            c->session = *sess;
            c->out = sess->output();
            c->in = sess->input();
            // INIT and the VERSION before the read loop: the one answer with
            // no id
            detail::Bytes init;
            detail::Writer w(init);
            size_t at = detail::begin_packet(w, detail::FxpInit);
            w.u32(detail::Version);
            detail::end_packet(w, at);
            auto sent = co_await c->in.async_write(ssh::detail::bytes_of(init));
            if (!sent) {
                (void)sess->close();
                co_return unexpected(sent.error());
            }
            // a server that never answers (no SFTP behind the subsystem's
            // name) ends the wait after 30 s
            ssh::detail::SessionAccess::set_read_deadline(*sess, sgcl::clock::now() + 30 * second);
            uint8_t len[4];
            auto h = co_await c->out.async_read_full(ssh::detail::mutable_bytes_of(len, 4));
            const uint32_t n = h && *h == 4 ? detail::load32(len) : 0;
            if (n < 5 || n > 65536) {
                (void)sess->close();
                co_return unexpected(h ? detail::protocol_error("no VERSION from the server") : h.error());
            }
            detail::Bytes v(n);
            auto b = co_await c->out.async_read_full(ssh::detail::mutable_bytes_of(v.data(), n));
            if (!b || *b != n || v[0] != detail::FxpVersion) {
                (void)sess->close();
                co_return unexpected(b ? detail::protocol_error("no VERSION from the server") : b.error());
            }
            detail::Reader r(v.data() + 1, n - 1);
            const uint32_t version = r.u32();
            while (r.ok() && r.left()) {
                detail::Span name = r.string();
                detail::Span data = r.string();
                if (r.ok()) {
                    c->extensions.push_back(std::string(name.view()) + "=" + std::string(data.view()));
                }
            }
            if (!r.ok() || version != detail::Version) {
                (void)sess->close();
                co_return unexpected(detail::protocol_error(version != detail::Version ? "a server of another SFTP version than 3" : "a malformed VERSION"));
            }
            ssh::detail::SessionAccess::set_read_deadline(*sess, time_point());
            async::go(detail::read_loop(c));
            if (c->has("limits@openssh.com")) {
                detail::Bytes f;
                detail::Writer fw(f);
                fw.string("limits@openssh.com");
                auto reply = co_await detail::co_call(c, detail::FxpExtended, std::move(f));
                auto lr = detail::expect(reply, detail::FxpExtendedReply, "limits", "");
                if (lr) {
                    c->lim.max_packet_length = lr->u64();
                    c->lim.max_read_length = lr->u64();
                    c->lim.max_write_length = lr->u64();
                    c->lim.max_open_handles = lr->u64();
                    if (lr->ok()) {
                        // a read or write within what the server takes and what this side reads
                        auto pick = [](uint64_t v) { return uint32_t(v ? std::min<uint64_t>(v, detail::MaxData) : 32768); };
                        c->max_read = pick(c->lim.max_read_length);
                        c->max_write = pick(c->lim.max_write_length);
                    } else {
                        c->lim = sftp::limits{};
                    }
                }
            }
            co_return client(c);
        }

        static async::task<expected<file, io::error>> _co_open(C c, string path, uint32_t pflags, io::permissions p) noexcept {
            detail::Bytes f;
            detail::Writer w(f);
            w.string(path.view()).u32(pflags);
            detail::Attrs a;
            if (pflags & detail::FxfCreat) {
                a.flags = detail::AttrPermissions;
                a.permissions = uint32_t(p) & 07777;
            }
            detail::write_attrs(w, a);
            auto reply = co_await detail::co_call(c, detail::FxpOpen, std::move(f));
            auto r = detail::expect(reply, detail::FxpHandle, "open", path.view());
            if (!r) {
                co_return unexpected(r.error());
            }
            detail::Span h = r->string();
            if (!r->ok() || h.n == 0 || h.n > 256) {
                co_return unexpected(detail::protocol_error("a malformed HANDLE"));
            }
            tracked_ptr s = make_tracked<detail::FileState>();
            s->conn = c;
            s->handle.assign(h.view());
            s->path = path;
            co_return detail::FileAccess::make(s);
        }

        static async::task<expected<file_info, io::error>> _co_stat(C c, string path, uint8_t type) noexcept {
            auto reply = co_await detail::co_call(c, type, detail::path_fields(path.view()));
            auto r = detail::expect(reply, detail::FxpAttrs, type == detail::FxpStat ? "stat" : "lstat", path.view());
            if (!r) {
                co_return unexpected(r.error());
            }
            detail::Attrs a;
            if (!detail::read_attrs(*r, a)) {
                co_return unexpected(detail::protocol_error("malformed attributes"));
            }
            co_return detail::info_of(a, io::path::base(path).view());
        }

        static async::task<expected<bool, io::error>> _co_exists(C c, string path) noexcept {
            auto st = co_await _co_stat(c, path, detail::FxpStat);
            if (st) {
                co_return true;
            }
            if (st.error().is_not_found()) {
                co_return false;
            }
            co_return unexpected(st.error());
        }

        static async::task<expected<void, io::error>> _co_set_stat(C c, string path, attributes at) noexcept {
            optional<file_info> cur;
            if (bool(at.accessed) != bool(at.modified) || bool(at.uid) != bool(at.gid)) {
                auto st = co_await _co_stat(c, path, detail::FxpStat);
                if (!st) {
                    co_return unexpected(st.error());
                }
                cur = *st;
            }
            detail::Bytes f;
            detail::Writer w(f);
            w.string(path.view());
            detail::write_attrs(w, detail::attrs_of(at, cur ? &*cur : nullptr));
            co_return detail::expect_ok(co_await detail::co_call(c, detail::FxpSetstat, std::move(f)), "set_stat", path.view());
        }

        static async::task<expected<vector<file_info>, io::error>> _co_read_dir(C c, string path) noexcept {
            auto reply = co_await detail::co_call(c, detail::FxpOpendir, detail::path_fields(path.view()));
            auto r = detail::expect(reply, detail::FxpHandle, "read_dir", path.view());
            if (!r) {
                co_return unexpected(r.error());
            }
            detail::Span hs = r->string();
            if (!r->ok()) {
                co_return unexpected(detail::protocol_error("a malformed HANDLE"));
            }
            std::string handle(hs.view());
            vector<file_info> out;
            optional<io::error> err;
            for (;;) {
                auto d = co_await detail::co_call(c, detail::FxpReaddir, detail::path_fields(handle));
                if (d && !d->empty() && (*d)[0] == detail::FxpStatus) {
                    detail::Reader sr(d->data() + 5, d->size() - 5);
                    if (sr.u32() == detail::FxEof) {
                        break;
                    }
                }
                auto nr = detail::expect(d, detail::FxpName, "read_dir", path.view());
                if (!nr) {
                    err = nr.error();
                    break;
                }
                uint32_t count = nr->u32();
                for (uint32_t i = 0; i < count && nr->ok(); ++i) {
                    detail::Span name = nr->string();
                    (void)nr->string();
                    detail::Attrs a;
                    if (!detail::read_attrs(*nr, a)) {
                        break;
                    }
                    if (name.view() != "." && name.view() != "..") {
                        out.push_back(detail::info_of(a, name.view()));
                    }
                }
                if (!nr->ok()) {
                    err = detail::protocol_error("a malformed NAME");
                    break;
                }
            }
            auto closed = co_await detail::co_call(c, detail::FxpClose, detail::path_fields(handle));
            if (err) {
                co_return unexpected(*err);
            }
            if (auto ok = detail::expect_ok(closed, "read_dir", path.view()); !ok) {
                co_return unexpected(ok.error());
            }
            co_return out;
        }

        static async::task<expected<void, io::error>> _co_mkdir(C c, string path, io::permissions p) noexcept {
            detail::Bytes f;
            detail::Writer w(f);
            w.string(path.view());
            detail::Attrs a;
            a.flags = detail::AttrPermissions;
            a.permissions = uint32_t(p) & 07777;
            detail::write_attrs(w, a);
            co_return detail::expect_ok(co_await detail::co_call(c, detail::FxpMkdir, std::move(f)), "mkdir", path.view());
        }

        static async::task<expected<void, io::error>> _co_mkdir_all(C c, string path, io::permissions p) noexcept {
            // the parents from the top, each one there or made
            std::vector<std::string> parts;
            std::string_view v = path.view();
            std::string prefix = !v.empty() && v[0] == '/' ? "/" : "";
            size_t from = 0;
            while (from <= v.size()) {
                size_t slash = v.find('/', from);
                std::string_view part = v.substr(from, slash == std::string_view::npos ? std::string_view::npos : slash - from);
                if (!part.empty() && part != ".") {
                    parts.emplace_back(part);
                }
                if (slash == std::string_view::npos) {
                    break;
                }
                from = slash + 1;
            }
            std::string at = prefix;
            for (size_t i = 0; i < parts.size(); ++i) {
                if (!at.empty() && at.back() != '/') {
                    at += '/';
                }
                at += parts[i];
                auto st = co_await _co_stat(c, string(at), detail::FxpStat);
                if (st) {
                    if (!st->is_directory()) {
                        co_return unexpected(io::error(std::make_error_code(std::errc::not_a_directory), "mkdir_all", string(at)));
                    }
                    continue;
                }
                if (!st.error().is_not_found()) {
                    co_return unexpected(st.error());
                }
                auto m = co_await _co_mkdir(c, string(at), p);
                if (!m) {
                    // made meanwhile by another: a directory there now is fine
                    auto again = co_await _co_stat(c, string(at), detail::FxpStat);
                    if (!again || !again->is_directory()) {
                        co_return unexpected(m.error());
                    }
                }
            }
            co_return expected<void, io::error>();
        }

        static async::task<expected<void, io::error>> _co_path_op(C c, uint8_t type, string path, const char* op) noexcept {
            co_return detail::expect_ok(co_await detail::co_call(c, type, detail::path_fields(path.view())), op, path.view());
        }

        static async::task<expected<void, io::error>> _co_remove(C c, string path) noexcept {
            auto r = co_await _co_path_op(c, detail::FxpRemove, path, "remove");
            if (r) {
                co_return r;
            }
            auto st = co_await _co_stat(c, path, detail::FxpLstat);
            if (st && st->is_directory()) {
                co_return co_await _co_path_op(c, detail::FxpRmdir, path, "remove");
            }
            co_return r;
        }

        static async::task<expected<void, io::error>> _co_remove_all(C c, string path, int depth) noexcept {
            auto st = co_await _co_stat(c, path, detail::FxpLstat);
            if (!st) {
                if (st.error().is_not_found()) {
                    co_return expected<void, io::error>();
                }
                co_return unexpected(st.error());
            }
            if (st->is_directory()) {
                if (depth > 256) {
                    co_return unexpected(io::error(std::make_error_code(std::errc::too_many_symbolic_link_levels), "remove_all", path));
                }
                auto entries = co_await _co_read_dir(c, path);
                if (!entries) {
                    co_return unexpected(entries.error());
                }
                for (auto& e : *entries) {
                    auto r = co_await _co_remove_all(c, io::path::join(path, e.name), depth + 1);
                    if (!r) {
                        co_return r;
                    }
                }
                co_return co_await _co_path_op(c, detail::FxpRmdir, path, "remove_all");
            }
            co_return co_await _co_path_op(c, detail::FxpRemove, path, "remove_all");
        }

        static async::task<expected<void, io::error>> _co_rename(C c, string from, string to) noexcept {
            if (c->has("posix-rename@openssh.com")) {
                co_return co_await _co_two_paths_ext(c, "posix-rename@openssh.com", from, to, "rename");
            }
            detail::Bytes f;
            detail::Writer w(f);
            w.string(from.view()).string(to.view());
            co_return detail::expect_ok(co_await detail::co_call(c, detail::FxpRename, std::move(f)), "rename", from.view());
        }

        static async::task<expected<void, io::error>> _co_symlink(C c, string target, string link) noexcept {
            // OpenSSH's order: the target first, the link second
            detail::Bytes f;
            detail::Writer w(f);
            w.string(target.view()).string(link.view());
            co_return detail::expect_ok(co_await detail::co_call(c, detail::FxpSymlink, std::move(f)), "symlink", link.view());
        }

        static async::task<expected<void, io::error>> _co_two_paths_ext(C c, const char* name, string a, string b, const char* op) noexcept {
            if (!c->has(name)) {
                co_return unexpected(io::error(std::make_error_code(std::errc::not_supported), op, a));
            }
            detail::Bytes f;
            detail::Writer w(f);
            w.string(name).string(a.view()).string(b.view());
            co_return detail::expect_ok(co_await detail::co_call(c, detail::FxpExtended, std::move(f)), op, a.view());
        }

        static async::task<expected<string, io::error>> _co_name(C c, uint8_t type, string path, const char* op) noexcept {
            co_return detail::one_name(co_await detail::co_call(c, type, detail::path_fields(path.view())), op, path.view());
        }

        static async::task<expected<string, io::error>> _co_home(C c) noexcept {
            if (c->has("home-directory")) {
                detail::Bytes f;
                detail::Writer w(f);
                w.string("home-directory").string("");
                auto r = detail::one_name(co_await detail::co_call(c, detail::FxpExtended, std::move(f)), "home_dir", "");
                if (r) {
                    co_return r;
                }
            }
            co_return co_await _co_name(c, detail::FxpRealpath, string("."), "home_dir");
        }

        static async::task<expected<file_system_info, io::error>> _co_stat_fs(C c, string path) noexcept {
            if (!c->has("statvfs@openssh.com")) {
                co_return unexpected(io::error(std::make_error_code(std::errc::not_supported), "stat_fs", path));
            }
            detail::Bytes f;
            detail::Writer w(f);
            w.string("statvfs@openssh.com").string(path.view());
            auto reply = co_await detail::co_call(c, detail::FxpExtended, std::move(f));
            auto r = detail::expect(reply, detail::FxpExtendedReply, "stat_fs", path.view());
            if (!r) {
                co_return unexpected(r.error());
            }
            file_system_info v;
            v.block_size = r->u64();
            v.fragment_size = r->u64();
            v.blocks = r->u64();
            v.blocks_free = r->u64();
            v.blocks_available = r->u64();
            v.files = r->u64();
            v.files_free = r->u64();
            v.files_available = r->u64();
            v.id = r->u64();
            v.flags = r->u64();
            v.max_name_length = r->u64();
            if (!r->ok()) {
                co_return unexpected(detail::protocol_error("a malformed statvfs answer"));
            }
            co_return v;
        }

        // A remote file read with requests in flight, each piece handed to
        // sink(offset, data) as it comes (in any order); a short answer asks
        // for the rest again. The file's size (the end of what was read)
        template<class Sink>
        static async::task<expected<uint64_t, io::error>> _co_pull(C c, string path, Sink sink) noexcept {
            auto f = co_await _co_open(c, path, detail::FxfRead, io::permissions(0));
            if (!f) {
                co_return unexpected(f.error());
            }
            const tracked_ptr<detail::FileState> s = detail::FileAccess::make_state(*f);
            const uint32_t chunk = c->max_read;
            const size_t window = detail::window_of(chunk);
            struct Ask {
                uint64_t offset;
                uint32_t length;
                tracked_ptr<detail::Reply> reply;
            };
            deque<Ask> flight;
            uint64_t next = 0, end = 0;
            bool eof = false;
            optional<io::error> err;
            std::vector<std::pair<uint64_t, uint32_t>> again;
            auto ask = [&](uint64_t offset, uint32_t length) -> async::task<bool> {
                detail::Bytes fl;
                detail::Writer w(fl);
                w.string(s->handle).u64(offset).u32(length);
                auto r = co_await detail::co_send(c, detail::FxpRead, std::move(fl));
                if (!r) {
                    err = r.error();
                    co_return false;
                }
                flight.push_back(Ask{offset, length, *r});
                co_return true;
            };
            for (;;) {
                while (!err && flight.size() < window && (!again.empty() || !eof)) {
                    if (!again.empty()) {
                        auto [o, l] = again.back();
                        again.pop_back();
                        if (!co_await ask(o, l)) {
                            break;
                        }
                    } else {
                        if (!co_await ask(next, chunk)) {
                            break;
                        }
                        next += chunk;
                    }
                }
                if (flight.empty()) {
                    break;
                }
                Ask a = std::move(flight.front());
                flight.pop_front();
                auto reply = co_await detail::co_await_reply(c, a.reply);
                if (err) {
                    continue;   // the rest drained
                }
                if (reply && !reply->empty() && (*reply)[0] == detail::FxpStatus) {
                    detail::Reader sr(reply->data() + 5, reply->size() - 5);
                    if (sr.u32() == detail::FxEof) {
                        eof = true;
                        continue;
                    }
                }
                auto r = detail::expect(reply, detail::FxpData, "read", path.view());
                if (!r) {
                    err = r.error();
                    continue;
                }
                detail::Span data = r->string();
                if (!r->ok() || data.n > a.length) {
                    err = detail::protocol_error("a DATA answer longer than asked for");
                    continue;
                }
                if (data.n) {
                    if (auto e = co_await sink(a.offset, data.p, data.n)) {
                        err = *e;
                        continue;
                    }
                    end = std::max(end, a.offset + data.n);
                }
                if (data.n < a.length && data.n) {
                    again.emplace_back(a.offset + data.n, uint32_t(a.length - data.n));
                } else if (data.n == 0) {
                    eof = true;
                }
            }
            auto closed = co_await f->async_close();
            if (err) {
                co_return unexpected(*err);
            }
            if (!closed) {
                co_return unexpected(closed.error());
            }
            co_return end;
        }

        static async::task<expected<vector<byte>, io::error>> _co_read_file(C c, string path) noexcept {
            auto st = co_await _co_stat(c, path, detail::FxpStat);
            vector<byte> buf;
            if (st && st->size > 0 && st->size < (uint64_t(1) << 32)) {
                buf.reserve(size_t(st->size));
            }
            auto got = co_await _co_pull(c, path, [&buf](uint64_t offset, const uint8_t* p, size_t n) -> async::task<optional<io::error>> {
                if (buf.size() < offset + n) {
                    buf.resize(size_t(offset + n));
                }
                sgcl::detail::copy_bytes(buf.data() + offset, p, n);
                co_return nullopt;
            });
            if (!got) {
                co_return unexpected(got.error());
            }
            buf.resize(size_t(*got));
            co_return buf;
        }

        static async::task<expected<string, io::error>> _co_read_text(C c, string path) noexcept {
            auto b = co_await _co_read_file(c, path);
            if (!b) {
                co_return unexpected(b.error());
            }
            co_return string(std::string_view(reinterpret_cast<const char*>(b->data()), b->size()));
        }

        static async::task<expected<void, io::error>> _co_write_file(C c, string path, slice<const byte> data, io::permissions p) noexcept {
            auto f = co_await _co_open(c, path, detail::FxfWrite | detail::FxfCreat | detail::FxfTrunc, p);
            if (!f) {
                co_return unexpected(f.error());
            }
            auto w = co_await f->async_write_at(data, 0);
            auto closed = co_await f->async_close();
            if (!w) {
                co_return unexpected(w.error());
            }
            co_return closed;
        }

        static async::task<expected<uint64_t, io::error>> _co_upload(C c, string local, string remote) noexcept {
            auto src = co_await io::async_open(local);
            if (!src) {
                co_return unexpected(src.error());
            }
            auto f = co_await _co_open(c, remote, detail::FxfWrite | detail::FxfCreat | detail::FxfTrunc, io::permissions(0666));
            if (!f) {
                (void)src->close();
                co_return unexpected(f.error());
            }
            const tracked_ptr<detail::FileState> s = detail::FileAccess::make_state(*f);
            const size_t chunk = c->max_write;
            const size_t window = detail::window_of(chunk);
            deque<tracked_ptr<detail::Reply>> flight;
            uint64_t sent = 0;
            bool end = false;
            optional<io::error> err;
            detail::Bytes block(chunk);
            while (!end || !flight.empty()) {
                while (!end && !err && flight.size() < window) {
                    auto n = co_await src->async_read_at(ssh::detail::mutable_bytes_of(block.data(), chunk), sent);
                    if (!n) {
                        err = n.error();
                        break;
                    }
                    if (*n == 0) {
                        end = true;
                        break;
                    }
                    detail::Bytes fl;
                    detail::Writer w(fl);
                    w.string(s->handle).u64(sent).string(block.data(), *n);
                    auto r = co_await detail::co_send(c, detail::FxpWrite, std::move(fl));
                    if (!r) {
                        err = r.error();
                        break;
                    }
                    flight.push_back(*r);
                    sent += *n;
                }
                if (err) {
                    end = true;
                }
                if (flight.empty()) {
                    break;
                }
                auto reply = co_await detail::co_await_reply(c, flight.front());
                flight.pop_front();
                auto ok = detail::expect_ok(reply, "upload", remote.view());
                if (!ok && !err) {
                    err = ok.error();
                    end = true;
                }
            }
            (void)src->close();
            auto closed = co_await f->async_close();
            if (err) {
                co_return unexpected(*err);
            }
            if (!closed) {
                co_return unexpected(closed.error());
            }
            co_return sent;
        }

        static async::task<expected<uint64_t, io::error>> _co_download(C c, string remote, string local) noexcept {
            auto dst = co_await io::async_open(local, io::open_flags::write | io::open_flags::create | io::open_flags::truncate, io::permissions(0666));
            if (!dst) {
                co_return unexpected(dst.error());
            }
            io::file out = *dst;
            auto got = co_await _co_pull(c, remote, [out](uint64_t offset, const uint8_t* p, size_t n) -> async::task<optional<io::error>> {
                auto w = co_await out.async_write_at(ssh::detail::bytes_of(p, n), offset);
                if (!w) {
                    co_return w.error();
                }
                co_return nullopt;
            });
            auto closed = out.close();
            if (!got) {
                co_return unexpected(got.error());
            }
            if (!closed) {
                co_return unexpected(closed.error());
            }
            co_return *got;
        }

        tracked_ptr<detail::SftpConn> _c;
    };

    namespace detail {
        struct ClientAccess {
            SGCL_INLINE_HOT static const tracked_ptr<SftpConn>& conn(const client& c) noexcept {
                return c._c;
            }
        };
    }
}

namespace sgcl::io::detail {
    // An SFTP file is a stream handle (io/stream.h): a stream made of one
    // binds its state, as one made of an io::file binds the file's
    template<>
    inline constexpr bool IsStreamHandle<net::sftp::file> = true;
}
