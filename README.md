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

