# SGCL: a C++20 application platform
# Copyright (c) 2022-2026 Sebastian Nibisz
# SPDX-License-Identifier: Apache-2.0
#
# The oracle for the recurrence rules of sgcl/encoding (recurrence.h): a second
# implementation of RFC 5545 §3.3.10, written for the tests apart from the
# library's and in another way (Python's datetime and zoneinfo, every day of a
# period tried against every part of the rule), asked to expand random rules
# from random starts in three zones; the instants it gives written out as a
# C++ header the tests include:
#
#     python3 tools/rrule_oracle.py > tests/encoding/rrule_tests.h
#
# The rules' semantics, as recurrence.h states them: the instances are the
# start, then the rule's times after it; BYDAY's number counts in the month for
# MONTHLY and for YEARLY with BYMONTH, in the year otherwise; the period of a
# YEARLY rule with BYWEEKNO is its week-year (weeks starting on WKST, week 1
# the first with four days of the year); BYSETPOS picks from a period's times
# in order; a time the zone skips moves on by the skip, one it shows twice is
# the first.
import datetime as dt
import random
import zoneinfo

WD = ['MO', 'TU', 'WE', 'TH', 'FR', 'SA', 'SU']
FREQS = ['SECONDLY', 'MINUTELY', 'HOURLY', 'DAILY', 'WEEKLY', 'MONTHLY', 'YEARLY']


def week1(year, wkst):
    jan4 = dt.date(year, 1, 4)
    return jan4 - dt.timedelta(days=(jan4.weekday() - wkst) % 7)


def week_of(d, wkst):
    y = d.year
    if d < week1(y, wkst):
        y -= 1
    elif d >= week1(y + 1, wkst):
        y += 1
    w1 = week1(y, wkst)
    n = (d - w1).days // 7 + 1
    weeks = (week1(y + 1, wkst) - w1).days // 7
    return y, n, weeks


def month_len(y, m):
    return ((dt.date(y + (m == 12), m % 12 + 1, 1)) - dt.date(y, m, 1)).days


def day_matches(d, r, freq, defaults):
    months = r.get('BYMONTH') or defaults.get('BYMONTH')
    if months and d.month not in months:
        return False
    mdays = r.get('BYMONTHDAY') or defaults.get('BYMONTHDAY')
    if mdays:
        ml = month_len(d.year, d.month)
        if not any(x == d.day or x == d.day - ml - 1 for x in mdays):
            return False
    if r.get('BYYEARDAY'):
        yd = d.timetuple().tm_yday
        yl = 366 if (d.year % 4 == 0 and d.year % 100 != 0) or d.year % 400 == 0 else 365
        if not any(x == yd or x == yd - yl - 1 for x in r['BYYEARDAY']):
            return False
    if r.get('BYWEEKNO'):
        _, n, weeks = week_of(d, r['WKST'])
        if not any(x == n or x == n - weeks - 1 for x in r['BYWEEKNO']):
            return False
    days = r.get('BYDAY') or defaults.get('BYDAY')
    if days:
        in_month = freq == 'MONTHLY' or (freq == 'YEARLY' and r.get('BYMONTH'))
        ok = False
        for (ordinal, wd) in days:
            if d.weekday() != wd:
                continue
            if ordinal == 0:
                ok = True
                break
            if in_month:
                first = dt.date(d.year, d.month, 1)
                last = dt.date(d.year, d.month, month_len(d.year, d.month))
            else:
                first = dt.date(d.year, 1, 1)
                last = dt.date(d.year, 12, 31)
            n = (d - first).days // 7 + 1
            m = (last - d).days // 7 + 1
            if ordinal == n or ordinal == -m:
                ok = True
                break
        if not ok:
            return False
    return True


def expand(start, r, limit, horizon):
    """The walls (naive datetimes) of the instances: start, then the rule's."""
    freq = r['FREQ']
    interval = r.get('INTERVAL', 1)
    defaults = {}
    if not any(r.get(k) for k in ('BYWEEKNO', 'BYYEARDAY', 'BYMONTHDAY', 'BYDAY')):
        if freq == 'YEARLY':
            if not r.get('BYMONTH'):
                defaults['BYMONTH'] = [start.month]
            defaults['BYMONTHDAY'] = [start.day]
        elif freq == 'MONTHLY':
            defaults['BYMONTHDAY'] = [start.day]
        elif freq == 'WEEKLY':
            defaults['BYDAY'] = [(0, start.weekday())]
    hours = sorted(set(r.get('BYHOUR') or [start.hour]))
    minutes = sorted(set(r.get('BYMINUTE') or [start.minute]))
    seconds = sorted(set(s for s in (r.get('BYSECOND') or [start.second]) if s < 60))
    count = r.get('COUNT')
    until = r.get('UNTIL')
    if until is not None and until(start):
        return []
    out = [start]
    k = 0
    while len(out) < limit:
        # the days and times of period k, tried one by one
        cands = []
        if freq == 'YEARLY':
            y = start.year + k * interval
            if y > horizon.year:
                break
            if r.get('BYWEEKNO'):
                d, last = week1(y, r['WKST']), week1(y + 1, r['WKST']) - dt.timedelta(days=1)
            else:
                d, last = dt.date(y, 1, 1), dt.date(y, 12, 31)
            days = []
            while d <= last:
                days.append(d)
                d += dt.timedelta(days=1)
        elif freq == 'MONTHLY':
            mi = start.year * 12 + start.month - 1 + k * interval
            y, m = divmod(mi, 12)
            m += 1
            if y > horizon.year:
                break
            days = [dt.date(y, m, i) for i in range(1, month_len(y, m) + 1)]
        elif freq == 'WEEKLY':
            ws = start.date() - dt.timedelta(days=(start.weekday() - r['WKST']) % 7) + dt.timedelta(days=7 * k * interval)
            if ws > horizon.date():
                break
            days = [ws + dt.timedelta(days=i) for i in range(7)]
        elif freq == 'DAILY':
            d = start.date() + dt.timedelta(days=k * interval)
            if d > horizon.date():
                break
            days = [d]
        else:
            unit = {'HOURLY': 3600, 'MINUTELY': 60, 'SECONDLY': 1}[freq]
            base = start.replace(microsecond=0)
            if freq == 'HOURLY':
                base = base.replace(minute=0, second=0)
            elif freq == 'MINUTELY':
                base = base.replace(second=0)
            p = base + dt.timedelta(seconds=k * interval * unit)
            if p > horizon:
                break
            days = []
            if day_matches(p.date(), r, freq, defaults):
                if (not r.get('BYHOUR') or p.hour in r['BYHOUR']):
                    if freq == 'HOURLY':
                        cands = [p.replace(minute=mm, second=ss) for mm in minutes for ss in seconds]
                    elif not r.get('BYMINUTE') or p.minute in r['BYMINUTE']:
                        if freq == 'MINUTELY':
                            cands = [p.replace(second=ss) for ss in seconds]
                        elif not r.get('BYSECOND') or p.second in r['BYSECOND']:
                            cands = [p]
        for d in days:
            if day_matches(d, r, freq, defaults):
                for h in hours:
                    for mm in minutes:
                        for ss in seconds:
                            cands.append(dt.datetime(d.year, d.month, d.day, h, mm, ss))
        cands.sort()
        if r.get('BYSETPOS') and cands:
            n = len(cands)
            picked = set()
            for pos in r['BYSETPOS']:
                i = pos - 1 if pos > 0 else n + pos
                if 0 <= i < n:
                    picked.add(cands[i])
            cands = sorted(picked)
        stop = False
        for c in cands:
            if c <= start:
                continue
            if until is not None and until(c):
                stop = True
                break
            if count is not None and len(out) >= count:
                stop = True
                break
            out.append(c)
            if len(out) >= limit:
                break
        if stop:
            break
        k += 1
    if count is not None:
        out = out[:count]
    return out


def instant(wall, zone):
    """A wall's Unix seconds: a skipped time moved on by the skip, a repeated one the first."""
    if zone is None:
        return int(wall.replace(tzinfo=dt.timezone.utc).timestamp())
    a = wall.replace(tzinfo=zone, fold=0)
    b = wall.replace(tzinfo=zone, fold=1)
    back = a.astimezone(dt.timezone.utc).astimezone(zone).replace(tzinfo=None)
    if back != wall:
        # skipped: the offset before the gap (fold=0 maps it so in zoneinfo? use fold=1's offset: the earlier one)
        before = min(a.utcoffset(), b.utcoffset())
        after = max(a.utcoffset(), b.utcoffset())
        return int((wall - before).replace(tzinfo=dt.timezone.utc).timestamp())
    return int(min(a.timestamp(), b.timestamp()))


def random_rule(rnd):
    freq = rnd.choice(FREQS[2:] * 3 + FREQS[:2])
    r = {'FREQ': freq, 'WKST': 0}
    parts = ['FREQ=' + freq]
    if rnd.random() < 0.4:
        r['INTERVAL'] = rnd.choice([2, 3, 5])
        parts.append('INTERVAL=%d' % r['INTERVAL'])

    def pick(name, values, k):
        vals = sorted(set(rnd.sample(values, k)))
        r[name] = vals
        parts.append('%s=%s' % (name, ','.join(map(str, vals))))
    if rnd.random() < 0.3:
        pick('BYMONTH', list(range(1, 13)), rnd.randint(1, 4))
    if freq != 'WEEKLY' and rnd.random() < 0.3:
        pick('BYMONTHDAY', list(range(1, 32)) + list(range(-31, 0)), rnd.randint(1, 4))
    if freq in ('YEARLY', 'HOURLY', 'MINUTELY', 'SECONDLY') and rnd.random() < 0.2:
        pick('BYYEARDAY', list(range(1, 367)) + list(range(-366, 0)), rnd.randint(1, 5))
    weekno = freq == 'YEARLY' and rnd.random() < 0.25
    if weekno:
        pick('BYWEEKNO', list(range(1, 54)) + list(range(-53, 0)), rnd.randint(1, 3))
    if rnd.random() < 0.45:
        days = rnd.sample(range(7), rnd.randint(1, 4))
        items = []
        for wd in sorted(days):
            if freq in ('MONTHLY', 'YEARLY') and not weekno and rnd.random() < 0.4:
                o = rnd.choice([1, 2, 3, 4, -1, -2] if freq == 'MONTHLY' or r.get('BYMONTH') else [1, 2, 10, 20, 52, -1, -10])
                items.append((o, wd))
            else:
                items.append((0, wd))
        r['BYDAY'] = items
        parts.append('BYDAY=' + ','.join((str(o) if o else '') + WD[wd] for o, wd in items))
    if freq in ('YEARLY', 'MONTHLY', 'WEEKLY', 'DAILY') and rnd.random() < 0.3:
        pick('BYHOUR', list(range(24)), rnd.randint(1, 3))
    if freq in ('YEARLY', 'MONTHLY', 'WEEKLY', 'DAILY', 'HOURLY') and rnd.random() < 0.25:
        pick('BYMINUTE', list(range(0, 60, 5)), rnd.randint(1, 3))
    if freq in ('HOURLY', 'MINUTELY', 'SECONDLY') and rnd.random() < 0.3:
        pick('BYHOUR', list(range(24)), rnd.randint(3, 10))
    if freq in ('MINUTELY', 'SECONDLY') and rnd.random() < 0.3:
        pick('BYMINUTE', list(range(60)), rnd.randint(5, 30))
    if freq == 'SECONDLY' and rnd.random() < 0.5:
        pick('BYSECOND', list(range(60)), rnd.randint(1, 10))
    if len(parts) > 1 + ('INTERVAL' in r) and rnd.random() < 0.25:
        pick('BYSETPOS', [1, 2, 3, -1, -2], rnd.randint(1, 2))
    if rnd.random() < 0.3:
        r['WKST'] = rnd.randrange(7)
        parts.append('WKST=' + WD[r['WKST']])
    return r, parts


def main():
    rnd = random.Random(20261006)
    zones = [('America/New_York', zoneinfo.ZoneInfo('America/New_York')), ('Europe/Warsaw', zoneinfo.ZoneInfo('Europe/Warsaw')), ('UTC', None)]
    print('// Generated by tools/rrule_oracle.py: do not edit.')
    print('#pragma once\n#include <cstdint>\n#include <string_view>\nnamespace rrule_oracle {')
    print('struct expansion { std::string_view zone; int64_t start_wall; std::string_view rule; std::string_view instants; };')
    print('inline constexpr expansion expansions[] = {')
    n = 0
    while n < 600:
        r, parts = random_rule(rnd)
        zname, zone = rnd.choice(zones)
        start = dt.datetime(rnd.randint(1995, 2030), rnd.randint(1, 12), rnd.randint(1, 28), rnd.randrange(24), rnd.choice([0, 0, 15, 30, 59]), rnd.choice([0, 0, 0, 30]))
        if rnd.random() < 0.5:
            r['COUNT'] = rnd.randint(1, 40)
            parts.append('COUNT=%d' % r['COUNT'])
        elif rnd.random() < 0.5:
            u = start + dt.timedelta(days=rnd.randint(0, 900), seconds=rnd.randrange(86400))
            if rnd.random() < 0.5 or zone is None:
                ut = instant(u, zone)
                r['UNTIL'] = lambda c, ut=ut, zone=zone: instant(c, zone) > ut
                parts.append('UNTIL=' + dt.datetime.fromtimestamp(ut, dt.timezone.utc).strftime('%Y%m%dT%H%M%SZ'))
            else:
                r['UNTIL'] = lambda c, u=u: c > u
                parts.append('UNTIL=' + u.strftime('%Y%m%dT%H%M%S'))
        limit = 40
        horizon = start + dt.timedelta(days=366 * 6 if r['FREQ'] in ('YEARLY', 'MONTHLY') else 400 if r['FREQ'] in ('WEEKLY', 'DAILY') else 3 if r['FREQ'] == 'HOURLY' else 0.25)
        walls = expand(start, r, limit, horizon)
        # the instants before the horizon only: what both sides reach
        walls = [w for w in walls if w <= horizon]
        if len(walls) >= limit:
            walls = walls[:limit]
        sw = int(start.replace(tzinfo=dt.timezone.utc).timestamp())
        hz = instant(horizon, zone)
        print('    {"%s", %d, "%s", "%s;%s"},' % (zname, sw, ';'.join(parts), hz, ','.join(str(instant(w, zone)) for w in walls)))
        n += 1
    print('};\n}')


main()
