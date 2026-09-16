// The Java counterpart of benchmarks/concurrent.cpp: the lock-free
// containers of java.util.concurrent that SGCL's are modelled on,
// ConcurrentLinkedQueue (Michael–Scott), ConcurrentLinkedDeque at one end
// (a stack) and ConcurrentSkipListMap, holding Item objects of one long.
//   java Concurrent <queue|stack> [threads=4] [mode=mixed] [n=200000]
//   java Concurrent <map|umap|set> [threads=4] [keys=200000] [n=200000]
//   java Concurrent cow [threads=16] [n=2000000]
//   java Concurrent chan [threads=4] [capacity=64] [n=200000]
// queue, stack: mixed, every thread pushes an item and pops one, n times
// over; pairs, half the threads push n each, the other half pop n each.
// map: insert, the threads insert `keys` disjoint keys; find, every thread
// looks up n random keys of those; mixed, every thread does n operations
// over twice the range, 80% lookups, 10% insertions, 10% erasures. The
// keys are Long: boxed, as a Java map's are. map is ConcurrentSkipListMap,
// umap ConcurrentHashMap, set ConcurrentSkipListSet. cow is a
// CopyOnWriteArrayList of 64 Longs: threads - 1 readers sum it n times
// each through its snapshot iterator, one writer sets an element as fast
// as it can meanwhile (a copy of the array under the list's lock). chan
// is a BlockingQueue of Items, ArrayBlockingQueue of the capacity or
// SynchronousQueue for 0, between threads / 2 producers of n items each
// and threads / 2 consumers, a null Item as the end.
import java.util.SplittableRandom;
import java.util.concurrent.ConcurrentLinkedDeque;
import java.util.concurrent.ConcurrentLinkedQueue;
import java.util.concurrent.ConcurrentHashMap;
import java.util.concurrent.ConcurrentSkipListMap;
import java.util.concurrent.ConcurrentSkipListSet;
import java.util.concurrent.CopyOnWriteArrayList;
import java.util.concurrent.atomic.AtomicBoolean;
import java.util.concurrent.ArrayBlockingQueue;
import java.util.concurrent.BlockingQueue;
import java.util.concurrent.SynchronousQueue;

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

    interface Keyed { boolean insert(long k); long find(long k); boolean erase(long k); }

    static final class SkipMap implements Keyed {
        final ConcurrentSkipListMap<Long, Item> m = new ConcurrentSkipListMap<>();
        public boolean insert(long k) { return m.putIfAbsent(k, new Item(k)) == null; }
        public long find(long k) { Item it = m.get(k); return it == null ? -1 : it.value; }
        public boolean erase(long k) { return m.remove(k) != null; }
    }

    static final class HashMap implements Keyed {
        final ConcurrentHashMap<Long, Item> m = new ConcurrentHashMap<>();
        public boolean insert(long k) { return m.putIfAbsent(k, new Item(k)) == null; }
        public long find(long k) { Item it = m.get(k); return it == null ? -1 : it.value; }
        public boolean erase(long k) { return m.remove(k) != null; }
    }

    static final class SkipSet implements Keyed {
        final ConcurrentSkipListSet<Long> s = new ConcurrentSkipListSet<>();
        public boolean insert(long k) { return s.add(k); }
        public long find(long k) { return s.contains(k) ? k : -1; }
        public boolean erase(long k) { return s.remove(k); }
    }

    static void runMap(String what, Keyed m, int threads, long keys, long n) throws Exception {
        double insert = phase(threads, t -> {
            for (long k = t; k < keys; k += threads) m.insert(k);
        });
        double find = phase(threads, t -> {
            SplittableRandom rng = new SplittableRandom(1234 + t);
            long sum = 0;
            for (long i = 0; i < n; ++i) sum += m.find(rng.nextLong(keys));
            if (sum == -1) System.out.print("?");
        });
        double mixed = phase(threads, t -> {
            SplittableRandom rng = new SplittableRandom(4321 + t);
            long sum = 0;
            for (long i = 0; i < n; ++i) {
                long r = rng.nextLong();
                long k = Long.remainderUnsigned(r >>> 8, 2 * keys);
                long op = r & 0xFF;
                if (op < 205) sum += m.find(k);
                else if (op < 230) sum += m.insert(k) ? 1 : 0;
                else sum += m.erase(k) ? 1 : 0;
            }
            if (sum == -1) System.out.print("?");
        });
        double ops = (double) n * threads;
        System.out.printf("%s threads=%d keys=%d insert=%.1f find=%.1f mixed=%.1f wall=%.2fs cpu=%.2fs%n", what, threads, keys, insert * 1e9 / keys, find * 1e9 / ops, mixed * 1e9 / ops, insert + find + mixed, Common.cpuSeconds());
    }

    static void runCow(int threads, long n) throws Exception {
        final CopyOnWriteArrayList<Long> v = new CopyOnWriteArrayList<>();
        for (int i = 0; i < 64; ++i) v.add(0L);
        final AtomicBoolean stop = new AtomicBoolean();
        final long[] writes = new long[1];
        final double[] writeTime = new double[1];
        Thread[] ws = new Thread[threads - 1];
        long t0 = System.nanoTime();
        for (int t = 0; t < threads - 1; ++t) {
            ws[t] = new Thread(() -> {
                long sum = 0;
                for (long i = 0; i < n; ++i) { for (long x : v) sum += x; }
                if (sum == -1) System.out.print("?");
            });
            ws[t].start();
        }
        Thread writer = new Thread(() -> {
            long w0 = System.nanoTime();
            long i = 0;
            while (!stop.get()) { int k = (int) (i % 64); v.set(k, (v.get(k) + 1) % 100); ++i; }
            writes[0] = i;
            writeTime[0] = (System.nanoTime() - w0) / 1e9;
        });
        writer.start();
        for (Thread w : ws) w.join();
        stop.set(true);
        writer.join();
        double wall = (System.nanoTime() - t0) / 1e9;
        double reads = (double) n * (threads - 1);
        System.out.printf("cow threads=%d ns/read=%.1f ns/write=%.1f writes=%d reads/s=%.0f wall=%.2fs cpu=%.2fs%n", threads, wall * 1e9 / reads, writes[0] > 0 ? writeTime[0] * 1e9 / writes[0] : 0.0, writes[0], reads / wall, wall, Common.cpuSeconds());
    }

    static final Item END = new Item(-1);

    static void runChan(int threads, int capacity, long n) throws Exception {
        final BlockingQueue<Item> q = capacity > 0 ? new ArrayBlockingQueue<>(capacity) : new SynchronousQueue<>();
        int producers = Math.max(1, threads / 2), consumers = Math.max(1, threads / 2);
        Thread[] ps = new Thread[producers], cs = new Thread[consumers];
        long t0 = System.nanoTime();
        for (int t = 0; t < producers; ++t) {
            ps[t] = new Thread(() -> {
                try { for (long i = 0; i < n; ++i) q.put(new Item(i)); } catch (InterruptedException e) { throw new RuntimeException(e); }
            });
            ps[t].start();
        }
        for (int t = 0; t < consumers; ++t) {
            cs[t] = new Thread(() -> {
                long sum = 0;
                try { for (;;) { Item it = q.take(); if (it == END) break; sum += it.value; } } catch (InterruptedException e) { throw new RuntimeException(e); }
                if (sum == -1) System.out.print("?");
            });
            cs[t].start();
        }
        for (Thread p : ps) p.join();
        for (int t = 0; t < consumers; ++t) q.put(END);
        for (Thread c : cs) c.join();
        double wall = (System.nanoTime() - t0) / 1e9;
        double ops = (double) n * producers;
        System.out.printf("chan threads=%d capacity=%d ns/op=%.1f ops/s=%.0f wall=%.2fs cpu=%.2fs%n", threads, capacity, wall * 1e9 / ops, ops / wall, wall, Common.cpuSeconds());
    }

    public static void main(String[] args) throws Exception {
        String what = args.length > 0 ? args[0] : "queue";
        if (what.equals("chan")) {
            runChan(args.length > 1 ? Integer.parseInt(args[1]) : 4, args.length > 2 ? Integer.parseInt(args[2]) : 64, args.length > 3 ? Long.parseLong(args[3]) : 200_000L);
            return;
        }
        if (what.equals("cow")) {
            runCow(args.length > 1 ? Integer.parseInt(args[1]) : 16, args.length > 2 ? Long.parseLong(args[2]) : 2_000_000L);
            return;
        }
        int threads = args.length > 1 ? Integer.parseInt(args[1]) : 4;
        if (what.equals("map") || what.equals("umap") || what.equals("set")) {
            long keys = args.length > 2 ? Long.parseLong(args[2]) : 200_000L;
            long n = args.length > 3 ? Long.parseLong(args[3]) : 200_000L;
            runMap(what, what.equals("map") ? new SkipMap() : what.equals("umap") ? new HashMap() : new SkipSet(), threads, keys, n);
            return;
        }
        String mode = args.length > 2 ? args[2] : "mixed";
        long n = args.length > 3 ? Long.parseLong(args[3]) : 200_000L;
        runContainer(what, what.equals("stack") ? new Stack() : new Queue(), threads, mode, n);
    }
}
