// The Java counterpart of benchmarks/concurrent.cpp: the lock-free
// containers of java.util.concurrent that SGCL's are modelled on,
// ConcurrentLinkedQueue (Michael–Scott), ConcurrentLinkedDeque at one end
// (a stack) and ConcurrentSkipListMap, holding Item objects of one long.
//   java Concurrent <queue|stack> [threads=4] [mode=mixed] [n=200000]
//   java Concurrent map [threads=4] [keys=200000] [n=200000]
// queue, stack: mixed, every thread pushes an item and pops one, n times
// over; pairs, half the threads push n each, the other half pop n each.
// map: insert, the threads insert `keys` disjoint keys; find, every thread
// looks up n random keys of those; mixed, every thread does n operations
// over twice the range, 80% lookups, 10% insertions, 10% erasures. The
// keys are Long: boxed, as a Java map's are.
import java.util.SplittableRandom;
import java.util.concurrent.ConcurrentLinkedDeque;
import java.util.concurrent.ConcurrentLinkedQueue;
import java.util.concurrent.ConcurrentSkipListMap;

public final class Concurrent {
    static final class Item { final long value; Item(long v) { value = v; } }

    interface Container { void push(long v); long pop(); }

    static final class Queue implements Container {
        final ConcurrentLinkedQueue<Item> q = new ConcurrentLinkedQueue<>();
        public void push(long v) { q.add(new Item(v)); }
        public long pop() { Item i = q.poll(); return i == null ? -1 : i.value; }
    }

    static final class Stack implements Container {
        final ConcurrentLinkedDeque<Item> d = new ConcurrentLinkedDeque<>();
        public void push(long v) { d.push(new Item(v)); }
        public long pop() { Item i = d.poll(); return i == null ? -1 : i.value; }
    }

    static void runContainer(String what, Container c, int threads, String mode, long n) throws Exception {
        boolean pairs = mode.equals("pairs");
        Thread[] ws = new Thread[threads];
        long t0 = System.nanoTime();
        for (int t = 0; t < threads; ++t) {
            final int id = t;
            ws[t] = new Thread(() -> {
                long sum = 0;
                if (!pairs) { for (long i = 0; i < n; ++i) { c.push(i); sum += c.pop(); } }
                else if (id % 2 == 0) { for (long i = 0; i < n; ++i) c.push(i); }
                else { for (long i = 0; i < n;) { long v = c.pop(); if (v >= 0) { sum += v; ++i; } else Thread.yield(); } }
                if (sum == -1) System.out.print("?");
            });
            ws[t].start();
        }
        for (Thread w : ws) w.join();
        double wall = (System.nanoTime() - t0) / 1e9;
        double ops = (double) n * threads * (pairs ? 1 : 2);
        System.out.printf("%s threads=%d mode=%s ns/op=%.1f ops/s=%.0f wall=%.2fs cpu=%.2fs%n", what, threads, mode, wall * 1e9 / ops, ops / wall, wall, Common.cpuSeconds());
    }

    interface Body { void run(int t); }

    static double phase(int threads, Body body) throws Exception {
        Thread[] ws = new Thread[threads];
        long t0 = System.nanoTime();
        for (int t = 0; t < threads; ++t) {
            final int id = t;
            ws[t] = new Thread(() -> body.run(id));
            ws[t].start();
        }
        for (Thread w : ws) w.join();
        return (System.nanoTime() - t0) / 1e9;
    }

    static void runMap(int threads, long keys, long n) throws Exception {
        final ConcurrentSkipListMap<Long, Item> m = new ConcurrentSkipListMap<>();
        double insert = phase(threads, t -> {
            for (long k = t; k < keys; k += threads) m.putIfAbsent(k, new Item(k));
        });
        double find = phase(threads, t -> {
            SplittableRandom rng = new SplittableRandom(1234 + t);
            long sum = 0;
            for (long i = 0; i < n; ++i) { Item it = m.get(rng.nextLong(keys)); sum += it == null ? -1 : it.value; }
            if (sum == -1) System.out.print("?");
        });
        double mixed = phase(threads, t -> {
            SplittableRandom rng = new SplittableRandom(4321 + t);
            long sum = 0;
            for (long i = 0; i < n; ++i) {
                long r = rng.nextLong();
                long k = Long.remainderUnsigned(r >>> 8, 2 * keys);
                long op = r & 0xFF;
                if (op < 205) { Item it = m.get(k); sum += it == null ? -1 : it.value; }
                else if (op < 230) { sum += m.putIfAbsent(k, new Item(k)) == null ? 1 : 0; }
                else { sum += m.remove(k) != null ? 1 : 0; }
            }
            if (sum == -1) System.out.print("?");
        });
        double ops = (double) n * threads;
        System.out.printf("map threads=%d keys=%d insert=%.1f find=%.1f mixed=%.1f wall=%.2fs cpu=%.2fs%n", threads, keys, insert * 1e9 / keys, find * 1e9 / ops, mixed * 1e9 / ops, insert + find + mixed, Common.cpuSeconds());
    }

    public static void main(String[] args) throws Exception {
        String what = args.length > 0 ? args[0] : "queue";
        int threads = args.length > 1 ? Integer.parseInt(args[1]) : 4;
        if (what.equals("map")) {
            long keys = args.length > 2 ? Long.parseLong(args[2]) : 200_000L;
            long n = args.length > 3 ? Long.parseLong(args[3]) : 200_000L;
            runMap(threads, keys, n);
            return;
        }
        String mode = args.length > 2 ? args[2] : "mixed";
        long n = args.length > 3 ? Long.parseLong(args[3]) : 200_000L;
        runContainer(what, what.equals("stack") ? new Stack() : new Queue(), threads, mode, n);
    }
}
