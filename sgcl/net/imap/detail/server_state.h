//------------------------------------------------------------------------------
// SGCL: a C++20 application platform
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

#include "mime.h"
#include "search.h"
#include "syntax.h"
#include "../backend.h"
#include "../types.h"
#include "../../connection.h"
#include "../../tls.h"
#include "../../../async/wait_group.h"
#include "../../../core/function.h"
#include "../../../core/map.h"
#include "../../../core/tracked_ptr.h"
#include "../../../core/vector.h"

#include <algorithm>
#include <atomic>
#include <iostream>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

// What a server keeps between its sessions: the settings a serve() took,
// the connections, and a hub per mailbox open in any session. A hub holds
// the mailbox's messages as the backend gave them (UID, flags, mod-
// sequence, date, size) while a session has it selected, makes every
// change through the backend and then into itself, and hands each other
// session that has the mailbox selected what changed (new UIDs, UIDs
// expunged, UIDs whose flags changed), which that session announces at
// its next chance (RFC 9051 §7: EXISTS, EXPUNGE, FETCH) — at once while it
// idles. A session's view is its own numbering of the messages: the
// sequence numbers move only when that session is told of an expunge.
namespace sgcl::net::imap::detail {
    struct ServerSettings {
        tracked_ptr<Backend> backend;
        function<bool(const string&, const string&)> check_password;
        function<bool(const string&, const string&)> check_token;
        optional<net::tls::config> tls;           // STARTTLS offered
        duration idle_timeout;
        duration login_timeout;
        duration poll_interval;
        size_t max_literal = 0;
        size_t max_command = 0;
        size_t max_connections = 0;
        int max_auth_failures = 3;
        bool compress = true;
        vector<pair<string, string>> id;
        string greeting;
        function<void(const string&)> on_error;

        void report(const string& what) const {
            if (on_error) {
                on_error(what);
            } else {
                std::cerr << "imap: " << what.view() << '\n';
            }
        }
    };

    // Set while a session changes a mailbox through the backend: the
    // backend's watchers fire on the same thread inside the call, and the
    // server's skips the change it makes itself (the hub takes it in
    // after the call, under the lock the watcher would wait for)
    inline thread_local bool hub_change = false;

    struct HubChange {
        HubChange() noexcept {
            hub_change = true;
        }

        ~HubChange() {
            hub_change = false;
        }

        HubChange(const HubChange&) = delete;
        HubChange& operator=(const HubChange&) = delete;
    };

    // What a hub keeps of a message once it was read (shared by the
    // sessions; its own lock)
    struct MsgCache {
        std::mutex m;
        bool parsed[2] = {false, false};     // [utf8]
        std::string envelope[2];
        std::string body[2];                 // BODY: the non-extensible structure
        std::string structure[2];            // BODYSTRUCTURE
        std::unique_ptr<SortInfo> sort;
    };

    struct Msg {
        uint32_t uid = 0;
        uint8_t flags = 0;
        std::vector<std::string> keywords;
        uint64_t modseq = 1;
        DateTime date;
        uint64_t size = 0;
        std::shared_ptr<MsgCache> cache;
    };

    inline DateTime date_of(const time::datetime& t) noexcept {
        return DateTime{t.unix(), int(t.offset().seconds() / 60)};
    }

    inline Msg msg_of(const stored_message& s) {
        Msg m;
        m.uid = s.uid;
        for (const auto& f : s.flags) {
            const uint8_t b = system_flag(f.view());
            if (b) {
                m.flags |= b;
            } else if (!f.empty() && f.view()[0] != '\\') {
                m.keywords.emplace_back(f.view());
            }
        }
        m.modseq = s.modseq;
        m.date = date_of(s.internal_date);
        m.size = s.size;
        m.cache = std::make_shared<MsgCache>();
        return m;
    }

    inline vector<string> flags_of(uint8_t sys, const std::vector<std::string>& keywords) {
        vector<string> out;
        for (uint8_t b : {FlagSeen, FlagAnswered, FlagFlagged, FlagDeleted, FlagDraft}) {
            if (sys & b) {
                out.push_back(string(system_flag_name(b)));
            }
        }
        for (const auto& k : keywords) {
            out.push_back(string(k));
        }
        return out;
    }

    class Hub;

    // A session's selection of a mailbox
    struct View {
        tracked_ptr<Hub> hub;
        std::vector<uint32_t> uids;          // the session's numbering: uids[seq - 1]
        std::vector<uint32_t> recent;        // the UIDs recent in this session, ascending
        std::vector<uint32_t> new_uids;      // waiting to be announced
        std::vector<uint32_t> gone_uids;
        std::vector<uint32_t> changed_uids;
        bool keywords_changed = false;
        bool read_only = false;
        bool dead = false;                   // the mailbox deleted or renamed under it
        function<void()> wake;               // set while the session idles

        // The sequence number of a UID in this view, 0 for none
        uint32_t seq_of(uint32_t uid) const noexcept {
            auto it = std::lower_bound(uids.begin(), uids.end(), uid);
            return it != uids.end() && *it == uid ? uint32_t(it - uids.begin() + 1) : 0;
        }

        bool has_pending() const noexcept {
            return !new_uids.empty() || !gone_uids.empty() || !changed_uids.empty() || keywords_changed || dead;
        }
    };

    class Hub {
    public:
        std::mutex m;
        string user;
        string name;
        uint32_t uid_validity = 1;
        uint32_t uid_next = 1;
        uint64_t highest_modseq = 1;
        uint64_t revision = 0;
        bool dead = false;
        uint32_t recent_from = 1;            // the UIDs from here are recent for the next session that selects read-write
        std::vector<Msg> msgs;               // ascending UIDs
        std::vector<std::string> keywords;   // every keyword in use, for FLAGS
        vector<tracked_ptr<View>> views;

        Msg* find(uint32_t uid) noexcept {
            auto it = std::lower_bound(msgs.begin(), msgs.end(), uid, [](const Msg& a, uint32_t u) { return a.uid < u; });
            return it != msgs.end() && it->uid == uid ? &*it : nullptr;
        }

        void load(const mailbox_contents& c) {
            uid_validity = c.uid_validity;
            uid_next = c.uid_next;
            highest_modseq = c.highest_modseq;
            msgs.clear();
            msgs.reserve(c.messages.size());
            for (const auto& s : c.messages) {
                msgs.push_back(msg_of(s));
                if (s.uid >= uid_next) {
                    uid_next = s.uid + 1;
                }
                if (s.modseq > highest_modseq) {
                    highest_modseq = s.modseq;
                }
            }
            std::sort(msgs.begin(), msgs.end(), [](const Msg& a, const Msg& b) { return a.uid < b.uid; });
            recent_from = uid_next;
            collect_keywords();
        }

        // The keywords in use; whether new ones came
        bool collect_keywords() {
            bool added = false;
            for (const auto& msg : msgs) {
                for (const auto& k : msg.keywords) {
                    bool has = false;
                    for (const auto& x : keywords) {
                        has |= iequal(x, k);
                    }
                    if (!has) {
                        keywords.push_back(k);
                        added = true;
                    }
                }
            }
            return added;
        }

        // A new message, announced to every view (the actor's own too)
        void added(const stored_message& s, vector<function<void()>>& wakes) {
            if (find(s.uid)) {
                return;
            }
            Msg m = msg_of(s);
            if (s.uid >= uid_next) {
                uid_next = s.uid + 1;
            }
            if (s.modseq > highest_modseq) {
                highest_modseq = s.modseq;
            }
            const bool sorted = msgs.empty() || msgs.back().uid < s.uid;
            msgs.push_back(std::move(m));
            if (!sorted) {
                std::sort(msgs.begin(), msgs.end(), [](const Msg& a, const Msg& b) { return a.uid < b.uid; });
            }
            const bool kw = collect_keywords();
            for (auto& v : views) {
                v->new_uids.push_back(s.uid);
                v->keywords_changed |= kw;
                if (v->wake) {
                    wakes.push_back(v->wake);
                }
            }
        }

        // UIDs gone, announced to the views but the actor's
        void removed(const std::vector<uint32_t>& uids, uint64_t modseq, const View* actor, vector<function<void()>>& wakes) {
            if (uids.empty()) {
                return;
            }
            std::vector<uint32_t> sorted = uids;
            std::sort(sorted.begin(), sorted.end());
            msgs.erase(std::remove_if(msgs.begin(), msgs.end(), [&](const Msg& m) { return std::binary_search(sorted.begin(), sorted.end(), m.uid); }),
                       msgs.end());
            if (modseq > highest_modseq) {
                highest_modseq = modseq;
            }
            for (auto& v : views) {
                if (v.get() == actor) {
                    continue;
                }
                v->gone_uids.insert(v->gone_uids.end(), sorted.begin(), sorted.end());
                if (v->wake) {
                    wakes.push_back(v->wake);
                }
            }
        }

        // Flags changed, announced to the views but the actor's
        void changed(const std::vector<uint32_t>& uids, const View* actor, vector<function<void()>>& wakes) {
            if (uids.empty()) {
                return;
            }
            const bool kw = collect_keywords();
            for (auto& v : views) {
                if (v.get() == actor) {
                    v->keywords_changed |= kw;
                    continue;
                }
                v->changed_uids.insert(v->changed_uids.end(), uids.begin(), uids.end());
                v->keywords_changed |= kw;
                if (v->wake) {
                    wakes.push_back(v->wake);
                }
            }
        }

        // The mailbox gone (deleted, renamed): every view told
        void killed(vector<function<void()>>& wakes) {
            dead = true;
            for (auto& v : views) {
                v->dead = true;
                if (v->wake) {
                    wakes.push_back(v->wake);
                }
            }
        }

        // The backend's mailbox read again when its revision moved (a
        // delivery, a change from outside the server): what differs from
        // the hub announced
        void refresh(vector<function<void()>>& wakes) {
            Backend& b = *backend;
            if (!b.has_revision()) {
                return;
            }
            const uint64_t r = b.revision(user, name);
            if (r == revision) {
                return;
            }
            auto c = b.open(user, name);
            revision = r;
            if (!c) {
                if (c.error().code() == make_error_code(errc::nonexistent)) {
                    killed(wakes);
                }
                return;
            }
            if (c->uid_validity != uid_validity) {
                killed(wakes);
                return;
            }
            // diff: gone, changed, new
            std::vector<uint32_t> gone, changed_list;
            size_t i = 0;
            std::vector<const stored_message*> fresh;
            for (const auto& s : c->messages) {
                fresh.push_back(&s);
            }
            std::sort(fresh.begin(), fresh.end(), [](const stored_message* a, const stored_message* b2) { return a->uid < b2->uid; });
            size_t j = 0;
            std::vector<const stored_message*> added_list;
            while (i < msgs.size() || j < fresh.size()) {
                if (j >= fresh.size() || (i < msgs.size() && msgs[i].uid < fresh[j]->uid)) {
                    gone.push_back(msgs[i].uid);
                    ++i;
                } else if (i >= msgs.size() || fresh[j]->uid < msgs[i].uid) {
                    added_list.push_back(fresh[j]);
                    ++j;
                } else {
                    Msg n = msg_of(*fresh[j]);
                    Msg& o = msgs[i];
                    if (n.flags != o.flags || n.keywords != o.keywords || n.modseq != o.modseq) {
                        o.flags = n.flags;
                        o.keywords = n.keywords;
                        o.modseq = n.modseq;
                        changed_list.push_back(o.uid);
                    }
                    ++i;
                    ++j;
                }
            }
            if (c->highest_modseq > highest_modseq) {
                highest_modseq = c->highest_modseq;
            }
            removed(gone, c->highest_modseq, nullptr, wakes);
            changed(changed_list, nullptr, wakes);
            for (const auto* s : added_list) {
                added(*s, wakes);
            }
            if (c->uid_next > uid_next) {
                uid_next = c->uid_next;
            }
        }

        // The revision as it is after a change the hub made itself
        void settle() {
            if (backend->has_revision()) {
                revision = backend->revision(user, name);
            }
        }

        tracked_ptr<Backend> backend;
    };

    // A connection as the server tracks it: waiting for a command (idle),
    // running one (active), or closed by a shutdown
    struct ConnEntry {
        enum : int { idle, active, closed };
        net::connection c;
        std::atomic<int> state{idle};
    };

    // The server's shared state
    struct ServerImpl {
        std::mutex lock;
        vector<net::listener> listeners;
        map<string, tracked_ptr<Hub>> hubs;   // user "\n" mailbox
        map<uint64_t, tracked_ptr<ConnEntry>> connections;
        uint64_t next_id = 0;
        async::wait_group running;
        std::atomic<bool> shutting_down{false};
        std::atomic<bool> closed{false};
        std::atomic<size_t> active{0};
        bool watching = false;

        static string key(const string& user, const string& name) {
            return user + "\n" + name.view();
        }

        void close_listeners() {
            vector<net::listener> ls;
            {
                std::lock_guard<std::mutex> g(lock);
                ls = listeners;
            }
            for (auto& l : ls) {
                (void)l.close();
            }
        }

        vector<tracked_ptr<ConnEntry>> snapshot() {
            std::lock_guard<std::mutex> g(lock);
            vector<tracked_ptr<ConnEntry>> out;
            for (const auto& [id, c] : connections) {
                out.push_back(c);
            }
            return out;
        }

        // The hub of an open mailbox, nullptr when no session has it open
        tracked_ptr<Hub> find_hub(const string& user, const string& name) {
            std::lock_guard<std::mutex> g(lock);
            auto it = hubs.find(key(user, name));
            return it == hubs.end() ? tracked_ptr<Hub>() : it->second;
        }

        // A mailbox changed through the backend from outside the server's
        // sessions: its hub, if there is one, reads it again
        void changed_outside(const string& user, const string& name) {
            tracked_ptr<Hub> h = find_hub(user, name);
            if (!h) {
                return;
            }
            vector<function<void()>> wakes;
            {
                std::lock_guard<std::mutex> g(h->m);
                h->refresh(wakes);
            }
            for (auto& w : wakes) {
                w();
            }
        }

        // Every hub of the user under a name (a delete or a rename):
        // dropped and their views told
        void drop_hubs(const string& user, const string& name) {
            vector<tracked_ptr<Hub>> gone;
            {
                std::lock_guard<std::mutex> g(lock);
                vector<string> keys;
                for (const auto& [k, h] : hubs) {
                    if (h->user == user && (h->name == name || is_under(h->name.view(), name.view()))) {
                        keys.push_back(k);
                        gone.push_back(h);
                    }
                }
                for (const auto& k : keys) {
                    hubs.erase(k);
                }
            }
            for (auto& h : gone) {
                vector<function<void()>> wakes;
                {
                    std::lock_guard<std::mutex> g(h->m);
                    h->killed(wakes);
                }
                for (auto& w : wakes) {
                    w();
                }
            }
        }
    };
}
