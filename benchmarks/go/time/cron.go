// A cron expression of five fields as the usual Go schedulers read and
// search it (bit sets per field, Vixie's OR of the day fields, the next
// time found by stepping a time.Time of the location field by field with
// time.Date), for the time module's cron cases; Go's standard library has
// no cron. Numbers, ranges, steps, lists and the names of the days.
package main

import (
	"strconv"
	"strings"
	"time"
)

type cronSpec struct {
	minute, hour, dom, month, dow uint64
	domStar, dowStar              bool
}

func cronField(f string, lo, hi int, names map[string]int) (uint64, bool) {
	var bits uint64
	for _, item := range strings.Split(f, ",") {
		step := 1
		if i := strings.IndexByte(item, '/'); i >= 0 {
			s, err := strconv.Atoi(item[i+1:])
			if err != nil || s <= 0 {
				return 0, false
			}
			step = s
			item = item[:i]
		}
		a, b := lo, hi
		if item != "*" {
			parts := strings.SplitN(item, "-", 2)
			value := func(s string) (int, bool) {
				if v, ok := names[strings.ToLower(s)]; ok {
					return v, true
				}
				v, err := strconv.Atoi(s)
				return v, err == nil
			}
			var ok bool
			if a, ok = value(parts[0]); !ok {
				return 0, false
			}
			b = a
			if len(parts) == 2 {
				if b, ok = value(parts[1]); !ok {
					return 0, false
				}
			} else if step > 1 {
				b = hi
			}
		}
		if a < lo || b > hi || b < a {
			return 0, false
		}
		for v := a; v <= b; v += step {
			bits |= 1 << uint(v)
		}
	}
	return bits, true
}

var cronDays = map[string]int{"sun": 0, "mon": 1, "tue": 2, "wed": 3, "thu": 4, "fri": 5, "sat": 6}

func parseCron(s string) (cronSpec, bool) {
	f := strings.Fields(s)
	if len(f) != 5 {
		return cronSpec{}, false
	}
	var c cronSpec
	var ok bool
	if c.minute, ok = cronField(f[0], 0, 59, nil); !ok {
		return c, false
	}
	if c.hour, ok = cronField(f[1], 0, 23, nil); !ok {
		return c, false
	}
	if c.dom, ok = cronField(f[2], 1, 31, nil); !ok {
		return c, false
	}
	if c.month, ok = cronField(f[3], 1, 12, nil); !ok {
		return c, false
	}
	if c.dow, ok = cronField(f[4], 0, 7, cronDays); !ok {
		return c, false
	}
	if c.dow&(1<<7) != 0 {
		c.dow |= 1
	}
	c.domStar = f[2][0] == '*'
	c.dowStar = f[4][0] == '*'
	return c, true
}

func (c *cronSpec) dayMatches(t time.Time) bool {
	dom := c.dom&(1<<uint(t.Day())) != 0
	dow := c.dow&(1<<uint(t.Weekday())) != 0
	switch {
	case c.domStar && c.dowStar:
		return true
	case c.domStar:
		return dow
	case c.dowStar:
		return dom
	}
	return dom || dow
}

// The first time after t, stepping the fields in t's location
func (c *cronSpec) next(t time.Time) time.Time {
	loc := t.Location()
	t = t.Add(time.Minute - time.Duration(t.Second())*time.Second - time.Duration(t.Nanosecond()))
	limit := t.Year() + 5
	for t.Year() <= limit {
		if c.month&(1<<uint(t.Month())) == 0 {
			t = time.Date(t.Year(), t.Month()+1, 1, 0, 0, 0, 0, loc)
			continue
		}
		if !c.dayMatches(t) {
			t = time.Date(t.Year(), t.Month(), t.Day()+1, 0, 0, 0, 0, loc)
			continue
		}
		if c.hour&(1<<uint(t.Hour())) == 0 {
			t = time.Date(t.Year(), t.Month(), t.Day(), t.Hour()+1, 0, 0, 0, loc)
			continue
		}
		if c.minute&(1<<uint(t.Minute())) == 0 {
			t = t.Add(time.Minute)
			continue
		}
		return t
	}
	return time.Time{}
}
