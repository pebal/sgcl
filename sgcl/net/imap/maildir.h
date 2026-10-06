//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "backend.h"
#include "error.h"
#include "types.h"
#include "detail/maildir_names.h"
#include "detail/search.h"
#include "detail/syntax.h"
#include "../../core/aliases.h"
#include "../../core/make_tracked.h"
#include "../../core/map.h"
#include "../../core/string.h"
#include "../../core/tracked_ptr.h"
#include "../../core/vector.h"
#include "../../io/lock.h"
#include "../../time/datetime.h"

#include <algorithm>
#include <atomic>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

#include <dirent.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>

// Mail in Maildir directories (the format of qmail and Courier, Maildir++
// folders): a user's INBOX is <root>/<user>/ with cur/, new/ and tmp/, a
// folder "a/b" is <root>/<user>/.a.b/ (the names in modified UTF-7, a "."
// inside a name written "&AC4-"); a message is a file of its own, its flags
// in its name after ":2," (D F R S T, keywords as the letters a to z mapped
// by sgcl-keywords), delivered through tmp/ and renamed into place, so
// that a delivery never shows half a message and needs no lock. What IMAP
// adds is in each folder's sgcl-uidlist: UIDVALIDITY, the next UID, the
// highest mod-sequence, and per message its UID, mod-sequence and flags as
// last seen; rewritten whole through a temporary file and a rename, under
// an flock of sgcl-uidlist.lock, so that two processes (two servers, a
// delivery agent and a server) keep it consistent. Files that come into
// new/ or cur/ from outside (an MTA's delivery, another program's) get
// their UIDs when the folder is next read; files gone are expunged; a
// flag changed by a rename gets a new mod-sequence.
namespace sgcl::net::imap {
    namespace detail {
        struct MdMessage {
            uint32_t uid = 0;
            uint64_t modseq = 1;
            std::string base;        // the name before ":2,"
            std::string letters;     // the flags as the file name has them (sorted)
            bool in_new = false;
            uint64_t size = 0;
            int64_t mtime = 0;
        };

        struct MdStamp {
            int64_t uidlist = -1;
            int64_t uidlist_size = -1;
            int64_t cur = -1;
            int64_t fresh = -1;      // new/

            bool operator==(const MdStamp&) const = default;
        };

        inline MdStamp stamp_of(const std::string& dir) noexcept {
            MdStamp s;
            s.uidlist = stat_mtime_ns((dir + "/sgcl-uidlist").c_str(), &s.uidlist_size);
            s.cur = stat_mtime_ns((dir + "/cur").c_str());
            s.fresh = stat_mtime_ns((dir + "/new").c_str());
            return s;
        }

        struct MdFolder {
            std::mutex m;
            std::string path;        // the folder's directory
            bool loaded = false;
            uint32_t validity = 0;
            uint32_t next = 1;
            uint64_t modseq = 1;
            std::vector<MdMessage> msgs;          // ascending UIDs
            std::vector<std::string> keywords;    // index = letter - 'a'
            MdStamp stamp;

            MdMessage* find(uint32_t uid) noexcept {
                auto it = std::lower_bound(msgs.begin(), msgs.end(), uid, [](const MdMessage& a, uint32_t u) { return a.uid < u; });
                return it != msgs.end() && it->uid == uid ? &*it : nullptr;
            }
        };

        struct MdUser {
            std::string password;
            bool has_password = false;
            uint64_t storage_limit = 0;
            uint64_t message_limit = 0;
        };

        struct MaildirState {
            std::string root;
            std::mutex m;
            std::map<std::string, std::shared_ptr<MdFolder>> folders;   // by path
            std::map<std::string, MdUser> users;
            Watchers watchers;
        };

        inline int64_t mtime_ns(const struct stat& st) noexcept {
#if defined(__APPLE__)
            return int64_t(st.st_mtimespec.tv_sec) * 1000000000 + st.st_mtimespec.tv_nsec;
#else
            return int64_t(st.st_mtim.tv_sec) * 1000000000 + st.st_mtim.tv_nsec;
#endif
        }

        inline io::error sys_error(int e, const char* op, const std::string& path) noexcept {
            return io::error(error_code(e, std::system_category()), op, string(path));
        }

        // A lock of a folder across processes: the whole lock file locked
        // (io::lock_file, flock), as other programs of a Maildir lock it
        class MdLock {
        public:
            explicit MdLock(const std::string& folder) noexcept {
                auto f = io::open(string(folder + "/sgcl-uidlist.lock"), io::open_flags::read | io::open_flags::write | io::open_flags::create, io::permissions(0600));
                if (f) {
                    if (auto held = io::lock_file(*f)) {
                        _held = std::move(*held);
                    }
                }
            }

            // unlocked and the descriptor given back now, not when the
            // collector finds the file
            ~MdLock() {
                if (_held) {
                    io::file f = _held.file();
                    (void)_held.unlock();
                    (void)f.close();
                }
            }

            MdLock(const MdLock&) = delete;
            MdLock& operator=(const MdLock&) = delete;

            bool ok() const noexcept {
                return (bool)_held;
            }

        private:
            io::file_lock _held;
        };

        inline bool read_file(const std::string& path, std::string& out) {
            const int fd = ::open(path.c_str(), O_RDONLY | O_CLOEXEC);
            if (fd < 0) {
                return false;
            }
            struct stat st;
            if (::fstat(fd, &st) == 0 && st.st_size > 0) {
                out.reserve(size_t(st.st_size));
            }
            char buf[65536];
            for (;;) {
                const ssize_t n = ::read(fd, buf, sizeof(buf));
                if (n < 0) {
                    if (errno == EINTR) {
                        continue;
                    }
                    ::close(fd);
                    return false;
                }
                if (n == 0) {
                    break;
                }
                out.append(buf, size_t(n));
            }
            ::close(fd);
            return true;
        }

        // A file written whole: to a temporary name, flushed, renamed over
        inline int write_file(const std::string& path, std::string_view data, bool sync) {
            const std::string tmp = path + ".tmp." + std::to_string(::getpid());
            const int fd = ::open(tmp.c_str(), O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0600);
            if (fd < 0) {
                return errno;
            }
            size_t done = 0;
            while (done < data.size()) {
                const ssize_t n = ::write(fd, data.data() + done, data.size() - done);
                if (n < 0) {
                    if (errno == EINTR) {
                        continue;
                    }
                    const int e = errno;
                    ::close(fd);
                    ::unlink(tmp.c_str());
                    return e;
                }
                done += size_t(n);
            }
            if (sync) {
                ::fsync(fd);
            }
            ::close(fd);
            if (::rename(tmp.c_str(), path.c_str()) != 0) {
                const int e = errno;
                ::unlink(tmp.c_str());
                return e;
            }
            return 0;
        }

        inline void fsync_dir(const std::string& path) noexcept {
            const int fd = ::open(path.c_str(), O_RDONLY | O_CLOEXEC);
            if (fd >= 0) {
                ::fsync(fd);
                ::close(fd);
            }
        }

        inline bool is_dir(const std::string& path) noexcept {
            struct stat st;
            return ::stat(path.c_str(), &st) == 0 && S_ISDIR(st.st_mode);
        }

        inline int make_maildir(const std::string& path) noexcept {
            if (::mkdir(path.c_str(), 0700) != 0 && errno != EEXIST) {
                return errno;
            }
            for (const char* sub : {"/cur", "/new", "/tmp"}) {
                const std::string p = path + sub;
                if (::mkdir(p.c_str(), 0700) != 0 && errno != EEXIST) {
                    return errno;
                }
            }
            return 0;
        }

        inline void list_dir(const std::string& path, std::vector<std::string>& out) {
            DIR* d = ::opendir(path.c_str());
            if (!d) {
                return;
            }
            while (struct dirent* e = ::readdir(d)) {
                if (e->d_name[0] == '.') {
                    continue;
                }
                out.emplace_back(e->d_name);
            }
            ::closedir(d);
        }

        inline int remove_tree(const std::string& path) noexcept {
            DIR* d = ::opendir(path.c_str());
            if (!d) {
                return errno;
            }
            std::vector<std::string> names;
            while (struct dirent* e = ::readdir(d)) {
                if (std::strcmp(e->d_name, ".") == 0 || std::strcmp(e->d_name, "..") == 0) {
                    continue;
                }
                names.emplace_back(e->d_name);
            }
            ::closedir(d);
            for (const auto& n : names) {
                const std::string p = path + "/" + n;
                struct stat st;
                if (::lstat(p.c_str(), &st) == 0 && S_ISDIR(st.st_mode)) {
                    remove_tree(p);
                } else {
                    ::unlink(p.c_str());
                }
            }
            return ::rmdir(path.c_str()) == 0 ? 0 : errno;
        }

        // A name of a new message file: unique across the machine's
        // deliveries (the Maildir spec: time, microseconds, the pid, a count,
        // the host), with Courier's ",S=" size
        inline std::string unique_base(uint64_t size) {
            static std::atomic<uint64_t> counter{0};
            struct timeval tv;
            ::gettimeofday(&tv, nullptr);
            char host[256] = {0};
            ::gethostname(host, sizeof(host) - 1);
            std::string h;
            for (const char* p = host; *p; ++p) {
                if (*p == '/') {
                    h += "\\057";
                } else if (*p == ':') {
                    h += "\\072";
                } else {
                    h += *p;
                }
            }
            if (h.empty()) {
                h = "localhost";
            }
            return std::to_string(tv.tv_sec) + ".M" + std::to_string(tv.tv_usec) + "P" + std::to_string(::getpid()) + "Q" + std::to_string(counter.fetch_add(1) + 1) + "." + h +
                   ",S=" + std::to_string(size);
        }

        // The flags of the name as the store's (system flags, keywords)
        inline vector<string> md_flags(std::string_view letters, const std::vector<std::string>& keywords) {
            vector<string> out;
            for (char c : letters) {
                switch (c) {
                    case 'S': out.push_back(string("\\Seen")); break;
                    case 'R': out.push_back(string("\\Answered")); break;
                    case 'F': out.push_back(string("\\Flagged")); break;
                    case 'T': out.push_back(string("\\Deleted")); break;
                    case 'D': out.push_back(string("\\Draft")); break;
                    default:
                        if (c >= 'a' && c <= 'z' && size_t(c - 'a') < keywords.size()) {
                            out.push_back(string(keywords[size_t(c - 'a')]));
                        }
                }
            }
            return out;
        }

        // The letters of flags (keywords given letters, new ones added to
        // the list); false past 26 keywords
        inline bool md_letters(const vector<string>& flags, std::vector<std::string>& keywords, bool& keywords_changed, std::string& out) {
            out.clear();
            for (const auto& f : flags) {
                std::string_view v = f.view();
                char c = 0;
                if (iequal(v, "\\Seen")) {
                    c = 'S';
                } else if (iequal(v, "\\Answered")) {
                    c = 'R';
                } else if (iequal(v, "\\Flagged")) {
                    c = 'F';
                } else if (iequal(v, "\\Deleted")) {
                    c = 'T';
                } else if (iequal(v, "\\Draft")) {
                    c = 'D';
                } else if (!v.empty() && v[0] != '\\') {
                    size_t i = 0;
                    while (i < keywords.size() && !iequal(keywords[i], v)) {
                        ++i;
                    }
                    if (i == keywords.size()) {
                        if (keywords.size() >= 26) {
                            return false;
                        }
                        keywords.emplace_back(v);
                        keywords_changed = true;
                    }
                    c = char('a' + i);
                }
                if (c && out.find(c) == std::string::npos) {
                    out += c;
                }
            }
            std::sort(out.begin(), out.end());
            return true;
        }

        // CRLF for every line end (an MTA's files have LF alone)
        inline std::string crlf(std::string_view s) {
            size_t bare = 0;
            for (size_t i = 0; i < s.size(); ++i) {
                bare += s[i] == '\n' && (i == 0 || s[i - 1] != '\r');
            }
            if (!bare) {
                return std::string(s);
            }
            std::string out;
            out.reserve(s.size() + bare);
            for (size_t i = 0; i < s.size(); ++i) {
                if (s[i] == '\n' && (i == 0 || s[i - 1] != '\r')) {
                    out += '\r';
                }
                out += s[i];
            }
            return out;
        }
    }

    // Mail in Maildir directories, durable, without a database: each user's
    // Maildir under the root (<root>/<user>/), folders as Maildir++'s
    // .Name directories (see the namespace's comment for the files). A
    // handle of one word: copies share the state; safe from many threads
    // and next to other processes using the same directories (another
    // server, a delivery agent writing into new/).
    class maildir_backend {
    public:
        // The users' Maildirs under root (made when missing)
        explicit maildir_backend(const string& root)
        : _s(make_tracked<detail::MaildirState>()) {
            _s->root = std::string(root.view());
            while (_s->root.size() > 1 && _s->root.back() == '/') {
                _s->root.pop_back();
            }
            ::mkdir(_s->root.c_str(), 0700);
        }

        // A user's Maildir made (with its INBOX) and the password the
        // server's LOGIN checks when it has no check_password of its own
        expected<void, io::error> add_user(const string& user, const string& password = string()) const {
            if (!detail::valid_user_name(user.view())) {
                return unexpected(detail::imap_error(errc::cannot, "add_user", user));
            }
            const std::string dir = _user_dir(user);
            if (int e = detail::make_maildir(dir)) {
                return unexpected(detail::sys_error(e, "mkdir", dir));
            }
            std::lock_guard<std::mutex> g(_s->m);
            auto& u = _s->users[std::string(user.view())];
            u.password = std::string(password.view());
            u.has_password = !password.empty();
            return {};
        }

        // Limits of a user's mail (KiB, messages; 0 none), kept by this
        // handle; an append past one is errc::over_quota
        void set_quota(const string& user, uint64_t storage_kib, uint64_t messages) const {
            std::lock_guard<std::mutex> g(_s->m);
            auto& u = _s->users[std::string(user.view())];
            u.storage_limit = storage_kib;
            u.message_limit = messages;
        }

        // The directory of a mailbox ("INBOX" the user's Maildir)
        string path(const string& user, const string& mailbox) const {
            return string(_folder_dir(user, mailbox));
        }

        // --- what a server asks (memory_backend's methods, the same rules) ---

        bool authenticate(const string& user, const string& password) const {
            std::lock_guard<std::mutex> g(_s->m);
            auto it = _s->users.find(std::string(user.view()));
            return it != _s->users.end() && it->second.has_password && it->second.password == password.view();
        }

        expected<vector<list_entry>, io::error> mailboxes(const string& user) const {
            if (!detail::valid_user_name(user.view())) {
                return unexpected(detail::imap_error(errc::cannot, "list", user));
            }
            const std::string dir = _user_dir(user);
            if (!detail::is_dir(dir)) {
                if (int e = detail::make_maildir(dir)) {
                    return unexpected(detail::sys_error(e, "mkdir", dir));
                }
            }
            std::vector<std::string> subs = _subscriptions(dir);
            auto subscribed = [&](std::string_view n) {
                return std::find(subs.begin(), subs.end(), n) != subs.end();
            };
            vector<list_entry> out;
            auto entry = [&](const std::string& name, const std::string& folder) {
                list_entry e;
                e.name = string(name);
                std::string use;
                if (detail::read_file(folder + "/sgcl-special-use", use)) {
                    while (!use.empty() && (use.back() == '\n' || use.back() == '\r')) {
                        use.pop_back();
                    }
                    if (!use.empty()) {
                        e.attributes.push_back(string(use));
                    }
                }
                if (subscribed(name)) {
                    e.attributes.push_back(string("\\Subscribed"));
                }
                out.push_back(e);
            };
            entry("INBOX", dir);
            DIR* d = ::opendir(dir.c_str());
            std::vector<std::string> names;
            if (d) {
                while (struct dirent* de = ::readdir(d)) {
                    std::string_view n = de->d_name;
                    if (n.size() < 2 || n[0] != '.' || n == "..") {
                        continue;
                    }
                    std::string decoded;
                    if (!detail::folder_name_of(n.substr(1), decoded)) {
                        continue;
                    }
                    if (detail::is_dir(dir + "/" + std::string(n) + "/cur")) {
                        names.push_back(decoded);
                        entry(decoded, dir + "/" + std::string(n));
                    }
                }
                ::closedir(d);
            }
            for (const auto& s : subs) {
                if (s != "INBOX" && std::find(names.begin(), names.end(), s) == names.end()) {
                    list_entry e;
                    e.name = string(s);
                    e.attributes = {string("\\NonExistent"), string("\\Subscribed")};
                    out.push_back(e);
                }
            }
            return out;
        }

        expected<void, io::error> create(const string& user, const string& name, const string& special_use) const {
            if (!detail::valid_mailbox_name(name.view()) || name == "INBOX" || !detail::valid_user_name(user.view())) {
                return unexpected(detail::imap_error(name == "INBOX" ? errc::already_exists : errc::cannot, "create", name));
            }
            if (detail::folder_dir_name(name.view()).size() >= 255) {
                return unexpected(detail::imap_error(errc::cannot, "create", string("a name too long for the file system")));
            }
            const std::string dir = _folder_dir(user, name);
            if (detail::is_dir(dir)) {
                return unexpected(detail::imap_error(errc::already_exists, "create", name));
            }
            if (!detail::is_dir(_user_dir(user))) {
                if (int e = detail::make_maildir(_user_dir(user))) {
                    return unexpected(detail::sys_error(e, "mkdir", _user_dir(user)));
                }
            }
            if (int e = detail::make_maildir(dir)) {
                return unexpected(detail::sys_error(e, "mkdir", dir));
            }
            detail::write_file(dir + "/maildirfolder", "", false);
            if (!special_use.empty()) {
                detail::write_file(dir + "/sgcl-special-use", std::string(special_use.view()) + "\n", false);
            }
            return {};
        }

        expected<void, io::error> remove(const string& user, const string& name) const {
            if (name == "INBOX") {
                return unexpected(detail::imap_error(errc::cannot, "delete", name));
            }
            const std::string dir = _folder_dir(user, name);
            if (!detail::is_dir(dir + "/cur")) {
                return unexpected(detail::imap_error(errc::nonexistent, "delete", name));
            }
            // out of sight first (a rename), then removed
            const std::string gone = _user_dir(user) + "/..DELETED." + std::to_string(::getpid()) + "." + std::to_string(std::time(nullptr)) + "." + std::to_string(_count());
            if (::rename(dir.c_str(), gone.c_str()) != 0) {
                return unexpected(detail::sys_error(errno, "rename", dir));
            }
            detail::remove_tree(gone);
            _forget(dir);
            _s->watchers.fire(user, name);
            return {};
        }

        expected<void, io::error> rename(const string& user, const string& from, const string& to) const {
            if (!detail::valid_mailbox_name(to.view()) || to == "INBOX" || detail::folder_dir_name(to.view()).size() >= 255) {
                return unexpected(detail::imap_error(to == "INBOX" ? errc::already_exists : errc::cannot, "rename", to));
            }
            const std::string src = _folder_dir(user, from);
            const std::string dst = _folder_dir(user, to);
            if (!detail::is_dir(src + "/cur") && !_has_children(user, from)) {
                return unexpected(detail::imap_error(errc::nonexistent, "rename", from));
            }
            if (detail::is_dir(dst)) {
                return unexpected(detail::imap_error(errc::already_exists, "rename", to));
            }
            if (from == "INBOX") {
                // INBOX's messages into a new folder, INBOX left empty
                if (int e = detail::make_maildir(dst)) {
                    return unexpected(detail::sys_error(e, "mkdir", dst));
                }
                detail::write_file(dst + "/maildirfolder", "", false);
                auto f = _folder(src);
                std::lock_guard<std::mutex> g(f->m);
                detail::MdLock lock(src);
                if (auto e = _sync(*f); !e) {
                    return unexpected(e.error());
                }
                for (const auto& m : f->msgs) {
                    const std::string a = src + (m.in_new ? "/new/" : "/cur/") + m.base + (m.in_new ? "" : ":2," + m.letters);
                    const std::string b = dst + "/cur/" + m.base + ":2," + m.letters;
                    ::rename(a.c_str(), b.c_str());
                }
                if (!f->keywords.empty()) {
                    _write_keywords(dst, f->keywords);
                }
                f->msgs.clear();
                f->modseq += 1;
                _write_uidlist(*f);
                _s->watchers.fire(user, from);
                return {};
            }
            // the folder and those under it: every ".from" and ".from.*"
            const std::string user_dir = _user_dir(user);
            const std::string from_dir = detail::folder_dir_name(from.view());
            const std::string to_dir = detail::folder_dir_name(to.view());
            DIR* d = ::opendir(user_dir.c_str());
            std::vector<std::string> moving;
            if (d) {
                while (struct dirent* de = ::readdir(d)) {
                    std::string n = de->d_name;
                    if (n == "." + from_dir || n.rfind("." + from_dir + ".", 0) == 0) {
                        moving.push_back(n);
                    }
                }
                ::closedir(d);
            }
            for (const auto& n : moving) {
                const std::string renamed = "." + to_dir + n.substr(from_dir.size() + 1);
                if (detail::is_dir(user_dir + "/" + renamed)) {
                    return unexpected(detail::imap_error(errc::already_exists, "rename", to));
                }
            }
            for (const auto& n : moving) {
                const std::string renamed = "." + to_dir + n.substr(from_dir.size() + 1);
                const std::string a = user_dir + "/" + n, b = user_dir + "/" + renamed;
                if (::rename(a.c_str(), b.c_str()) != 0) {
                    return unexpected(detail::sys_error(errno, "rename", a));
                }
                _forget(a);
                // a new UIDVALIDITY: the UIDs of the old name do not carry over
                auto f = _folder(b);
                std::lock_guard<std::mutex> g(f->m);
                detail::MdLock lock(b);
                if (_sync(*f)) {
                    f->validity = detail::new_uid_validity();
                    _write_uidlist(*f);
                }
            }
            _s->watchers.fire(user, from);
            return {};
        }

        expected<void, io::error> subscribe(const string& user, const string& name, bool on) const {
            const std::string dir = _user_dir(user);
            std::lock_guard<std::mutex> g(_s->m);
            std::vector<std::string> subs = _subscriptions(dir);
            subs.erase(std::remove(subs.begin(), subs.end(), std::string(name.view())), subs.end());
            if (on) {
                subs.emplace_back(name.view());
            }
            std::string text;
            for (const auto& s : subs) {
                text += s;
                text += '\n';
            }
            if (int e = detail::write_file(dir + "/subscriptions", text, false)) {
                return unexpected(detail::sys_error(e, "write", dir + "/subscriptions"));
            }
            return {};
        }

        expected<mailbox_contents, io::error> open(const string& user, const string& name) const {
            const std::string dir = _folder_dir(user, name);
            if (!detail::is_dir(dir + "/cur")) {
                if (name == "INBOX" && detail::valid_user_name(user.view())) {
                    if (int e = detail::make_maildir(dir)) {
                        return unexpected(detail::sys_error(e, "mkdir", dir));
                    }
                } else {
                    return unexpected(detail::imap_error(errc::nonexistent, "select", name));
                }
            }
            auto f = _folder(dir);
            std::lock_guard<std::mutex> g(f->m);
            if (!_fresh(*f)) {
                detail::MdLock lock(dir);
                if (auto e = _sync(*f); !e) {
                    return unexpected(e.error());
                }
            }
            mailbox_contents c;
            c.uid_validity = f->validity;
            c.uid_next = f->next;
            c.highest_modseq = f->modseq;
            for (const auto& m : f->msgs) {
                stored_message s;
                s.uid = m.uid;
                s.flags = detail::md_flags(m.letters, f->keywords);
                s.modseq = m.modseq;
                s.internal_date = time::datetime::from_unix(m.mtime, time::zone::utc());
                s.size = m.size;
                c.messages.push_back(s);
            }
            return c;
        }

        // The folder's UIDVALIDITY (the folder read when the list is not
        // as it was)
        expected<uint32_t, io::error> uid_validity(const string& user, const string& name) const {
            const std::string dir = _folder_dir(user, name);
            if (!detail::is_dir(dir + "/cur")) {
                return unexpected(detail::imap_error(errc::nonexistent, "status", name));
            }
            auto f = _folder(dir);
            std::lock_guard<std::mutex> g(f->m);
            if (!_fresh(*f)) {
                detail::MdLock lock(dir);
                if (auto e = _sync(*f); !e) {
                    return unexpected(e.error());
                }
            }
            return f->validity;
        }

        // A number that moves with every change of the folder's files
        uint64_t revision(const string& user, const string& name) const {
            const std::string dir = _folder_dir(user, name);
            detail::MdStamp s = detail::stamp_of(dir);
            uint64_t h = 1469598103934665603ull;
            for (int64_t v : {s.uidlist, s.uidlist_size, s.cur, s.fresh}) {
                h = (h ^ uint64_t(v)) * 1099511628211ull;
            }
            return h ? h : 1;
        }

        expected<string, io::error> read(const string& user, const string& name, uint32_t uid) const {
            const std::string dir = _folder_dir(user, name);
            auto f = _folder(dir);
            for (int attempt = 0; attempt < 2; ++attempt) {
                std::string file;
                {
                    std::lock_guard<std::mutex> g(f->m);
                    if (!f->loaded || attempt > 0 || !_fresh(*f)) {
                        detail::MdLock lock(dir);
                        if (auto e = _sync(*f); !e) {
                            return unexpected(e.error());
                        }
                    }
                    const detail::MdMessage* m = f->find(uid);
                    if (!m) {
                        return unexpected(detail::imap_error(errc::expunged, "read", name));
                    }
                    file = dir + (m->in_new ? "/new/" + m->base : "/cur/" + m->base + ":2," + m->letters);
                }
                std::string data;
                if (detail::read_file(file, data)) {
                    return string(detail::crlf(data));
                }
            }
            return unexpected(detail::imap_error(errc::expunged, "read", name));
        }

        // A message delivered: written into tmp/ (flushed to disk), renamed
        // into cur/ with its flags, given its UID; safe beside other
        // deliveries and servers (`mail.append("alice", "INBOX", text)` in
        // an SMTP server's handler)
        expected<stored_message, io::error> append(const string& user, const string& name, const string& message, const vector<string>& flags = {},
                                                   const time::datetime& date = time::datetime::from_unix(std::time(nullptr), time::zone::utc())) const {
            const std::string dir = _folder_dir(user, name);
            if (!detail::is_dir(dir + "/cur")) {
                if (name == "INBOX" && detail::valid_user_name(user.view())) {
                    if (int e = detail::make_maildir(dir)) {
                        return unexpected(detail::sys_error(e, "mkdir", dir));
                    }
                } else {
                    return unexpected(detail::imap_error(errc::nonexistent, "append", name));
                }
            }
            const std::string text = detail::crlf(message.view());
            if (auto q = _check_quota(user, 1, text.size()); !q) {
                return unexpected(q.error());
            }
            const std::string base = detail::unique_base(text.size());
            const std::string tmp = dir + "/tmp/" + base;
            if (int e = detail::write_file(tmp + ".part", text, true)) {
                return unexpected(detail::sys_error(e, "write", tmp));
            }
            ::rename((tmp + ".part").c_str(), tmp.c_str());
            struct timeval tv[2];
            tv[0].tv_sec = tv[1].tv_sec = time_t(date.unix());
            tv[0].tv_usec = tv[1].tv_usec = 0;
            ::utimes(tmp.c_str(), tv);
            stored_message out;
            {
                auto f = _folder(dir);
                std::lock_guard<std::mutex> g(f->m);
                detail::MdLock lock(dir);
                if (auto e = _sync(*f); !e) {
                    ::unlink(tmp.c_str());
                    return unexpected(e.error());
                }
                std::string letters;
                bool kw_changed = false;
                if (!detail::md_letters(detail::normal_flags(flags), f->keywords, kw_changed, letters)) {
                    ::unlink(tmp.c_str());
                    return unexpected(detail::imap_error(errc::limit, "append", string("more than 26 keywords")));
                }
                if (f->next == UINT32_MAX) {
                    ::unlink(tmp.c_str());
                    return unexpected(detail::imap_error(errc::limit, "append", name));
                }
                const std::string target = dir + "/cur/" + base + ":2," + letters;
                if (::rename(tmp.c_str(), target.c_str()) != 0) {
                    const int e = errno;
                    ::unlink(tmp.c_str());
                    return unexpected(detail::sys_error(e, "rename", target));
                }
                detail::fsync_dir(dir + "/cur");
                if (kw_changed) {
                    _write_keywords(dir, f->keywords);
                }
                detail::MdMessage m;
                m.uid = f->next++;
                m.modseq = ++f->modseq;
                m.base = base;
                m.letters = letters;
                m.size = text.size();
                m.mtime = date.unix();
                f->msgs.push_back(m);
                if (auto e = _write_uidlist(*f); !e) {
                    return unexpected(e.error());
                }
                out.uid = m.uid;
                out.flags = detail::md_flags(letters, f->keywords);
                out.modseq = m.modseq;
                out.internal_date = date;
                out.size = m.size;
            }
            _s->watchers.fire(user, name);
            return out;
        }

        expected<uint64_t, io::error> store(const string& user, const string& name, const vector<flag_update>& changes) const {
            const std::string dir = _folder_dir(user, name);
            uint64_t modseq;
            {
                auto f = _folder(dir);
                std::lock_guard<std::mutex> g(f->m);
                detail::MdLock lock(dir);
                if (auto e = _sync(*f); !e) {
                    return unexpected(e.error());
                }
                modseq = ++f->modseq;
                bool kw_changed = false;
                for (const auto& c : changes) {
                    detail::MdMessage* m = f->find(c.uid);
                    if (!m) {
                        continue;
                    }
                    std::string letters;
                    if (!detail::md_letters(c.flags, f->keywords, kw_changed, letters)) {
                        return unexpected(detail::imap_error(errc::limit, "store", string("more than 26 keywords")));
                    }
                    const std::string a = dir + (m->in_new ? "/new/" + m->base : "/cur/" + m->base + ":2," + m->letters);
                    const std::string b = dir + "/cur/" + m->base + ":2," + letters;
                    if (a != b && ::rename(a.c_str(), b.c_str()) != 0) {
                        continue;   // gone meanwhile: the next sync sees it
                    }
                    m->letters = letters;
                    m->in_new = false;
                    m->modseq = modseq;
                }
                if (kw_changed) {
                    _write_keywords(dir, f->keywords);
                }
                if (auto e = _write_uidlist(*f); !e) {
                    return unexpected(e.error());
                }
            }
            _s->watchers.fire(user, name);
            return modseq;
        }

        expected<uint64_t, io::error> expunge(const string& user, const string& name, const vector<uint32_t>& uids) const {
            const std::string dir = _folder_dir(user, name);
            uint64_t modseq;
            {
                auto f = _folder(dir);
                std::lock_guard<std::mutex> g(f->m);
                detail::MdLock lock(dir);
                if (auto e = _sync(*f); !e) {
                    return unexpected(e.error());
                }
                std::vector<uint32_t> gone(uids.begin(), uids.end());
                std::sort(gone.begin(), gone.end());
                std::vector<detail::MdMessage> keep;
                for (const auto& m : f->msgs) {
                    if (std::binary_search(gone.begin(), gone.end(), m.uid)) {
                        const std::string p = dir + (m.in_new ? "/new/" + m.base : "/cur/" + m.base + ":2," + m.letters);
                        ::unlink(p.c_str());
                    } else {
                        keep.push_back(m);
                    }
                }
                f->msgs = std::move(keep);
                modseq = ++f->modseq;
                if (auto e = _write_uidlist(*f); !e) {
                    return unexpected(e.error());
                }
            }
            _s->watchers.fire(user, name);
            return modseq;
        }

        // The messages copied by hard links (a copy where a link cannot be
        // made), their flags and dates kept
        expected<vector<stored_message>, io::error> copy(const string& user, const string& from, const vector<uint32_t>& uids, const string& to) const {
            const std::string src = _folder_dir(user, from);
            const std::string dst = _folder_dir(user, to);
            if (!detail::is_dir(dst + "/cur")) {
                return unexpected(detail::imap_error(errc::nonexistent, "copy", to));
            }
            struct Source {
                std::string file;
                vector<string> flags;
                int64_t mtime;
                uint64_t size;
            };
            vector<Source> sources;   // managed: the flags are strings
            uint64_t bytes = 0;
            {
                auto f = _folder(src);
                std::lock_guard<std::mutex> g(f->m);
                detail::MdLock lock(src);
                if (auto e = _sync(*f); !e) {
                    return unexpected(e.error());
                }
                for (uint32_t uid : uids) {
                    const detail::MdMessage* m = f->find(uid);
                    if (!m) {
                        continue;
                    }
                    Source s;
                    s.file = src + (m->in_new ? "/new/" + m->base : "/cur/" + m->base + ":2," + m->letters);
                    s.flags = detail::md_flags(m->letters, f->keywords);
                    s.mtime = m->mtime;
                    s.size = m->size;
                    bytes += m->size;
                    sources.push_back(s);
                }
            }
            if (auto q = _check_quota(user, sources.size(), bytes); !q) {
                return unexpected(q.error());
            }
            vector<stored_message> out;
            {
                auto f = _folder(dst);
                std::lock_guard<std::mutex> g(f->m);
                detail::MdLock lock(dst);
                if (auto e = _sync(*f); !e) {
                    return unexpected(e.error());
                }
                const uint64_t modseq = ++f->modseq;
                bool kw_changed = false;
                for (const auto& src_msg : sources) {
                    const Source* s = &src_msg;
                    std::string letters;
                    if (!detail::md_letters(s->flags, f->keywords, kw_changed, letters)) {
                        return unexpected(detail::imap_error(errc::limit, "copy", string("more than 26 keywords")));
                    }
                    const std::string base = detail::unique_base(s->size);
                    const std::string target = dst + "/cur/" + base + ":2," + letters;
                    if (::link(s->file.c_str(), target.c_str()) != 0) {
                        std::string data;
                        if (!detail::read_file(s->file, data)) {
                            continue;   // expunged meanwhile
                        }
                        const std::string tmp = dst + "/tmp/" + base;
                        if (detail::write_file(tmp, data, true) != 0 || ::rename(tmp.c_str(), target.c_str()) != 0) {
                            return unexpected(detail::sys_error(errno, "copy", target));
                        }
                        struct timeval tv[2];
                        tv[0].tv_sec = tv[1].tv_sec = time_t(s->mtime);
                        tv[0].tv_usec = tv[1].tv_usec = 0;
                        ::utimes(target.c_str(), tv);
                    }
                    if (f->next == UINT32_MAX) {
                        ::unlink(target.c_str());
                        return unexpected(detail::imap_error(errc::limit, "copy", to));
                    }
                    detail::MdMessage m;
                    m.uid = f->next++;
                    m.modseq = modseq;
                    m.base = base;
                    m.letters = letters;
                    m.size = s->size;
                    m.mtime = s->mtime;
                    f->msgs.push_back(m);
                    stored_message sm;
                    sm.uid = m.uid;
                    sm.flags = s->flags;
                    sm.modseq = modseq;
                    sm.internal_date = time::datetime::from_unix(s->mtime, time::zone::utc());
                    sm.size = s->size;
                    out.push_back(sm);
                }
                detail::fsync_dir(dst + "/cur");
                if (kw_changed) {
                    _write_keywords(dst, f->keywords);
                }
                if (auto e = _write_uidlist(*f); !e) {
                    return unexpected(e.error());
                }
            }
            _s->watchers.fire(user, to);
            return out;
        }

        expected<imap::quota, io::error> quota(const string& user) const {
            imap::quota q;
            auto boxes = mailboxes(user);
            if (!boxes) {
                return unexpected(boxes.error());
            }
            for (const auto& b : *boxes) {
                if (b.has_attribute(string("\\NonExistent"))) {
                    continue;
                }
                auto c = open(user, b.name);
                if (!c) {
                    continue;
                }
                q.messages_used += c->messages.size();
                for (const auto& m : c->messages) {
                    q.storage_used += m.size;
                }
            }
            q.storage_used = (q.storage_used + 1023) / 1024;
            std::lock_guard<std::mutex> g(_s->m);
            auto it = _s->users.find(std::string(user.view()));
            if (it != _s->users.end()) {
                q.storage_limit = it->second.storage_limit;
                q.messages_limit = it->second.message_limit;
            }
            return q;
        }

    private:
        friend struct detail::WatchAccess;

        void _watch(const detail::Watcher& w) const {
            _s->watchers.add(w);
        }

        std::string _user_dir(const string& user) const {
            return _s->root + "/" + std::string(user.view());
        }

        std::string _folder_dir(const string& user, const string& name) const {
            if (name == "INBOX") {
                return _user_dir(user);
            }
            return _user_dir(user) + "/." + detail::folder_dir_name(name.view());
        }

        // Whether folders under the name exist (a parent never made itself)
        bool _has_children(const string& user, const string& name) const {
            const std::string prefix = "." + detail::folder_dir_name(name.view()) + ".";
            DIR* d = ::opendir(_user_dir(user).c_str());
            bool found = false;
            if (d) {
                while (struct dirent* de = ::readdir(d)) {
                    found |= std::string_view(de->d_name).substr(0, prefix.size()) == prefix;
                }
                ::closedir(d);
            }
            return found;
        }

        static uint64_t _count() noexcept {
            static std::atomic<uint64_t> n{0};
            return n.fetch_add(1) + 1;
        }

        std::shared_ptr<detail::MdFolder> _folder(const std::string& dir) const {
            std::lock_guard<std::mutex> g(_s->m);
            auto& f = _s->folders[dir];
            if (!f) {
                f = std::make_shared<detail::MdFolder>();
                f->path = dir;
            }
            return f;
        }

        void _forget(const std::string& dir) const {
            std::lock_guard<std::mutex> g(_s->m);
            for (auto it = _s->folders.begin(); it != _s->folders.end();) {
                if (it->first == dir || it->first.rfind(dir + ".", 0) == 0) {
                    it = _s->folders.erase(it);
                } else {
                    ++it;
                }
            }
        }

        std::vector<std::string> _subscriptions(const std::string& user_dir) const {
            std::vector<std::string> out;
            std::string text;
            if (detail::read_file(user_dir + "/subscriptions", text)) {
                size_t i = 0;
                while (i < text.size()) {
                    size_t e = text.find('\n', i);
                    if (e == std::string::npos) {
                        e = text.size();
                    }
                    std::string line = text.substr(i, e - i);
                    if (!line.empty() && line.back() == '\r') {
                        line.pop_back();
                    }
                    if (!line.empty()) {
                        out.push_back(line);
                    }
                    i = e + 1;
                }
            }
            return out;
        }

        expected<void, io::error> _check_quota(const string& user, uint64_t messages, uint64_t bytes) const {
            uint64_t storage_limit = 0, message_limit = 0;
            {
                std::lock_guard<std::mutex> g(_s->m);
                auto it = _s->users.find(std::string(user.view()));
                if (it == _s->users.end() || (!it->second.storage_limit && !it->second.message_limit)) {
                    return {};
                }
                storage_limit = it->second.storage_limit;
                message_limit = it->second.message_limit;
            }
            auto q = quota(user);
            if (!q) {
                return unexpected(q.error());
            }
            if ((message_limit && q->messages_used + messages > message_limit) || (storage_limit && q->storage_used + (bytes + 1023) / 1024 > storage_limit)) {
                return unexpected(detail::imap_error(errc::over_quota, "append", user));
            }
            return {};
        }

        // Whether the folder's cache matches its files (the stamps of the
        // uidlist, cur/ and new/ as when it was last read)
        static bool _fresh(detail::MdFolder& f) {
            return f.loaded && detail::stamp_of(f.path) == f.stamp;
        }

        static void _write_keywords(const std::string& dir, const std::vector<std::string>& keywords) {
            std::string text;
            for (size_t i = 0; i < keywords.size(); ++i) {
                text += std::to_string(i) + " " + keywords[i] + "\n";
            }
            detail::write_file(dir + "/sgcl-keywords", text, false);
        }

        static expected<void, io::error> _write_uidlist(detail::MdFolder& f) {
            std::string text = "SGCL1 " + std::to_string(f.validity) + " " + std::to_string(f.next) + " " + std::to_string(f.modseq) + "\n";
            for (const auto& m : f.msgs) {
                text += std::to_string(m.uid) + " " + std::to_string(m.modseq) + " " + (m.letters.empty() ? "-" : m.letters) + " " + m.base + "\n";
            }
            if (int e = detail::write_file(f.path + "/sgcl-uidlist", text, true)) {
                return unexpected(detail::sys_error(e, "write", f.path + "/sgcl-uidlist"));
            }
            f.stamp = detail::stamp_of(f.path);
            return {};
        }

        // The folder read again under its lock: the uidlist, the keywords,
        // the files of new/ (moved into cur/) and cur/; UIDs for files the
        // list does not know, files gone dropped, flags changed by a rename
        // given a new mod-sequence; the list written when anything differed
        static expected<void, io::error> _sync(detail::MdFolder& f) {
            const detail::MdStamp now = detail::stamp_of(f.path);
            if (f.loaded && now == f.stamp) {
                return {};
            }
            if (!detail::is_dir(f.path + "/cur")) {
                return unexpected(detail::imap_error(errc::nonexistent, "read", string(f.path)));
            }
            // the list
            std::string text;
            std::vector<detail::MdMessage> listed;
            uint32_t validity = 0, next = 1;
            uint64_t modseq = 1;
            bool dirty = false;
            if (detail::read_file(f.path + "/sgcl-uidlist", text)) {
                if (!detail::parse_uidlist(text, validity, next, modseq, listed)) {
                    // a list that does not read: kept aside, rebuilt
                    ::rename((f.path + "/sgcl-uidlist").c_str(), (f.path + "/sgcl-uidlist.broken").c_str());
                    listed.clear();
                    validity = 0;
                }
            }
            if (!validity) {
                validity = detail::new_uid_validity();
                next = 1;
                modseq = 1;
                dirty = true;
            }
            std::vector<std::string> keywords;
            std::string kw;
            if (detail::read_file(f.path + "/sgcl-keywords", kw)) {
                detail::parse_keywords(kw, keywords);
            }
            // new/ into cur/
            std::vector<std::string> fresh;
            detail::list_dir(f.path + "/new", fresh);
            for (const auto& n : fresh) {
                const std::string a = f.path + "/new/" + n;
                const std::string b = f.path + "/cur/" + n + (n.find(":2,") == std::string::npos ? ":2," : "");
                ::rename(a.c_str(), b.c_str());
            }
            std::vector<std::string> files;
            detail::list_dir(f.path + "/cur", files);
            struct OnDisk {
                std::string base, letters;
                bool seen = false;
            };
            std::map<std::string, OnDisk> disk;
            for (const auto& n : files) {
                OnDisk d;
                detail::split_file_name(n, d.base, d.letters);
                if (d.base.empty()) {
                    continue;
                }
                disk[d.base] = d;
            }
            std::vector<detail::MdMessage> msgs;
            msgs.reserve(listed.size());
            for (auto& m : listed) {
                auto it = disk.find(m.base);
                if (it == disk.end()) {
                    dirty = true;   // expunged from outside
                    continue;
                }
                it->second.seen = true;
                if (it->second.letters != m.letters) {
                    m.letters = it->second.letters;
                    m.modseq = ++modseq;
                    dirty = true;
                }
                m.in_new = false;
                msgs.push_back(m);
            }
            // files the list does not know: UIDs in the order of their times
            std::vector<detail::MdMessage> added;
            for (auto& [base, d] : disk) {
                if (d.seen) {
                    continue;
                }
                detail::MdMessage m;
                m.base = base;
                m.letters = d.letters;
                added.push_back(m);
            }
            for (auto& m : added) {
                struct stat st;
                const std::string p = f.path + "/cur/" + m.base + ":2," + m.letters;
                if (::stat(p.c_str(), &st) == 0) {
                    m.mtime = int64_t(st.st_mtime);
                    m.size = detail::size_in_name(m.base, uint64_t(st.st_size));
                    if (m.base.find(",W=") == std::string::npos) {
                        // a file from outside may have LF line ends: its
                        // size as IMAP reads it (CRLF), counted once
                        std::string data;
                        if (detail::read_file(p, data)) {
                            m.size = detail::crlf(data).size();
                        }
                    }
                }
            }
            std::sort(added.begin(), added.end(), [](const detail::MdMessage& a, const detail::MdMessage& b) {
                return a.mtime != b.mtime ? a.mtime < b.mtime : a.base < b.base;
            });
            for (auto& m : added) {
                if (next == UINT32_MAX) {
                    break;
                }
                m.uid = next++;
                m.modseq = ++modseq;
                msgs.push_back(m);
                dirty = true;
            }
            std::sort(msgs.begin(), msgs.end(), [](const detail::MdMessage& a, const detail::MdMessage& b) { return a.uid < b.uid; });
            // sizes and dates of the listed (",S=" and ",W=" in the name, else the file's)
            for (auto& m : msgs) {
                if (m.mtime && m.size) {
                    continue;
                }
                const std::string p = f.path + "/cur/" + m.base + ":2," + m.letters;
                struct stat st;
                if (::stat(p.c_str(), &st) == 0) {
                    m.mtime = int64_t(st.st_mtime);
                    m.size = detail::size_in_name(m.base, uint64_t(st.st_size));
                }
            }
            f.validity = validity;
            f.next = next;
            f.modseq = modseq;
            f.msgs = std::move(msgs);
            f.keywords = std::move(keywords);
            f.loaded = true;
            if (dirty) {
                return _write_uidlist(f);
            }
            f.stamp = detail::stamp_of(f.path);
            return {};
        }

        tracked_ptr<detail::MaildirState> _s;
    };
}
