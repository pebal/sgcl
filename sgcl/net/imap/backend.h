//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "error.h"
#include "types.h"
#include "detail/search.h"
#include "detail/syntax.h"
#include "../../core/aliases.h"
#include "../../core/function.h"
#include "../../core/make_tracked.h"
#include "../../core/map.h"
#include "../../core/string.h"
#include "../../core/tracked_ptr.h"
#include "../../core/vector.h"
#include "../../core/weak_ptr.h"
#include "../../time/datetime.h"

#include <algorithm>
#include <atomic>
#include <ctime>
#include <mutex>
#include <string>
#include <type_traits>
#include <utility>

namespace sgcl::net::imap {
    // What a server keeps mail in, as the server asks it (the methods of
    // memory_backend and maildir_backend, and of a program's own type
    // given to a backend): the users' mailboxes, their messages with
    // UIDs, UIDVALIDITY, flags and mod-sequences, append, store, expunge.
    // The server does the rest of IMAP over it: sequence numbers, the
    // notifications of other sessions, SEARCH, SORT and THREAD, FETCH's
    // sections. Names are UTF-8, "/" the delimiter, INBOX always written
    // so.
    namespace detail {
        using Watcher = function<void(const string&, const string&)>;

        class Backend {
        public:
            virtual ~Backend() = default;
            virtual bool has_authenticate() const noexcept = 0;
            virtual bool authenticate(const string& user, const string& password) = 0;
            virtual expected<vector<list_entry>, io::error> mailboxes(const string& user) = 0;
            virtual expected<void, io::error> create(const string& user, const string& name, const string& special_use) = 0;
            virtual expected<void, io::error> remove(const string& user, const string& name) = 0;
            virtual expected<void, io::error> rename(const string& user, const string& from, const string& to) = 0;
            virtual expected<void, io::error> subscribe(const string& user, const string& name, bool on) = 0;
            virtual expected<mailbox_contents, io::error> open(const string& user, const string& name) = 0;
            virtual bool has_revision() const noexcept = 0;
            virtual uint64_t revision(const string& user, const string& name) = 0;
            // The UIDVALIDITY of a mailbox (APPENDUID, COPYUID of one not
            // open): the backend's own when it has one, else open()'s
            virtual expected<uint32_t, io::error> uid_validity(const string& user, const string& name) = 0;
            virtual expected<string, io::error> read(const string& user, const string& name, uint32_t uid) = 0;
            virtual expected<stored_message, io::error> append(const string& user, const string& name, const string& message, const vector<string>& flags,
                                                              const time::datetime& date) = 0;
            virtual expected<uint64_t, io::error> store(const string& user, const string& name, const vector<flag_update>& changes) = 0;
            virtual expected<uint64_t, io::error> expunge(const string& user, const string& name, const vector<uint32_t>& uids) = 0;
            virtual bool has_copy() const noexcept = 0;
            virtual expected<vector<stored_message>, io::error> copy(const string& user, const string& from, const vector<uint32_t>& uids, const string& to) = 0;
            virtual bool has_quota() const noexcept = 0;
            virtual expected<imap::quota, io::error> quota(const string& user) = 0;
            virtual bool has_watch() const noexcept = 0;
            virtual void watch(const Watcher& w) = 0;
        };

        // The hook of the module's backends for their changes (a private
        // method each, reached through this friend)
        struct WatchAccess {
            template<class B>
            static auto watch(const B& b, const Watcher& w) -> decltype(b._watch(w)) {
                return b._watch(w);
            }
        };

        // A program's type as a backend: its methods called, the optional
        // ones (authenticate, revision, copy, quota, watch) where it has them
        template<class B>
        class BackendOf final : public Backend {
        public:
            explicit BackendOf(B b) noexcept(std::is_nothrow_move_constructible_v<B>)
            : _b(std::move(b)) {
            }

            bool has_authenticate() const noexcept override {
                return requires(B& b, const string& s) { { b.authenticate(s, s) } -> std::convertible_to<bool>; };
            }

            bool authenticate(const string& user, const string& password) override {
                if constexpr (requires(B& b, const string& s) { { b.authenticate(s, s) } -> std::convertible_to<bool>; }) {
                    return _b.authenticate(user, password);
                } else {
                    (void)user;
                    (void)password;
                    return false;
                }
            }

            expected<vector<list_entry>, io::error> mailboxes(const string& user) override {
                return _b.mailboxes(user);
            }

            expected<void, io::error> create(const string& user, const string& name, const string& special_use) override {
                return _b.create(user, name, special_use);
            }

            expected<void, io::error> remove(const string& user, const string& name) override {
                return _b.remove(user, name);
            }

            expected<void, io::error> rename(const string& user, const string& from, const string& to) override {
                return _b.rename(user, from, to);
            }

            expected<void, io::error> subscribe(const string& user, const string& name, bool on) override {
                return _b.subscribe(user, name, on);
            }

            expected<mailbox_contents, io::error> open(const string& user, const string& name) override {
                return _b.open(user, name);
            }

            bool has_revision() const noexcept override {
                return requires(B& b, const string& s) { { b.revision(s, s) } -> std::convertible_to<uint64_t>; };
            }

            uint64_t revision(const string& user, const string& name) override {
                if constexpr (requires(B& b, const string& s) { { b.revision(s, s) } -> std::convertible_to<uint64_t>; }) {
                    return _b.revision(user, name);
                } else {
                    (void)user;
                    (void)name;
                    return 0;
                }
            }

            expected<uint32_t, io::error> uid_validity(const string& user, const string& name) override {
                if constexpr (requires(B& b, const string& s) { { b.uid_validity(s, s) } -> std::same_as<expected<uint32_t, io::error>>; }) {
                    return _b.uid_validity(user, name);
                } else {
                    auto c = _b.open(user, name);
                    if (!c) {
                        return unexpected(c.error());
                    }
                    return c->uid_validity;
                }
            }

            expected<string, io::error> read(const string& user, const string& name, uint32_t uid) override {
                return _b.read(user, name, uid);
            }

            expected<stored_message, io::error> append(const string& user, const string& name, const string& message, const vector<string>& flags,
                                                       const time::datetime& date) override {
                return _b.append(user, name, message, flags, date);
            }

            expected<uint64_t, io::error> store(const string& user, const string& name, const vector<flag_update>& changes) override {
                return _b.store(user, name, changes);
            }

            expected<uint64_t, io::error> expunge(const string& user, const string& name, const vector<uint32_t>& uids) override {
                return _b.expunge(user, name, uids);
            }

            bool has_copy() const noexcept override {
                return requires(B& b, const string& s, const vector<uint32_t>& u) { b.copy(s, s, u, s); };
            }

            expected<vector<stored_message>, io::error> copy(const string& user, const string& from, const vector<uint32_t>& uids, const string& to) override {
                if constexpr (requires(B& b, const string& s, const vector<uint32_t>& u) { b.copy(s, s, u, s); }) {
                    return _b.copy(user, from, uids, to);
                } else {
                    // read and append, one by one, with the flags and dates kept
                    auto src = _b.open(user, from);
                    if (!src) {
                        return unexpected(src.error());
                    }
                    vector<stored_message> out;
                    for (uint32_t uid : uids) {
                        const stored_message* m = nullptr;
                        for (const auto& x : src->messages) {
                            if (x.uid == uid) {
                                m = &x;
                                break;
                            }
                        }
                        if (!m) {
                            continue;
                        }
                        auto text = _b.read(user, from, uid);
                        if (!text) {
                            return unexpected(text.error());
                        }
                        auto r = _b.append(user, to, *text, m->flags, m->internal_date);
                        if (!r) {
                            return unexpected(r.error());
                        }
                        out.push_back(*r);
                    }
                    return out;
                }
            }

            bool has_quota() const noexcept override {
                return requires(B& b, const string& s) { { b.quota(s) } -> std::same_as<expected<imap::quota, io::error>>; };
            }

            expected<imap::quota, io::error> quota(const string& user) override {
                if constexpr (requires(B& b, const string& s) { { b.quota(s) } -> std::same_as<expected<imap::quota, io::error>>; }) {
                    return _b.quota(user);
                } else {
                    (void)user;
                    return unexpected(imap_error(errc::not_supported, "quota"));
                }
            }

            bool has_watch() const noexcept override {
                return requires(B& b, const Watcher& w) { WatchAccess::watch(b, w); };
            }

            void watch(const Watcher& w) override {
                if constexpr (requires(B& b, const Watcher& w2) { WatchAccess::watch(b, w2); }) {
                    WatchAccess::watch(_b, w);
                } else {
                    (void)w;
                }
            }

        private:
            B _b;
        };

        // The watchers of a backend of the module's: called after a change
        // made through the backend itself (a delivery the program made, or a
        // change of the server's), so that a server tells its sessions
        // (IDLE) at once
        struct Watchers {
            std::mutex m;
            vector<Watcher> list;

            void add(const Watcher& w) {
                std::lock_guard<std::mutex> g(m);
                list.push_back(w);
            }

            void fire(const string& user, const string& name) {
                vector<Watcher> copy;
                {
                    std::lock_guard<std::mutex> g(m);
                    copy = list;
                }
                for (auto& w : copy) {
                    w(user, name);
                }
            }
        };

        // A UIDVALIDITY for a mailbox made now: the seconds of the clock,
        // never the same twice in the process
        inline uint32_t new_uid_validity() noexcept {
            static std::atomic<uint32_t> last{0};
            uint32_t now = uint32_t(std::time(nullptr)) & 0x7FFFFFFF;
            if (now == 0) {
                now = 1;
            }
            uint32_t prev = last.load();
            for (;;) {
                const uint32_t next = now > prev ? now : prev + 1;
                if (last.compare_exchange_weak(prev, next)) {
                    return next;
                }
            }
        }

        // The flags as stored: the system flags first in their order, then
        // the keywords, no repeats, "\Recent" left out (the server's own)
        inline vector<string> normal_flags(const vector<string>& in) {
            uint8_t sys = 0;
            std::vector<std::string> kw;
            for (const auto& f : in) {
                const uint8_t b = system_flag(f.view());
                if (b) {
                    sys |= b;
                    continue;
                }
                if (iequal(f.view(), "\\Recent") || f.empty() || f.view()[0] == '\\') {
                    continue;
                }
                bool dup = false;
                for (const auto& k : kw) {
                    dup |= iequal(k, f.view());
                }
                if (!dup) {
                    kw.emplace_back(f.view());
                }
            }
            vector<string> out;
            for (uint8_t b : {FlagSeen, FlagAnswered, FlagFlagged, FlagDeleted, FlagDraft}) {
                if (sys & b) {
                    out.push_back(string(system_flag_name(b)));
                }
            }
            for (const auto& k : kw) {
                out.push_back(string(k));
            }
            return out;
        }

        // Whether name is under parent ("a/b" under "a")
        inline bool is_under(std::string_view name, std::string_view parent) noexcept {
            return name.size() > parent.size() && name.compare(0, parent.size(), parent) == 0 && name[parent.size()] == '/';
        }

        // A name a store takes: not empty, no empty level, no "%" or "*",
        // no control characters, UTF-8
        inline bool valid_mailbox_name(std::string_view n) noexcept {
            if (n.empty() || n.size() > 1024 || n.front() == '/' || n.back() == '/' || n.find("//") != std::string_view::npos) {
                return false;
            }
            for (unsigned char c : n) {
                if (c < 0x20 || c == 0x7f || c == '%' || c == '*') {
                    return false;
                }
            }
            return valid_utf8(n);
        }

        // --- the memory backend's state ---------------------------------

        struct MemMessage {
            stored_message meta;
            string content;
        };

        struct MemMailbox {
            string name;
            string special_use;
            uint32_t uid_validity = 1;
            uint32_t uid_next = 1;
            uint64_t highest_modseq = 1;
            uint64_t revision = 1;
            uint64_t bytes = 0;
            vector<MemMessage> messages;
        };

        struct MemUser {
            string password;
            bool has_password = false;
            map<string, tracked_ptr<MemMailbox>> mailboxes;
            vector<string> subscriptions;
            uint64_t storage_limit = 0;   // KiB, 0: none
            uint64_t message_limit = 0;
        };

        struct MemoryState {
            std::mutex m;
            map<string, tracked_ptr<MemUser>> users;
            Watchers watchers;
        };
    }

    // Mail in memory: users, their mailboxes and messages on the managed
    // heap, gone with the process (tests, a server of a program's own
    // data). A handle of one word: copies share the mail. Safe from many
    // threads. A user's INBOX is made with the user.
    class memory_backend {
    public:
        SGCL_INLINE_HOT memory_backend()
        : _s(make_tracked<detail::MemoryState>()) {
        }

        // A user with a password the server's LOGIN and AUTHENTICATE check
        // (when the server has no check_password of its own), and an
        // INBOX; a user already there gets the password
        void add_user(const string& user, const string& password) const {
            std::lock_guard<std::mutex> g(_s->m);
            auto u = _user(user);
            u->password = password;
            u->has_password = true;
        }

        // Limits of a user's mail: the storage in KiB and the messages, 0
        // for none; an append past one is errc::over_quota
        void set_quota(const string& user, uint64_t storage_kib, uint64_t messages) const {
            std::lock_guard<std::mutex> g(_s->m);
            auto u = _user(user);
            u->storage_limit = storage_kib;
            u->message_limit = messages;
        }

        // --- what a server asks ---

        // Whether the password is the user's
        bool authenticate(const string& user, const string& password) const {
            std::lock_guard<std::mutex> g(_s->m);
            auto it = _s->users.find(user);
            return it != _s->users.end() && it->second->has_password && it->second->password == password;
        }

        // The user's mailboxes, with "\\Subscribed" and the special use in
        // their attributes, and the names subscribed without a mailbox
        // ("\\NonExistent")
        expected<vector<list_entry>, io::error> mailboxes(const string& user) const {
            std::lock_guard<std::mutex> g(_s->m);
            auto u = _user(user);
            vector<list_entry> out;
            for (const auto& [name, mb] : u->mailboxes) {
                list_entry e;
                e.name = name;
                if (!mb->special_use.empty()) {
                    e.attributes.push_back(mb->special_use);
                }
                if (_subscribed(*u, name)) {
                    e.attributes.push_back(string("\\Subscribed"));
                }
                out.push_back(e);
            }
            for (const auto& s : u->subscriptions) {
                if (u->mailboxes.find(s) == u->mailboxes.end()) {
                    list_entry e;
                    e.name = s;
                    e.attributes = {string("\\NonExistent"), string("\\Subscribed")};
                    out.push_back(e);
                }
            }
            return out;
        }

        expected<void, io::error> create(const string& user, const string& name, const string& special_use) const {
            if (!detail::valid_mailbox_name(name.view())) {
                return unexpected(detail::imap_error(errc::cannot, "create", name));
            }
            std::lock_guard<std::mutex> g(_s->m);
            auto u = _user(user);
            if (u->mailboxes.find(name) != u->mailboxes.end()) {
                return unexpected(detail::imap_error(errc::already_exists, "create", name));
            }
            u->mailboxes.emplace(name, _new_mailbox(name, special_use));
            return {};
        }

        expected<void, io::error> remove(const string& user, const string& name) const {
            {
                std::lock_guard<std::mutex> g(_s->m);
                auto u = _user(user);
                if (name == "INBOX") {
                    return unexpected(detail::imap_error(errc::cannot, "delete", name));
                }
                auto it = u->mailboxes.find(name);
                if (it == u->mailboxes.end()) {
                    return unexpected(detail::imap_error(errc::nonexistent, "delete", name));
                }
                u->mailboxes.erase(it);
            }
            _s->watchers.fire(user, name);
            return {};
        }

        // The mailbox and those under it renamed; INBOX's messages moved
        // into a new mailbox, INBOX left empty (RFC 9051 §6.3.6)
        expected<void, io::error> rename(const string& user, const string& from, const string& to) const {
            if (!detail::valid_mailbox_name(to.view())) {
                return unexpected(detail::imap_error(errc::cannot, "rename", to));
            }
            {
                std::lock_guard<std::mutex> g(_s->m);
                auto u = _user(user);
                auto it = u->mailboxes.find(from);
                bool children = false;
                for (const auto& [n, mb] : u->mailboxes) {
                    children |= detail::is_under(n.view(), from.view());
                }
                if (it == u->mailboxes.end() && !children) {
                    return unexpected(detail::imap_error(errc::nonexistent, "rename", from));
                }
                if (u->mailboxes.find(to) != u->mailboxes.end()) {
                    return unexpected(detail::imap_error(errc::already_exists, "rename", to));
                }
                if (from == "INBOX") {
                    tracked_ptr<detail::MemMailbox> inbox = it->second;
                    auto moved = _new_mailbox(to, string());
                    moved->messages = inbox->messages;
                    moved->uid_next = inbox->uid_next;
                    moved->highest_modseq = inbox->highest_modseq;
                    moved->bytes = inbox->bytes;
                    inbox->messages = vector<detail::MemMessage>();
                    inbox->bytes = 0;
                    inbox->highest_modseq += 1;
                    inbox->revision += 1;
                    u->mailboxes.emplace(to, moved);
                } else {
                    vector<pair<string, tracked_ptr<detail::MemMailbox>>> moving;
                    for (const auto& [n, mb] : u->mailboxes) {
                        if (n == from || detail::is_under(n.view(), from.view())) {
                            moving.push_back(pair<string, tracked_ptr<detail::MemMailbox>>(n, mb));
                        }
                    }
                    for (const auto& [n, mb] : moving) {
                        string renamed = to + n.view().substr(from.size());
                        if (u->mailboxes.find(renamed) != u->mailboxes.end()) {
                            return unexpected(detail::imap_error(errc::already_exists, "rename", renamed));
                        }
                    }
                    for (const auto& [n, mb] : moving) {
                        string renamed = to + n.view().substr(from.size());
                        u->mailboxes.erase(n);
                        mb->name = renamed;
                        mb->uid_validity = detail::new_uid_validity();
                        u->mailboxes.emplace(renamed, mb);
                    }
                }
            }
            _s->watchers.fire(user, from);
            return {};
        }

        expected<void, io::error> subscribe(const string& user, const string& name, bool on) const {
            std::lock_guard<std::mutex> g(_s->m);
            auto u = _user(user);
            vector<string> next;
            for (const auto& s : u->subscriptions) {
                if (s != name) {
                    next.push_back(s);
                }
            }
            if (on) {
                next.push_back(name);
            }
            u->subscriptions = next;
            return {};
        }

        expected<mailbox_contents, io::error> open(const string& user, const string& name) const {
            std::lock_guard<std::mutex> g(_s->m);
            auto mb = _find(user, name);
            if (!mb) {
                return unexpected(detail::imap_error(errc::nonexistent, "select", name));
            }
            mailbox_contents c;
            c.uid_validity = mb->uid_validity;
            c.uid_next = mb->uid_next;
            c.highest_modseq = mb->highest_modseq;
            for (const auto& m : mb->messages) {
                c.messages.push_back(m.meta);
            }
            return c;
        }

        // A number that grows with every change of the mailbox (0 for none)
        uint64_t revision(const string& user, const string& name) const {
            std::lock_guard<std::mutex> g(_s->m);
            auto mb = _find(user, name);
            return mb ? mb->revision : 0;
        }

        // The mailbox's UIDVALIDITY
        expected<uint32_t, io::error> uid_validity(const string& user, const string& name) const {
            std::lock_guard<std::mutex> g(_s->m);
            auto mb = _find(user, name);
            if (!mb) {
                return unexpected(detail::imap_error(errc::nonexistent, "status", name));
            }
            return mb->uid_validity;
        }

        expected<string, io::error> read(const string& user, const string& name, uint32_t uid) const {
            std::lock_guard<std::mutex> g(_s->m);
            auto mb = _find(user, name);
            if (!mb) {
                return unexpected(detail::imap_error(errc::nonexistent, "read", name));
            }
            const auto* m = _message(*mb, uid);
            if (!m) {
                return unexpected(detail::imap_error(errc::expunged, "read", name));
            }
            return m->content;
        }

        // A message added with its flags and date: its UID, mod-sequence
        // and size (a delivery: `mail.append("alice", "INBOX", text)`)
        expected<stored_message, io::error> append(const string& user, const string& name, const string& message, const vector<string>& flags = {},
                                                   const time::datetime& date = time::datetime::from_unix(std::time(nullptr), time::zone::utc())) const {
            stored_message out;
            {
                std::lock_guard<std::mutex> g(_s->m);
                auto u = _user(user);
                auto it = u->mailboxes.find(name);
                if (it == u->mailboxes.end()) {
                    return unexpected(detail::imap_error(errc::nonexistent, "append", name));
                }
                auto mb = it->second;
                if (u->message_limit || u->storage_limit) {
                    uint64_t count = 0, bytes = 0;
                    for (const auto& [n, x] : u->mailboxes) {
                        count += x->messages.size();
                        bytes += x->bytes;
                    }
                    if ((u->message_limit && count + 1 > u->message_limit) || (u->storage_limit && (bytes + message.size() + 1023) / 1024 > u->storage_limit)) {
                        return unexpected(detail::imap_error(errc::over_quota, "append", name));
                    }
                }
                if (mb->uid_next == UINT32_MAX) {
                    return unexpected(detail::imap_error(errc::limit, "append", name));
                }
                detail::MemMessage m;
                m.meta.uid = mb->uid_next++;
                m.meta.flags = detail::normal_flags(flags);
                m.meta.modseq = ++mb->highest_modseq;
                m.meta.internal_date = date;
                m.meta.size = message.size();
                m.content = message;
                mb->messages.push_back(m);
                mb->bytes += message.size();
                mb->revision += 1;
                out = m.meta;
            }
            _s->watchers.fire(user, name);
            return out;
        }

        // New flags stored: the mod-sequence they were given
        expected<uint64_t, io::error> store(const string& user, const string& name, const vector<flag_update>& changes) const {
            uint64_t modseq;
            {
                std::lock_guard<std::mutex> g(_s->m);
                auto mb = _find(user, name);
                if (!mb) {
                    return unexpected(detail::imap_error(errc::nonexistent, "store", name));
                }
                modseq = ++mb->highest_modseq;
                for (const auto& c : changes) {
                    auto* m = _message(*mb, c.uid);
                    if (m) {
                        m->meta.flags = detail::normal_flags(c.flags);
                        m->meta.modseq = modseq;
                    }
                }
                mb->revision += 1;
            }
            _s->watchers.fire(user, name);
            return modseq;
        }

        // The messages of the UIDs removed: the mod-sequence of the removal
        expected<uint64_t, io::error> expunge(const string& user, const string& name, const vector<uint32_t>& uids) const {
            uint64_t modseq;
            {
                std::lock_guard<std::mutex> g(_s->m);
                auto mb = _find(user, name);
                if (!mb) {
                    return unexpected(detail::imap_error(errc::nonexistent, "expunge", name));
                }
                std::vector<uint32_t> gone(uids.begin(), uids.end());
                std::sort(gone.begin(), gone.end());
                vector<detail::MemMessage> keep;
                for (const auto& m : mb->messages) {
                    if (std::binary_search(gone.begin(), gone.end(), m.meta.uid)) {
                        mb->bytes -= m.meta.size;
                    } else {
                        keep.push_back(m);
                    }
                }
                mb->messages = keep;
                modseq = ++mb->highest_modseq;
                mb->revision += 1;
            }
            _s->watchers.fire(user, name);
            return modseq;
        }

        // The messages copied into another mailbox, their flags and dates
        // kept: what they became there, in the order of the UIDs given
        expected<vector<stored_message>, io::error> copy(const string& user, const string& from, const vector<uint32_t>& uids, const string& to) const {
            vector<stored_message> out;
            {
                std::lock_guard<std::mutex> g(_s->m);
                auto src = _find(user, from);
                auto dst = _find(user, to);
                if (!src) {
                    return unexpected(detail::imap_error(errc::nonexistent, "copy", from));
                }
                if (!dst) {
                    return unexpected(detail::imap_error(errc::nonexistent, "copy", to));
                }
                auto u = _user(user);
                if (u->message_limit || u->storage_limit) {
                    uint64_t count = 0, bytes = 0, more = 0, more_bytes = 0;
                    for (const auto& [n, x] : u->mailboxes) {
                        count += x->messages.size();
                        bytes += x->bytes;
                    }
                    for (uint32_t uid : uids) {
                        if (const auto* m = _message(*src, uid)) {
                            ++more;
                            more_bytes += m->meta.size;
                        }
                    }
                    if ((u->message_limit && count + more > u->message_limit) || (u->storage_limit && (bytes + more_bytes + 1023) / 1024 > u->storage_limit)) {
                        return unexpected(detail::imap_error(errc::over_quota, "copy", to));
                    }
                }
                const uint64_t modseq = ++dst->highest_modseq;
                for (uint32_t uid : uids) {
                    const auto* m = _message(*src, uid);
                    if (!m) {
                        continue;
                    }
                    if (dst->uid_next == UINT32_MAX) {
                        return unexpected(detail::imap_error(errc::limit, "copy", to));
                    }
                    detail::MemMessage c = *m;
                    c.meta.uid = dst->uid_next++;
                    c.meta.modseq = modseq;
                    dst->messages.push_back(c);
                    dst->bytes += c.meta.size;
                    out.push_back(c.meta);
                }
                dst->revision += 1;
            }
            _s->watchers.fire(user, to);
            return out;
        }

        // What the user's mail takes against the limits set (root "")
        expected<imap::quota, io::error> quota(const string& user) const {
            std::lock_guard<std::mutex> g(_s->m);
            auto u = _user(user);
            imap::quota q;
            uint64_t bytes = 0;
            for (const auto& [n, x] : u->mailboxes) {
                q.messages_used += x->messages.size();
                bytes += x->bytes;
            }
            q.storage_used = (bytes + 1023) / 1024;
            q.storage_limit = u->storage_limit;
            q.messages_limit = u->message_limit;
            return q;
        }

    private:
        friend struct detail::WatchAccess;

        // The server's hook for the changes made through this handle
        void _watch(const detail::Watcher& w) const {
            _s->watchers.add(w);
        }

        tracked_ptr<detail::MemUser> _user(const string& user) const {
            auto it = _s->users.find(user);
            if (it != _s->users.end()) {
                return it->second;
            }
            tracked_ptr u = make_tracked<detail::MemUser>();
            u->mailboxes.emplace(string("INBOX"), _new_mailbox(string("INBOX"), string()));
            _s->users.emplace(user, u);
            return u;
        }

        static tracked_ptr<detail::MemMailbox> _new_mailbox(const string& name, const string& special_use) {
            tracked_ptr mb = make_tracked<detail::MemMailbox>();
            mb->name = name;
            mb->special_use = special_use;
            mb->uid_validity = detail::new_uid_validity();
            return mb;
        }

        tracked_ptr<detail::MemMailbox> _find(const string& user, const string& name) const {
            auto u = _user(user);
            auto it = u->mailboxes.find(name);
            return it == u->mailboxes.end() ? tracked_ptr<detail::MemMailbox>() : it->second;
        }

        static bool _subscribed(const detail::MemUser& u, const string& name) noexcept {
            for (const auto& s : u.subscriptions) {
                if (s == name) {
                    return true;
                }
            }
            return false;
        }

        static detail::MemMessage* _message(detail::MemMailbox& mb, uint32_t uid) noexcept {
            auto& v = mb.messages;
            size_t lo = 0, hi = v.size();
            while (lo < hi) {
                const size_t mid = (lo + hi) / 2;
                if (v[mid].meta.uid < uid) {
                    lo = mid + 1;
                } else {
                    hi = mid;
                }
            }
            return lo < v.size() && v[lo].meta.uid == uid ? &v[lo] : nullptr;
        }

        tracked_ptr<detail::MemoryState> _s;
    };

    class backend;

    namespace detail {
        struct BackendAccess {
            static const tracked_ptr<Backend>& get(const backend& b) noexcept;
        };
    }

    // The store of a server (imap::server's field): a memory_backend, a
    // maildir_backend, or a program's own type with the methods of a
    // backend (see memory_backend for each), taken by value. A handle of
    // one word: copies share the store.
    class backend {
    public:
        // A memory_backend of its own
        backend()
        : backend(memory_backend()) {
        }

        template<class B>
            requires(!std::is_same_v<std::remove_cvref_t<B>, backend>)
        backend(B b)
        : _b(tracked_ptr<detail::Backend>(make_tracked<detail::BackendOf<B>>(std::move(b)))) {
        }

        backend(const backend&) = default;
        backend& operator=(const backend&) = default;

    private:
        friend struct detail::BackendAccess;

        tracked_ptr<detail::Backend> _b;
    };

    namespace detail {
        SGCL_INLINE_HOT const tracked_ptr<Backend>& BackendAccess::get(const backend& b) noexcept {
            return b._b;
        }
    }
}
