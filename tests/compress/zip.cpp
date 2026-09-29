//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#include "common.h"

#include <cstdio>
#include <atomic>
#include <map>
#include <thread>

using namespace compress_test;
namespace zip = compress::zip;

namespace {
    // Go's reading of its own test archives (tools: a program over
    // archive/zip), one file a line and its entries indented under it
    struct GoEntry {
        std::string name;
        uint64_t size, csize;
        uint32_t crc;
        int method;
        int64_t modified;
        std::string status;
        uint32_t data_crc;
        uint64_t data_size;
    };

    struct GoArchive {
        bool ok = false;
        size_t count = 0;
        std::vector<GoEntry> entries;
    };

    // a Go %q string: the escapes it writes for these names
    std::string unquote(const std::string& q) {
        std::string s;
        for (size_t i = 1; i + 1 < q.size(); ++i) {
            if (q[i] != '\\') {
                s += q[i];
                continue;
            }
            char c = q[++i];
            if (c == 'n') s += '\n';
            else if (c == 'r') s += '\r';
            else if (c == 't') s += '\t';
            else if (c == 'x') { s += char(std::stoi(q.substr(i + 1, 2), nullptr, 16)); i += 2; }
            else if (c == 'u') {
                uint32_t cp = uint32_t(std::stoul(q.substr(i + 1, 4), nullptr, 16));
                i += 4;
                if (cp < 0x80) s += char(cp);
                else if (cp < 0x800) { s += char(0xC0 | (cp >> 6)); s += char(0x80 | (cp & 0x3F)); }
                else { s += char(0xE0 | (cp >> 12)); s += char(0x80 | ((cp >> 6) & 0x3F)); s += char(0x80 | (cp & 0x3F)); }
            } else s += c;
        }
        return s;
    }

    std::map<std::string, GoArchive> go_listing() {
        std::map<std::string, GoArchive> m;
        std::istringstream in(read_oracle("zip-go-listing.txt"));
        std::string line, current;
        while (std::getline(in, line)) {
            std::vector<std::string> f;
            size_t at = 0;
            while (at <= line.size()) {
                size_t tab = line.find('\t', at);
                f.push_back(line.substr(at, tab == std::string::npos ? std::string::npos : tab - at));
                if (tab == std::string::npos) break;
                at = tab + 1;
            }
            if (!f[0].empty()) {
                current = f[0];
                m[current].ok = f[1] == "ok";
                if (m[current].ok) m[current].count = std::stoul(f[2]);
                continue;
            }
            GoEntry e{unquote(f[1]), std::stoull(f[2]), std::stoull(f[3]), uint32_t(std::stoul(f[4], nullptr, 16)), std::stoi(f[5]), std::stoll(f[6]), f[7], uint32_t(std::stoul(f[8], nullptr, 16)), std::stoull(f[9])};
            m[current].entries.push_back(e);
        }
        return m;
    }

    std::string archive_bytes(const std::string& name) {
        auto data = read_oracle("zip/" + name);
        if (data.empty()) {
            // the .base64 ones
            auto b64 = read_oracle("zip/" + name + ".base64");
            std::string clean;
            for (char c : b64) if (!isspace(uint8_t(c))) clean += c;
            static const std::string abc = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
            uint32_t acc = 0; int bits = 0;
            for (char c : clean) {
                if (c == '=') break;
                acc = acc << 6 | uint32_t(abc.find(c));
                bits += 6;
                if (bits >= 8) { bits -= 8; data += char((acc >> bits) & 0xFF); }
            }
        }
        return data;
    }
}

// Every archive of Go's test data read as Go reads it: whether it opens,
// the entries' names, sizes, CRC-32s, methods and times, and the data
TEST(Zip_Tests, GoTestArchivesReadAsGoReadsThem) {
    auto listing = go_listing();
    ASSERT_GT(listing.size(), 25u);
    for (auto& [name, go] : listing) {
        auto data = archive_bytes(name);
        auto a = zip::archive::from(bytes(data));
        ASSERT_EQ(bool(a), go.ok) << name << (a ? "" : ": " + std::string(a.error().message().view()));
        if (!a) {
            continue;
        }
        auto entries = a->entries();
        ASSERT_EQ(entries.size(), go.count) << name;
        for (size_t i = 0; i < entries.size(); ++i) {
            auto& e = entries[i];
            auto& g = go.entries[i];
            EXPECT_EQ(std::string(e.name.view()), g.name) << name;
            EXPECT_EQ(e.size, g.size) << name << " " << g.name;
            EXPECT_EQ(e.compressed_size, g.csize) << name << " " << g.name;
            EXPECT_EQ(e.crc32, g.crc) << name << " " << g.name;
            EXPECT_EQ(int(e.method), g.method) << name << " " << g.name;
            EXPECT_EQ(e.modified.unix(), g.modified) << name << " " << g.name;
            auto r = a->reader(e);
            ASSERT_TRUE(r);
            std::string got;
            std::byte buf[4096];
            bool failed = false;
            for (;;) {
                auto n = r->read(buf);
                if (!n) { failed = true; break; }
                if (*n == 0) break;
                got.append(reinterpret_cast<const char*>(buf), *n);
            }
            if (g.status == "data-ok") {
                EXPECT_FALSE(failed) << name << " " << g.name;
                EXPECT_EQ(got.size(), g.data_size) << name << " " << g.name;
                EXPECT_EQ(uint32_t(::crc32(0, (const Bytef*)got.data(), uInt(got.size()))), g.data_crc) << name << " " << g.name;
            } else {
                EXPECT_TRUE(failed) << name << " " << g.name << " (Go: " << g.status << ")";
            }
        }
    }
}

// Our archives: read back by us, by zlib's CRC and by the system's unzip
TEST(Zip_Tests, WrittenArchivesReadBack) {
    sgcl::io::buffer sink;
    zip::writer w(sink);
    auto corpus_ = corpus();
    for (auto& [name, t] : corpus_) {
        std::string entry_name = std::string(name);
        for (auto& c : entry_name) if (c == '/') c = '_';
        ASSERT_TRUE(w.add(sgcl::string(entry_name), bytes(t)));
    }
    zip::entry stored;
    stored.name = "stored.txt";
    stored.method = zip::method::store;
    stored.modified = sgcl::time::datetime::from_unix(1600000000, sgcl::time::zone::utc());
    stored.comment = "a comment \xC3\xA9";
    auto sw = w.create(stored);
    ASSERT_TRUE(sw);
    ASSERT_TRUE(sw->write(std::string("stored ")));
    ASSERT_TRUE(sw->write(std::string("data")));
    ASSERT_TRUE(w.create("dir/"));
    ASSERT_TRUE(w.add("dir/\xC5\xBC\xC3\xB3\xC5\x82w.txt", bytes(std::string("utf-8 name"))));
    ASSERT_TRUE(w.set_comment("archive comment"));
    ASSERT_TRUE(w.close());
    std::string data(reinterpret_cast<const char*>(sink.data().data()), sink.size());

    auto a = zip::archive::from(bytes(data));
    ASSERT_TRUE(a) << a.error().message();
    EXPECT_EQ(std::string(a->comment().view()), "archive comment");
    ASSERT_EQ(a->entries().size(), corpus_.size() + 3);
    for (size_t i = 0; i < corpus_.size(); ++i) {
        auto got = a->read(a->entries()[i]);
        ASSERT_TRUE(got) << got.error().message();
        EXPECT_EQ(text(*got), corpus_[i].second);
    }
    auto s = a->find("stored.txt");
    ASSERT_TRUE(s);
    EXPECT_EQ(s->method, zip::method::store);
    EXPECT_EQ(s->modified.unix(), 1600000000);
    EXPECT_EQ(std::string(s->comment.view()), "a comment \xC3\xA9");
    EXPECT_EQ(text(value_of(a->read(*s))), "stored data");
    auto d = a->find("dir/");
    ASSERT_TRUE(d);
    EXPECT_TRUE(d->is_directory());
    EXPECT_EQ(text(value_of(a->read("dir/\xC5\xBC\xC3\xB3\xC5\x82w.txt"))), "utf-8 name");

    // the system's unzip tests every entry's CRC
    std::string path = std::string(std::getenv("TMPDIR") ? std::getenv("TMPDIR") : "/tmp/") + "/sgcl-zip-test.zip";
    { std::ofstream f(path, std::ios::binary); f << data; }
    EXPECT_EQ(std::system(("unzip -tq '" + path + "' > /dev/null").c_str()), 0);
    // and read from the file, sync and async
    auto file = zip::archive::open(sgcl::string(path));
    ASSERT_TRUE(file) << file.error().message();
    EXPECT_EQ(text(value_of(file->read("stored.txt"))), "stored data");
    auto t = sgcl::async::spawn([](std::string path) -> sgcl::async::task<std::string> {
        auto a = co_await zip::archive::async_open(sgcl::string(path));
        if (!a) co_return "open failed";
        auto e = a->find("stored.txt");
        auto got = co_await a->async_read(*e);
        (void)a->close();
        co_return got ? text(*got) : std::string("read failed");
    }(path));
    EXPECT_EQ(t.wait(), "stored data");
    sgcl::async::scheduler::stop();
    EXPECT_TRUE(file->close());
    std::remove(path.c_str());
}

TEST(Zip_Tests, ManyEntriesGoZip64) {
    // 70 000 empty entries: past the 16-bit count, so ZIP64 records
    sgcl::io::buffer sink;
    zip::writer w(sink);
    zip::entry e;
    e.method = zip::method::store;
    for (int i = 0; i < 70000; ++i) {
        e.name = sgcl::string("f" + std::to_string(i));
        ASSERT_TRUE(w.create(e));
    }
    ASSERT_TRUE(w.close());
    std::string data(reinterpret_cast<const char*>(sink.data().data()), sink.size());
    auto a = zip::archive::from(bytes(data));
    ASSERT_TRUE(a) << a.error().message();
    EXPECT_EQ(a->entries().size(), 70000u);
    EXPECT_EQ(std::string(a->entries()[69999].name.view()), "f69999");
    std::string path = std::string(std::getenv("TMPDIR") ? std::getenv("TMPDIR") : "/tmp/") + "/sgcl-zip64-test.zip";
    { std::ofstream f(path, std::ios::binary); f << data; }
    EXPECT_EQ(std::system(("python3 -c \"import zipfile,sys; z=zipfile.ZipFile(sys.argv[1]); assert len(z.namelist())==70000; z.testzip()\" '" + path + "'").c_str()), 0);
    std::remove(path.c_str());
}

TEST(Zip_Tests, HostileArchivesFailCleanly) {
    auto data = archive_bytes("test.zip");
    // cut at every length: an error or fewer entries, never a crash (ASan)
    for (size_t n = 0; n < data.size(); n += 7) {
        auto a = zip::archive::from(bytes(data.substr(0, n)));
        if (a) {
            for (auto& e : a->entries()) {
                (void)a->read(e);
            }
        }
    }
    // a bit flipped anywhere
    for (size_t bit = 0; bit < data.size() * 8; bit += 13) {
        auto flipped = data;
        flipped[bit / 8] = char(flipped[bit / 8] ^ (1 << (bit % 8)));
        auto a = zip::archive::from(bytes(flipped));
        if (a) {
            for (auto& e : a->entries()) {
                (void)a->read(e);
            }
        }
    }
    // a count the directory cannot hold is refused before anything is allocated
    auto big = data;
    size_t eocd = big.rfind(std::string("PK\x05\x06", 4));
    ASSERT_NE(eocd, std::string::npos);
    big[eocd + 10] = char(0xFF);
    big[eocd + 11] = char(0xFF);
    EXPECT_FALSE(zip::archive::from(bytes(big)));
    // the limit is checked against the directory's size before reading
    auto a = zip::archive::from(bytes(data));
    ASSERT_TRUE(a);
    auto limited = a->read(a->entries()[0], compress::limits{1});
    ASSERT_FALSE(limited);
    EXPECT_EQ(limited.error().code(), compress::errc::too_large);
}

// An end record that puts the directory at the file's start does not
// make the reader take the whole file in one buffer: it reads the
// directory in pieces and stops at the first bytes that are no record
TEST(Zip_Tests, ADirectoryIsReadInPieces) {
    // a large archive: 2000 entries, then its directory read piece by piece
    sgcl::io::buffer sink;
    zip::writer w(sink);
    for (int i = 0; i < 2000; ++i) {
        ASSERT_TRUE(w.add(sgcl::string("a-rather-long-name-for-an-entry/number-" + std::to_string(i) + ".txt"), bytes(std::string(300, char('a' + i % 26)))));
    }
    ASSERT_TRUE(w.close());
    std::string data(reinterpret_cast<const char*>(sink.data().data()), sink.size());
    auto a = zip::archive::from(bytes(data));
    ASSERT_TRUE(a) << a.error().message();
    ASSERT_EQ(a->entries().size(), 2000u);
    EXPECT_EQ(text(value_of(a->read(a->entries()[1999]))), std::string(300, char('a' + 1999 % 26)));
    // 3 MB of junk with an end record saying the directory is at 0 and
    // empty: an empty archive, as Go opens it, and the junk is not read
    // as a directory (only a piece of it is looked at)
    std::string junk(3 << 20, 'j');
    junk += std::string("PK\x05\x06\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0", 22);
    auto j = zip::archive::from(bytes(junk));
    ASSERT_TRUE(j) << j.error().message();
    EXPECT_EQ(j->entries().size(), 0u);
}

TEST(Zip_Tests, IsLocal) {
    zip::entry e;
    for (auto [name, local] : std::vector<std::pair<const char*, bool>>{
             {"a.txt", true}, {"dir/a.txt", true}, {"a/../b", true}, {"../a", false}, {"/etc/passwd", false},
             {"a\\b", false}, {"", false}, {"..", false}, {"a/..", true}, {"a/../..", false}, {"..a/b", true}, {"a/b..", true}}) {
        e.name = name;
        EXPECT_EQ(e.is_local(), local) << name;
    }
}

namespace {
    std::string one_entry_archive(zip::method m, const std::string& data) {
        sgcl::io::buffer sink;
        zip::writer w(sink);
        zip::entry e;
        e.name = "a.txt";
        e.method = m;
        auto ew = w.create(e);
        (void)ew->write(bytes(data));
        (void)w.close();
        return std::string(reinterpret_cast<const char*>(sink.data().data()), sink.size());
    }
}

// The checks against the central directory, each with its own code
TEST(Zip_Tests, TheCodesOfABadEntry) {
    auto data = one_entry_archive(zip::method::store, "stored data");
    // the CRC-32 in the central record flipped: checksum
    auto crc = data;
    size_t central = crc.find(std::string("PK\x01\x02", 4));
    crc[central + 16] = char(crc[central + 16] ^ 1);
    auto a = zip::archive::from(bytes(crc));
    ASSERT_TRUE(a);
    auto r = a->read(a->entries()[0]);
    ASSERT_FALSE(r);
    EXPECT_EQ(r.error().code(), compress::errc::checksum);
    EXPECT_NE(std::string(r.error().message().view()).find("a.txt"), std::string::npos);
    // a method the library does not read (12, bzip2), in both headers: unsupported
    auto m = data;
    for (auto sig : {std::string("PK\x03\x04", 4), std::string("PK\x01\x02", 4)}) {
        size_t at = m.find(sig);
        m[at + (sig[2] == 3 ? 8 : 10)] = 12;
    }
    auto b = zip::archive::from(bytes(m));
    ASSERT_TRUE(b);
    auto u = b->read(b->entries()[0]);
    ASSERT_FALSE(u);
    EXPECT_EQ(u.error().code(), compress::errc::unsupported);
    // a size the data does not have: corrupt
    auto sz = data;
    sz[central + 24] = char(sz[central + 24] + 1);
    sz[central + 20] = char(sz[central + 20] + 1);
    auto c = zip::archive::from(bytes(sz));
    ASSERT_TRUE(c);
    auto s = c->read(c->entries()[0]);
    ASSERT_FALSE(s);
}

// §11 A5: the entry writers of a zip writer
TEST(Zip_Tests, EntryWritersEndAsTheyShould) {
    sgcl::io::buffer sink;
    zip::writer w(sink);
    auto first = w.create("first.txt");
    ASSERT_TRUE(first);
    ASSERT_TRUE(first->write(std::string("one")));
    ASSERT_TRUE(first->close());                        // ends the entry; optional
    auto second = w.create("second.txt");
    ASSERT_TRUE(second);
    ASSERT_TRUE(second->write(std::string("two")));
    auto third = w.create("third.txt");                  // ends the second
    ASSERT_TRUE(third);
    ASSERT_TRUE(w.close());
    std::string data(reinterpret_cast<const char*>(sink.data().data()), sink.size());
    auto a = zip::archive::from(bytes(data));
    ASSERT_TRUE(a);
    EXPECT_EQ(text(value_of(a->read("first.txt"))), "one");
    EXPECT_EQ(text(value_of(a->read("second.txt"))), "two");
    EXPECT_EQ(text(value_of(a->read("third.txt"))), "");
    // the task's forms
    auto t = sgcl::async::spawn([]() -> sgcl::async::task<std::string> {
        sgcl::io::buffer sink;
        zip::writer w(sink);
        zip::entry e;
        e.name = "x.txt";
        auto ew = co_await w.async_create(e);
        if (!ew) co_return "create";
        (void)co_await ew->async_write(bytes(std::string("async")));
        if (!co_await w.async_add("y.txt", bytes(std::string("added")))) co_return "add";
        if (!co_await w.async_close()) co_return "close";
        std::string data(reinterpret_cast<const char*>(sink.data().data()), sink.size());
        auto a = zip::archive::from(bytes(data));
        co_return text(*a->read("x.txt")) + "+" + text(*a->read("y.txt"));
    }());
    EXPECT_EQ(t.wait(), "async+added");
    sgcl::async::scheduler::stop();
}

// The first failure of out is kept: create still gives an entry writer,
// and every write, add and close after the failure gives it at once and
// writes nothing, so an archive is written freely and checked once, at
// the close; last_error() holds it
TEST(Zip_Tests, AFailingSinkIsKeptToTheClose) {
    auto io_eio = [](const compress::error& e) {
        return e.code() == compress::errc::io && e.io_error() && is_eio(*e.io_error());
    };
    for (auto m : {zip::method::store, zip::method::deflate}) {
        failing_after f(100);
        zip::writer w(f);
        ASSERT_TRUE(w.add("notes.txt", bytes(std::string(30, 'n'))));   // 48 + 30 (or less) + 16 bytes
        EXPECT_FALSE(w.last_error());
        zip::entry e;
        e.name = "logs/app.log";
        e.method = m;
        auto log = w.create(e);   // its header does not fit: out fails
        ASSERT_TRUE(log) << int(m);
        EXPECT_EQ(f.failures, 1);
        int calls = f.calls;
        auto line = log->write(std::string("a line\n"));
        ASSERT_FALSE(line) << int(m);
        EXPECT_TRUE(is_eio(line.error())) << int(m);
        auto added = w.add("more.txt", bytes(std::string("more")));
        ASSERT_FALSE(added) << int(m);
        EXPECT_TRUE(io_eio(added.error())) << int(m);
        auto closed = w.close();
        ASSERT_FALSE(closed) << int(m);
        EXPECT_TRUE(io_eio(closed.error())) << int(m);
        auto second = w.close();
        ASSERT_FALSE(second) << int(m);
        EXPECT_TRUE(io_eio(second.error())) << int(m);
        EXPECT_EQ(f.calls, calls) << int(m);   // nothing tried after the failure
        ASSERT_TRUE(w.last_error()) << int(m);
        EXPECT_TRUE(io_eio(*w.last_error())) << int(m);
    }
    // the task's forms
    auto t = sgcl::async::spawn([]() -> sgcl::async::task<std::string> {
        failing_after f(100);
        zip::writer w(f);
        if (!co_await w.async_add("notes.txt", bytes(std::string(30, 'n')))) co_return "add";
        zip::entry e;
        e.name = "logs/app.log";
        e.method = zip::method::store;   // its bytes go out as they are written
        auto log = co_await w.async_create(e);   // a task's: the header waits for the entry's first bytes
        if (!log) co_return "create";
        if (f.failures != 0) co_return "header written alone";
        auto line = co_await log->async_write(std::string("a line\n"));   // the header and the line in one write: out fails
        if (line || !is_eio(line.error()) || f.failures != 1) co_return "write";
        int calls = f.calls;
        auto closed = co_await w.async_close();
        if (closed || !closed.error().io_error() || !is_eio(*closed.error().io_error())) co_return "close";
        co_return f.calls == calls ? "ok" : "written after";
    }());
    EXPECT_EQ(t.wait(), "ok");
    sgcl::async::scheduler::stop();
}

// The caller's errors are the writer's first too, kept as a failure of
// out is: a write to an entry that was ended, data for a directory, an
// entry create refuses, a create after close, a comment too long; what
// comes after gives that error at once, writes nothing, and the close
// gives it
TEST(Zip_Tests, ACallersErrorIsKeptToTheClose) {
    // what every call after the first error gives, nothing written
    auto expect_kept = [](zip::writer& w, const sgcl::io::buffer& sink, const compress::error& first) {
        size_t size = sink.size();
        auto added = w.add("ok.txt", bytes(std::string("ok")));
        ASSERT_FALSE(added);
        EXPECT_TRUE(added.error() == first) << std::string(added.error().message().view());
        auto created = w.create("ok2.txt");   // an entry writer, whose writes give it
        ASSERT_TRUE(created);
        auto wrote = created->write(std::string("ok"));
        ASSERT_FALSE(wrote);
        if (first.io_error()) {
            EXPECT_TRUE(wrote.error() == *first.io_error());
        } else {
            EXPECT_EQ(wrote.error().code(), make_error_code(first.code()));
        }
        auto comment = w.set_comment("fine");
        ASSERT_FALSE(comment);
        EXPECT_TRUE(comment.error() == first);
        auto closed = w.close();
        ASSERT_FALSE(closed);
        EXPECT_TRUE(closed.error() == first) << std::string(closed.error().message().view());
        auto again = w.close();
        ASSERT_FALSE(again);
        EXPECT_TRUE(again.error() == first);
        EXPECT_EQ(sink.size(), size);
        ASSERT_TRUE(w.last_error());
        EXPECT_TRUE(*w.last_error() == first);
    };
    {   // a write to an entry that was ended
        sgcl::io::buffer sink;
        zip::writer w(sink);
        auto first = w.create("first.txt");
        ASSERT_TRUE(first);
        ASSERT_TRUE(first->write(std::string("one")));
        auto second = w.create("second.txt");
        ASSERT_TRUE(second);
        EXPECT_FALSE(w.last_error());
        auto stale = first->write(std::string("more"));
        ASSERT_FALSE(stale);
        EXPECT_EQ(stale.error().code(), sgcl::io::errc::closed);
        auto two = second->write(std::string("two"));   // a correct write gives it
        ASSERT_FALSE(two);
        EXPECT_TRUE(two.error() == stale.error());
        ASSERT_TRUE(w.last_error());
        EXPECT_EQ(w.last_error()->code(), compress::errc::io);
        EXPECT_TRUE(w.last_error()->io_error()->is_closed());
        expect_kept(w, sink, *w.last_error());
    }
    {   // data for a directory
        sgcl::io::buffer sink;
        zip::writer w(sink);
        auto dir = w.create("logs/");
        ASSERT_TRUE(dir);
        auto data = dir->write(std::string("x"));
        ASSERT_FALSE(data);
        EXPECT_EQ(data.error().code(), make_error_code(compress::errc::invalid_argument));
        ASSERT_TRUE(w.last_error());
        EXPECT_EQ(w.last_error()->code(), compress::errc::invalid_argument);
        expect_kept(w, sink, *w.last_error());
    }
    {   // an entry create refuses: a name too long, a method it does not write
        for (int i = 0; i < 2; ++i) {
            sgcl::io::buffer sink;
            zip::writer w(sink);
            zip::entry e;
            e.name = i == 0 ? sgcl::string(std::string(70000, 'n')) : sgcl::string("a.bz2");
            if (i == 1) {
                e.method = zip::method(12);
            }
            auto refused = w.create(e);
            ASSERT_FALSE(refused) << i;
            EXPECT_EQ(refused.error().code(), i == 0 ? compress::errc::invalid_argument : compress::errc::unsupported);
            expect_kept(w, sink, refused.error());
            EXPECT_EQ(sink.size(), 0u);
        }
    }
    {   // a create after close
        sgcl::io::buffer sink;
        zip::writer w(sink);
        ASSERT_TRUE(w.add("a.txt", bytes(std::string("a"))));
        ASSERT_TRUE(w.close());
        ASSERT_TRUE(w.close());   // a second does nothing
        size_t size = sink.size();
        auto late = w.create("late.txt");
        ASSERT_FALSE(late);
        EXPECT_EQ(late.error().code(), compress::errc::invalid_argument);
        auto closed = w.close();
        ASSERT_FALSE(closed);
        EXPECT_TRUE(closed.error() == late.error());
        EXPECT_EQ(sink.size(), size);
        std::string data(reinterpret_cast<const char*>(sink.data().data()), sink.size());
        auto a = zip::archive::from(bytes(data));   // what was written is a whole archive
        ASSERT_TRUE(a);
        EXPECT_EQ(text(value_of(a->read("a.txt"))), "a");
    }
    {   // a comment too long
        sgcl::io::buffer sink;
        zip::writer w(sink);
        auto comment = w.set_comment(sgcl::string(std::string(70000, 'c')));
        ASSERT_FALSE(comment);
        EXPECT_EQ(comment.error().code(), compress::errc::invalid_argument);
        expect_kept(w, sink, comment.error());
    }
    // the task's forms
    auto t = sgcl::async::spawn([]() -> sgcl::async::task<std::string> {
        sgcl::io::buffer sink;
        zip::writer w(sink);
        zip::entry d;
        d.name = "logs/";
        auto dir = co_await w.async_create(d);
        if (!dir) co_return "create";
        size_t size = sink.size();
        auto data = co_await dir->async_write(std::string("x"));
        if (data) co_return "directory data";
        auto added = co_await w.async_add("ok.txt", bytes(std::string("ok")));
        if (added || added.error().code() != compress::errc::invalid_argument) co_return "add after";
        auto closed = co_await w.async_close();
        if (closed || !(closed.error() == added.error())) co_return "close";
        co_return sink.size() == size ? "ok" : "written after";
    }());
    EXPECT_EQ(t.wait(), "ok");
    sgcl::async::scheduler::stop();
}

// Readers of one archive in two threads at once, each its own entry
TEST(Zip_Tests, TwoThreadsReadOneArchive) {
    sgcl::io::buffer sink;
    zip::writer w(sink);
    std::string a1(50000, 'a'), b1(50000, 'b');
    ASSERT_TRUE(w.add("a", bytes(a1)));
    ASSERT_TRUE(w.add("b", bytes(b1)));
    ASSERT_TRUE(w.close());
    std::string data(reinterpret_cast<const char*>(sink.data().data()), sink.size());
    auto a = zip::archive::from(bytes(data));
    ASSERT_TRUE(a);
    std::atomic<int> good = 0;
    std::thread t1([&] { for (int i = 0; i < 50; ++i) good += text(*a->read("a")) == a1; });
    std::thread t2([&] { for (int i = 0; i < 50; ++i) good += text(*a->read("b")) == b1; });
    t1.join();
    t2.join();
    EXPECT_EQ(good.load(), 100);
}
