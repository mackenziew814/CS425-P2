# Submission Report

- Submission generated at 10/12/2026 at 03:08:20

- Machine info: Linux runnervmmprz5 6.17.0-1022-azure #22-Ubuntu SMP Mon Jul 27 17:24:03 UTC 2026 x86_64 x86_64 x86_64 GNU/Linux

## Note to Students

Please read this report carefully before submission.
Ensure that all sections are complete and accurate.
Look for any errors in the build or test outputs.
If you find any issues, correct them before submitting.
Post any questions on the class discussion board for help.


---

## README

# Project 2 - Reliable Data Transfer

- Name: Mackenzie Wright
- Email: mackenziewright@u.boisestate.edu
- Class: CS425-001

## Known Bugs or Issues

No known issues or bugs in this code. 

## Experience

To complete this project, I used the Claude CLI. It was extremely helpful and was able to complete a majority of the project without my input/changes. The main area it struggled was with developing the threading for the Go-Back-N functionality. When attempting to test the relay with window 16 becuase it had incorrect functionailty for parallelism. Instead of running at the same time, each process was running individually one after the other and each of the 16 windows still had the 50 ms before and after it's relay. This caused a significant increase in time and resulted in the CLI in becoming paused/constantly running without stopping. It took some investigation, but with its help I was able to find that the problem was with the piping in the Python file, and we were able to resolve the issue. Other than this, there were no significant issues that occurred while completing this project. I enjoy using AI to solve projects like this because it's something I will be doing in my future career, and it provides good experience with collaborating with AI on issues and solving them. It also demonstrated that you can't blindly trust AI's initial solution as it can sometimes be flawed, like with the threading issue. 

## Design 
The protocol logic is split into three layers to keep the parts that are hard to test separate from the parts that need to be tested in-depth. The first layer is split into the packet functionality. It's responsible for computing a checksum, encodind a packet into a buffer, and decoding and validating a buffer into a packet.These are pure functions that deal with byte-handling, so they're easy to create unit tests for. The next section handles Go-Back-N state machines which require more complex tests. These states consist of sender and reciever structs and the functions that feed its events. They accept events and return an action struct describing what the caller should do next, and my tests allow any clock (in milliseconds) to be used. The third and final section pertains to the I/O. This section should be a thin wrapper and do should read in an event, hand it to the state machine, and carry out what returns. This is tested through the relay rather than through unit tests. As you can see each section requires a different style of testing, which is why they are split into these groups. This is also what makes 100% line coverage of the protocol logic achievable.

## Results

All runs transfer a 1 MiB file (`head -c 1048576 /dev/urandom > 1mib.bin`)
through the relay started with `--delay 50` (100 ms simulated round trip).
Sender uses the default 250 ms retransmission timeout. Each combination
was run three times; every run verified with `cmp`.

| Window | Loss | Mean time (ms) | Throughput (KiB/s) |
|-------:|-----:|---------------:|-------------------:|
|      1 | 0    |        104,077 |                9.8 |
|     16 | 0    |          6,620 |              154.7 |
|      1 | 0.05 |        133,360 |                7.7 |
|     16 | 0.05 |         27,030 |               37.9 |

### Analysis

**Measured round-trip time.**
The window-1, no-loss run transferred 1024 DATA packets plus one FIN —
1025 packets total — and the sender waited one full round trip for each
before sending the next. The mean transfer time was 104,077 ms, giving a
measured RTT of 104,077 / 1025 ≈ **101.5 ms**. The relay contributes
100 ms (50 ms each way). The extra 1.5 ms is the real overhead, or the
time for the sender to call `sendto`, for the loopback stack to deliver
the datagram to the relay process. This is the actual time it took to travel
without the added relay.

**Window-16 speedup.**
With window 16 the sender keeps up to 16 packets in flight simultaneously, so one round trip of 101.5 ms covers 16 packets instead of 1. The expected transfer time is therefore 104,077 / 16 ≈ 6,505 ms, a speedup of 15.7× and the measured mean is 6,620 ms. This speedup is close to 16× because the pipeline stays nearly full for the vast majority of the transfer — each ACK that slides the window immediately allows a new packet to enter flight, so the link is utilized at close to 16 packets per RTT throughout. The small gap between the theoretical 16× and the measured 15.7× happens because the final partial window cannot be filled to capacity, and the two HELLO registration handshakes at the start of each session add fixed overhead that cannot be parallelized regardless of window size.

**Why 5% loss hurts window 16 so much more than window 1.**
With window 1, a lost packet costs exactly one extra RTT The timer fires
after 250 ms and the sender retransmits just that one packet. With window
16, Go-Back-N functionality retransmits the entire window on a timeout,
even though only one was lost. The receiver discards the 15
good packets that followed the gap and re-ACKs the same sequence number,
forcing all 15 to be sent again. At 5% loss the number of loss events is
similar in both cases, but each event wastes up to 16 transmissions
instead of 1 in the window-16 case, plus the 250 ms timeout penalty
applies each time. This is the fundamental cost of Go-Back-N's simple
receiver.


---


## Build Output

This section was generated by running `make all` in the project root directory.

```bash
make[1]: Entering directory '/home/runner/work/CS425-P2/CS425-P2'
mkdir -p build/debug
cc -g -O0 -DDEBUG -fno-omit-frame-pointer -fsanitize=address -c src/lab.c -o build/debug/lab.c.o
mkdir -p build/debug
cc -g -O0 -DDEBUG -fno-omit-frame-pointer -fsanitize=address -c src/main.c -o build/debug/main.c.o
cc -g -O0 -DDEBUG -fno-omit-frame-pointer -fsanitize=address build/debug/lab.c.o build/debug/main.c.o -o build/debug/myapp_d -fsanitize=address
make[1]: Leaving directory '/home/runner/work/CS425-P2/CS425-P2'
make[1]: Entering directory '/home/runner/work/CS425-P2/CS425-P2'
mkdir -p build/release
cc -Wall -Wextra -O2 -fPIE -MMD -MP -Wformat -Wformat=2 -Wconversion -Wsign-conversion -Wimplicit-fallthrough -fstack-protector-strong -Werror=format-security -Werror=implicit -Werror=incompatible-pointer-types -Werror=int-conversion -c src/lab.c -o build/release/lab.c.o
mkdir -p build/release
cc -Wall -Wextra -O2 -fPIE -MMD -MP -Wformat -Wformat=2 -Wconversion -Wsign-conversion -Wimplicit-fallthrough -fstack-protector-strong -Werror=format-security -Werror=implicit -Werror=incompatible-pointer-types -Werror=int-conversion -c src/main.c -o build/release/main.c.o
cc -Wall -Wextra -O2 -fPIE -MMD -MP -Wformat -Wformat=2 -Wconversion -Wsign-conversion -Wimplicit-fallthrough -fstack-protector-strong -Werror=format-security -Werror=implicit -Werror=incompatible-pointer-types -Werror=int-conversion build/release/lab.c.o build/release/main.c.o -o build/release/myapp 
make[1]: Leaving directory '/home/runner/work/CS425-P2/CS425-P2'
make[1]: Entering directory '/home/runner/work/CS425-P2/CS425-P2'
mkdir -p build/tests
cc -g -O0 -DTEST -fprofile-arcs -ftest-coverage -c src/lab.c -o build/tests/lab.c.o
mkdir -p build/tests
cc -g -O0 -DTEST -fprofile-arcs -ftest-coverage -c src/main.c -o build/tests/main.c.o
mkdir -p build/tests/
cc -g -O0 -DTEST -fprofile-arcs -ftest-coverage -c tests/lab-test.c -o build/tests/lab-test.c.o
mkdir -p build/tests/harness/
cc -g -O0 -DTEST -fprofile-arcs -ftest-coverage -c tests/harness/unity.c -o build/tests/harness/unity.c.o
cc -g -O0 -DTEST -fprofile-arcs -ftest-coverage build/tests/lab.c.o build/tests/main.c.o build/tests/lab-test.c.o build/tests/harness/unity.c.o -o build/tests/myapp_t -fprofile-arcs -ftest-coverage
make[1]: Leaving directory '/home/runner/work/CS425-P2/CS425-P2'
make[1]: Entering directory '/home/runner/work/CS425-P2/CS425-P2'
mkdir -p build/debug-test
cc -g -O0 -DDEBUG -DTEST -fno-omit-frame-pointer -fsanitize=address -c src/lab.c -o build/debug-test/lab.c.o
mkdir -p build/debug-test
cc -g -O0 -DDEBUG -DTEST -fno-omit-frame-pointer -fsanitize=address -c src/main.c -o build/debug-test/main.c.o
mkdir -p build/debug-test/
cc -g -O0 -DDEBUG -DTEST -fno-omit-frame-pointer -fsanitize=address -c tests/lab-test.c -o build/debug-test/lab-test.c.o
mkdir -p build/debug-test/harness/
cc -g -O0 -DDEBUG -DTEST -fno-omit-frame-pointer -fsanitize=address -c tests/harness/unity.c -o build/debug-test/harness/unity.c.o
cc -g -O0 -DDEBUG -DTEST -fno-omit-frame-pointer -fsanitize=address build/debug-test/lab.c.o build/debug-test/main.c.o build/debug-test/lab-test.c.o build/debug-test/harness/unity.c.o -o build/debug-test/myapp_td -fsanitize=address
make[1]: Leaving directory '/home/runner/work/CS425-P2/CS425-P2'
Builds completed. You can run the application with: ./build/release/myapp
You can run the debug build with: ./build/debug/myapp_d
You can run the test build with: ./build/tests/myapp_t
You can run the debug-test build with: ./build/debug-test/myapp_td
```

---

## Coverage Report

This section was generated by running `make report` in the project root directory.

```bash
tests/lab-test.c:1217:test_checksum_rfc1071_example:PASS
tests/lab-test.c:1218:test_checksum_verify_roundtrip:PASS
tests/lab-test.c:1219:test_checksum_odd_length:PASS
tests/lab-test.c:1220:test_checksum_odd_three_bytes:PASS
tests/lab-test.c:1221:test_checksum_data_hi:PASS
tests/lab-test.c:1222:test_checksum_ack3_verify:PASS
tests/lab-test.c:1223:test_checksum_detects_single_bit_flip:PASS
tests/lab-test.c:1226:test_encode_data_length:PASS
tests/lab-test.c:1227:test_encode_data_fields:PASS
tests/lab-test.c:1228:test_encode_data_max_payload:PASS
tests/lab-test.c:1229:test_encode_ack_length_and_fields:PASS
tests/lab-test.c:1230:test_encode_fin_length_and_fields:PASS
tests/lab-test.c:1231:test_encode_seq_boundary_values:PASS
tests/lab-test.c:1232:test_encode_checksum_embeds_correctly:PASS
tests/lab-test.c:1235:test_decode_valid_data:PASS
tests/lab-test.c:1236:test_decode_valid_ack:PASS
tests/lab-test.c:1237:test_decode_valid_fin:PASS
tests/lab-test.c:1238:test_decode_rejects_too_short:PASS
tests/lab-test.c:1239:test_decode_rejects_length_mismatch_short:PASS
tests/lab-test.c:1240:test_decode_rejects_length_mismatch_long:PASS
tests/lab-test.c:1241:test_decode_rejects_length_over_max:PASS
tests/lab-test.c:1242:test_decode_rejects_unknown_type:PASS
tests/lab-test.c:1243:test_decode_rejects_nonzero_reserved:PASS
tests/lab-test.c:1244:test_decode_rejects_corrupted_payload:PASS
tests/lab-test.c:1245:test_decode_rejects_corrupted_header:PASS
tests/lab-test.c:1248:test_sender_init_zeroes_state:PASS
tests/lab-test.c:1249:test_sender_window_not_full_initially:PASS
tests/lab-test.c:1250:test_sender_window_full_at_capacity:PASS
tests/lab-test.c:1253:test_sender_data_sends_one_packet:PASS
tests/lab-test.c:1254:test_sender_data_packet_decodes_correctly:PASS
tests/lab-test.c:1255:test_sender_data_ignored_when_window_full:PASS
tests/lab-test.c:1256:test_sender_data_timer_starts_on_first:PASS
tests/lab-test.c:1259:test_sender_eof_empty_file:PASS
tests/lab-test.c:1260:test_sender_eof_deferred_when_window_full:PASS
tests/lab-test.c:1261:test_sender_eof_idempotent:PASS
tests/lab-test.c:1262:test_sender_exact_multiple_of_1024:PASS
tests/lab-test.c:1265:test_sender_ack_duplicate_ignored:PASS
tests/lab-test.c:1266:test_sender_ack_cumulative_slide:PASS
tests/lab-test.c:1267:test_sender_ack_drains_window_stops_timer:PASS
tests/lab-test.c:1268:test_sender_ack_resets_timeout_counter:PASS
tests/lab-test.c:1269:test_sender_ack_fin_sets_done:PASS
tests/lab-test.c:1272:test_sender_timeout_retransmits_window:PASS
tests/lab-test.c:1273:test_sender_timeout_includes_fin:PASS
tests/lab-test.c:1274:test_sender_timeout_give_up:PASS
tests/lab-test.c:1277:test_recv_init_zeroes_state:PASS
tests/lab-test.c:1278:test_recv_inorder_data:PASS
tests/lab-test.c:1279:test_recv_duplicate_packet:PASS
tests/lab-test.c:1280:test_recv_beyond_gap:PASS
tests/lab-test.c:1281:test_recv_fin_sets_done:PASS
tests/lab-test.c:1282:test_recv_repeated_fin:PASS
tests/lab-test.c:1283:test_recv_ignores_ack_packets:PASS
tests/lab-test.c:1286:test_channel_small_file_seed42:PASS
tests/lab-test.c:1287:test_channel_small_file_seed1234:PASS
tests/lab-test.c:1288:test_channel_exact_1024_multiple:PASS
tests/lab-test.c:1289:test_channel_empty_file:PASS
tests/lab-test.c:1290:test_channel_large_file_window1:PASS
tests/lab-test.c:1291:test_channel_large_file_window16:PASS
tests/lab-test.c:1292:test_channel_seed_999:PASS

-----------------------
58 Tests 0 Failures 0 Ignored 
OK
./build/tests/myapp_t
tests/lab-test.c:1217:test_checksum_rfc1071_example:PASS
tests/lab-test.c:1218:test_checksum_verify_roundtrip:PASS
tests/lab-test.c:1219:test_checksum_odd_length:PASS
tests/lab-test.c:1220:test_checksum_odd_three_bytes:PASS
tests/lab-test.c:1221:test_checksum_data_hi:PASS
tests/lab-test.c:1222:test_checksum_ack3_verify:PASS
tests/lab-test.c:1223:test_checksum_detects_single_bit_flip:PASS
tests/lab-test.c:1226:test_encode_data_length:PASS
tests/lab-test.c:1227:test_encode_data_fields:PASS
tests/lab-test.c:1228:test_encode_data_max_payload:PASS
tests/lab-test.c:1229:test_encode_ack_length_and_fields:PASS
tests/lab-test.c:1230:test_encode_fin_length_and_fields:PASS
tests/lab-test.c:1231:test_encode_seq_boundary_values:PASS
tests/lab-test.c:1232:test_encode_checksum_embeds_correctly:PASS
tests/lab-test.c:1235:test_decode_valid_data:PASS
tests/lab-test.c:1236:test_decode_valid_ack:PASS
tests/lab-test.c:1237:test_decode_valid_fin:PASS
tests/lab-test.c:1238:test_decode_rejects_too_short:PASS
tests/lab-test.c:1239:test_decode_rejects_length_mismatch_short:PASS
tests/lab-test.c:1240:test_decode_rejects_length_mismatch_long:PASS
tests/lab-test.c:1241:test_decode_rejects_length_over_max:PASS
tests/lab-test.c:1242:test_decode_rejects_unknown_type:PASS
tests/lab-test.c:1243:test_decode_rejects_nonzero_reserved:PASS
tests/lab-test.c:1244:test_decode_rejects_corrupted_payload:PASS
tests/lab-test.c:1245:test_decode_rejects_corrupted_header:PASS
tests/lab-test.c:1248:test_sender_init_zeroes_state:PASS
tests/lab-test.c:1249:test_sender_window_not_full_initially:PASS
tests/lab-test.c:1250:test_sender_window_full_at_capacity:PASS
tests/lab-test.c:1253:test_sender_data_sends_one_packet:PASS
tests/lab-test.c:1254:test_sender_data_packet_decodes_correctly:PASS
tests/lab-test.c:1255:test_sender_data_ignored_when_window_full:PASS
tests/lab-test.c:1256:test_sender_data_timer_starts_on_first:PASS
tests/lab-test.c:1259:test_sender_eof_empty_file:PASS
tests/lab-test.c:1260:test_sender_eof_deferred_when_window_full:PASS
tests/lab-test.c:1261:test_sender_eof_idempotent:PASS
tests/lab-test.c:1262:test_sender_exact_multiple_of_1024:PASS
tests/lab-test.c:1265:test_sender_ack_duplicate_ignored:PASS
tests/lab-test.c:1266:test_sender_ack_cumulative_slide:PASS
tests/lab-test.c:1267:test_sender_ack_drains_window_stops_timer:PASS
tests/lab-test.c:1268:test_sender_ack_resets_timeout_counter:PASS
tests/lab-test.c:1269:test_sender_ack_fin_sets_done:PASS
tests/lab-test.c:1272:test_sender_timeout_retransmits_window:PASS
tests/lab-test.c:1273:test_sender_timeout_includes_fin:PASS
tests/lab-test.c:1274:test_sender_timeout_give_up:PASS
tests/lab-test.c:1277:test_recv_init_zeroes_state:PASS
tests/lab-test.c:1278:test_recv_inorder_data:PASS
tests/lab-test.c:1279:test_recv_duplicate_packet:PASS
tests/lab-test.c:1280:test_recv_beyond_gap:PASS
tests/lab-test.c:1281:test_recv_fin_sets_done:PASS
tests/lab-test.c:1282:test_recv_repeated_fin:PASS
tests/lab-test.c:1283:test_recv_ignores_ack_packets:PASS
tests/lab-test.c:1286:test_channel_small_file_seed42:PASS
tests/lab-test.c:1287:test_channel_small_file_seed1234:PASS
tests/lab-test.c:1288:test_channel_exact_1024_multiple:PASS
tests/lab-test.c:1289:test_channel_empty_file:PASS
tests/lab-test.c:1290:test_channel_large_file_window1:PASS
tests/lab-test.c:1291:test_channel_large_file_window16:PASS
tests/lab-test.c:1292:test_channel_seed_999:PASS

-----------------------
58 Tests 0 Failures 0 Ignored 
OK
mkdir -p ./build/report/html
mkdir -p ./build/report/txt
gcovr -r . --html --html-details --exclude-directories build/tests/harness --exclude '.*main\.c$' --exclude '.*test\.c$' -o ./build/report/html/coverage_report.html
(INFO) Reading coverage data...

(INFO) Writing coverage report...

gcovr -r . --txt                 --exclude-directories build/tests/harness --exclude '.*main\.c$' --exclude '.*test\.c$'
(INFO) Reading coverage data...

(INFO) Writing coverage report...

------------------------------------------------------------------------------
                           GCC Code Coverage Report
Directory: .
------------------------------------------------------------------------------
File                                       Lines     Exec  Cover   Missing
------------------------------------------------------------------------------
src/lab.c                                    181      181   100%
------------------------------------------------------------------------------
TOTAL                                        181      181   100%
------------------------------------------------------------------------------
```

---

## Address Sanitizer Report

This section was generated by running `make leak-test` in the project root directory.

```bash
tests/lab-test.c:1217:test_checksum_rfc1071_example:PASS
tests/lab-test.c:1218:test_checksum_verify_roundtrip:PASS
tests/lab-test.c:1219:test_checksum_odd_length:PASS
tests/lab-test.c:1220:test_checksum_odd_three_bytes:PASS
tests/lab-test.c:1221:test_checksum_data_hi:PASS
tests/lab-test.c:1222:test_checksum_ack3_verify:PASS
tests/lab-test.c:1223:test_checksum_detects_single_bit_flip:PASS
tests/lab-test.c:1226:test_encode_data_length:PASS
tests/lab-test.c:1227:test_encode_data_fields:PASS
tests/lab-test.c:1228:test_encode_data_max_payload:PASS
tests/lab-test.c:1229:test_encode_ack_length_and_fields:PASS
tests/lab-test.c:1230:test_encode_fin_length_and_fields:PASS
tests/lab-test.c:1231:test_encode_seq_boundary_values:PASS
tests/lab-test.c:1232:test_encode_checksum_embeds_correctly:PASS
tests/lab-test.c:1235:test_decode_valid_data:PASS
tests/lab-test.c:1236:test_decode_valid_ack:PASS
tests/lab-test.c:1237:test_decode_valid_fin:PASS
tests/lab-test.c:1238:test_decode_rejects_too_short:PASS
tests/lab-test.c:1239:test_decode_rejects_length_mismatch_short:PASS
tests/lab-test.c:1240:test_decode_rejects_length_mismatch_long:PASS
tests/lab-test.c:1241:test_decode_rejects_length_over_max:PASS
tests/lab-test.c:1242:test_decode_rejects_unknown_type:PASS
tests/lab-test.c:1243:test_decode_rejects_nonzero_reserved:PASS
tests/lab-test.c:1244:test_decode_rejects_corrupted_payload:PASS
tests/lab-test.c:1245:test_decode_rejects_corrupted_header:PASS
tests/lab-test.c:1248:test_sender_init_zeroes_state:PASS
tests/lab-test.c:1249:test_sender_window_not_full_initially:PASS
tests/lab-test.c:1250:test_sender_window_full_at_capacity:PASS
tests/lab-test.c:1253:test_sender_data_sends_one_packet:PASS
tests/lab-test.c:1254:test_sender_data_packet_decodes_correctly:PASS
tests/lab-test.c:1255:test_sender_data_ignored_when_window_full:PASS
tests/lab-test.c:1256:test_sender_data_timer_starts_on_first:PASS
tests/lab-test.c:1259:test_sender_eof_empty_file:PASS
tests/lab-test.c:1260:test_sender_eof_deferred_when_window_full:PASS
tests/lab-test.c:1261:test_sender_eof_idempotent:PASS
tests/lab-test.c:1262:test_sender_exact_multiple_of_1024:PASS
tests/lab-test.c:1265:test_sender_ack_duplicate_ignored:PASS
tests/lab-test.c:1266:test_sender_ack_cumulative_slide:PASS
tests/lab-test.c:1267:test_sender_ack_drains_window_stops_timer:PASS
tests/lab-test.c:1268:test_sender_ack_resets_timeout_counter:PASS
tests/lab-test.c:1269:test_sender_ack_fin_sets_done:PASS
tests/lab-test.c:1272:test_sender_timeout_retransmits_window:PASS
tests/lab-test.c:1273:test_sender_timeout_includes_fin:PASS
tests/lab-test.c:1274:test_sender_timeout_give_up:PASS
tests/lab-test.c:1277:test_recv_init_zeroes_state:PASS
tests/lab-test.c:1278:test_recv_inorder_data:PASS
tests/lab-test.c:1279:test_recv_duplicate_packet:PASS
tests/lab-test.c:1280:test_recv_beyond_gap:PASS
tests/lab-test.c:1281:test_recv_fin_sets_done:PASS
tests/lab-test.c:1282:test_recv_repeated_fin:PASS
tests/lab-test.c:1283:test_recv_ignores_ack_packets:PASS
tests/lab-test.c:1286:test_channel_small_file_seed42:PASS
tests/lab-test.c:1287:test_channel_small_file_seed1234:PASS
tests/lab-test.c:1288:test_channel_exact_1024_multiple:PASS
tests/lab-test.c:1289:test_channel_empty_file:PASS
tests/lab-test.c:1290:test_channel_large_file_window1:PASS
tests/lab-test.c:1291:test_channel_large_file_window16:PASS
tests/lab-test.c:1292:test_channel_seed_999:PASS

-----------------------
58 Tests 0 Failures 0 Ignored 
OK
```

---

## Src Files
### lab.c

```c

/*
 * lab.c  –  CS 425 Project 2: Go-Back-N protocol library implementation
 *
 * Layer 1: pure packet encode / decode / validate (pkt_*)
 * Layer 2: Go-Back-N state machines (gbn_sender_*, gbn_recv_*)
 *
 * No sockets.  No clocks.  No file I/O.
 */

#include "lab.h"

#include <string.h>   /* memset, memcpy */
#include <stddef.h>

/* =========================================================================
 * Internal helpers: big-endian field read / write
 * ======================================================================= */

static void write16(uint8_t *p, uint16_t v)
{
    p[0] = (uint8_t)(v >> 8);
    p[1] = (uint8_t)(v & 0xff);
}

static void write32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)(v >> 24);
    p[1] = (uint8_t)((v >> 16) & 0xff);
    p[2] = (uint8_t)((v >>  8) & 0xff);
    p[3] = (uint8_t)(v & 0xff);
}

static uint16_t read16(const uint8_t *p)
{
    return (uint16_t)(((uint16_t)p[0] << 8) | p[1]);
}

static uint32_t read32(const uint8_t *p)
{
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16)
         | ((uint32_t)p[2] <<  8) |  (uint32_t)p[3];
}

/* =========================================================================
 * LAYER 1 – pkt_checksum
 * ======================================================================= */

uint16_t pkt_checksum(const uint8_t *buf, size_t len)
{
    uint32_t sum = 0;
    size_t i;

    for (i = 0; i + 1 < len; i += 2)
        sum += ((uint32_t)buf[i] << 8) | buf[i + 1];

    if (i < len)                      /* odd byte: pad high, zero low */
        sum += (uint32_t)buf[i] << 8;

    while (sum >> 16)
        sum = (sum & 0xffff) + (sum >> 16);

    return (uint16_t)(~sum);
}

/* =========================================================================
 * LAYER 1 – pkt_encode_*
 * ======================================================================= */

size_t pkt_encode_data(uint8_t *buf, uint32_t seq,
                       const uint8_t *payload, uint16_t paylen)
{
    buf[OFF_TYPE] = PKT_TYPE_DATA;
    buf[OFF_RSVD] = 0;
    buf[OFF_CKSUM]     = 0;
    buf[OFF_CKSUM + 1] = 0;
    write32(buf + OFF_SEQ, seq);
    write16(buf + OFF_LEN, paylen);
    memcpy(buf + OFF_PAYLOAD, payload, paylen);

    size_t total = (size_t)PKT_HDR_LEN + paylen;
    write16(buf + OFF_CKSUM, pkt_checksum(buf, total));
    return total;
}

size_t pkt_encode_ack(uint8_t *buf, uint32_t seq)
{
    buf[OFF_TYPE] = PKT_TYPE_ACK;
    buf[OFF_RSVD] = 0;
    buf[OFF_CKSUM]     = 0;
    buf[OFF_CKSUM + 1] = 0;
    write32(buf + OFF_SEQ, seq);
    write16(buf + OFF_LEN, 0);

    write16(buf + OFF_CKSUM, pkt_checksum(buf, PKT_HDR_LEN));
    return PKT_HDR_LEN;
}

size_t pkt_encode_fin(uint8_t *buf, uint32_t seq)
{
    buf[OFF_TYPE] = PKT_TYPE_FIN;
    buf[OFF_RSVD] = 0;
    buf[OFF_CKSUM]     = 0;
    buf[OFF_CKSUM + 1] = 0;
    write32(buf + OFF_SEQ, seq);
    write16(buf + OFF_LEN, 0);

    write16(buf + OFF_CKSUM, pkt_checksum(buf, PKT_HDR_LEN));
    return PKT_HDR_LEN;
}

/* =========================================================================
 * LAYER 1 – pkt_decode
 * ======================================================================= */

int pkt_decode(const uint8_t *buf, size_t len, pkt_t *out)
{
    /* Rule 1: minimum size */
    if (len < PKT_HDR_LEN)
        return 0;

    /* Rule 2: length field consistency – read before touching payload */
    uint16_t payload_len = read16(buf + OFF_LEN);
    if (payload_len > PKT_MAX_PAYLOAD)
        return 0;
    if (len != (size_t)PKT_HDR_LEN + payload_len)
        return 0;

    /* Rule 3: type and reserved */
    uint8_t type = buf[OFF_TYPE];
    if (type > PKT_TYPE_FIN)
        return 0;
    if (buf[OFF_RSVD] != 0)
        return 0;

    /* Rule 4: checksum – valid packet gives 0x0000 */
    if (pkt_checksum(buf, len) != 0x0000)
        return 0;

    out->type        = type;
    out->seq         = read32(buf + OFF_SEQ);
    out->payload_len = payload_len;
    out->payload     = (payload_len > 0) ? (buf + OFF_PAYLOAD) : NULL;
    return 1;
}

/* =========================================================================
 * LAYER 2 – gbn_sender helpers
 * ======================================================================= */

/* empty action – caller fills in what it needs */
static snd_action_t make_empty_action(const gbn_sender_t *s)
{
    snd_action_t a;
    memset(&a, 0, sizeof(a));
    a.timer_deadline_ms = s->timer_deadline_ms;
    return a;
}

/*
 * queue_send: add one window slot to a's send list.
 * slot must be in [0, GBN_MAX_WINDOW).
 */
static void queue_send(snd_action_t *a, gbn_sender_t *s, int slot)
{
    int i = a->send_count;
    a->send_pkts[i].buf = s->winbuf[slot];
    a->send_pkts[i].len = s->winlen[slot];
    a->send_count++;
}

/*
 * try_send_fin: if EOF has been seen and all DATA sent, build+queue the FIN.
 * Returns 1 if FIN was queued, 0 otherwise.
 */
static int try_send_fin(gbn_sender_t *s, snd_action_t *a, long long now_ms)
{
    if (!s->eof_seen || s->fin_sent)
        return 0;
    /* Invariant: fin_seq == next at the moment eof_seen is set, and next
     * is never incremented again until try_send_fin itself sets it to
     * fin_seq+1.  So next == fin_seq always holds here. */

    int slot = (int)(s->fin_seq % (uint32_t)s->winsize);
    s->winlen[slot] = pkt_encode_fin(s->winbuf[slot], s->fin_seq);
    s->fin_sent = 1;
    s->next     = s->fin_seq + 1;

    if (s->timer_deadline_ms == 0)
        s->timer_deadline_ms = now_ms + s->timeout_ms;

    queue_send(a, s, slot);
    a->timer_deadline_ms = s->timer_deadline_ms;
    return 1;
}

/* =========================================================================
 * LAYER 2 – gbn_sender_* public API
 * ======================================================================= */

void gbn_sender_init(gbn_sender_t *s, int winsize, long long timeout_ms)
{
    memset(s, 0, sizeof(*s));
    s->winsize    = winsize;
    s->timeout_ms = timeout_ms;
}

int gbn_sender_window_full(const gbn_sender_t *s)
{
    return (int)(s->next - s->base) >= s->winsize;
}

snd_action_t gbn_sender_data(gbn_sender_t *s, long long now_ms,
                              const uint8_t *payload, uint16_t paylen)
{
    snd_action_t a = make_empty_action(s);

    /* Window full or EOF already signalled: caller must not advance file */
    if (gbn_sender_window_full(s) || s->eof_seen)
        return a;

    int slot = (int)(s->next % (uint32_t)s->winsize);
    s->winlen[slot] = pkt_encode_data(s->winbuf[slot], s->next,
                                      payload, paylen);

    /* Start timer on first in-flight packet */
    if (s->timer_deadline_ms == 0)
        s->timer_deadline_ms = now_ms + s->timeout_ms;

    s->next++;
    queue_send(&a, s, slot);
    a.timer_deadline_ms = s->timer_deadline_ms;

    /* If this was a short read the caller will follow with gbn_sender_eof */
    return a;
}

snd_action_t gbn_sender_eof(gbn_sender_t *s, long long now_ms)
{
    snd_action_t a = make_empty_action(s);

    if (s->eof_seen)
        return a;   /* idempotent */

    s->eof_seen = 1;
    s->fin_seq  = s->next;  /* FIN seq = number of DATA packets */

    /* If the window has room, send the FIN immediately */
    if (!gbn_sender_window_full(s))
        try_send_fin(s, &a, now_ms);

    return a;
}

snd_action_t gbn_sender_ack(gbn_sender_t *s, long long now_ms,
                             uint32_t ack_seq)
{
    snd_action_t a = make_empty_action(s);

    /* Duplicate or stale: ignore */
    if (ack_seq <= s->base)
        return a;

    /* Advance base */
    s->base            = ack_seq;
    s->consec_timeouts = 0;

    /* Check for FIN ACK */
    if (s->fin_sent && ack_seq == s->fin_seq + 1) {
        s->fin_acked          = 1;
        s->timer_deadline_ms  = 0;
        a.timer_deadline_ms   = 0;
        a.done                = 1;
        return a;
    }

    /* Anything still in flight? */
    int in_flight = s->fin_sent
                    ? (s->base <= s->fin_seq)
                    : (s->base < s->next);

    if (in_flight) {
        s->timer_deadline_ms = now_ms + s->timeout_ms;
    } else {
        /* Window empty and FIN not yet sent: stop timer; the FIN send
         * (triggered when the I/O layer calls gbn_sender_eof or the next
         * gbn_sender_data pass) will restart it. */
        s->timer_deadline_ms = 0;
    }
    a.timer_deadline_ms = s->timer_deadline_ms;

    /*
     * If EOF has been seen and the window just drained below capacity,
     * try to send the FIN now.
     */
    if (!gbn_sender_window_full(s))
        try_send_fin(s, &a, now_ms);

    return a;
}

snd_action_t gbn_sender_timeout(gbn_sender_t *s, long long now_ms)
{
    snd_action_t a = make_empty_action(s);

    s->consec_timeouts++;
    if (s->consec_timeouts >= GBN_MAX_TIMEOUTS) {
        a.give_up           = 1;
        a.timer_deadline_ms = 0;
        s->timer_deadline_ms = 0;
        return a;
    }

    /* Retransmit base … next-1 (next is fin_seq+1 when FIN is in flight) */
    uint32_t end = s->fin_sent ? s->fin_seq + 1 : s->next;
    uint32_t seq;
    for (seq = s->base; seq < end; seq++) {
        int slot = (int)(seq % (uint32_t)s->winsize);
        queue_send(&a, s, slot);
    }

    /* Restart timer */
    s->timer_deadline_ms = now_ms + s->timeout_ms;
    a.timer_deadline_ms  = s->timer_deadline_ms;
    return a;
}

/* =========================================================================
 * LAYER 2 – gbn_recv_* public API
 * ======================================================================= */

void gbn_recv_init(gbn_recv_t *r, long long linger_ms)
{
    memset(r, 0, sizeof(*r));
    r->linger_ms = linger_ms;
}

rcv_action_t gbn_recv_pkt(gbn_recv_t *r, long long now_ms,
                           const pkt_t *pkt)
{
    rcv_action_t a;
    memset(&a, 0, sizeof(a));

    if (pkt->type == PKT_TYPE_DATA) {
        if (pkt->seq == r->expected) {
            /* In-order: deliver payload and advance expected */
            a.payload     = pkt->payload;
            a.payload_len = pkt->payload_len;
            r->expected++;
        }
        /* Out-of-order or duplicate: fall through, ACK current expected */
        a.ack_len = pkt_encode_ack(a.ack_buf, r->expected);

    } else if (pkt->type == PKT_TYPE_FIN) {
        if (pkt->seq == r->expected) {
            /* Expected FIN: transfer complete */
            r->expected++;
            r->done = 1;
            a.ack_len            = pkt_encode_ack(a.ack_buf, r->expected);
            a.done               = 1;
            a.linger_deadline_ms = now_ms + r->linger_ms;
        } else {
            /* Out-of-order FIN: re-ACK what we're waiting for */
            a.ack_len = pkt_encode_ack(a.ack_buf, r->expected);
        }
    }
    /* ACK packets arriving at the receiver are silently ignored (ack_len=0) */

    return a;
}
```

### lab.h

```c

/*
 * lab.h  –  CS 425 Project 2: testable Go-Back-N protocol library
 *
 * Three layers, cleanly separated:
 *
 *   Layer 1  pkt_*        Pure packet encode/decode/validate.
 *                         No state.  Bytes in, bytes or struct out.
 *
 *   Layer 2  gbn_sender_t  Go-Back-N sender state machine.
 *            gbn_recv_t    Go-Back-N receiver state machine.
 *                         No sockets, no clocks, no files.
 *                         The current time is always passed in (ms).
 *                         Each function returns what to do next.
 *
 *   Layer 3  run_send() / run_recv()  in myapp.c
 *                         Thin I/O shell: socket, poll, clock, fread/fwrite.
 *                         Calls layer 2, acts on what comes back.
 *
 * Tests drive layer 2 directly with a fake clock and an in-memory channel.
 * A "lost" packet is just one the test does not forward.
 * A "timeout" is just passing a larger value for `now_ms`.
 */

#ifndef LAB_H
#define LAB_H

#include <stdint.h>
#include <stddef.h>

/* =========================================================================
 * LAYER 1 – Packet encode / decode / validate
 * ======================================================================= */

/* Wire-format constants */
#define PKT_TYPE_DATA    0
#define PKT_TYPE_ACK     1
#define PKT_TYPE_FIN     2

#define PKT_HDR_LEN      10
#define PKT_MAX_PAYLOAD  1024
#define PKT_MAX_LEN      (PKT_HDR_LEN + PKT_MAX_PAYLOAD)   /* 1034 */

/* Byte offsets within a flat wire buffer */
#define OFF_TYPE         0
#define OFF_RSVD         1
#define OFF_CKSUM        2   /* 2 bytes, big-endian */
#define OFF_SEQ          4   /* 4 bytes, big-endian */
#define OFF_LEN          8   /* 2 bytes, big-endian */
#define OFF_PAYLOAD      10

/* ------------------------------------------------------------------
 * pkt_checksum(buf, len)
 *
 * RFC 1071 Internet checksum: 16-bit one's-complement of the
 * one's-complement sum, treating the data as big-endian 16-bit words.
 * An odd-length buffer is padded with one zero byte (high byte) for
 * the calculation only.
 *
 * To COMPUTE: call with checksum field zeroed; store the result.
 * To VERIFY:  call over the whole packet including the stored checksum.
 *             Returns 0x0000 for an undamaged packet.
 * ------------------------------------------------------------------ */
uint16_t pkt_checksum(const uint8_t *buf, size_t len);

/* ------------------------------------------------------------------
 * pkt_encode_data(buf, seq, payload, paylen)  →  total bytes written
 *
 * Serialises a DATA packet into buf (which must be >= PKT_MAX_LEN).
 * paylen must be in [1, PKT_MAX_PAYLOAD].
 * ------------------------------------------------------------------ */
size_t pkt_encode_data(uint8_t *buf, uint32_t seq,
                       const uint8_t *payload, uint16_t paylen);

/* ------------------------------------------------------------------
 * pkt_encode_ack(buf, seq)  →  total bytes written (always PKT_HDR_LEN)
 *
 * seq is the next expected packet index (cumulative ACK).
 * ------------------------------------------------------------------ */
size_t pkt_encode_ack(uint8_t *buf, uint32_t seq);

/* ------------------------------------------------------------------
 * pkt_encode_fin(buf, seq)  →  total bytes written (always PKT_HDR_LEN)
 *
 * seq is the number of DATA packets (= index after last DATA).
 * ------------------------------------------------------------------ */
size_t pkt_encode_fin(uint8_t *buf, uint32_t seq);

/* ------------------------------------------------------------------
 * pkt_decode result: filled in by pkt_decode() on success.
 * ------------------------------------------------------------------ */
typedef struct {
    uint8_t         type;        /* PKT_TYPE_DATA / ACK / FIN */
    uint32_t        seq;
    uint16_t        payload_len; /* 0 for ACK / FIN */
    const uint8_t  *payload;     /* points into the original buffer */
} pkt_t;

/* ------------------------------------------------------------------
 * pkt_decode(buf, len, out)  →  1 if valid, 0 if invalid (discard)
 *
 * Validates all four spec rules and fills *out on success:
 *   1. datagram >= 10 bytes
 *   2. 10 + length == datagram size, length <= 1024
 *   3. type in {0,1,2}, reserved == 0
 *   4. checksum verifies (== 0x0000 when re-computed over the packet)
 * ------------------------------------------------------------------ */
int pkt_decode(const uint8_t *buf, size_t len, pkt_t *out);


/* =========================================================================
 * LAYER 2 – Go-Back-N state machines
 *
 * Neither machine calls sendto, recvfrom, poll, clock_gettime, fread,
 * or fwrite.  The current wall-clock time is always passed in as `now_ms`
 * (a long long, milliseconds from an arbitrary epoch – use CLOCK_MONOTONIC).
 * ======================================================================= */

#define GBN_MAX_WINDOW     64
#define GBN_MAX_TIMEOUTS   10   /* give-up threshold */

/* ------------------------------------------------------------------
 * snd_action_t: what the sender wants the I/O layer to do.
 *
 * The I/O layer inspects this after every gbn_sender_* call and acts
 * on it before calling again.
 *
 * send_count packets starting at send_pkts[0] should be sent (in order).
 * Each entry is a (buf, len) pair pointing into the sender's own window
 * buffer – the I/O layer must not free them.
 *
 * timer_deadline_ms:
 *   > 0  start/restart the timer; fire at this absolute time (ms)
 *   = 0  stop the timer (nothing in flight)
 *
 * done:     transfer complete; I/O layer should exit 0.
 * give_up:  10 consecutive timeouts; I/O layer should exit 2.
 * ------------------------------------------------------------------ */
typedef struct {
    uint8_t  *buf;
    size_t    len;
} snd_iovec_t;

typedef struct {
    snd_iovec_t   send_pkts[GBN_MAX_WINDOW + 1]; /* +1 for the FIN */
    int           send_count;
    long long     timer_deadline_ms;              /* 0 = stop */
    int           done;
    int           give_up;
} snd_action_t;

/* ------------------------------------------------------------------
 * gbn_sender_t: all sender state.  Zero-initialise, then call
 * gbn_sender_init() before any other function.
 * ------------------------------------------------------------------ */
typedef struct {
    /* configuration (set by init, never changed) */
    int       winsize;
    long long timeout_ms;

    /* sliding-window state */
    uint32_t  base;          /* oldest unACK'd seq */
    uint32_t  next;          /* next seq never sent */
    uint32_t  fin_seq;       /* seq of the FIN packet */
    int       eof_seen;      /* file data is exhausted */
    int       fin_sent;      /* FIN has been transmitted at least once */
    int       fin_acked;     /* ACK fin_seq+1 received */

    /* timer */
    long long timer_deadline_ms;  /* 0 = stopped */
    int       consec_timeouts;

    /* window buffer: circular, indexed by seq % winsize */
    uint8_t   winbuf[GBN_MAX_WINDOW][PKT_MAX_LEN];
    size_t    winlen[GBN_MAX_WINDOW];
} gbn_sender_t;

/* ------------------------------------------------------------------
 * gbn_sender_init(s, winsize, timeout_ms)
 *
 * Zeroes s and stores configuration.  Call once before anything else.
 * winsize must be in [1, GBN_MAX_WINDOW].
 * ------------------------------------------------------------------ */
void gbn_sender_init(gbn_sender_t *s, int winsize, long long timeout_ms);

/* ------------------------------------------------------------------
 * gbn_sender_data(s, now_ms, payload, paylen)  →  snd_action_t
 *
 * Offer one chunk of file data (up to PKT_MAX_PAYLOAD bytes) to the
 * sender.  If the window has room the packet is built, stored, and
 * included in the returned action.  If the window is full the payload
 * is ignored – the I/O layer must not advance the file position.
 *
 * The I/O layer calls this repeatedly until send_count == 0, which
 * means the window is full and the layer should wait for ACKs.
 * ------------------------------------------------------------------ */
snd_action_t gbn_sender_data(gbn_sender_t *s, long long now_ms,
                              const uint8_t *payload, uint16_t paylen);

/* ------------------------------------------------------------------
 * gbn_sender_eof(s, now_ms)  →  snd_action_t
 *
 * Signal end-of-file.  The sender will build and send the FIN once
 * all preceding DATA packets have been sent (and the window allows).
 * May be called before gbn_sender_data drains the window; the FIN
 * will be queued and sent when gbn_sender_timeout or gbn_sender_ack
 * next opens a slot.
 * ------------------------------------------------------------------ */
snd_action_t gbn_sender_eof(gbn_sender_t *s, long long now_ms);

/* ------------------------------------------------------------------
 * gbn_sender_ack(s, now_ms, ack_seq)  →  snd_action_t
 *
 * Feed a received, validated ACK seq number.  Slides base, resets the
 * consecutive-timeout counter, and may open room in the window.
 * The action's send_count will be 0 (the I/O layer fills new slots
 * from the file on its next pass); timer_deadline_ms reflects the new
 * in-flight state.
 * ------------------------------------------------------------------ */
snd_action_t gbn_sender_ack(gbn_sender_t *s, long long now_ms,
                             uint32_t ack_seq);

/* ------------------------------------------------------------------
 * gbn_sender_timeout(s, now_ms)  →  snd_action_t
 *
 * The timer has fired.  Retransmits everything from base to next-1
 * (including the FIN if sent), increments consec_timeouts, and
 * restarts the timer.  Sets give_up when consec_timeouts reaches
 * GBN_MAX_TIMEOUTS.
 * ------------------------------------------------------------------ */
snd_action_t gbn_sender_timeout(gbn_sender_t *s, long long now_ms);

/* ------------------------------------------------------------------
 * gbn_sender_window_full(s)  →  1 if the window is full, 0 if there
 * is room.  Convenience for the I/O layer's read loop.
 * ------------------------------------------------------------------ */
int gbn_sender_window_full(const gbn_sender_t *s);

/* ------------------------------------------------------------------
 * rcv_action_t: what the receiver wants the I/O layer to do.
 *
 * If ack_len > 0, send ack_buf back to the relay.
 * If payload_len > 0, write payload_len bytes starting at payload
 *   to the output file.
 * done: the FIN has been processed and the file closed; the I/O layer
 *   should now enter the 2-second linger.
 * linger_deadline_ms: absolute time (ms) when the linger ends.
 *   Meaningful only when done == 1.
 * ------------------------------------------------------------------ */
typedef struct {
    uint8_t        ack_buf[PKT_HDR_LEN];
    size_t         ack_len;            /* 0 = nothing to send */

    const uint8_t *payload;            /* points into the caller's buffer */
    uint16_t       payload_len;        /* 0 = nothing to write */

    int            done;               /* FIN processed */
    long long      linger_deadline_ms; /* meaningful when done == 1 */
} rcv_action_t;

/* ------------------------------------------------------------------
 * gbn_recv_t: all receiver state.  Zero-initialise, then call
 * gbn_recv_init() before any other function.
 * ------------------------------------------------------------------ */
typedef struct {
    uint32_t expected;          /* next seq the receiver wants */
    int      done;              /* FIN has been processed */
    long long linger_ms;        /* linger duration (ms) */
} gbn_recv_t;

/* ------------------------------------------------------------------
 * gbn_recv_init(r, linger_ms)
 *
 * Zeroes r and stores the linger duration.  Call once before anything.
 * ------------------------------------------------------------------ */
void gbn_recv_init(gbn_recv_t *r, long long linger_ms);

/* ------------------------------------------------------------------
 * gbn_recv_pkt(r, now_ms, pkt)  →  rcv_action_t
 *
 * Feed one already-validated and decoded packet to the receiver.
 * The caller must have called pkt_decode() first (layer 1).
 * Returns what to ACK, what to write, and whether the transfer is done.
 * ------------------------------------------------------------------ */
rcv_action_t gbn_recv_pkt(gbn_recv_t *r, long long now_ms,
                           const pkt_t *pkt);

#endif /* LAB_H */
```

### main.c

```c

/*
 * myapp.c  –  CS 425 Project 2: Go-Back-N over UDP
 *
 * Layer 3 (I/O) only.  All protocol logic lives in lab.c (lab.h).
 *
 * Build:  make release   →  ./build/release/myapp
 * Usage (no args):  prints usage and exits 0  (required by `make leak`)
 */

#include "lab.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>
#include <errno.h>
#include <time.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <poll.h>

/* -------------------------------------------------------------------------
 * CLI constants
 * ---------------------------------------------------------------------- */
#define DEFAULT_WINDOW    8
#define DEFAULT_TIMEOUT   250
#define DEFAULT_LOSS      0.0
#define DEFAULT_CORRUPT   0.0
#define DEFAULT_DUP       0.0
#define DEFAULT_PORT      4250
#define MIN_WINDOW        1
#define MAX_WINDOW        64

/* I/O-layer constants */
#define HELLO_TIMEOUT_SEC  1
#define HELLO_MAX_TRIES    5
#define RECV_IDLE_TIMEOUT  30000LL   /* ms */
#define LINGER_MS          2000LL

/* -------------------------------------------------------------------------
 * Config
 * ---------------------------------------------------------------------- */
typedef enum { MODE_SEND, MODE_RECV } app_mode_t;

typedef struct {
    app_mode_t  mode;
    char       *session;
    int         window;
    int         timeout_ms;
    double      loss;
    double      corrupt;
    double      dup;
    int         port;
    char       *relay;
    char       *file;
} config_t;

/* -------------------------------------------------------------------------
 * Monotonic clock  (used only by Layer 3)
 * ---------------------------------------------------------------------- */
static long long now_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (long long)ts.tv_sec * 1000LL + ts.tv_nsec / 1000000LL;
}

/* -------------------------------------------------------------------------
 * print_usage
 * ---------------------------------------------------------------------- */
static void print_usage(void)
{
    puts("Usage: myapp send -s <session> [-w window] [-T timeout-ms] [-l loss]");
    puts("                  [-c corrupt] [-d dup] [-p port] <relay> <file>");
    puts("       myapp recv -s <session> [-p port] <relay> <file>");
    puts("");
    puts("  -s <session>     session name shared by the sender and the receiver");
    puts("  -w <window>      Go-Back-N window size in packets, 1 to 64 (default: 8)");
    puts("  -T <timeout-ms>  retransmission timeout in milliseconds (default: 250)");
    puts("  -l <loss>        probability the relay drops a packet (default: 0)");
    puts("  -c <corrupt>     probability the relay flips a bit (default: 0)");
    puts("  -d <dup>         probability the relay duplicates a packet (default: 0)");
    puts("  -p <port>        relay port (default: 4250)");
    puts("  <relay>          host name or address of the relay");
    puts("  <file>           file to send, or file to write what is received");
}

/* -------------------------------------------------------------------------
 * Argument parsers
 * ---------------------------------------------------------------------- */
static int parse_double_arg(const char *s, double *out, const char *flag)
{
    char *end;
    double v = strtod(s, &end);
    if (end == s || *end != '\0') {
        fprintf(stderr, "myapp: %s: not a number: %s\n", flag, s);
        return -1;
    }
    if (v < 0.0 || v > 0.5) {
        fprintf(stderr, "myapp: %s: must be between 0 and 0.5: %s\n", flag, s);
        return -1;
    }
    *out = v;
    return 0;
}

static int parse_int_arg(const char *s, int *out, const char *flag)
{
    char *end;
    long v = strtol(s, &end, 10);
    if (end == s || *end != '\0' || v <= 0) {
        fprintf(stderr, "myapp: %s: must be a positive integer: %s\n", flag, s);
        return -1;
    }
    *out = (int)v;
    return 0;
}

static int parse_args(int argc, char *argv[], config_t *cfg)
{
    memset(cfg, 0, sizeof(*cfg));
    cfg->window     = DEFAULT_WINDOW;
    cfg->timeout_ms = DEFAULT_TIMEOUT;
    cfg->loss       = DEFAULT_LOSS;
    cfg->corrupt    = DEFAULT_CORRUPT;
    cfg->dup        = DEFAULT_DUP;
    cfg->port       = DEFAULT_PORT;

    if (argc < 2) return -1;

    if (strcmp(argv[1], "send") == 0)
        cfg->mode = (app_mode_t)MODE_SEND;
    else if (strcmp(argv[1], "recv") == 0)
        cfg->mode = (app_mode_t)MODE_RECV;
    else {
        fprintf(stderr, "myapp: unknown mode '%s'\n", argv[1]);
        return -1;
    }

    int rem_argc = argc - 1;
    char **rem_argv = argv + 1;
    optind = 1;

    int opt;
    while ((opt = getopt(rem_argc, rem_argv, "s:w:T:l:c:d:p:")) != -1) {
        switch (opt) {
        case 's': cfg->session = optarg; break;
        case 'w':
            if (parse_int_arg(optarg, &cfg->window, "-w") != 0) return -1;
            if (cfg->window < MIN_WINDOW || cfg->window > MAX_WINDOW) {
                fprintf(stderr, "myapp: -w: window must be %d-%d\n",
                        MIN_WINDOW, MAX_WINDOW);
                return -1;
            }
            break;
        case 'T':
            if (parse_int_arg(optarg, &cfg->timeout_ms, "-T") != 0) return -1;
            break;
        case 'l':
            if (parse_double_arg(optarg, &cfg->loss, "-l") != 0) return -1;
            break;
        case 'c':
            if (parse_double_arg(optarg, &cfg->corrupt, "-c") != 0) return -1;
            break;
        case 'd':
            if (parse_double_arg(optarg, &cfg->dup, "-d") != 0) return -1;
            break;
        case 'p':
            if (parse_int_arg(optarg, &cfg->port, "-p") != 0) return -1;
            if (cfg->port < 1 || cfg->port > 65535) {
                fprintf(stderr, "myapp: -p: port out of range\n");
                return -1;
            }
            break;
        default: return -1;
        }
    }

    if (!cfg->session) {
        fprintf(stderr, "myapp: -s <session> is required\n");
        return -1;
    }

    int npos = rem_argc - optind;
    if (npos < 2) {
        fprintf(stderr, "myapp: missing positional argument%s\n",
                npos == 1 ? " <file>" : "s (<relay> <file>)");
        return -1;
    }
    if (npos > 2) {
        fprintf(stderr, "myapp: too many positional arguments\n");
        return -1;
    }
    cfg->relay = rem_argv[optind];
    cfg->file  = rem_argv[optind + 1];
    return 0;
}

/* -------------------------------------------------------------------------
 * open_udp_socket: resolve relay host:port, return bound UDP fd.
 * ---------------------------------------------------------------------- */
static int open_udp_socket(const char *host, int port,
                            struct sockaddr_storage *dest, socklen_t *destlen)
{
    char portstr[8];
    struct addrinfo hints, *res, *rp;
    int fd = -1;

    snprintf(portstr, sizeof(portstr), "%d", port);
    memset(&hints, 0, sizeof(hints));
    hints.ai_family   = AF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;

    int rc = getaddrinfo(host, portstr, &hints, &res);
    if (rc != 0) {
        fprintf(stderr, "myapp: getaddrinfo(%s): %s\n", host, gai_strerror(rc));
        return -1;
    }

    for (rp = res; rp; rp = rp->ai_next) {
        fd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (fd >= 0) {
            memcpy(dest, rp->ai_addr, rp->ai_addrlen);
            *destlen = (socklen_t)rp->ai_addrlen;
            break;
        }
    }
    freeaddrinfo(res);

    if (fd < 0) {
        fprintf(stderr, "myapp: could not create UDP socket for %s:%d\n",
                host, port);
    }
    return fd;
}

/* -------------------------------------------------------------------------
 * register_with_relay: send HELLO, wait for OK.  Up to 5 attempts.
 * ---------------------------------------------------------------------- */
static int register_with_relay(int fd,
                                const struct sockaddr_storage *relay,
                                socklen_t relaylen,
                                const char *hello_msg)
{
    struct timeval tv = { HELLO_TIMEOUT_SEC, 0 };
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    char reply[256];
    int attempt;
    for (attempt = 0; attempt < HELLO_MAX_TRIES; attempt++) {
        if (sendto(fd, hello_msg, strlen(hello_msg), 0,
                   (const struct sockaddr *)relay, relaylen) < 0) {
            perror("myapp: sendto HELLO");
            return -1;
        }

        ssize_t n = recvfrom(fd, reply, sizeof(reply) - 1, 0, NULL, NULL);
        if (n < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) continue;
            perror("myapp: recvfrom HELLO reply");
            return -1;
        }
        reply[n] = '\0';

        if (strncmp(reply, "OK", 2) == 0) {
            /* clear the receive timeout – data phase uses poll */
            tv.tv_sec = tv.tv_usec = 0;
            setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
            return 0;
        }
        if (strncmp(reply, "ERR", 3) == 0) {
            /* strip trailing whitespace */
            while (n > 0 && (reply[n-1] == '\n' || reply[n-1] == '\r'
                              || reply[n-1] == ' '))
                reply[--n] = '\0';
            fprintf(stderr, "myapp: relay refused: %s\n", reply);
            return -1;
        }
    }

    fprintf(stderr, "myapp: no reply from relay after %d attempts\n",
            HELLO_MAX_TRIES);
    return -1;
}

/* -------------------------------------------------------------------------
 * do_send: send all packets in a snd_action_t to the relay.
 * ---------------------------------------------------------------------- */
static void do_send(int fd,
                    const struct sockaddr_storage *relay, socklen_t relaylen,
                    const snd_action_t *a)
{
    int i;
    for (i = 0; i < a->send_count; i++)
        sendto(fd, a->send_pkts[i].buf, a->send_pkts[i].len, 0,
               (const struct sockaddr *)relay, relaylen);
}

/* =========================================================================
 * run_recv  –  Layer 3 receiver I/O shell
 * ======================================================================= */
static int run_recv(const config_t *cfg)
{
    struct sockaddr_storage relay_addr;
    socklen_t relay_addrlen;

    int fd = open_udp_socket(cfg->relay, cfg->port, &relay_addr, &relay_addrlen);
    if (fd < 0) return 2;

    char hello[128];
    snprintf(hello, sizeof(hello), "HELLO %s recv", cfg->session);
    if (register_with_relay(fd, &relay_addr, relay_addrlen, hello) != 0) {
        close(fd);
        return 2;
    }

    FILE *outf = fopen(cfg->file, "wb");
    if (!outf) {
        fprintf(stderr, "myapp: cannot open '%s': %s\n",
                cfg->file, strerror(errno));
        close(fd);
        return 2;
    }

    /* ---- state machine ---- */
    gbn_recv_t r;
    gbn_recv_init(&r, LINGER_MS);

    uint8_t  rawbuf[PKT_MAX_LEN];
    long long deadline = now_ms() + RECV_IDLE_TIMEOUT;

    for (;;) {
        /* set SO_RCVTIMEO to remaining idle budget */
        long long left = deadline - now_ms();
        if (left <= 0) {
            fprintf(stderr, "myapp: idle timeout\n");
            fclose(outf);
            close(fd);
            return 2;
        }
        struct timeval tv;
        tv.tv_sec  = left / 1000;
        tv.tv_usec = (long)(left % 1000) * 1000L;
        setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

        struct sockaddr_storage src;
        socklen_t srclen = sizeof(src);
        ssize_t n = recvfrom(fd, rawbuf, sizeof(rawbuf), 0,
                             (struct sockaddr *)&src, &srclen);
        if (n < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                fprintf(stderr, "myapp: idle timeout\n");
                fclose(outf);
                close(fd);
                return 2;
            }
            perror("myapp: recvfrom");
            fclose(outf);
            close(fd);
            return 2;
        }

        pkt_t pkt;
        if (!pkt_decode(rawbuf, (size_t)n, &pkt)) continue;

        deadline = now_ms() + RECV_IDLE_TIMEOUT;

        rcv_action_t a = gbn_recv_pkt(&r, now_ms(), &pkt);

        /* write payload */
        if (a.payload_len > 0) {
            if (fwrite(a.payload, 1, a.payload_len, outf) != a.payload_len) {
                fprintf(stderr, "myapp: fwrite: %s\n", strerror(errno));
                fclose(outf);
                close(fd);
                return 2;
            }
        }

        /* send ACK */
        if (a.ack_len > 0)
            sendto(fd, a.ack_buf, a.ack_len, 0,
                   (const struct sockaddr *)&relay_addr, relay_addrlen);

        if (!a.done) continue;

        /* ---- FIN processed: close file, enter linger ---- */
        fclose(outf);

        /* Linger: re-ACK repeated FINs until the deadline */
        long long linger_end = a.linger_deadline_ms;
        for (;;) {
            long long ll = linger_end - now_ms();
            if (ll <= 0) break;

            struct timeval ltv;
            ltv.tv_sec  = ll / 1000;
            ltv.tv_usec = (long)(ll % 1000) * 1000L;
            setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &ltv, sizeof(ltv));

            ssize_t ln = recvfrom(fd, rawbuf, sizeof(rawbuf), 0,
                                  (struct sockaddr *)&src, &srclen);
            if (ln < 0) break;

            pkt_t lp;
            if (!pkt_decode(rawbuf, (size_t)ln, &lp)) continue;

            /* Re-ACK the FIN: pass it back through the state machine.
             * r.expected is already past the FIN, so a repeated FIN
             * with seq == r.expected-1 will be treated as out-of-order
             * and generate ACK r.expected again. */
            rcv_action_t la = gbn_recv_pkt(&r, now_ms(), &lp);
            if (la.ack_len > 0)
                sendto(fd, la.ack_buf, la.ack_len, 0,
                       (const struct sockaddr *)&relay_addr, relay_addrlen);
        }

        close(fd);
        return 0;
    }
}

/* =========================================================================
 * run_send  –  Layer 3 sender I/O shell
 * ======================================================================= */
static int run_send(const config_t *cfg)
{
    struct sockaddr_storage relay_addr;
    socklen_t relay_addrlen;

    int fd = open_udp_socket(cfg->relay, cfg->port, &relay_addr, &relay_addrlen);
    if (fd < 0) return 2;

    char hello[256];
    snprintf(hello, sizeof(hello), "HELLO %s send %.4f %.4f %.4f",
             cfg->session, cfg->loss, cfg->corrupt, cfg->dup);
    if (register_with_relay(fd, &relay_addr, relay_addrlen, hello) != 0) {
        close(fd);
        return 2;
    }

    FILE *inf = fopen(cfg->file, "rb");
    if (!inf) {
        fprintf(stderr, "myapp: cannot open '%s': %s\n",
                cfg->file, strerror(errno));
        close(fd);
        return 2;
    }

    /* ---- state machine ---- */
    gbn_sender_t s;
    gbn_sender_init(&s, cfg->window, (long long)cfg->timeout_ms);

    uint8_t  payload[PKT_MAX_PAYLOAD];
    uint8_t  rawbuf[PKT_MAX_LEN];

    struct pollfd pfd = { fd, POLLIN, 0 };

    for (;;) {
        /* ---- fill window from file ---- */
        while (!gbn_sender_window_full(&s) && !s.eof_seen) {
            size_t nr = fread(payload, 1, PKT_MAX_PAYLOAD, inf);
            if (nr > 0) {
                snd_action_t a = gbn_sender_data(&s, now_ms(),
                                                 payload, (uint16_t)nr);
                do_send(fd, &relay_addr, relay_addrlen, &a);
                if (nr < PKT_MAX_PAYLOAD) {
                    /* short read → EOF */
                    snd_action_t ea = gbn_sender_eof(&s, now_ms());
                    do_send(fd, &relay_addr, relay_addrlen, &ea);
                    break;
                }
            } else {
                snd_action_t ea = gbn_sender_eof(&s, now_ms());
                do_send(fd, &relay_addr, relay_addrlen, &ea);
                break;
            }
        }

        if (s.fin_acked) {
            fclose(inf);
            close(fd);
            return 0;
        }

        /* ---- poll: wait for ACK or timer ---- */
        int poll_ms = -1;
        if (s.timer_deadline_ms > 0) {
            long long left = s.timer_deadline_ms - now_ms();
            poll_ms = (left > 0) ? (int)left : 0;
        }

        int rc = poll(&pfd, 1, poll_ms);
        if (rc < 0) {
            if (errno == EINTR) continue;
            perror("myapp: poll");
            fclose(inf);
            close(fd);
            return 2;
        }

        /* ---- timeout ---- */
        if (rc == 0 || (s.timer_deadline_ms > 0
                        && now_ms() >= s.timer_deadline_ms)) {
            snd_action_t a = gbn_sender_timeout(&s, now_ms());
            do_send(fd, &relay_addr, relay_addrlen, &a);
            if (a.give_up) {
                fprintf(stderr,
                        "myapp: gave up after %d consecutive timeouts\n",
                        GBN_MAX_TIMEOUTS);
                fclose(inf);
                close(fd);
                return 2;
            }
            continue;
        }

        /* ---- ACK received ---- */
        if (!(pfd.revents & POLLIN)) continue;

        ssize_t n = recvfrom(fd, rawbuf, sizeof(rawbuf), 0, NULL, NULL);
        if (n < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) continue;
            perror("myapp: recvfrom");
            fclose(inf);
            close(fd);
            return 2;
        }

        pkt_t pkt;
        if (!pkt_decode(rawbuf, (size_t)n, &pkt)) continue;
        if (pkt.type != PKT_TYPE_ACK) continue;

        snd_action_t a = gbn_sender_ack(&s, now_ms(), pkt.seq);
        do_send(fd, &relay_addr, relay_addrlen, &a);
        if (a.done) {
            fclose(inf);
            close(fd);
            return 0;
        }
    }
}

/* =========================================================================
 * main
 * ======================================================================= */
#ifndef TEST
int main(int argc, char *argv[])
{
    if (argc == 1) { print_usage(); return 0; }

    config_t cfg;
    if (parse_args(argc, argv, &cfg) != 0) {
        print_usage();
        return 1;
    }

    return cfg.mode == (app_mode_t)MODE_RECV
           ? run_recv(&cfg)
           : run_send(&cfg);
}
#endif
```

## Tests Files
### lab-test.c

```c

/*
 * tests/test_lab.c  –  Unity test suite for lab.h (layers 1 and 2)
 *
 * Covers every public function declared in lab.h.
 * No sockets, no relay, no real clock.  Fully deterministic.
 *
 * Build & run:
 *   make check
 *
 * Required coverage (per project spec):
 *   Checksum: RFC 1071 example, odd-length, single flipped bit caught
 *   pkt_decode rejects: too-short, length mismatch, unknown type, rsvd!=0
 *   Receiver: duplicate pkt, beyond-gap pkt, repeated FIN
 *   Sender: window full, cumulative ACK slide, duplicate ACK,
 *           timeout retransmit, 10-timeout give-up
 *   Files: empty, exact multiple of 1024 bytes
 *   In-memory channel: 20% loss/corrupt/dup, fixed seed, bytes verified
 */

#include "harness/unity.h"
#include "../src/lab.h"

#include <string.h>
#include <stdint.h>
#include <stdlib.h>
 
/* =========================================================================
 * Unity boilerplate
 * ======================================================================= */
void setUp(void)    {}
void tearDown(void) {}

/* =========================================================================
 * Minimal LCG for deterministic channel damage
 *
 * Knuth's constants; period 2^32.  Returns values in [0, 2^32).
 * ======================================================================= */
static uint32_t lcg_state;

static void lcg_seed(uint32_t s) { lcg_state = s; }

static uint32_t lcg_next(void)
{
    lcg_state = lcg_state * 1664525u + 1013904223u;
    return lcg_state;
}

/* probability p in [0,1]: returns 1 with that probability */
static int lcg_prob(double p)
{
    return (double)(lcg_next() & 0xffffffff) < p * 4294967296.0;
}

/* =========================================================================
 * In-memory packet channel
 *
 * Holds up to CHAN_CAP raw packet copies.  Duplicates are stored as two
 * separate entries so the consumer sees them one at a time.
 * ======================================================================= */
#define CHAN_CAP  512

typedef struct {
    uint8_t  data[PKT_MAX_LEN];
    size_t   len;
} chan_entry_t;

typedef struct {
    chan_entry_t entries[CHAN_CAP];
    int          head;   /* next to read  */
    int          tail;   /* next to write */
    int          count;
} chan_t;

static void chan_init(chan_t *c)
{
    memset(c, 0, sizeof(*c));
}

/* push a packet into the channel (returns 0 if full) */
static int chan_push(chan_t *c, const uint8_t *buf, size_t len)
{
    if (c->count >= CHAN_CAP) return 0;
    memcpy(c->entries[c->tail].data, buf, len);
    c->entries[c->tail].len = len;
    c->tail = (c->tail + 1) % CHAN_CAP;
    c->count++;
    return 1;
}

/*
 * chan_deliver: take the front entry, apply damage, push result(s) into dst.
 * loss=0.2, corrupt=0.2, dup=0.2 means each independently.
 * Returns 0 if the source channel was empty.
 */
static int chan_deliver(chan_t *src, chan_t *dst,
                        double loss, double corrupt, double dup)
{
    if (src->count == 0) return 0;

    chan_entry_t e = src->entries[src->head];
    src->head = (src->head + 1) % CHAN_CAP;
    src->count--;

    /* loss: drop silently */
    if (lcg_prob(loss)) return 1;

    /* corrupt: flip one bit */
    if (lcg_prob(corrupt)) {
        size_t byte_off = lcg_next() % e.len;
        int    bit      = (int)(lcg_next() % 8);
        e.data[byte_off] ^= (uint8_t)(1u << bit);
    }

    /* dup: deliver twice */
    if (lcg_prob(dup))
        chan_push(dst, e.data, e.len);

    chan_push(dst, e.data, e.len);
    return 1;
}

/* =========================================================================
 * Helper: decode a packet from a wire buffer; TEST_ASSERT on failure
 * ======================================================================= */
static pkt_t must_decode(const uint8_t *buf, size_t len)
{
    pkt_t p;
    TEST_ASSERT_MESSAGE(pkt_decode(buf, len, &p), "pkt_decode failed unexpectedly");
    return p;
}

/* =========================================================================
 * Helper: flip one bit at byte_off, bit position bit (0=LSB)
 * ======================================================================= */
static void flip_bit(uint8_t *buf, size_t byte_off, int bit)
{
    buf[byte_off] ^= (uint8_t)(1u << bit);
}

/* =========================================================================
 * ── LAYER 1: pkt_checksum ──────────────────────────────────────────────
 * ======================================================================= */

/* RFC 1071 §1 worked example: 00 01 f2 03 f4 f5 f6 f7 → 0x220d */
void test_checksum_rfc1071_example(void)
{
    uint8_t buf[] = {0x00,0x01,0xf2,0x03,0xf4,0xf5,0xf6,0xf7};
    TEST_ASSERT_EQUAL_HEX16(0x220d, pkt_checksum(buf, sizeof(buf)));
}

/* Verification: adding a checksum into the buffer gives 0x0000 */
void test_checksum_verify_roundtrip(void)
{
    uint8_t buf[] = {0x00,0x01,0xf2,0x03,0xf4,0xf5,0xf6,0xf7};
    uint16_t cs = pkt_checksum(buf, sizeof(buf));
    /* store big-endian */
    uint8_t full[10];
    memcpy(full, buf, 8);
    full[8] = (uint8_t)(cs >> 8);
    full[9] = (uint8_t)(cs & 0xff);
    TEST_ASSERT_EQUAL_HEX16(0x0000, pkt_checksum(full, 10));
}

/* Odd-length: single byte 0x01 → high byte 0x01, low byte 0x00 → sum 0x0100
 * complement → 0xfeff */
void test_checksum_odd_length(void)
{
    uint8_t buf[] = {0x01};
    TEST_ASSERT_EQUAL_HEX16(0xfeff, pkt_checksum(buf, 1));
}

/* Odd-length two-byte block: 0x01 0x02 = word 0x0102,
 * with a third byte 0x03 padded → 0x0300 added → sum = 0x0402,
 * complement = 0xfbfd */
void test_checksum_odd_three_bytes(void)
{
    uint8_t buf[] = {0x01, 0x02, 0x03};
    /* manually: 0x0102 + 0x0300 = 0x0402, ~0x0402 = 0xfbfd */
    TEST_ASSERT_EQUAL_HEX16(0xfbfd, pkt_checksum(buf, 3));
}

/* Spec Example 2: DATA seq=2 payload "Hi!" → checksum 0x9691 */
void test_checksum_data_hi(void)
{
    uint8_t buf[13] = {0};
    buf[0] = 0x00; buf[1] = 0x00;                          /* type, rsvd */
    /* checksum zeroed */
    buf[4]=0x00; buf[5]=0x00; buf[6]=0x00; buf[7]=0x02;   /* seq=2 BE */
    buf[8]=0x00; buf[9]=0x03;                               /* len=3 */
    buf[10]='H'; buf[11]='i'; buf[12]='!';
    TEST_ASSERT_EQUAL_HEX16(0x9691, pkt_checksum(buf, 13));
}

/* Spec Example 2: ACK 3 full packet → verification gives 0x0000 */
void test_checksum_ack3_verify(void)
{
    uint8_t buf[] = {0x01,0x00,0xfe,0xfc,0x00,0x00,0x00,0x03,0x00,0x00};
    TEST_ASSERT_EQUAL_HEX16(0x0000, pkt_checksum(buf, sizeof(buf)));
}

/* A single flipped bit must change the checksum */
void test_checksum_detects_single_bit_flip(void)
{
    uint8_t buf[PKT_MAX_LEN];
    uint8_t pay[8] = {0xDE,0xAD,0xBE,0xEF,0xCA,0xFE,0xBA,0xBE};
    size_t len = pkt_encode_data(buf, 42, pay, 8);

    /* flip each bit in the payload and check detection */
    int detected = 1;
    size_t i;
    for (i = 0; i < len; i++) {
        int b;
        for (b = 0; b < 8; b++) {
            uint8_t tmp[PKT_MAX_LEN];
            memcpy(tmp, buf, len);
            flip_bit(tmp, i, b);
            if (pkt_checksum(tmp, len) == 0x0000) {
                detected = 0;  /* this bit position evaded detection */
            }
        }
    }
    TEST_ASSERT_MESSAGE(detected,
        "a single flipped bit should always be caught by the checksum");
}

/* =========================================================================
 * ── LAYER 1: pkt_encode_* ──────────────────────────────────────────────
 * ======================================================================= */

void test_encode_data_length(void)
{
    uint8_t buf[PKT_MAX_LEN];
    uint8_t pay[5] = {1,2,3,4,5};
    size_t len = pkt_encode_data(buf, 0, pay, 5);
    TEST_ASSERT_EQUAL_size_t(PKT_HDR_LEN + 5, len);
}

void test_encode_data_fields(void)
{
    uint8_t buf[PKT_MAX_LEN];
    uint8_t pay[3] = {0xAA,0xBB,0xCC};
    pkt_encode_data(buf, 7, pay, 3);
    pkt_t p = must_decode(buf, PKT_HDR_LEN + 3);
    TEST_ASSERT_EQUAL_UINT8(PKT_TYPE_DATA, p.type);
    TEST_ASSERT_EQUAL_UINT32(7, p.seq);
    TEST_ASSERT_EQUAL_UINT16(3, p.payload_len);
    TEST_ASSERT_EQUAL_UINT8(0xAA, p.payload[0]);
    TEST_ASSERT_EQUAL_UINT8(0xCC, p.payload[2]);
}

void test_encode_data_max_payload(void)
{
    uint8_t buf[PKT_MAX_LEN];
    uint8_t pay[PKT_MAX_PAYLOAD];
    memset(pay, 0x5A, PKT_MAX_PAYLOAD);
    size_t len = pkt_encode_data(buf, 0, pay, PKT_MAX_PAYLOAD);
    TEST_ASSERT_EQUAL_size_t(PKT_MAX_LEN, len);
    pkt_t p = must_decode(buf, len);
    TEST_ASSERT_EQUAL_UINT16(PKT_MAX_PAYLOAD, p.payload_len);
    TEST_ASSERT_EQUAL_HEX8(0x5A, p.payload[0]);
    TEST_ASSERT_EQUAL_HEX8(0x5A, p.payload[PKT_MAX_PAYLOAD - 1]);
}

void test_encode_ack_length_and_fields(void)
{
    uint8_t buf[PKT_HDR_LEN];
    size_t len = pkt_encode_ack(buf, 99);
    TEST_ASSERT_EQUAL_size_t(PKT_HDR_LEN, len);
    pkt_t p = must_decode(buf, len);
    TEST_ASSERT_EQUAL_UINT8(PKT_TYPE_ACK, p.type);
    TEST_ASSERT_EQUAL_UINT32(99, p.seq);
    TEST_ASSERT_EQUAL_UINT16(0, p.payload_len);
}

void test_encode_fin_length_and_fields(void)
{
    uint8_t buf[PKT_HDR_LEN];
    size_t len = pkt_encode_fin(buf, 42);
    TEST_ASSERT_EQUAL_size_t(PKT_HDR_LEN, len);
    pkt_t p = must_decode(buf, len);
    TEST_ASSERT_EQUAL_UINT8(PKT_TYPE_FIN, p.type);
    TEST_ASSERT_EQUAL_UINT32(42, p.seq);
    TEST_ASSERT_EQUAL_UINT16(0, p.payload_len);
}

/* seq field round-trips at boundary values */
void test_encode_seq_boundary_values(void)
{
    uint8_t buf[PKT_HDR_LEN];
    pkt_t p;

    pkt_encode_ack(buf, 0);
    p = must_decode(buf, PKT_HDR_LEN);
    TEST_ASSERT_EQUAL_UINT32(0, p.seq);

    pkt_encode_ack(buf, 0xffffffff);
    p = must_decode(buf, PKT_HDR_LEN);
    TEST_ASSERT_EQUAL_UINT32(0xffffffff, p.seq);
}

/* Every encoded packet verifies (checksum over full packet == 0x0000) */
void test_encode_checksum_embeds_correctly(void)
{
    uint8_t buf[PKT_MAX_LEN];
    uint8_t pay[7] = {1,2,3,4,5,6,7};

    size_t len = pkt_encode_data(buf, 3, pay, 7);
    TEST_ASSERT_EQUAL_HEX16(0x0000, pkt_checksum(buf, len));

    len = pkt_encode_ack(buf, 5);
    TEST_ASSERT_EQUAL_HEX16(0x0000, pkt_checksum(buf, len));

    len = pkt_encode_fin(buf, 5);
    TEST_ASSERT_EQUAL_HEX16(0x0000, pkt_checksum(buf, len));
}

/* =========================================================================
 * ── LAYER 1: pkt_decode ─────────────────────────────────────────────────
 * ======================================================================= */

/* Happy path: DATA packet */
void test_decode_valid_data(void)
{
    uint8_t buf[PKT_MAX_LEN];
    uint8_t pay[4] = {9,8,7,6};
    size_t len = pkt_encode_data(buf, 5, pay, 4);
    pkt_t p;
    TEST_ASSERT_TRUE(pkt_decode(buf, len, &p));
    TEST_ASSERT_EQUAL_UINT8(PKT_TYPE_DATA, p.type);
    TEST_ASSERT_EQUAL_UINT32(5, p.seq);
    TEST_ASSERT_EQUAL_UINT16(4, p.payload_len);
    /* payload pointer must be inside the original buffer */
    TEST_ASSERT_TRUE(p.payload >= buf && p.payload < buf + len);
    TEST_ASSERT_EQUAL_UINT8(9, p.payload[0]);
}

/* Happy path: ACK */
void test_decode_valid_ack(void)
{
    uint8_t buf[PKT_HDR_LEN];
    pkt_encode_ack(buf, 17);
    pkt_t p;
    TEST_ASSERT_TRUE(pkt_decode(buf, PKT_HDR_LEN, &p));
    TEST_ASSERT_EQUAL_UINT8(PKT_TYPE_ACK, p.type);
    TEST_ASSERT_EQUAL_UINT32(17, p.seq);
    TEST_ASSERT_EQUAL_UINT16(0, p.payload_len);
    TEST_ASSERT_NULL(p.payload);
}

/* Happy path: FIN */
void test_decode_valid_fin(void)
{
    uint8_t buf[PKT_HDR_LEN];
    pkt_encode_fin(buf, 8);
    pkt_t p;
    TEST_ASSERT_TRUE(pkt_decode(buf, PKT_HDR_LEN, &p));
    TEST_ASSERT_EQUAL_UINT8(PKT_TYPE_FIN, p.type);
}

/* Rule 1: datagram shorter than 10 bytes is rejected */
void test_decode_rejects_too_short(void)
{
    uint8_t buf[PKT_MAX_LEN];
    uint8_t pay[1] = {0};
    pkt_encode_data(buf, 0, pay, 1);
    pkt_t p;
    /* try every length from 0 to 9 */
    size_t i;
    for (i = 0; i < PKT_HDR_LEN; i++)
        TEST_ASSERT_FALSE_MESSAGE(pkt_decode(buf, i, &p), "should reject short datagram");
}

/* Rule 2: length field says 100 bytes but datagram is only 13 bytes */
void test_decode_rejects_length_mismatch_short(void)
{
    uint8_t buf[PKT_MAX_LEN];
    uint8_t pay[3] = {1,2,3};
    size_t len = pkt_encode_data(buf, 0, pay, 3);  /* total 13 */
    /* tamper: set length field to 100 without updating checksum */
    buf[OFF_LEN]   = 0x00;
    buf[OFF_LEN+1] = 100;
    pkt_t p;
    TEST_ASSERT_FALSE(pkt_decode(buf, len, &p));
}

/* Rule 2: length field says 0 but datagram has extra payload bytes */
void test_decode_rejects_length_mismatch_long(void)
{
    uint8_t buf[PKT_MAX_LEN];
    uint8_t pay[5] = {1,2,3,4,5};
    size_t len = pkt_encode_data(buf, 0, pay, 5);  /* total 15 */
    /* claim length = 0 */
    buf[OFF_LEN] = 0x00; buf[OFF_LEN+1] = 0x00;
    pkt_t p;
    TEST_ASSERT_FALSE(pkt_decode(buf, len, &p));
}

/* Rule 2: length field > 1024 */
void test_decode_rejects_length_over_max(void)
{
    /* Craft a minimal valid-looking 10-byte header then set length=1025 */
    uint8_t buf[PKT_HDR_LEN + 1024 + 2] = {0};
    buf[OFF_TYPE]   = PKT_TYPE_DATA;
    buf[OFF_LEN]    = 0x04;   /* 0x0401 = 1025 */
    buf[OFF_LEN+1]  = 0x01;
    pkt_t p;
    /* datagram size = PKT_HDR_LEN + 1025 bytes, length field = 1025 → rule 2 */
    TEST_ASSERT_FALSE(pkt_decode(buf, PKT_HDR_LEN + 1025, &p));
}

/* Rule 3: unknown type (3, 127, 255) */
void test_decode_rejects_unknown_type(void)
{
    uint8_t buf[PKT_HDR_LEN];
    pkt_encode_ack(buf, 0);
    pkt_t p;

    uint8_t bad_types[] = {3, 4, 127, 255};
    size_t i;
    for (i = 0; i < sizeof(bad_types); i++) {
        uint8_t tmp[PKT_HDR_LEN];
        memcpy(tmp, buf, PKT_HDR_LEN);
        tmp[OFF_TYPE] = bad_types[i];
        TEST_ASSERT_FALSE(pkt_decode(tmp, PKT_HDR_LEN, &p));
    }
}

/* Rule 3: reserved byte != 0 */
void test_decode_rejects_nonzero_reserved(void)
{
    uint8_t buf[PKT_HDR_LEN];
    pkt_encode_ack(buf, 0);
    buf[OFF_RSVD] = 1;
    pkt_t p;
    TEST_ASSERT_FALSE(pkt_decode(buf, PKT_HDR_LEN, &p));
}

/* Rule 4: single flipped bit in payload is caught */
void test_decode_rejects_corrupted_payload(void)
{
    uint8_t buf[PKT_MAX_LEN];
    uint8_t pay[8] = {1,2,3,4,5,6,7,8};
    size_t len = pkt_encode_data(buf, 0, pay, 8);
    flip_bit(buf, OFF_PAYLOAD + 3, 5);
    pkt_t p;
    TEST_ASSERT_FALSE(pkt_decode(buf, len, &p));
}

/* Rule 4: single flipped bit in header is caught */
void test_decode_rejects_corrupted_header(void)
{
    uint8_t buf[PKT_HDR_LEN];
    pkt_encode_fin(buf, 0);
    /* flip a bit in the seq field */
    flip_bit(buf, OFF_SEQ + 2, 0);
    pkt_t p;
    TEST_ASSERT_FALSE(pkt_decode(buf, PKT_HDR_LEN, &p));
}

/* =========================================================================
 * ── LAYER 2: gbn_sender_init / gbn_sender_window_full ──────────────────
 * ======================================================================= */

void test_sender_init_zeroes_state(void)
{
    gbn_sender_t s;
    /* poison memory first */
    memset(&s, 0xFF, sizeof(s));
    gbn_sender_init(&s, 4, 250);
    TEST_ASSERT_EQUAL_INT(4,   s.winsize);
    TEST_ASSERT_EQUAL_INT(250, (int)s.timeout_ms);
    TEST_ASSERT_EQUAL_UINT32(0, s.base);
    TEST_ASSERT_EQUAL_UINT32(0, s.next);
    TEST_ASSERT_EQUAL_INT(0, s.eof_seen);
    TEST_ASSERT_EQUAL_INT(0, s.fin_sent);
    TEST_ASSERT_EQUAL_INT(0, s.fin_acked);
    TEST_ASSERT_EQUAL_INT(0, s.consec_timeouts);
    TEST_ASSERT_EQUAL_UINT64(0, (unsigned long long)s.timer_deadline_ms);
}

void test_sender_window_not_full_initially(void)
{
    gbn_sender_t s;
    gbn_sender_init(&s, 4, 250);
    TEST_ASSERT_FALSE(gbn_sender_window_full(&s));
}

/* Window becomes full at exactly winsize in-flight packets */
void test_sender_window_full_at_capacity(void)
{
    gbn_sender_t s;
    gbn_sender_init(&s, 3, 250);
    uint8_t pay[1] = {0};
    long long t = 0;

    gbn_sender_data(&s, t, pay, 1);
    TEST_ASSERT_FALSE(gbn_sender_window_full(&s));
    gbn_sender_data(&s, t, pay, 1);
    TEST_ASSERT_FALSE(gbn_sender_window_full(&s));
    gbn_sender_data(&s, t, pay, 1);
    TEST_ASSERT_TRUE(gbn_sender_window_full(&s));   /* exactly winsize=3 */
}

/* =========================================================================
 * ── LAYER 2: gbn_sender_data ───────────────────────────────────────────
 * ======================================================================= */

void test_sender_data_sends_one_packet(void)
{
    gbn_sender_t s;
    gbn_sender_init(&s, 4, 250);
    uint8_t pay[5] = {1,2,3,4,5};

    snd_action_t a = gbn_sender_data(&s, 0, pay, 5);
    TEST_ASSERT_EQUAL_INT(1, a.send_count);
    TEST_ASSERT_EQUAL_UINT32(1, s.next);
    TEST_ASSERT_EQUAL_INT(250, (int)a.timer_deadline_ms); /* t=0+250 */
}

void test_sender_data_packet_decodes_correctly(void)
{
    gbn_sender_t s;
    gbn_sender_init(&s, 4, 250);
    uint8_t pay[3] = {0xAA, 0xBB, 0xCC};

    snd_action_t a = gbn_sender_data(&s, 0, pay, 3);
    pkt_t p = must_decode(a.send_pkts[0].buf, a.send_pkts[0].len);
    TEST_ASSERT_EQUAL_UINT8(PKT_TYPE_DATA, p.type);
    TEST_ASSERT_EQUAL_UINT32(0, p.seq);
    TEST_ASSERT_EQUAL_UINT16(3, p.payload_len);
    TEST_ASSERT_EQUAL_UINT8(0xBB, p.payload[1]);
}

/* window full → data call returns send_count == 0 */
void test_sender_data_ignored_when_window_full(void)
{
    gbn_sender_t s;
    gbn_sender_init(&s, 2, 250);
    uint8_t pay[1] = {0};

    gbn_sender_data(&s, 0, pay, 1);
    gbn_sender_data(&s, 0, pay, 1);
    TEST_ASSERT_TRUE(gbn_sender_window_full(&s));

    snd_action_t a = gbn_sender_data(&s, 0, pay, 1);
    TEST_ASSERT_EQUAL_INT(0, a.send_count);
    TEST_ASSERT_EQUAL_UINT32(2, s.next);   /* did not advance */
}

/* Timer starts on first data, does not reset on subsequent data */
void test_sender_data_timer_starts_on_first(void)
{
    gbn_sender_t s;
    gbn_sender_init(&s, 4, 100);
    uint8_t pay[1] = {0};

    snd_action_t a0 = gbn_sender_data(&s, 1000, pay, 1);
    TEST_ASSERT_EQUAL_INT(1100, (int)a0.timer_deadline_ms);

    snd_action_t a1 = gbn_sender_data(&s, 2000, pay, 1);
    /* timer was already running; deadline must NOT be reset */
    TEST_ASSERT_EQUAL_INT(1100, (int)a1.timer_deadline_ms);
}

/* =========================================================================
 * ── LAYER 2: gbn_sender_eof ────────────────────────────────────────────
 * ======================================================================= */

/* Empty file: eof immediately → FIN seq 0 sent */
void test_sender_eof_empty_file(void)
{
    gbn_sender_t s;
    gbn_sender_init(&s, 8, 250);

    snd_action_t a = gbn_sender_eof(&s, 0);
    TEST_ASSERT_EQUAL_INT(1, a.send_count);
    TEST_ASSERT_EQUAL_UINT32(0, s.fin_seq);
    TEST_ASSERT_TRUE(s.fin_sent);
    TEST_ASSERT_EQUAL_INT(250, (int)a.timer_deadline_ms);

    pkt_t fp = must_decode(a.send_pkts[0].buf, a.send_pkts[0].len);
    TEST_ASSERT_EQUAL_UINT8(PKT_TYPE_FIN, fp.type);
    TEST_ASSERT_EQUAL_UINT32(0, fp.seq);
}

/* EOF with window full: FIN deferred until ACK opens a slot */
void test_sender_eof_deferred_when_window_full(void)
{
    gbn_sender_t s;
    gbn_sender_init(&s, 2, 250);
    uint8_t pay[1] = {0};
    long long t = 0;

    gbn_sender_data(&s, t, pay, 1);
    gbn_sender_data(&s, t, pay, 1);
    TEST_ASSERT_TRUE(gbn_sender_window_full(&s));

    snd_action_t a = gbn_sender_eof(&s, t);
    TEST_ASSERT_EQUAL_INT(0, a.send_count);   /* FIN not sent yet */
    TEST_ASSERT_TRUE(s.eof_seen);
    TEST_ASSERT_FALSE(s.fin_sent);

    /* ACK 1 slides base; FIN should appear now */
    a = gbn_sender_ack(&s, t+10, 1);
    TEST_ASSERT_EQUAL_INT(1, a.send_count);   /* FIN */
    pkt_t fp = must_decode(a.send_pkts[0].buf, a.send_pkts[0].len);
    TEST_ASSERT_EQUAL_UINT8(PKT_TYPE_FIN, fp.type);
    TEST_ASSERT_EQUAL_UINT32(2, fp.seq);      /* fin_seq = 2 DATA pkts */
}

/* EOF is idempotent */
void test_sender_eof_idempotent(void)
{
    gbn_sender_t s;
    gbn_sender_init(&s, 8, 250);

    snd_action_t a0 = gbn_sender_eof(&s, 0);
    snd_action_t a1 = gbn_sender_eof(&s, 0);
    TEST_ASSERT_EQUAL_INT(1, a0.send_count);
    TEST_ASSERT_EQUAL_INT(0, a1.send_count);  /* second call is no-op */
}

/* Exact multiple of 1024 bytes: FIN seq must equal number of DATA packets */
void test_sender_exact_multiple_of_1024(void)
{
    gbn_sender_t s;
    gbn_sender_init(&s, 16, 250);
    uint8_t pay[PKT_MAX_PAYLOAD];
    memset(pay, 0xAB, PKT_MAX_PAYLOAD);
    long long t = 0;
    int i;

    /* Send 4 full packets (= 4096 bytes) */
    for (i = 0; i < 4; i++)
        gbn_sender_data(&s, t, pay, PKT_MAX_PAYLOAD);

    /* ACK all of them */
    gbn_sender_ack(&s, t+10, 4);

    /* Signal EOF */
    snd_action_t a = gbn_sender_eof(&s, t+10);
    TEST_ASSERT_EQUAL_INT(1, a.send_count);
    pkt_t fp = must_decode(a.send_pkts[0].buf, a.send_pkts[0].len);
    TEST_ASSERT_EQUAL_UINT8(PKT_TYPE_FIN, fp.type);
    TEST_ASSERT_EQUAL_UINT32(4, fp.seq);   /* 4 DATA packets */
}

/* =========================================================================
 * ── LAYER 2: gbn_sender_ack ────────────────────────────────────────────
 * ======================================================================= */

/* Duplicate ACK (ack_seq <= base) is silently ignored */
void test_sender_ack_duplicate_ignored(void)
{
    gbn_sender_t s;
    gbn_sender_init(&s, 4, 250);
    uint8_t pay[1] = {0};

    gbn_sender_data(&s, 0, pay, 1);   /* pkt 0 */
    gbn_sender_data(&s, 0, pay, 1);   /* pkt 1 */
    gbn_sender_ack(&s, 10, 1);        /* base → 1 */

    snd_action_t a = gbn_sender_ack(&s, 20, 1);  /* duplicate */
    TEST_ASSERT_EQUAL_INT(0, a.send_count);
    TEST_ASSERT_EQUAL_UINT32(1, s.base);

    a = gbn_sender_ack(&s, 20, 0);   /* stale */
    TEST_ASSERT_EQUAL_INT(0, a.send_count);
    TEST_ASSERT_EQUAL_UINT32(1, s.base);
}

/* Cumulative ACK slides window by several at once */
void test_sender_ack_cumulative_slide(void)
{
    gbn_sender_t s;
    gbn_sender_init(&s, 8, 250);
    uint8_t pay[1] = {0};
    int i;

    for (i = 0; i < 5; i++)
        gbn_sender_data(&s, 0, pay, 1);

    /* ACKs 1,2 lost; ACK 3 arrives: slides base from 0 to 3 at once */
    snd_action_t a = gbn_sender_ack(&s, 100, 3);
    TEST_ASSERT_EQUAL_UINT32(3, s.base);
    TEST_ASSERT_EQUAL_INT(0, s.consec_timeouts);
    /* still have pkts 3,4 in flight: timer must be running */
    TEST_ASSERT_TRUE(a.timer_deadline_ms > 0);
}

/* ACK that drains the window stops the timer */
void test_sender_ack_drains_window_stops_timer(void)
{
    gbn_sender_t s;
    gbn_sender_init(&s, 4, 250);
    uint8_t pay[1] = {0};

    gbn_sender_data(&s, 0, pay, 1);
    gbn_sender_data(&s, 0, pay, 1);

    /* ACK 2: all DATA acked, no EOF yet → window empty, timer stops */
    snd_action_t a = gbn_sender_ack(&s, 50, 2);
    TEST_ASSERT_EQUAL_UINT32(2, s.base);
    TEST_ASSERT_EQUAL_INT(0, (int)a.timer_deadline_ms);
}

/* ACK resets consec_timeouts */
void test_sender_ack_resets_timeout_counter(void)
{
    gbn_sender_t s;
    gbn_sender_init(&s, 4, 100);
    uint8_t pay[1] = {0};
    long long t = 0;

    gbn_sender_data(&s, t, pay, 1);

    /* burn 3 timeouts */
    gbn_sender_timeout(&s, t+=100);
    gbn_sender_timeout(&s, t+=100);
    gbn_sender_timeout(&s, t+=100);
    TEST_ASSERT_EQUAL_INT(3, s.consec_timeouts);

    /* ACK arrives: counter must reset */
    gbn_sender_ack(&s, t+10, 1);
    TEST_ASSERT_EQUAL_INT(0, s.consec_timeouts);
}

/* FIN ACK sets done and fin_acked flags */
void test_sender_ack_fin_sets_done(void)
{
    gbn_sender_t s;
    gbn_sender_init(&s, 4, 250);

    gbn_sender_eof(&s, 0);            /* empty file, FIN seq=0 */
    snd_action_t a = gbn_sender_ack(&s, 10, 1);   /* ACK fin_seq+1 = 1 */
    TEST_ASSERT_TRUE(a.done);
    TEST_ASSERT_TRUE(s.fin_acked);
    TEST_ASSERT_EQUAL_INT(0, (int)a.timer_deadline_ms);
}

/* =========================================================================
 * ── LAYER 2: gbn_sender_timeout ────────────────────────────────────────
 * ======================================================================= */

/* Timeout retransmits every in-flight packet */
void test_sender_timeout_retransmits_window(void)
{
    gbn_sender_t s;
    gbn_sender_init(&s, 4, 250);
    uint8_t pay[3] = {1,2,3};
    long long t = 0;

    gbn_sender_data(&s, t, pay, 3);   /* pkt 0 */
    pay[0]=4; gbn_sender_data(&s, t, pay, 3);   /* pkt 1 */
    pay[0]=7; gbn_sender_data(&s, t, pay, 3);   /* pkt 2 */

    /* ACK 1: base slides to 1, pkts 1,2 still in flight */
    gbn_sender_ack(&s, t+50, 1);

    /* Timeout: should retransmit pkts 1 and 2 only */
    snd_action_t a = gbn_sender_timeout(&s, t+250);
    TEST_ASSERT_EQUAL_INT(2, a.send_count);
    TEST_ASSERT_EQUAL_INT(1, (int)s.consec_timeouts);
    TEST_ASSERT_EQUAL_INT((int)(t+250+250), (int)a.timer_deadline_ms);

    /* Verify the retransmitted packet seq numbers */
    pkt_t p0 = must_decode(a.send_pkts[0].buf, a.send_pkts[0].len);
    pkt_t p1 = must_decode(a.send_pkts[1].buf, a.send_pkts[1].len);
    TEST_ASSERT_EQUAL_UINT32(1, p0.seq);
    TEST_ASSERT_EQUAL_UINT32(2, p1.seq);
}

/* Timeout includes the FIN when it is in flight */
void test_sender_timeout_includes_fin(void)
{
    gbn_sender_t s;
    gbn_sender_init(&s, 4, 250);
    uint8_t pay[1] = {0};
    long long t = 0;

    gbn_sender_data(&s, t, pay, 1);   /* pkt 0 */
    gbn_sender_eof(&s, t);            /* FIN seq=1, immediately sent */

    /* Both pkt 0 and FIN are unACKed */
    snd_action_t a = gbn_sender_timeout(&s, t+250);
    TEST_ASSERT_EQUAL_INT(2, a.send_count);

    pkt_t p0 = must_decode(a.send_pkts[0].buf, a.send_pkts[0].len);
    pkt_t p1 = must_decode(a.send_pkts[1].buf, a.send_pkts[1].len);
    TEST_ASSERT_EQUAL_UINT8(PKT_TYPE_DATA, p0.type);
    TEST_ASSERT_EQUAL_UINT8(PKT_TYPE_FIN,  p1.type);
}

/* consec_timeouts increments; give_up fires at exactly GBN_MAX_TIMEOUTS */
void test_sender_timeout_give_up(void)
{
    gbn_sender_t s;
    gbn_sender_init(&s, 1, 100);
    uint8_t pay[1] = {0};
    long long t = 0;

    gbn_sender_data(&s, t, pay, 1);

    snd_action_t a;
    int i;
    for (i = 0; i < GBN_MAX_TIMEOUTS - 1; i++) {
        t += 100;
        a = gbn_sender_timeout(&s, t);
        TEST_ASSERT_FALSE_MESSAGE(a.give_up, "should not give up yet");
        TEST_ASSERT_EQUAL_INT(i + 1, s.consec_timeouts);
    }

    t += 100;
    a = gbn_sender_timeout(&s, t);
    TEST_ASSERT_TRUE(a.give_up);
    TEST_ASSERT_EQUAL_INT(0, a.send_count);         /* no retransmit on give-up */
    TEST_ASSERT_EQUAL_INT(0, (int)a.timer_deadline_ms); /* timer stopped */
}

/* =========================================================================
 * ── LAYER 2: gbn_recv_init / gbn_recv_pkt ──────────────────────────────
 * ======================================================================= */

void test_recv_init_zeroes_state(void)
{
    gbn_recv_t r;
    memset(&r, 0xFF, sizeof(r));
    gbn_recv_init(&r, 2000);
    TEST_ASSERT_EQUAL_UINT32(0, r.expected);
    TEST_ASSERT_EQUAL_INT(0, r.done);
    TEST_ASSERT_EQUAL_INT(2000, (int)r.linger_ms);
}

/* In-order DATA: payload delivered, expected advances, ACK sent */
void test_recv_inorder_data(void)
{
    gbn_recv_t r;
    gbn_recv_init(&r, 2000);
    uint8_t raw[PKT_MAX_LEN];
    uint8_t pay[4] = {1,2,3,4};
    pkt_t pkt;

    pkt_decode(raw, pkt_encode_data(raw, 0, pay, 4), &pkt);
    rcv_action_t a = gbn_recv_pkt(&r, 0, &pkt);

    TEST_ASSERT_EQUAL_UINT16(4, a.payload_len);
    TEST_ASSERT_EQUAL_UINT8(1, a.payload[0]);
    TEST_ASSERT_EQUAL_UINT8(4, a.payload[3]);
    TEST_ASSERT_EQUAL_size_t(PKT_HDR_LEN, a.ack_len);
    TEST_ASSERT_EQUAL_UINT32(1, r.expected);

    pkt_t ap = must_decode(a.ack_buf, a.ack_len);
    TEST_ASSERT_EQUAL_UINT8(PKT_TYPE_ACK, ap.type);
    TEST_ASSERT_EQUAL_UINT32(1, ap.seq);   /* ACK expected=1 */
    TEST_ASSERT_FALSE(a.done);
}

/* Duplicate packet (seq < expected): discard payload, re-ACK expected */
void test_recv_duplicate_packet(void)
{
    gbn_recv_t r;
    gbn_recv_init(&r, 2000);
    uint8_t raw[PKT_MAX_LEN];
    uint8_t pay[2] = {5,6};
    pkt_t pkt;

    /* deliver pkt 0 */
    pkt_decode(raw, pkt_encode_data(raw, 0, pay, 2), &pkt);
    gbn_recv_pkt(&r, 0, &pkt);
    TEST_ASSERT_EQUAL_UINT32(1, r.expected);

    /* deliver pkt 0 again (duplicate) */
    pkt_decode(raw, pkt_encode_data(raw, 0, pay, 2), &pkt);
    rcv_action_t a = gbn_recv_pkt(&r, 0, &pkt);
    TEST_ASSERT_EQUAL_UINT16(0, a.payload_len);   /* discarded */
    pkt_t ap = must_decode(a.ack_buf, a.ack_len);
    TEST_ASSERT_EQUAL_UINT32(1, ap.seq);           /* re-ACK expected=1 */
    TEST_ASSERT_EQUAL_UINT32(1, r.expected);       /* did not advance */
}

/* Packet beyond gap (seq > expected): discard, re-ACK expected */
void test_recv_beyond_gap(void)
{
    gbn_recv_t r;
    gbn_recv_init(&r, 2000);
    uint8_t raw[PKT_MAX_LEN];
    uint8_t pay[2] = {1,2};
    pkt_t pkt;

    /* pkt 0 delivered fine */
    pkt_decode(raw, pkt_encode_data(raw, 0, pay, 2), &pkt);
    gbn_recv_pkt(&r, 0, &pkt);

    /* pkt 2 arrives (gap at 1) */
    pkt_decode(raw, pkt_encode_data(raw, 2, pay, 2), &pkt);
    rcv_action_t a = gbn_recv_pkt(&r, 0, &pkt);
    TEST_ASSERT_EQUAL_UINT16(0, a.payload_len);    /* discarded */
    pkt_t ap = must_decode(a.ack_buf, a.ack_len);
    TEST_ASSERT_EQUAL_UINT32(1, ap.seq);            /* re-ACK expected=1 */
    TEST_ASSERT_EQUAL_UINT32(1, r.expected);        /* did not advance */

    /* pkt 5 arrives (even further ahead) */
    pkt_decode(raw, pkt_encode_data(raw, 5, pay, 2), &pkt);
    a = gbn_recv_pkt(&r, 0, &pkt);
    TEST_ASSERT_EQUAL_UINT16(0, a.payload_len);
    ap = must_decode(a.ack_buf, a.ack_len);
    TEST_ASSERT_EQUAL_UINT32(1, ap.seq);
}

/* FIN at the right seq: done set, correct ACK, linger deadline */
void test_recv_fin_sets_done(void)
{
    gbn_recv_t r;
    gbn_recv_init(&r, 2000);
    uint8_t raw[PKT_HDR_LEN];
    pkt_t pkt;

    pkt_decode(raw, pkt_encode_fin(raw, 0), &pkt);   /* FIN seq=0 (empty file) */
    rcv_action_t a = gbn_recv_pkt(&r, 1000, &pkt);

    TEST_ASSERT_TRUE(a.done);
    TEST_ASSERT_EQUAL_UINT16(0, a.payload_len);
    TEST_ASSERT_EQUAL_INT(3000, (int)a.linger_deadline_ms); /* 1000+2000 */

    pkt_t ap = must_decode(a.ack_buf, a.ack_len);
    TEST_ASSERT_EQUAL_UINT8(PKT_TYPE_ACK, ap.type);
    TEST_ASSERT_EQUAL_UINT32(1, ap.seq);   /* ACK fin_seq+1 = 1 */
}

/* Repeated FIN after done: re-ACK the same seq (linger behaviour) */
void test_recv_repeated_fin(void)
{
    gbn_recv_t r;
    gbn_recv_init(&r, 2000);
    uint8_t raw[PKT_MAX_LEN];
    uint8_t pay[3] = {1,2,3};
    pkt_t pkt;

    /* deliver two DATA pkts then a FIN */
    pkt_decode(raw, pkt_encode_data(raw, 0, pay, 3), &pkt);
    gbn_recv_pkt(&r, 0, &pkt);
    pkt_decode(raw, pkt_encode_data(raw, 1, pay, 3), &pkt);
    gbn_recv_pkt(&r, 0, &pkt);

    pkt_decode(raw, pkt_encode_fin(raw, 2), &pkt);
    rcv_action_t a = gbn_recv_pkt(&r, 0, &pkt);
    TEST_ASSERT_TRUE(a.done);
    TEST_ASSERT_EQUAL_UINT32(3, r.expected);

    /* FIN arrives again (relay resent it): receiver re-ACKs */
    pkt_decode(raw, pkt_encode_fin(raw, 2), &pkt);
    rcv_action_t a2 = gbn_recv_pkt(&r, 500, &pkt);
    TEST_ASSERT_EQUAL_size_t(PKT_HDR_LEN, a2.ack_len);
    pkt_t ap = must_decode(a2.ack_buf, a2.ack_len);
    TEST_ASSERT_EQUAL_UINT32(3, ap.seq);   /* still ACK 3 */
}

/* ACK packets arriving at receiver are silently ignored */
void test_recv_ignores_ack_packets(void)
{
    gbn_recv_t r;
    gbn_recv_init(&r, 2000);
    uint8_t raw[PKT_HDR_LEN];
    pkt_t pkt;

    pkt_decode(raw, pkt_encode_ack(raw, 5), &pkt);
    rcv_action_t a = gbn_recv_pkt(&r, 0, &pkt);
    TEST_ASSERT_EQUAL_size_t(0, a.ack_len);
    TEST_ASSERT_EQUAL_UINT16(0, a.payload_len);
    TEST_ASSERT_FALSE(a.done);
}

/* =========================================================================
 * ── FULL IN-MEMORY TRANSFER with simulated channel damage ───────────────
 *
 * Sender and receiver state machines connected by two chan_t queues.
 * The channel applies 20% loss, 20% corrupt, 20% dup independently to
 * every packet in both directions.  A fixed LCG seed makes it deterministic.
 *
 * We run the protocol to completion with a fake monotonic clock that
 * advances by cfg.timeout_ms every time the sender would block waiting for
 * an ACK, simulating timer expiry without sleeping.
 * ======================================================================= */

/* Simulate one complete file transfer through a damaging channel.
 * file_bytes: the "file" (caller-allocated).
 * file_len:   total bytes.
 * winsize:    sender window.
 * seed:       LCG seed for reproducibility.
 * Returns 1 if the received bytes exactly match the sent bytes, 0 on failure
 * (give-up or data corruption). */
static int run_transfer(const uint8_t *file_bytes, size_t file_len,
                        int winsize, uint32_t seed)
{
    lcg_seed(seed);

    gbn_sender_t snd;
    gbn_recv_t   rcv;
    gbn_sender_init(&snd, winsize, 250);
    gbn_recv_init(&rcv, 2000);

    /* Channels: snd→rcv (data_chan) and rcv→snd (ack_chan) */
    chan_t data_chan, ack_chan;
    chan_init(&data_chan);
    chan_init(&ack_chan);

    /* Buffer to collect what the receiver delivers */
    uint8_t *received = (uint8_t *)malloc(file_len + PKT_MAX_PAYLOAD);
    if (!received) return 0;
    size_t   recv_len = 0;

    long long t         = 0;
    size_t    file_pos  = 0;
    int       eof_done  = 0;

    /* Safety valve: a correct implementation finishes in well under
     * file_len/1024 * MAX_TIMEOUTS * 2 iterations. */
    int max_iter = (int)((file_len / PKT_MAX_PAYLOAD + 2)
                         * (GBN_MAX_TIMEOUTS + 1) * 100);
    if (max_iter < 10000) max_iter = 10000;

    int iter;
    for (iter = 0; iter < max_iter; iter++) {

        /* ── Sender: fill window from file ── */
        while (!gbn_sender_window_full(&snd) && !snd.eof_seen) {
            size_t remaining = file_len - file_pos;
            if (remaining == 0) {
                snd_action_t ea = gbn_sender_eof(&snd, t);
                int k;
                for (k = 0; k < ea.send_count; k++)
                    chan_push(&data_chan, ea.send_pkts[k].buf,
                              ea.send_pkts[k].len);
                eof_done = 1;
                break;
            }
            uint16_t chunk = (uint16_t)(remaining > PKT_MAX_PAYLOAD
                                        ? PKT_MAX_PAYLOAD : remaining);
            snd_action_t da = gbn_sender_data(&snd, t,
                                              file_bytes + file_pos, chunk);
            if (da.send_count == 0) break;  /* window unexpectedly full */
            int k;
            for (k = 0; k < da.send_count; k++)
                chan_push(&data_chan, da.send_pkts[k].buf,
                          da.send_pkts[k].len);
            file_pos += chunk;
            if (chunk < PKT_MAX_PAYLOAD && !eof_done) {
                snd_action_t ea = gbn_sender_eof(&snd, t);
                for (k = 0; k < ea.send_count; k++)
                    chan_push(&data_chan, ea.send_pkts[k].buf,
                              ea.send_pkts[k].len);
                eof_done = 1;
                break;
            }
        }

        /* ── Channel: deliver one data packet to receiver ── */
        if (data_chan.count > 0) {
            uint8_t raw[PKT_MAX_LEN];
            size_t  rawlen;

            /* peek at the front, run through damage, deliver to ack side */
            chan_entry_t front = data_chan.entries[data_chan.head];
            data_chan.head  = (data_chan.head + 1) % CHAN_CAP;
            data_chan.count--;

            /* apply damage into a temporary entry */
            chan_t tmp_src, tmp_dst;
            chan_init(&tmp_src); chan_init(&tmp_dst);
            chan_push(&tmp_src, front.data, front.len);
            chan_deliver(&tmp_src, &tmp_dst, 0.20, 0.20, 0.20);

            /* drain tmp_dst: feed each surviving copy to the receiver */
            while (tmp_dst.count > 0) {
                chan_entry_t e = tmp_dst.entries[tmp_dst.head];
                tmp_dst.head  = (tmp_dst.head + 1) % CHAN_CAP;
                tmp_dst.count--;

                rawlen = e.len;
                memcpy(raw, e.data, rawlen);

                pkt_t pkt;
                if (!pkt_decode(raw, rawlen, &pkt)) continue;

                rcv_action_t ra = gbn_recv_pkt(&rcv, t, &pkt);
                if (ra.payload_len > 0) {
                    memcpy(received + recv_len, ra.payload, ra.payload_len);
                    recv_len += ra.payload_len;
                }
                if (ra.ack_len > 0)
                    chan_push(&ack_chan, ra.ack_buf, ra.ack_len);
            }
        }

        /* ── Channel: deliver one ACK to sender ── */
        if (ack_chan.count > 0) {
            chan_t tmp_src, tmp_dst;
            chan_init(&tmp_src); chan_init(&tmp_dst);

            chan_entry_t front = ack_chan.entries[ack_chan.head];
            ack_chan.head  = (ack_chan.head + 1) % CHAN_CAP;
            ack_chan.count--;
            chan_push(&tmp_src, front.data, front.len);
            chan_deliver(&tmp_src, &tmp_dst, 0.20, 0.20, 0.20);

            while (tmp_dst.count > 0) {
                chan_entry_t e = tmp_dst.entries[tmp_dst.head];
                tmp_dst.head  = (tmp_dst.head + 1) % CHAN_CAP;
                tmp_dst.count--;

                pkt_t pkt;
                if (!pkt_decode(e.data, e.len, &pkt)) continue;
                if (pkt.type != PKT_TYPE_ACK) continue;

                snd_action_t sa = gbn_sender_ack(&snd, t, pkt.seq);
                int k;
                for (k = 0; k < sa.send_count; k++)
                    chan_push(&data_chan, sa.send_pkts[k].buf,
                              sa.send_pkts[k].len);
                if (sa.done) {
                    int ok = (recv_len == file_len) &&
                             (file_len == 0 ||
                              memcmp(received, file_bytes, file_len) == 0);
                    free(received);
                    return ok;
                }
            }
        }

        /* ── Timer: if sender has nothing pending and both channels are
         *    empty, advance the clock to trigger a timeout ── */
        if (data_chan.count == 0 && ack_chan.count == 0
                && snd.timer_deadline_ms > 0) {
            t = snd.timer_deadline_ms;
            snd_action_t ta = gbn_sender_timeout(&snd, t);
            if (ta.give_up) { free(received); return 0; }
            int k;
            for (k = 0; k < ta.send_count; k++)
                chan_push(&data_chan, ta.send_pkts[k].buf,
                          ta.send_pkts[k].len);
        }
    }

    /* Did not finish within max_iter */
    free(received);
    return 0;
}

/* Small file, seed 42 */
void test_channel_small_file_seed42(void)
{
    uint8_t file[500];
    size_t i;
    for (i = 0; i < sizeof(file); i++) file[i] = (uint8_t)(i & 0xff);
    TEST_ASSERT_TRUE(run_transfer(file, sizeof(file), 8, 42));
}

/* Small file, seed 1234 – different damage pattern */
void test_channel_small_file_seed1234(void)
{
    uint8_t file[500];
    size_t i;
    for (i = 0; i < sizeof(file); i++) file[i] = (uint8_t)(i * 3);
    TEST_ASSERT_TRUE(run_transfer(file, sizeof(file), 8, 1234));
}

/* Exact multiple of 1024: no partial last packet, FIN immediately follows */
void test_channel_exact_1024_multiple(void)
{
    uint8_t file[4096];   /* exactly 4 DATA packets */
    size_t i;
    for (i = 0; i < sizeof(file); i++) file[i] = (uint8_t)(i ^ 0xA5);
    TEST_ASSERT_TRUE(run_transfer(file, sizeof(file), 8, 7));
}

/* Empty file: just a FIN, no DATA */
void test_channel_empty_file(void)
{
    TEST_ASSERT_TRUE(run_transfer(NULL, 0, 8, 99));
}

/* Larger file, window=1 (stop-and-wait mode) */
void test_channel_large_file_window1(void)
{
    uint8_t file[5000];
    size_t i;
    for (i = 0; i < sizeof(file); i++) file[i] = (uint8_t)(i * 7);
    TEST_ASSERT_TRUE(run_transfer(file, sizeof(file), 1, 13));
}

/* Larger file, window=16 */
void test_channel_large_file_window16(void)
{
    uint8_t file[16000];
    size_t i;
    for (i = 0; i < sizeof(file); i++) file[i] = (uint8_t)(i * 3 + 1);
    TEST_ASSERT_TRUE(run_transfer(file, sizeof(file), 16, 2025));
}

/* Different seed to vary damage positions */
void test_channel_seed_999(void)
{
    uint8_t file[3000];
    size_t i;
    for (i = 0; i < sizeof(file); i++) file[i] = (uint8_t)(i);
    TEST_ASSERT_TRUE(run_transfer(file, sizeof(file), 4, 999));
}

/* =========================================================================
 * main – Unity test runner
 * ======================================================================= */
int main(void)
{
    UNITY_BEGIN();

    /* pkt_checksum */
    RUN_TEST(test_checksum_rfc1071_example);
    RUN_TEST(test_checksum_verify_roundtrip);
    RUN_TEST(test_checksum_odd_length);
    RUN_TEST(test_checksum_odd_three_bytes);
    RUN_TEST(test_checksum_data_hi);
    RUN_TEST(test_checksum_ack3_verify);
    RUN_TEST(test_checksum_detects_single_bit_flip);

    /* pkt_encode_* */
    RUN_TEST(test_encode_data_length);
    RUN_TEST(test_encode_data_fields);
    RUN_TEST(test_encode_data_max_payload);
    RUN_TEST(test_encode_ack_length_and_fields);
    RUN_TEST(test_encode_fin_length_and_fields);
    RUN_TEST(test_encode_seq_boundary_values);
    RUN_TEST(test_encode_checksum_embeds_correctly);

    /* pkt_decode */
    RUN_TEST(test_decode_valid_data);
    RUN_TEST(test_decode_valid_ack);
    RUN_TEST(test_decode_valid_fin);
    RUN_TEST(test_decode_rejects_too_short);
    RUN_TEST(test_decode_rejects_length_mismatch_short);
    RUN_TEST(test_decode_rejects_length_mismatch_long);
    RUN_TEST(test_decode_rejects_length_over_max);
    RUN_TEST(test_decode_rejects_unknown_type);
    RUN_TEST(test_decode_rejects_nonzero_reserved);
    RUN_TEST(test_decode_rejects_corrupted_payload);
    RUN_TEST(test_decode_rejects_corrupted_header);

    /* gbn_sender_init / window_full */
    RUN_TEST(test_sender_init_zeroes_state);
    RUN_TEST(test_sender_window_not_full_initially);
    RUN_TEST(test_sender_window_full_at_capacity);

    /* gbn_sender_data */
    RUN_TEST(test_sender_data_sends_one_packet);
    RUN_TEST(test_sender_data_packet_decodes_correctly);
    RUN_TEST(test_sender_data_ignored_when_window_full);
    RUN_TEST(test_sender_data_timer_starts_on_first);

    /* gbn_sender_eof */
    RUN_TEST(test_sender_eof_empty_file);
    RUN_TEST(test_sender_eof_deferred_when_window_full);
    RUN_TEST(test_sender_eof_idempotent);
    RUN_TEST(test_sender_exact_multiple_of_1024);

    /* gbn_sender_ack */
    RUN_TEST(test_sender_ack_duplicate_ignored);
    RUN_TEST(test_sender_ack_cumulative_slide);
    RUN_TEST(test_sender_ack_drains_window_stops_timer);
    RUN_TEST(test_sender_ack_resets_timeout_counter);
    RUN_TEST(test_sender_ack_fin_sets_done);

    /* gbn_sender_timeout */
    RUN_TEST(test_sender_timeout_retransmits_window);
    RUN_TEST(test_sender_timeout_includes_fin);
    RUN_TEST(test_sender_timeout_give_up);

    /* gbn_recv_init / gbn_recv_pkt */
    RUN_TEST(test_recv_init_zeroes_state);
    RUN_TEST(test_recv_inorder_data);
    RUN_TEST(test_recv_duplicate_packet);
    RUN_TEST(test_recv_beyond_gap);
    RUN_TEST(test_recv_fin_sets_done);
    RUN_TEST(test_recv_repeated_fin);
    RUN_TEST(test_recv_ignores_ack_packets);

    /* in-memory channel transfers */
    RUN_TEST(test_channel_small_file_seed42);
    RUN_TEST(test_channel_small_file_seed1234);
    RUN_TEST(test_channel_exact_1024_multiple);
    RUN_TEST(test_channel_empty_file);
    RUN_TEST(test_channel_large_file_window1);
    RUN_TEST(test_channel_large_file_window16);
    RUN_TEST(test_channel_seed_999);

    return UNITY_END();
}
```

## Scripts Files
Report generated on 10/12/2026 at 03:08:22


---

## End of Report

SHA-256 Hash of the report: 2ae44cf7a14849b8ef446b83fd13493894d247d0d2618f39cd297649e61a5c15

Do not edit the generated report. Any changes will be reported as academic dishonesty

---
## GitHub Info
- GitHub repo name: mackenziew814/CS425-P2
- The repository visibility is public.
- The workflow was triggered by mackenziew814
