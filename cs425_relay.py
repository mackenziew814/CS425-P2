#!/usr/bin/env python3
"""
CS 425 relay – non-blocking delay implementation.
Uses a min-heap of (deliver_at, data, dest) so all in-flight packets
are pipelined correctly: a 50 ms delay on each packet does not serialize them.
"""
import socket, random, time, heapq, argparse, sys

def parse_args():
    p = argparse.ArgumentParser()
    p.add_argument('--port',  type=int,   default=4250)
    p.add_argument('--delay', type=int,   default=0)
    p.add_argument('--loss',  type=float, default=0.0)
    p.add_argument('--seed',  type=int,   default=None)
    p.add_argument('-v',      action='store_true')
    return p.parse_args()

def main():
    args = parse_args()
    if args.seed is not None:
        random.seed(args.seed)

    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    sock.settimeout(0.001)   # 1 ms poll so heap drains promptly
    try:
        sock.bind(('127.0.0.1', args.port))
    except OSError as e:
        print(f"relay: bind failed: {e}", file=sys.stderr)
        sys.exit(1)

    print(f"relay listening on UDP port {args.port}", flush=True)

    sessions = {}   # name → {recv, send, loss, corrupt, dup}
    counts   = {'sent': 0, 'dropped': 0, 'corrupted': 0, 'duped': 0}

    # Min-heap of (deliver_at_seconds, seq_counter, data_bytes, dest_addr)
    # seq_counter breaks ties so heapq never compares bytes objects.
    # The relay is single-threaded so no lock is needed.
    heap = []
    hseq = 0

    def enqueue(data, dest, loss, corrupt, dup):
        nonlocal hseq
        if random.random() < loss:
            counts['dropped'] += 1
            return
        pkt = bytearray(data)
        if random.random() < corrupt:
            i = random.randrange(len(pkt))
            b = random.randrange(8)
            pkt[i] ^= (1 << b)
            counts['corrupted'] += 1
        pkt = bytes(pkt)

        if args.delay == 0:
            # Fast path: no delay, deliver immediately without touching the heap.
            try:
                sock.sendto(pkt, dest)
                counts['sent'] += 1
            except OSError:
                pass
            if random.random() < dup:
                counts['duped'] += 1
                try:
                    sock.sendto(pkt, dest)
                    counts['sent'] += 1
                except OSError:
                    pass
            return

        # Delayed path: push original and (maybe) dup into heap atomically
        # before any drain can run, so both entries share the same deadline
        # and neither can be delivered without the other being queued.
        deliver_at = time.monotonic() + args.delay / 1000.0
        heapq.heappush(heap, (deliver_at, hseq, pkt, dest))
        hseq += 1
        if random.random() < dup:
            counts['duped'] += 1
            heapq.heappush(heap, (deliver_at, hseq, pkt, dest))
            hseq += 1

    def drain_heap():
        now = time.monotonic()
        while heap and heap[0][0] <= now:
            _, _, data, dest = heapq.heappop(heap)
            try:
                sock.sendto(data, dest)
                counts['sent'] += 1
            except OSError:
                pass

    try:
        while True:
            drain_heap()
            try:
                data, addr = sock.recvfrom(2048)
            except socket.timeout:
                continue
            except OSError:
                continue

            text = data.decode('ascii', errors='replace').strip()
            parts = text.split()

            if parts and parts[0] == 'HELLO':
                if len(parts) < 3:
                    sock.sendto(b'ERR malformed hello', addr)
                    continue
                name, role = parts[1], parts[2]
                if role == 'recv':
                    sessions.setdefault(name, {})
                    sessions[name]['recv'] = addr
                    sessions[name].update({'loss': 0, 'corrupt': 0, 'dup': 0})
                    sock.sendto(b'OK', addr)
                    if args.v: print(f"  recv: {name} {addr}")
                elif role == 'send':
                    if name not in sessions or 'recv' not in sessions[name]:
                        sock.sendto(b'ERR no receiver', addr)
                        continue
                    try:
                        loss    = float(parts[3]) if len(parts) > 3 else 0
                        corrupt = float(parts[4]) if len(parts) > 4 else 0
                        dup     = float(parts[5]) if len(parts) > 5 else 0
                    except (ValueError, IndexError):
                        sock.sendto(b'ERR bad rates', addr)
                        continue
                    sessions[name].update({'send': addr, 'loss': loss,
                                           'corrupt': corrupt, 'dup': dup})
                    sock.sendto(b'OK', addr)
                    if args.v: print(f"  send: {name} {addr}")
                else:
                    sock.sendto(b'ERR unknown role', addr)
                continue

            # Forward: find session and direction
            matched = None
            for name, sess in sessions.items():
                if addr == sess.get('send'):
                    matched = ('send', name); break
                if addr == sess.get('recv'):
                    matched = ('recv', name); break
            if matched is None:
                continue
            role, name = matched
            sess = sessions[name]
            dest = sess.get('recv') if role == 'send' else sess.get('send')
            if dest is None:
                continue
            enqueue(data, dest, sess['loss'], sess['corrupt'], sess['dup'])

    except KeyboardInterrupt:
        pass
    finally:
        print(f"\nrelay: sent={counts['sent']} dropped={counts['dropped']} "
              f"corrupted={counts['corrupted']} duped={counts['duped']}")
        sock.close()

if __name__ == '__main__':
    main()