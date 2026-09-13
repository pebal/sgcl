// A Treiber stack, the shape of benchmarks/lockfree_stack.cpp: the head an
// AtomicReference, the collector taking care of ABA. mixed: every thread
// pushes a node and pops one, n times over; pairs: half push n each, the
// other half pop n each.
//   java LockFreeStack [threads=4] [mode=mixed] [n=1000000]
import java.util.concurrent.atomic.AtomicReference;

public final class LockFreeStack {
    static final class Node { Node next; long value; }
    static final AtomicReference<Node> head = new AtomicReference<>();

    static void push(long v) {
        Node n = new Node(); n.value = v;
        for (;;) { Node h = head.get(); n.next = h; if (head.compareAndSet(h, n)) return; }
    }

    static long pop() {
        for (;;) { Node h = head.get(); if (h == null) return -1; if (head.compareAndSet(h, h.next)) return h.value; }
    }

    public static void main(String[] args) throws Exception {
        int threads = args.length > 0 ? Integer.parseInt(args[0]) : 4;
        String mode = args.length > 1 ? args[1] : "mixed";
        long n = args.length > 2 ? Long.parseLong(args[2]) : 1_000_000L;
        boolean pairs = mode.equals("pairs");
        Thread[] ws = new Thread[threads];
        long t0 = System.nanoTime();
        for (int t = 0; t < threads; ++t) {
            final int id = t;
            ws[t] = new Thread(() -> {
                long sum = 0;
                if (!pairs) { for (long i = 0; i < n; ++i) { push(i); sum += pop(); } }
                else if (id % 2 == 0) { for (long i = 0; i < n; ++i) push(i); }
                else { for (long i = 0; i < n;) { long v = pop(); if (v >= 0) { sum += v; ++i; } else Thread.yield(); } }
                if (sum == -1) System.out.print("?");
            });
            ws[t].start();
        }
        for (Thread w : ws) w.join();
        double wall = (System.nanoTime() - t0) / 1e9;
        double ops = (double) n * threads * (pairs ? 1 : 2);
        System.out.printf("threads=%d mode=%s ns/op=%.1f ops/s=%.0f wall=%.2fs cpu=%.2fs%n", threads, mode, wall * 1e9 / ops, ops / wall, wall, Common.cpuSeconds());
    }
}
