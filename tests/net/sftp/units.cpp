//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
// net::sftp without a connection: the attributes both ways, the status codes
// and their errno values, the resolver against a tree in memory (every way
// out tried: "..", absolute paths, symlinks absolute and relative, chains
// and loops of them, NULs), and the server's requests over that tree
// (INIT first, every request, the handles' limit, a read-only server,
// malformed packets); the disk's tree refusing a symlink put in place of a
// directory after the resolver walked it.
#include "tests/types.h"
#include "sgcl/net/sftp.h"

#include <filesystem>
#include <fstream>
#include <string>
#include <unistd.h>

using namespace sgcl;
namespace d = sgcl::net::sftp::detail;

namespace {
    // Requests built and their answers read, against a server over a tree
    // in memory
    struct Session {
        d::MemoryFs* fs;
        std::unique_ptr<d::ServerCore> core;
        uint32_t id = 1;

        explicit Session(net::sftp::server_options o = {}) {
            auto m = std::make_unique<d::MemoryFs>();
            fs = m.get();
            core = std::make_unique<d::ServerCore>(std::move(m), o);
            d::Bytes init, out;
            d::Writer w(init);
            w.u8(d::FxpInit).u32(3);
            EXPECT_TRUE(core->handle(init.data(), init.size(), out));
        }

        // A request's answer: its type and payload after the id
        d::Bytes call(uint8_t type, const d::Bytes& fields) {
            d::Bytes p, out;
            d::Writer w(p);
            w.u8(type).u32(id++);
            w.raw(fields.data(), fields.size());
            EXPECT_TRUE(core->handle(p.data(), p.size(), out));
            EXPECT_GE(out.size(), 9u);
            EXPECT_EQ(d::load32(out.data()), out.size() - 4);
            return d::Bytes(out.begin() + 4, out.end());
        }

        uint32_t status(uint8_t type, const d::Bytes& fields) {
            auto r = call(type, fields);
            EXPECT_EQ(r[0], d::FxpStatus);
            return d::load32(r.data() + 5);
        }

        static d::Bytes str(std::string_view s) {
            d::Bytes b;
            d::Writer w(b);
            w.string(s);
            return b;
        }

        static d::Bytes two(std::string_view a, std::string_view b) {
            d::Bytes x;
            d::Writer w(x);
            w.string(a).string(b);
            return x;
        }

        std::string handle(std::string_view path, uint32_t pflags) {
            d::Bytes f;
            d::Writer w(f);
            w.string(path).u32(pflags);
            d::write_attrs(w, d::Attrs{});
            auto r = call(d::FxpOpen, f);
            if (r[0] != d::FxpHandle) {
                return "";
            }
            d::Reader rd(r.data() + 5, r.size() - 5);
            return std::string(rd.string().view());
        }

        std::string name_of(uint8_t type, std::string_view path) {
            auto r = call(type, str(path));
            if (r[0] != d::FxpName) {
                return "status " + std::to_string(d::load32(r.data() + 5));
            }
            d::Reader rd(r.data() + 5, r.size() - 5);
            rd.u32();
            return std::string(rd.string().view());
        }
    };
}

TEST(SftpUnits, AttributesRoundTrip) {
    d::Attrs a;
    a.flags = d::AttrSize | d::AttrUidGid | d::AttrPermissions | d::AttrAcModTime;
    a.size = 1234567890123ull;
    a.uid = 501;
    a.gid = 20;
    a.permissions = S_IFREG | 0640;
    a.atime = 1700000000;
    a.mtime = 1700000001;
    d::Bytes b;
    d::Writer w(b);
    d::write_attrs(w, a);
    d::Reader r(b.data(), b.size());
    d::Attrs back;
    ASSERT_TRUE(d::read_attrs(r, back));
    EXPECT_TRUE(r.done());
    EXPECT_EQ(back.size, a.size);
    EXPECT_EQ(back.permissions, a.permissions);
    auto info = d::info_of(back, "x");
    EXPECT_TRUE(info.is_regular());
    EXPECT_EQ(unsigned(info.mode), 0640u);
    EXPECT_EQ(info.modified, io::file_time(std::chrono::seconds(1700000001)));
    // extended pairs are passed over; a count past 1024 refused
    d::Bytes e;
    d::Writer ew(e);
    ew.u32(d::AttrExtended).u32(2).string("a").string("b").string("c").string("d");
    d::Reader er(e.data(), e.size());
    EXPECT_TRUE(d::read_attrs(er, back));
    EXPECT_TRUE(er.done());
    d::Bytes many;
    d::Writer mw(many);
    mw.u32(d::AttrExtended).u32(5000);
    d::Reader mr(many.data(), many.size());
    EXPECT_FALSE(d::read_attrs(mr, back));
    // the times of a set_stat: one alone takes the other from the file
    net::sftp::file_info cur;
    cur.accessed = io::file_time(std::chrono::seconds(100));
    net::sftp::attributes at;
    at.modified = io::file_time(std::chrono::seconds(200));
    auto x = d::attrs_of(at, &cur);
    EXPECT_EQ(x.flags, d::AttrAcModTime);
    EXPECT_EQ(x.atime, 100u);
    EXPECT_EQ(x.mtime, 200u);
}

TEST(SftpUnits, StatusesAndErrnos) {
    EXPECT_EQ(d::status_error(d::FxNoSuchFile), std::errc::no_such_file_or_directory);
    EXPECT_EQ(d::status_error(d::FxPermissionDenied), std::errc::permission_denied);
    EXPECT_EQ(d::status_error(d::FxOpUnsupported), std::errc::not_supported);
    EXPECT_EQ(d::status_error(d::FxFailure), net::errc::sftp_failure);
    EXPECT_EQ(d::status_error(d::FxBadMessage), net::errc::sftp_protocol);
    EXPECT_EQ(d::status_error(d::FxConnectionLost), io::errc::closed);
    EXPECT_EQ(d::status_error(99), net::errc::sftp_failure);
    EXPECT_EQ(d::status_of_errno(ENOENT), d::FxNoSuchFile);
    EXPECT_EQ(d::status_of_errno(EACCES), d::FxPermissionDenied);
    EXPECT_EQ(d::status_of_errno(EEXIST), d::FxFailure);
    EXPECT_EQ(d::status_of_errno(0), d::FxOk);
}

TEST(SftpUnits, TheResolverNeverLeavesTheRoot) {
    d::MemoryFs fs;
    fs.put("home/ann/file", "x");
    fs.put("top", "t");
    ASSERT_EQ(fs.symlink("/home", "home/ann/abs"), 0);           // absolute: from the root
    ASSERT_EQ(fs.symlink("../../..", "home/ann/up"), 0);         // relative, past the root
    ASSERT_EQ(fs.symlink("loop2", "home/ann/loop1"), 0);
    ASSERT_EQ(fs.symlink("loop1", "home/ann/loop2"), 0);
    ASSERT_EQ(fs.symlink("/etc/passwd", "home/ann/passwd"), 0);  // dangling inside the root
    std::string rel;
    auto r = [&](std::string_view p, bool follow = true) {
        int e = d::resolve(fs, p, "/home/ann", follow, rel);
        return e ? "errno " + std::to_string(e) : "/" + rel;
    };
    EXPECT_EQ(r("/"), "/");
    EXPECT_EQ(r(""), "/home/ann");
    EXPECT_EQ(r("."), "/home/ann");
    EXPECT_EQ(r("file"), "/home/ann/file");
    EXPECT_EQ(r("../../../../../.."), "/");
    EXPECT_EQ(r("/../../top"), "/top");
    EXPECT_EQ(r("//home///ann/./file"), "/home/ann/file");
    EXPECT_EQ(r("abs/ann/file"), "/home/ann/file");
    EXPECT_EQ(r("up"), "/");
    EXPECT_EQ(r("up/top"), "/top");
    EXPECT_EQ(r("up/../../../top"), "/top");
    EXPECT_EQ(r("passwd"), "errno " + std::to_string(ENOENT));   // its target's directory not there under the root
    fs.mkdir("etc", 0755);
    EXPECT_EQ(r("passwd"), "/etc/passwd");                     // a name to make, under the root
    EXPECT_EQ(r("passwd", false), "/home/ann/passwd");
    EXPECT_EQ(r("loop1"), "errno " + std::to_string(ELOOP));
    EXPECT_EQ(r("loop1", false), "/home/ann/loop1");
    EXPECT_EQ(r("nothing/file"), "errno " + std::to_string(ENOENT));
    EXPECT_EQ(r("file/below"), "errno " + std::to_string(ENOTDIR));
    EXPECT_EQ(r(std::string_view("a\0b", 3)), "errno " + std::to_string(EINVAL));
    EXPECT_EQ(r(std::string(5000, 'a')), "errno " + std::to_string(EINVAL));
    // what any path resolves to is a path of plain names
    for (std::string_view p : {"..", "/..", "../x", "up/up/up", "abs/../..", "./../home/ann/up/home"}) {
        if (d::resolve(fs, p, "/home/ann", true, rel) == 0) {
            EXPECT_EQ(rel.find(".."), std::string::npos) << p;
            EXPECT_TRUE(rel.empty() || rel[0] != '/') << p;
        }
    }
}

TEST(SftpUnits, InitComesFirst) {
    auto m = std::make_unique<d::MemoryFs>();
    d::ServerCore core(std::move(m), {});
    d::Bytes p, out;
    d::Writer w(p);
    w.u8(d::FxpStat).u32(1).string("/");
    EXPECT_FALSE(core.handle(p.data(), p.size(), out));
    d::Bytes init;
    d::Writer iw(init);
    iw.u8(d::FxpInit).u32(3);
    EXPECT_TRUE(core.handle(init.data(), init.size(), out));
    d::Reader r(out.data() + 4, out.size() - 4);
    EXPECT_EQ(r.u8(), d::FxpVersion);
    EXPECT_EQ(r.u32(), 3u);
    int ext = 0;
    while (r.ok() && r.left()) {
        r.string();
        r.string();
        ++ext;
    }
    EXPECT_EQ(ext, int(std::size(d::extensions)));
    // an empty packet, a packet without its id
    d::Bytes none;
    EXPECT_FALSE(core.handle(none.data(), 0, out));
    uint8_t lone = d::FxpStat;
    EXPECT_FALSE(core.handle(&lone, 1, out));
}

TEST(SftpUnits, EveryRequestOverATreeInMemory) {
    Session s;
    // files: open, write, read, fstat, fsetstat, close
    auto h = s.handle("/f", d::FxfWrite | d::FxfCreat);
    ASSERT_EQ(h.size(), 4u);
    d::Bytes wr;
    d::Writer ww(wr);
    ww.string(h).u64(0).string("hello world");
    EXPECT_EQ(s.status(d::FxpWrite, wr), d::FxOk);
    EXPECT_EQ(s.status(d::FxpClose, Session::str(h)), d::FxOk);
    EXPECT_EQ(s.status(d::FxpClose, Session::str(h)), d::FxFailure);   // closed already
    h = s.handle("/f", d::FxfRead);
    d::Bytes rd;
    d::Writer rw(rd);
    rw.string(h).u64(6).u32(100);
    auto data = s.call(d::FxpRead, rd);
    ASSERT_EQ(data[0], d::FxpData);
    d::Reader dr(data.data() + 5, data.size() - 5);
    EXPECT_EQ(dr.string().view(), "world");
    d::Bytes past;
    d::Writer pw(past);
    pw.string(h).u64(11).u32(10);
    EXPECT_EQ(s.status(d::FxpRead, past), d::FxEof);
    auto st = s.call(d::FxpFstat, Session::str(h));
    ASSERT_EQ(st[0], d::FxpAttrs);
    // the write of a read handle's file: the tree refuses nothing here
    // (the handle was opened for reading; a disk refuses with EBADF)
    s.status(d::FxpClose, Session::str(h));
    // paths: stat, lstat, setstat, mkdir, opendir/readdir, rename, remove
    EXPECT_EQ(s.call(d::FxpStat, Session::str("/f"))[0], d::FxpAttrs);
    EXPECT_EQ(s.status(d::FxpStat, Session::str("/none")), d::FxNoSuchFile);
    d::Bytes ss;
    d::Writer sw(ss);
    sw.string("/f");
    d::Attrs size;
    size.flags = d::AttrSize;
    size.size = 3;
    d::write_attrs(sw, size);
    EXPECT_EQ(s.status(d::FxpSetstat, ss), d::FxOk);
    d::Bytes mk;
    d::Writer mw(mk);
    mw.string("/dir");
    d::write_attrs(mw, d::Attrs{});
    EXPECT_EQ(s.status(d::FxpMkdir, mk), d::FxOk);
    EXPECT_EQ(s.status(d::FxpMkdir, mk), d::FxFailure);   // there already
    auto dh = s.call(d::FxpOpendir, Session::str("/"));
    ASSERT_EQ(dh[0], d::FxpHandle);
    d::Reader hr(dh.data() + 5, dh.size() - 5);
    std::string dirh(hr.string().view());
    auto names = s.call(d::FxpReaddir, Session::str(dirh));
    ASSERT_EQ(names[0], d::FxpName);
    EXPECT_EQ(d::load32(names.data() + 5), 2u);   // dir, f
    EXPECT_EQ(s.status(d::FxpReaddir, Session::str(dirh)), d::FxEof);
    EXPECT_EQ(s.status(d::FxpClose, Session::str(dirh)), d::FxOk);
    EXPECT_EQ(s.status(d::FxpRename, Session::two("/f", "/dir/g")), d::FxOk);
    s.fs->put("other", "o");
    EXPECT_EQ(s.status(d::FxpRename, Session::two("/other", "/dir/g")), d::FxFailure);   // RENAME: no replacing
    d::Bytes pr;
    d::Writer prw(pr);
    prw.string("posix-rename@openssh.com").string("/other").string("/dir/g");
    EXPECT_EQ(s.status(d::FxpExtended, pr), d::FxOk);
    EXPECT_EQ(s.status(d::FxpRmdir, Session::str("/dir")), d::FxFailure);   // not empty
    EXPECT_EQ(s.status(d::FxpRemove, Session::str("/dir")), d::FxFailure);  // a directory
    EXPECT_EQ(s.status(d::FxpRemove, Session::str("/dir/g")), d::FxOk);
    EXPECT_EQ(s.status(d::FxpRmdir, Session::str("/dir")), d::FxOk);
    EXPECT_EQ(s.status(d::FxpRmdir, Session::str("/")), d::FxPermissionDenied);   // the root
    // symlinks in OpenSSH's order: target, link
    s.fs->put("t", "x");
    EXPECT_EQ(s.status(d::FxpSymlink, Session::two("../../t", "/l")), d::FxOk);
    EXPECT_EQ(s.name_of(d::FxpReadlink, "/l"), "../../t");
    EXPECT_EQ(s.name_of(d::FxpRealpath, "/l"), "/t");
    EXPECT_EQ(s.name_of(d::FxpRealpath, "/../.."), "/");
    // extensions
    d::Bytes lim;
    d::Writer lw(lim);
    lw.string("limits@openssh.com");
    EXPECT_EQ(s.call(d::FxpExtended, lim)[0], d::FxpExtendedReply);
    d::Bytes vfs;
    d::Writer vw(vfs);
    vw.string("statvfs@openssh.com").string("/");
    EXPECT_EQ(s.call(d::FxpExtended, vfs)[0], d::FxpExtendedReply);
    d::Bytes hl;
    d::Writer hw(hl);
    hw.string("hardlink@openssh.com").string("/t").string("/t2");
    EXPECT_EQ(s.status(d::FxpExtended, hl), d::FxOk);
    d::Bytes ex;
    d::Writer exw(ex);
    exw.string("expand-path@openssh.com").string("~/t");
    EXPECT_EQ(s.call(d::FxpExtended, ex)[0], d::FxpName);
    d::Bytes other;
    d::Writer ow(other);
    ow.string("expand-path@openssh.com").string("~root/x");
    EXPECT_EQ(s.status(d::FxpExtended, other), d::FxFailure);
    d::Bytes unk;
    d::Writer uw(unk);
    uw.string("copy-data");
    EXPECT_EQ(s.status(d::FxpExtended, unk), d::FxOpUnsupported);
    EXPECT_EQ(s.status(77, d::Bytes()), d::FxOpUnsupported);
    // malformed fields: a bad message, the session goes on
    d::Bytes cut = {0, 0, 0, 9, 'x'};
    EXPECT_EQ(s.status(d::FxpOpen, cut), d::FxBadMessage);
    EXPECT_EQ(s.call(d::FxpStat, Session::str("/t"))[0], d::FxpAttrs);
}

TEST(SftpUnits, TheHandlesLimitAndReadOnly) {
    net::sftp::server_options o;
    o.max_handles = 3;
    Session s(o);
    s.fs->put("f", "x");
    for (int i = 0; i < 3; ++i) {
        EXPECT_EQ(s.handle("/f", d::FxfRead).size(), 4u);
    }
    EXPECT_EQ(s.handle("/f", d::FxfRead), "");
    EXPECT_EQ(s.core->open_handles(), 3u);
    net::sftp::server_options ro;
    ro.read_only = true;
    Session r(ro);
    r.fs->put("f", "x");
    EXPECT_EQ(r.handle("/f", d::FxfRead).size(), 4u);
    EXPECT_EQ(r.handle("/f", d::FxfWrite), "");
    EXPECT_EQ(r.status(d::FxpRemove, Session::str("/f")), d::FxPermissionDenied);
    EXPECT_EQ(r.status(d::FxpRename, Session::two("/f", "/g")), d::FxPermissionDenied);
    EXPECT_EQ(r.status(d::FxpSymlink, Session::two("/f", "/g")), d::FxPermissionDenied);
    d::Bytes mk;
    d::Writer mw(mk);
    mw.string("/dir");
    d::write_attrs(mw, d::Attrs{});
    EXPECT_EQ(r.status(d::FxpMkdir, mk), d::FxPermissionDenied);
    EXPECT_EQ(r.call(d::FxpStat, Session::str("/f"))[0], d::FxpAttrs);
}

// A directory of a resolved path swapped for a symlink to the outside
// before the operation (another session's rename, another process): every
// call of the disk's tree refuses to follow it
TEST(SftpUnits, TheDiskNeverFollowsASymlinkComeMeanwhile) {
    namespace fsys = std::filesystem;
    const fsys::path base = fsys::temp_directory_path() / ("sgcl_sftp_swap_" + std::to_string(::getpid()));
    fsys::remove_all(base);
    fsys::create_directories(base / "root" / "d");
    fsys::create_directories(base / "outside");
    std::ofstream(base / "root" / "d" / "x") << "inside";
    std::ofstream(base / "outside" / "x") << "secret";
    d::DiskFs fs((base / "root").string());
    int h = 0;
    ASSERT_EQ(fs.open("d/x", d::FxfRead, 0, h), 0);
    fs.close(h);
    fsys::remove_all(base / "root" / "d");
    fsys::create_directory_symlink(base / "outside", base / "root" / "d");
    d::Attrs a;
    EXPECT_NE(fs.open("d/x", d::FxfRead, 0, h), 0);
    EXPECT_NE(fs.open("d/y", d::FxfWrite | d::FxfCreat, 0644, h), 0);
    EXPECT_NE(fs.lstat("d/x", a), 0);
    std::string text;
    EXPECT_NE(fs.readlink("d/x", text), 0);
    std::vector<d::DirEntry> list;
    EXPECT_NE(fs.list("d", list), 0);
    a.flags = d::AttrPermissions | d::AttrSize;
    a.permissions = 0777;
    a.size = 0;
    EXPECT_NE(fs.setstat("d/x", a, true), 0);
    EXPECT_NE(fs.remove("d/x"), 0);
    EXPECT_NE(fs.mkdir("d/new", 0755), 0);
    EXPECT_NE(fs.rmdir("d/new"), 0);
    EXPECT_NE(fs.rename("d/x", "taken", true), 0);
    EXPECT_NE(fs.symlink("/", "d/l"), 0);
    EXPECT_NE(fs.link("d/x", "h"), 0);
    net::sftp::file_system_info v;
    EXPECT_NE(fs.statvfs("d/x", v), 0);
    // the outside untouched, nothing made there
    EXPECT_EQ(fsys::file_size(base / "outside" / "x"), 6u);
    EXPECT_EQ(fsys::status(base / "outside" / "x").permissions() & fsys::perms::others_write, fsys::perms::none);
    EXPECT_FALSE(fsys::exists(base / "outside" / "y"));
    EXPECT_FALSE(fsys::exists(base / "outside" / "new"));
    EXPECT_FALSE(fsys::exists(base / "outside" / "l"));
    EXPECT_FALSE(fsys::exists(base / "root" / "taken"));
    EXPECT_FALSE(fsys::exists(base / "root" / "h"));
    // the root itself, and a root that is not there
    EXPECT_EQ(fs.lstat("", a), 0);
    EXPECT_EQ(fs.list("", list), 0);
    d::DiskFs gone((base / "none").string());
    EXPECT_EQ(gone.lstat("", a), ENOENT);
    fsys::remove_all(base);
}
