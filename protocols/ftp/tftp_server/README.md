## Example of usage 

See [tftp client readme](../tftp_client/README.md) for client output.

```
$ ./tftps
Usage: tftps port

$ ./tftps 6900
Listening port 6900
Received write request from 127.0.0.1:44748
C: 00 02 s  o  u  r  c  e  .  t  x  t  00 o  c  t  e  t  00
S: 00 04 00 00
C: 00 03 00 01 [ 512 bytes of data ]
S: 00 04 00 01
C: 00 03 00 02 [ 512 bytes of data ]
S: 00 04 00 02
C: 00 03 00 03 [ 258 bytes of data ] (last)
S: 00 04 00 03
Closing connection to client 127.0.0.1:44748
Received read request from 127.0.0.1:42073
C: 00 01 t  a  r  g  e  t  .  t  x  t  00 o  c  t  e  t  00
S: 00 03 00 01 [ 512 bytes of data ]
C: 00 04 00 01
S: 00 03 00 02 [ 512 bytes of data ]
C: 00 04 00 02
S: 00 03 00 03 [ 512 bytes of data ]
C: 00 04 00 03
S: 00 03 00 04 [ 512 bytes of data ]
C: 00 04 00 04
S: 00 03 00 05 [ 192 bytes of data ] (last)
C: 00 04 00 05
Closing connection to client 127.0.0.1:42073
^C
Interrupt, quitting...

$ sha256sum target.txt source.txt
b06fb6a6eb2e0389503ff355236f57ca921b518fe65fc327e4d88df4f520cab6  target.txt
85a9d734d9897b9a53d8f871bdd452d42e330cc7f3870f55766e846b3c32f253  source.txt
```

## Random errors test:

- [Packet capture](packet_captures/read_request.pcap) from read request
- [Packet capture](packet_captures/write_request.pcap) from write request

### Preparations

```
Packet delay (delay + jitter), loss, duplication and reordering (affects both server and client):
$ doas tc qdisc add dev lo root netem delay 100ms 50ms loss 20% duplicate 20% reorder 20%

Create random test file on server:
$ dd if=/dev/random of=server_file.bin bs=100B count=200
200+0 records in
200+0 records out
20000 bytes (20 kB, 20 KiB) copied, 0.00028928 s, 69.1 MB/s

Create random test file on client:
$ dd if=/dev/random of=client_file.bin bs=100B count=200
200+0 records in
200+0 records out
20000 bytes (20 kB, 20 KiB) copied, 0.00028841 s, 69.3 MB/s
```

### Read request test

```
Client:
$ ./tftpc -r server_file.bin localhost 6900
Reading server_file.bin from 127.0.0.1:6900
C: 00 01 s  e  r  v  e  r  _  f  i  l  e  .  b  i  n  00 o  c  t  e  t  00
S: 00 03 00 01 [ 512 bytes of data ]
C: 00 04 00 01
C: 00 04 00 01
S: 00 03 00 02 [ 512 bytes of data ]
C: 00 04 00 02
S: 00 03 00 02 [ 512 bytes of data ]
C: 00 04 00 02
S: 00 03 00 02 [ 512 bytes of data ]
C: 00 04 00 02
S: 00 03 00 03 [ 512 bytes of data ]
C: 00 04 00 03
S: 00 03 00 03 [ 512 bytes of data ]
C: 00 04 00 03
S: 00 03 00 03 [ 512 bytes of data ]
C: 00 04 00 03
S: 00 03 00 03 [ 512 bytes of data ]
C: 00 04 00 03
S: 00 03 00 04 [ 512 bytes of data ]
C: 00 04 00 04
S: 00 03 00 04 [ 512 bytes of data ]
C: 00 04 00 04
S: 00 03 00 04 [ 512 bytes of data ]
C: 00 04 00 04
S: 00 03 00 04 [ 512 bytes of data ]
C: 00 04 00 04
S: 00 03 00 04 [ 512 bytes of data ]
C: 00 04 00 04
S: 00 03 00 05 [ 512 bytes of data ]
C: 00 04 00 05
S: 00 03 00 05 [ 512 bytes of data ]
C: 00 04 00 05
S: 00 03 00 04 [ 512 bytes of data ]
C: 00 04 00 05
S: 00 03 00 06 [ 512 bytes of data ]
C: 00 04 00 06
S: 00 03 00 04 [ 512 bytes of data ]
C: 00 04 00 06
S: 00 03 00 06 [ 512 bytes of data ]
C: 00 04 00 06
S: 00 03 00 06 [ 512 bytes of data ]
C: 00 04 00 06
S: 00 03 00 06 [ 512 bytes of data ]
C: 00 04 00 06
S: 00 03 00 06 [ 512 bytes of data ]
C: 00 04 00 06
S: 00 03 00 06 [ 512 bytes of data ]
C: 00 04 00 06
S: 00 03 00 07 [ 512 bytes of data ]
C: 00 04 00 07
S: 00 03 00 07 [ 512 bytes of data ]
C: 00 04 00 07
S: 00 03 00 06 [ 512 bytes of data ]
C: 00 04 00 07
S: 00 03 00 07 [ 512 bytes of data ]
C: 00 04 00 07
S: 00 03 00 07 [ 512 bytes of data ]
C: 00 04 00 07
S: 00 03 00 08 [ 512 bytes of data ]
C: 00 04 00 08
S: 00 03 00 07 [ 512 bytes of data ]
C: 00 04 00 08
S: 00 03 00 08 [ 512 bytes of data ]
C: 00 04 00 08
S: 00 03 00 09 [ 512 bytes of data ]
C: 00 04 00 09
S: 00 03 00 09 [ 512 bytes of data ]
C: 00 04 00 09
S: 00 03 00 0a [ 512 bytes of data ]
C: 00 04 00 0a
S: 00 03 00 0a [ 512 bytes of data ]
C: 00 04 00 0a
S: 00 03 00 0a [ 512 bytes of data ]
C: 00 04 00 0a
S: 00 03 00 0a [ 512 bytes of data ]
C: 00 04 00 0a
S: 00 03 00 0b [ 512 bytes of data ]
C: 00 04 00 0b
S: 00 03 00 0c [ 512 bytes of data ]
C: 00 04 00 0c
S: 00 03 00 0d [ 512 bytes of data ]
C: 00 04 00 0d
S: 00 03 00 0d [ 512 bytes of data ]
C: 00 04 00 0d
S: 00 03 00 0d [ 512 bytes of data ]
C: 00 04 00 0d
S: 00 03 00 0e [ 512 bytes of data ]
C: 00 04 00 0e
S: 00 03 00 0e [ 512 bytes of data ]
C: 00 04 00 0e
S: 00 03 00 0e [ 512 bytes of data ]
C: 00 04 00 0e
S: 00 03 00 0e [ 512 bytes of data ]
C: 00 04 00 0e
S: 00 03 00 0e [ 512 bytes of data ]
C: 00 04 00 0e
S: 00 03 00 0f [ 512 bytes of data ]
C: 00 04 00 0f
S: 00 03 00 10 [ 512 bytes of data ]
C: 00 04 00 10
S: 00 03 00 10 [ 512 bytes of data ]
C: 00 04 00 10
S: 00 03 00 10 [ 512 bytes of data ]
C: 00 04 00 10
S: 00 03 00 10 [ 512 bytes of data ]
C: 00 04 00 10
S: 00 03 00 10 [ 512 bytes of data ]
C: 00 04 00 10
S: 00 03 00 11 [ 512 bytes of data ]
C: 00 04 00 11
S: 00 03 00 11 [ 512 bytes of data ]
C: 00 04 00 11
S: 00 03 00 11 [ 512 bytes of data ]
C: 00 04 00 11
S: 00 03 00 11 [ 512 bytes of data ]
C: 00 04 00 11
S: 00 03 00 11 [ 512 bytes of data ]
C: 00 04 00 11
S: 00 03 00 12 [ 512 bytes of data ]
C: 00 04 00 12
S: 00 03 00 12 [ 512 bytes of data ]
C: 00 04 00 12
S: 00 03 00 13 [ 512 bytes of data ]
C: 00 04 00 13
S: 00 03 00 14 [ 512 bytes of data ]
C: 00 04 00 14
S: 00 03 00 14 [ 512 bytes of data ]
C: 00 04 00 14
S: 00 03 00 15 [ 512 bytes of data ]
C: 00 04 00 15
S: 00 03 00 15 [ 512 bytes of data ]
C: 00 04 00 15
S: 00 03 00 15 [ 512 bytes of data ]
C: 00 04 00 15
S: 00 03 00 15 [ 512 bytes of data ]
C: 00 04 00 15
S: 00 03 00 15 [ 512 bytes of data ]
C: 00 04 00 15
S: 00 03 00 16 [ 512 bytes of data ]
C: 00 04 00 16
S: 00 03 00 16 [ 512 bytes of data ]
C: 00 04 00 16
S: 00 03 00 16 [ 512 bytes of data ]
C: 00 04 00 16
S: 00 03 00 16 [ 512 bytes of data ]
C: 00 04 00 16
S: 00 03 00 17 [ 512 bytes of data ]
C: 00 04 00 17
S: 00 03 00 18 [ 512 bytes of data ]
C: 00 04 00 18
S: 00 03 00 18 [ 512 bytes of data ]
C: 00 04 00 18
S: 00 03 00 19 [ 512 bytes of data ]
C: 00 04 00 19
S: 00 03 00 1a [ 512 bytes of data ]
C: 00 04 00 1a
S: 00 03 00 18 [ 512 bytes of data ]
C: 00 04 00 1a
S: 00 03 00 18 [ 512 bytes of data ]
C: 00 04 00 1a
S: 00 03 00 1b [ 512 bytes of data ]
C: 00 04 00 1b
S: 00 03 00 1a [ 512 bytes of data ]
C: 00 04 00 1b
S: 00 03 00 1b [ 512 bytes of data ]
C: 00 04 00 1b
S: 00 03 00 18 [ 512 bytes of data ]
C: 00 04 00 1b
S: 00 03 00 19 [ 512 bytes of data ]
C: 00 04 00 1b
S: 00 03 00 1b [ 512 bytes of data ]
C: 00 04 00 1b
S: 00 03 00 19 [ 512 bytes of data ]
C: 00 04 00 1b
S: 00 03 00 1c [ 512 bytes of data ]
C: 00 04 00 1c
S: 00 03 00 1c [ 512 bytes of data ]
C: 00 04 00 1c
S: 00 03 00 1c [ 512 bytes of data ]
C: 00 04 00 1c
S: 00 03 00 1c [ 512 bytes of data ]
C: 00 04 00 1c
S: 00 03 00 1c [ 512 bytes of data ]
C: 00 04 00 1c
S: 00 03 00 1d [ 512 bytes of data ]
C: 00 04 00 1d
S: 00 03 00 1d [ 512 bytes of data ]
C: 00 04 00 1d
S: 00 03 00 1d [ 512 bytes of data ]
C: 00 04 00 1d
S: 00 03 00 1e [ 512 bytes of data ]
C: 00 04 00 1e
S: 00 03 00 1e [ 512 bytes of data ]
C: 00 04 00 1e
S: 00 03 00 1e [ 512 bytes of data ]
C: 00 04 00 1e
S: 00 03 00 1e [ 512 bytes of data ]
C: 00 04 00 1e
S: 00 03 00 1f [ 512 bytes of data ]
C: 00 04 00 1f
S: 00 03 00 1f [ 512 bytes of data ]
C: 00 04 00 1f
S: 00 03 00 1f [ 512 bytes of data ]
C: 00 04 00 1f
S: 00 03 00 1f [ 512 bytes of data ]
C: 00 04 00 1f
S: 00 03 00 20 [ 512 bytes of data ]
C: 00 04 00 20
S: 00 03 00 20 [ 512 bytes of data ]
C: 00 04 00 20
S: 00 03 00 21 [ 512 bytes of data ]
C: 00 04 00 21
S: 00 03 00 22 [ 512 bytes of data ]
C: 00 04 00 22
S: 00 03 00 23 [ 512 bytes of data ]
C: 00 04 00 23
S: 00 03 00 23 [ 512 bytes of data ]
C: 00 04 00 23
C: 00 04 00 23
S: 00 03 00 24 [ 512 bytes of data ]
C: 00 04 00 24
C: 00 04 00 24
S: 00 03 00 24 [ 512 bytes of data ]
C: 00 04 00 24
S: 00 03 00 25 [ 512 bytes of data ]
C: 00 04 00 25
S: 00 03 00 26 [ 512 bytes of data ]
C: 00 04 00 26
S: 00 03 00 27 [ 512 bytes of data ]
C: 00 04 00 27
C: 00 04 00 27
S: 00 03 00 28 [ 32 bytes of data ] (last)
C: 00 04 00 28
S: 00 03 00 28 [ 32 bytes of data ] (last)
C: 00 04 00 28
Reading succeeded, disconnecting...

Server:
Listening port 6900
Received read request from 127.0.0.1:54323
C: 00 01 s  e  r  v  e  r  _  f  i  l  e  .  b  i  n  00 o  c  t  e  t  00
S: 00 03 00 01 [ 512 bytes of data ]
C: 00 04 00 01
S: 00 03 00 02 [ 512 bytes of data ]
S: 00 03 00 02 [ 512 bytes of data ]
C: 00 04 00 01
S: 00 03 00 02 [ 512 bytes of data ]
C: 00 04 00 02
S: 00 03 00 03 [ 512 bytes of data ]
C: 00 04 00 02
S: 00 03 00 03 [ 512 bytes of data ]
C: 00 04 00 02
S: 00 03 00 03 [ 512 bytes of data ]
C: 00 04 00 03
S: 00 03 00 04 [ 512 bytes of data ]
C: 00 04 00 03
S: 00 03 00 04 [ 512 bytes of data ]
C: 00 04 00 03
S: 00 03 00 04 [ 512 bytes of data ]
C: 00 04 00 03
S: 00 03 00 04 [ 512 bytes of data ]
C: 00 04 00 03
S: 00 03 00 04 [ 512 bytes of data ]
C: 00 04 00 03
S: 00 03 00 04 [ 512 bytes of data ]
C: 00 04 00 04
S: 00 03 00 05 [ 512 bytes of data ]
C: 00 04 00 05
S: 00 03 00 06 [ 512 bytes of data ]
C: 00 04 00 04
S: 00 03 00 06 [ 512 bytes of data ]
C: 00 04 00 04
S: 00 03 00 06 [ 512 bytes of data ]
C: 00 04 00 04
S: 00 03 00 06 [ 512 bytes of data ]
C: 00 04 00 05
S: 00 03 00 06 [ 512 bytes of data ]
C: 00 04 00 05
S: 00 03 00 06 [ 512 bytes of data ]
C: 00 04 00 05
S: 00 03 00 06 [ 512 bytes of data ]
C: 00 04 00 06
S: 00 03 00 07 [ 512 bytes of data ]
C: 00 04 00 06
S: 00 03 00 07 [ 512 bytes of data ]
C: 00 04 00 06
S: 00 03 00 07 [ 512 bytes of data ]
C: 00 04 00 06
S: 00 03 00 07 [ 512 bytes of data ]
C: 00 04 00 06
S: 00 03 00 07 [ 512 bytes of data ]
C: 00 04 00 06
S: 00 03 00 07 [ 512 bytes of data ]
C: 00 04 00 06
S: 00 03 00 07 [ 512 bytes of data ]
C: 00 04 00 07
S: 00 03 00 08 [ 512 bytes of data ]
C: 00 04 00 07
S: 00 03 00 08 [ 512 bytes of data ]
C: 00 04 00 08
S: 00 03 00 09 [ 512 bytes of data ]
C: 00 04 00 07
S: 00 03 00 09 [ 512 bytes of data ]
C: 00 04 00 07
S: 00 03 00 09 [ 512 bytes of data ]
C: 00 04 00 08
S: 00 03 00 09 [ 512 bytes of data ]
C: 00 04 00 09
S: 00 03 00 0a [ 512 bytes of data ]
C: 00 04 00 08
S: 00 03 00 0a [ 512 bytes of data ]
C: 00 04 00 09
S: 00 03 00 0a [ 512 bytes of data ]
C: 00 04 00 0a
S: 00 03 00 0b [ 512 bytes of data ]
C: 00 04 00 0a
S: 00 03 00 0b [ 512 bytes of data ]
C: 00 04 00 0b
S: 00 03 00 0c [ 512 bytes of data ]
C: 00 04 00 0c
S: 00 03 00 0d [ 512 bytes of data ]
C: 00 04 00 0a
S: 00 03 00 0d [ 512 bytes of data ]
C: 00 04 00 0d
S: 00 03 00 0e [ 512 bytes of data ]
C: 00 04 00 0d
S: 00 03 00 0e [ 512 bytes of data ]
C: 00 04 00 0d
S: 00 03 00 0e [ 512 bytes of data ]
C: 00 04 00 0e
S: 00 03 00 0f [ 512 bytes of data ]
C: 00 04 00 0f
S: 00 03 00 10 [ 512 bytes of data ]
C: 00 04 00 0e
S: 00 03 00 10 [ 512 bytes of data ]
C: 00 04 00 0e
S: 00 03 00 10 [ 512 bytes of data ]
C: 00 04 00 0f
S: 00 03 00 10 [ 512 bytes of data ]
C: 00 04 00 0e
S: 00 03 00 10 [ 512 bytes of data ]
C: 00 04 00 10
S: 00 03 00 11 [ 512 bytes of data ]
C: 00 04 00 10
S: 00 03 00 11 [ 512 bytes of data ]
C: 00 04 00 10
S: 00 03 00 11 [ 512 bytes of data ]
C: 00 04 00 10
S: 00 03 00 11 [ 512 bytes of data ]
C: 00 04 00 10
S: 00 03 00 11 [ 512 bytes of data ]
C: 00 04 00 11
S: 00 03 00 12 [ 512 bytes of data ]
C: 00 04 00 11
S: 00 03 00 12 [ 512 bytes of data ]
C: 00 04 00 11
S: 00 03 00 12 [ 512 bytes of data ]
C: 00 04 00 12
S: 00 03 00 13 [ 512 bytes of data ]
C: 00 04 00 13
S: 00 03 00 14 [ 512 bytes of data ]
C: 00 04 00 14
S: 00 03 00 15 [ 512 bytes of data ]
C: 00 04 00 14
S: 00 03 00 15 [ 512 bytes of data ]
C: 00 04 00 14
S: 00 03 00 15 [ 512 bytes of data ]
C: 00 04 00 15
S: 00 03 00 16 [ 512 bytes of data ]
C: 00 04 00 15
S: 00 03 00 16 [ 512 bytes of data ]
C: 00 04 00 15
S: 00 03 00 16 [ 512 bytes of data ]
C: 00 04 00 15
S: 00 03 00 16 [ 512 bytes of data ]
C: 00 04 00 16
S: 00 03 00 17 [ 512 bytes of data ]
C: 00 04 00 17
S: 00 03 00 18 [ 512 bytes of data ]
C: 00 04 00 16
S: 00 03 00 18 [ 512 bytes of data ]
C: 00 04 00 16
S: 00 03 00 18 [ 512 bytes of data ]
C: 00 04 00 18
S: 00 03 00 19 [ 512 bytes of data ]
C: 00 04 00 18
S: 00 03 00 19 [ 512 bytes of data ]
C: 00 04 00 19
S: 00 03 00 1a [ 512 bytes of data ]
C: 00 04 00 1a
S: 00 03 00 1b [ 512 bytes of data ]
C: 00 04 00 1a
S: 00 03 00 1b [ 512 bytes of data ]
C: 00 04 00 1b
S: 00 03 00 1c [ 512 bytes of data ]
C: 00 04 00 1a
S: 00 03 00 1c [ 512 bytes of data ]
C: 00 04 00 1b
S: 00 03 00 1c [ 512 bytes of data ]
C: 00 04 00 1b
S: 00 03 00 1c [ 512 bytes of data ]
C: 00 04 00 1b
S: 00 03 00 1c [ 512 bytes of data ]
C: 00 04 00 1b
S: 00 03 00 1c [ 512 bytes of data ]
C: 00 04 00 1b
S: 00 03 00 1c [ 512 bytes of data ]
C: 00 04 00 1b
S: 00 03 00 1c [ 512 bytes of data ]
C: 00 04 00 1c
S: 00 03 00 1d [ 512 bytes of data ]
C: 00 04 00 1c
S: 00 03 00 1d [ 512 bytes of data ]
C: 00 04 00 1d
S: 00 03 00 1e [ 512 bytes of data ]
C: 00 04 00 1d
S: 00 03 00 1e [ 512 bytes of data ]
C: 00 04 00 1d
S: 00 03 00 1e [ 512 bytes of data ]
C: 00 04 00 1e
S: 00 03 00 1f [ 512 bytes of data ]
C: 00 04 00 1e
S: 00 03 00 1f [ 512 bytes of data ]
C: 00 04 00 1e
S: 00 03 00 1f [ 512 bytes of data ]
C: 00 04 00 1e
S: 00 03 00 1f [ 512 bytes of data ]
C: 00 04 00 1f
S: 00 03 00 20 [ 512 bytes of data ]
C: 00 04 00 1f
S: 00 03 00 20 [ 512 bytes of data ]
C: 00 04 00 1f
S: 00 03 00 20 [ 512 bytes of data ]
C: 00 04 00 1f
S: 00 03 00 20 [ 512 bytes of data ]
C: 00 04 00 20
S: 00 03 00 21 [ 512 bytes of data ]
C: 00 04 00 21
S: 00 03 00 22 [ 512 bytes of data ]
C: 00 04 00 22
S: 00 03 00 23 [ 512 bytes of data ]
C: 00 04 00 23
S: 00 03 00 24 [ 512 bytes of data ]
C: 00 04 00 23
S: 00 03 00 24 [ 512 bytes of data ]
S: 00 03 00 24 [ 512 bytes of data ]
C: 00 04 00 24
S: 00 03 00 25 [ 512 bytes of data ]
C: 00 04 00 25
S: 00 03 00 26 [ 512 bytes of data ]
C: 00 04 00 26
S: 00 03 00 27 [ 512 bytes of data ]
C: 00 04 00 27
S: 00 03 00 28 [ 32 bytes of data ] (last)
C: 00 04 00 27
S: 00 03 00 28 [ 32 bytes of data ] (last)
C: 00 04 00 28
Closing connection to client 127.0.0.1:54323
```

### Write request test

```
Client:
$ ./tftpc -w client_file.bin localhost 6900
Writing client_file.bin to 127.0.0.1:6900
C: 00 02 c  l  i  e  n  t  _  f  i  l  e  .  b  i  n  00 o  c  t  e  t  00
S: 00 04 00 00
C: 00 03 00 01 [ 512 bytes of data ]
S: 00 04 00 01
C: 00 03 00 02 [ 512 bytes of data ]
C: 00 03 00 02 [ 512 bytes of data ]
S: 00 04 00 02
C: 00 03 00 03 [ 512 bytes of data ]
S: 00 04 00 03
C: 00 03 00 04 [ 512 bytes of data ]
S: 00 04 00 03
C: 00 03 00 04 [ 512 bytes of data ]
S: 00 04 00 04
C: 00 03 00 05 [ 512 bytes of data ]
S: 00 04 00 05
C: 00 03 00 06 [ 512 bytes of data ]
S: 00 04 00 05
C: 00 03 00 06 [ 512 bytes of data ]
S: 00 04 00 05
C: 00 03 00 06 [ 512 bytes of data ]
S: 00 04 00 06
C: 00 03 00 07 [ 512 bytes of data ]
S: 00 04 00 06
C: 00 03 00 07 [ 512 bytes of data ]
S: 00 04 00 06
C: 00 03 00 07 [ 512 bytes of data ]
S: 00 04 00 06
C: 00 03 00 07 [ 512 bytes of data ]
S: 00 04 00 07
C: 00 03 00 08 [ 512 bytes of data ]
S: 00 04 00 08
C: 00 03 00 09 [ 512 bytes of data ]
S: 00 04 00 09
C: 00 03 00 0a [ 512 bytes of data ]
S: 00 04 00 0a
C: 00 03 00 0b [ 512 bytes of data ]
S: 00 04 00 0a
C: 00 03 00 0b [ 512 bytes of data ]
S: 00 04 00 0b
C: 00 03 00 0c [ 512 bytes of data ]
S: 00 04 00 0c
C: 00 03 00 0d [ 512 bytes of data ]
C: 00 03 00 0d [ 512 bytes of data ]
S: 00 04 00 0c
C: 00 03 00 0d [ 512 bytes of data ]
S: 00 04 00 0d
C: 00 03 00 0e [ 512 bytes of data ]
S: 00 04 00 0d
C: 00 03 00 0e [ 512 bytes of data ]
S: 00 04 00 0e
C: 00 03 00 0f [ 512 bytes of data ]
S: 00 04 00 0e
C: 00 03 00 0f [ 512 bytes of data ]
S: 00 04 00 0e
C: 00 03 00 0f [ 512 bytes of data ]
S: 00 04 00 0f
C: 00 03 00 10 [ 512 bytes of data ]
S: 00 04 00 0f
C: 00 03 00 10 [ 512 bytes of data ]
S: 00 04 00 0f
C: 00 03 00 10 [ 512 bytes of data ]
S: 00 04 00 10
C: 00 03 00 11 [ 512 bytes of data ]
S: 00 04 00 10
C: 00 03 00 11 [ 512 bytes of data ]
S: 00 04 00 10
C: 00 03 00 11 [ 512 bytes of data ]
S: 00 04 00 11
C: 00 03 00 12 [ 512 bytes of data ]
S: 00 04 00 11
C: 00 03 00 12 [ 512 bytes of data ]
S: 00 04 00 11
C: 00 03 00 12 [ 512 bytes of data ]
S: 00 04 00 11
C: 00 03 00 12 [ 512 bytes of data ]
S: 00 04 00 12
C: 00 03 00 13 [ 512 bytes of data ]
S: 00 04 00 12
C: 00 03 00 13 [ 512 bytes of data ]
S: 00 04 00 13
C: 00 03 00 14 [ 512 bytes of data ]
S: 00 04 00 12
C: 00 03 00 14 [ 512 bytes of data ]
S: 00 04 00 13
C: 00 03 00 14 [ 512 bytes of data ]
S: 00 04 00 14
C: 00 03 00 15 [ 512 bytes of data ]
S: 00 04 00 13
C: 00 03 00 15 [ 512 bytes of data ]
S: 00 04 00 15
C: 00 03 00 16 [ 512 bytes of data ]
S: 00 04 00 16
C: 00 03 00 17 [ 512 bytes of data ]
S: 00 04 00 17
C: 00 03 00 18 [ 512 bytes of data ]
S: 00 04 00 15
C: 00 03 00 18 [ 512 bytes of data ]
S: 00 04 00 16
C: 00 03 00 18 [ 512 bytes of data ]
S: 00 04 00 18
C: 00 03 00 19 [ 512 bytes of data ]
S: 00 04 00 15
C: 00 03 00 19 [ 512 bytes of data ]
S: 00 04 00 17
C: 00 03 00 19 [ 512 bytes of data ]
S: 00 04 00 17
C: 00 03 00 19 [ 512 bytes of data ]
S: 00 04 00 18
C: 00 03 00 19 [ 512 bytes of data ]
S: 00 04 00 18
C: 00 03 00 19 [ 512 bytes of data ]
S: 00 04 00 17
C: 00 03 00 19 [ 512 bytes of data ]
S: 00 04 00 19
C: 00 03 00 1a [ 512 bytes of data ]
S: 00 04 00 19
C: 00 03 00 1a [ 512 bytes of data ]
S: 00 04 00 19
C: 00 03 00 1a [ 512 bytes of data ]
S: 00 04 00 19
C: 00 03 00 1a [ 512 bytes of data ]
S: 00 04 00 19
C: 00 03 00 1a [ 512 bytes of data ]
S: 00 04 00 19
C: 00 03 00 1a [ 512 bytes of data ]
S: 00 04 00 19
C: 00 03 00 1a [ 512 bytes of data ]
S: 00 04 00 19
C: 00 03 00 1a [ 512 bytes of data ]
S: 00 04 00 1a
C: 00 03 00 1b [ 512 bytes of data ]
S: 00 04 00 19
C: 00 03 00 1b [ 512 bytes of data ]
S: 00 04 00 1a
C: 00 03 00 1b [ 512 bytes of data ]
S: 00 04 00 19
C: 00 03 00 1b [ 512 bytes of data ]
S: 00 04 00 1a
C: 00 03 00 1b [ 512 bytes of data ]
S: 00 04 00 1a
C: 00 03 00 1b [ 512 bytes of data ]
S: 00 04 00 1b
C: 00 03 00 1c [ 512 bytes of data ]
S: 00 04 00 1b
C: 00 03 00 1c [ 512 bytes of data ]
S: 00 04 00 1b
C: 00 03 00 1c [ 512 bytes of data ]
S: 00 04 00 1b
C: 00 03 00 1c [ 512 bytes of data ]
S: 00 04 00 1b
C: 00 03 00 1c [ 512 bytes of data ]
S: 00 04 00 1b
C: 00 03 00 1c [ 512 bytes of data ]
S: 00 04 00 1b
C: 00 03 00 1c [ 512 bytes of data ]
S: 00 04 00 1c
C: 00 03 00 1d [ 512 bytes of data ]
S: 00 04 00 1c
C: 00 03 00 1d [ 512 bytes of data ]
S: 00 04 00 1c
C: 00 03 00 1d [ 512 bytes of data ]
S: 00 04 00 1c
C: 00 03 00 1d [ 512 bytes of data ]
S: 00 04 00 1c
C: 00 03 00 1d [ 512 bytes of data ]
S: 00 04 00 1c
C: 00 03 00 1d [ 512 bytes of data ]
S: 00 04 00 1c
C: 00 03 00 1d [ 512 bytes of data ]
S: 00 04 00 1d
C: 00 03 00 1e [ 512 bytes of data ]
S: 00 04 00 1d
C: 00 03 00 1e [ 512 bytes of data ]
S: 00 04 00 1d
C: 00 03 00 1e [ 512 bytes of data ]
S: 00 04 00 1d
C: 00 03 00 1e [ 512 bytes of data ]
S: 00 04 00 1c
C: 00 03 00 1e [ 512 bytes of data ]
S: 00 04 00 1e
C: 00 03 00 1f [ 512 bytes of data ]
S: 00 04 00 1f
C: 00 03 00 20 [ 512 bytes of data ]
S: 00 04 00 1f
C: 00 03 00 20 [ 512 bytes of data ]
S: 00 04 00 1e
C: 00 03 00 20 [ 512 bytes of data ]
S: 00 04 00 1f
C: 00 03 00 20 [ 512 bytes of data ]
S: 00 04 00 1f
C: 00 03 00 20 [ 512 bytes of data ]
S: 00 04 00 20
C: 00 03 00 21 [ 512 bytes of data ]
S: 00 04 00 20
C: 00 03 00 21 [ 512 bytes of data ]
S: 00 04 00 20
C: 00 03 00 21 [ 512 bytes of data ]
S: 00 04 00 20
C: 00 03 00 21 [ 512 bytes of data ]
S: 00 04 00 20
C: 00 03 00 21 [ 512 bytes of data ]
S: 00 04 00 21
C: 00 03 00 22 [ 512 bytes of data ]
S: 00 04 00 21
C: 00 03 00 22 [ 512 bytes of data ]
S: 00 04 00 21
C: 00 03 00 22 [ 512 bytes of data ]
S: 00 04 00 22
C: 00 03 00 23 [ 512 bytes of data ]
S: 00 04 00 21
C: 00 03 00 23 [ 512 bytes of data ]
S: 00 04 00 21
C: 00 03 00 23 [ 512 bytes of data ]
S: 00 04 00 22
C: 00 03 00 23 [ 512 bytes of data ]
S: 00 04 00 23
C: 00 03 00 24 [ 512 bytes of data ]
S: 00 04 00 23
C: 00 03 00 24 [ 512 bytes of data ]
S: 00 04 00 24
C: 00 03 00 25 [ 512 bytes of data ]
S: 00 04 00 24
C: 00 03 00 25 [ 512 bytes of data ]
S: 00 04 00 25
C: 00 03 00 26 [ 512 bytes of data ]
S: 00 04 00 25
C: 00 03 00 26 [ 512 bytes of data ]
S: 00 04 00 26
C: 00 03 00 27 [ 512 bytes of data ]
S: 00 04 00 27
C: 00 03 00 28 [ 32 bytes of data ] (last)
S: 00 04 00 27
C: 00 03 00 28 [ 32 bytes of data ] (last)
S: 00 04 00 28
Writing succeeded, disconnecting...

Server:
Listening port 6900
Received write request from 127.0.0.1:59533
C: 00 02 c  l  i  e  n  t  _  f  i  l  e  .  b  i  n  00 o  c  t  e  t  00
S: 00 04 00 00
C: 00 03 00 01 [ 512 bytes of data ]
S: 00 04 00 01
C: 00 03 00 02 [ 512 bytes of data ]
S: 00 04 00 02
S: 00 04 00 02
C: 00 03 00 02 [ 512 bytes of data ]
S: 00 04 00 02
C: 00 03 00 03 [ 512 bytes of data ]
S: 00 04 00 03
S: 00 04 00 03
C: 00 03 00 04 [ 512 bytes of data ]
S: 00 04 00 04
C: 00 03 00 05 [ 512 bytes of data ]
S: 00 04 00 05
S: 00 04 00 05
C: 00 03 00 06 [ 512 bytes of data ]
S: 00 04 00 06
C: 00 03 00 06 [ 512 bytes of data ]
S: 00 04 00 06
C: 00 03 00 06 [ 512 bytes of data ]
S: 00 04 00 06
C: 00 03 00 07 [ 512 bytes of data ]
S: 00 04 00 07
C: 00 03 00 07 [ 512 bytes of data ]
S: 00 04 00 07
C: 00 03 00 07 [ 512 bytes of data ]
S: 00 04 00 07
C: 00 03 00 07 [ 512 bytes of data ]
S: 00 04 00 07
C: 00 03 00 08 [ 512 bytes of data ]
S: 00 04 00 08
C: 00 03 00 09 [ 512 bytes of data ]
S: 00 04 00 09
C: 00 03 00 0a [ 512 bytes of data ]
S: 00 04 00 0a
S: 00 04 00 0a
C: 00 03 00 0b [ 512 bytes of data ]
S: 00 04 00 0b
C: 00 03 00 0c [ 512 bytes of data ]
S: 00 04 00 0c
S: 00 04 00 0c
C: 00 03 00 0d [ 512 bytes of data ]
S: 00 04 00 0d
C: 00 03 00 0d [ 512 bytes of data ]
S: 00 04 00 0d
C: 00 03 00 0e [ 512 bytes of data ]
S: 00 04 00 0e
C: 00 03 00 0e [ 512 bytes of data ]
S: 00 04 00 0e
C: 00 03 00 0e [ 512 bytes of data ]
S: 00 04 00 0e
C: 00 03 00 0f [ 512 bytes of data ]
S: 00 04 00 0f
C: 00 03 00 0f [ 512 bytes of data ]
S: 00 04 00 0f
C: 00 03 00 0f [ 512 bytes of data ]
S: 00 04 00 0f
C: 00 03 00 10 [ 512 bytes of data ]
S: 00 04 00 10
C: 00 03 00 10 [ 512 bytes of data ]
S: 00 04 00 10
C: 00 03 00 10 [ 512 bytes of data ]
S: 00 04 00 10
C: 00 03 00 11 [ 512 bytes of data ]
S: 00 04 00 11
C: 00 03 00 11 [ 512 bytes of data ]
S: 00 04 00 11
C: 00 03 00 11 [ 512 bytes of data ]
S: 00 04 00 11
C: 00 03 00 11 [ 512 bytes of data ]
S: 00 04 00 11
C: 00 03 00 12 [ 512 bytes of data ]
S: 00 04 00 12
C: 00 03 00 12 [ 512 bytes of data ]
S: 00 04 00 12
C: 00 03 00 12 [ 512 bytes of data ]
S: 00 04 00 12
C: 00 03 00 12 [ 512 bytes of data ]
S: 00 04 00 12
C: 00 03 00 13 [ 512 bytes of data ]
S: 00 04 00 13
C: 00 03 00 12 [ 512 bytes of data ]
S: 00 04 00 13
C: 00 03 00 13 [ 512 bytes of data ]
S: 00 04 00 13
C: 00 03 00 14 [ 512 bytes of data ]
S: 00 04 00 14
C: 00 03 00 15 [ 512 bytes of data ]
S: 00 04 00 15
C: 00 03 00 14 [ 512 bytes of data ]
S: 00 04 00 15
C: 00 03 00 16 [ 512 bytes of data ]
S: 00 04 00 16
C: 00 03 00 17 [ 512 bytes of data ]
S: 00 04 00 17
C: 00 03 00 14 [ 512 bytes of data ]
S: 00 04 00 17
C: 00 03 00 15 [ 512 bytes of data ]
S: 00 04 00 17
C: 00 03 00 18 [ 512 bytes of data ]
S: 00 04 00 18
C: 00 03 00 17 [ 512 bytes of data ]
S: 00 04 00 18
C: 00 03 00 15 [ 512 bytes of data ]
S: 00 04 00 18
C: 00 03 00 19 [ 512 bytes of data ]
S: 00 04 00 19
C: 00 03 00 18 [ 512 bytes of data ]
S: 00 04 00 19
C: 00 03 00 19 [ 512 bytes of data ]
S: 00 04 00 19
C: 00 03 00 19 [ 512 bytes of data ]
S: 00 04 00 19
C: 00 03 00 18 [ 512 bytes of data ]
S: 00 04 00 19
C: 00 03 00 19 [ 512 bytes of data ]
S: 00 04 00 19
C: 00 03 00 19 [ 512 bytes of data ]
S: 00 04 00 19
C: 00 03 00 1a [ 512 bytes of data ]
S: 00 04 00 1a
C: 00 03 00 1a [ 512 bytes of data ]
S: 00 04 00 1a
C: 00 03 00 1a [ 512 bytes of data ]
S: 00 04 00 1a
C: 00 03 00 19 [ 512 bytes of data ]
S: 00 04 00 1a
C: 00 03 00 1a [ 512 bytes of data ]
S: 00 04 00 1a
C: 00 03 00 1b [ 512 bytes of data ]
S: 00 04 00 1b
C: 00 03 00 1a [ 512 bytes of data ]
S: 00 04 00 1b
C: 00 03 00 1b [ 512 bytes of data ]
S: 00 04 00 1b
C: 00 03 00 1b [ 512 bytes of data ]
S: 00 04 00 1b
C: 00 03 00 1b [ 512 bytes of data ]
S: 00 04 00 1b
C: 00 03 00 1b [ 512 bytes of data ]
S: 00 04 00 1b
C: 00 03 00 1b [ 512 bytes of data ]
S: 00 04 00 1b
C: 00 03 00 1c [ 512 bytes of data ]
S: 00 04 00 1c
C: 00 03 00 1c [ 512 bytes of data ]
S: 00 04 00 1c
C: 00 03 00 1c [ 512 bytes of data ]
S: 00 04 00 1c
C: 00 03 00 1c [ 512 bytes of data ]
S: 00 04 00 1c
C: 00 03 00 1c [ 512 bytes of data ]
S: 00 04 00 1c
C: 00 03 00 1c [ 512 bytes of data ]
S: 00 04 00 1c
C: 00 03 00 1c [ 512 bytes of data ]
S: 00 04 00 1c
C: 00 03 00 1d [ 512 bytes of data ]
S: 00 04 00 1d
C: 00 03 00 1c [ 512 bytes of data ]
S: 00 04 00 1d
C: 00 03 00 1d [ 512 bytes of data ]
S: 00 04 00 1d
C: 00 03 00 1d [ 512 bytes of data ]
S: 00 04 00 1d
C: 00 03 00 1e [ 512 bytes of data ]
S: 00 04 00 1e
C: 00 03 00 1e [ 512 bytes of data ]
S: 00 04 00 1e
C: 00 03 00 1e [ 512 bytes of data ]
S: 00 04 00 1e
C: 00 03 00 1f [ 512 bytes of data ]
S: 00 04 00 1f
C: 00 03 00 1e [ 512 bytes of data ]
S: 00 04 00 1f
C: 00 03 00 1e [ 512 bytes of data ]
S: 00 04 00 1f
C: 00 03 00 20 [ 512 bytes of data ]
S: 00 04 00 20
C: 00 03 00 20 [ 512 bytes of data ]
S: 00 04 00 20
C: 00 03 00 20 [ 512 bytes of data ]
S: 00 04 00 20
C: 00 03 00 20 [ 512 bytes of data ]
S: 00 04 00 20
C: 00 03 00 20 [ 512 bytes of data ]
S: 00 04 00 20
C: 00 03 00 21 [ 512 bytes of data ]
S: 00 04 00 21
C: 00 03 00 21 [ 512 bytes of data ]
S: 00 04 00 21
C: 00 03 00 21 [ 512 bytes of data ]
S: 00 04 00 21
C: 00 03 00 21 [ 512 bytes of data ]
S: 00 04 00 21
C: 00 03 00 22 [ 512 bytes of data ]
S: 00 04 00 22
C: 00 03 00 22 [ 512 bytes of data ]
S: 00 04 00 22
C: 00 03 00 22 [ 512 bytes of data ]
S: 00 04 00 22
C: 00 03 00 23 [ 512 bytes of data ]
S: 00 04 00 23
C: 00 03 00 23 [ 512 bytes of data ]
S: 00 04 00 23
C: 00 03 00 23 [ 512 bytes of data ]
S: 00 04 00 23
C: 00 03 00 23 [ 512 bytes of data ]
S: 00 04 00 23
C: 00 03 00 24 [ 512 bytes of data ]
S: 00 04 00 24
C: 00 03 00 24 [ 512 bytes of data ]
S: 00 04 00 24
C: 00 03 00 25 [ 512 bytes of data ]
S: 00 04 00 25
C: 00 03 00 25 [ 512 bytes of data ]
S: 00 04 00 25
C: 00 03 00 26 [ 512 bytes of data ]
S: 00 04 00 26
C: 00 03 00 26 [ 512 bytes of data ]
S: 00 04 00 26
C: 00 03 00 27 [ 512 bytes of data ]
S: 00 04 00 27
C: 00 03 00 28 [ 32 bytes of data ] (last)
S: 00 04 00 28
C: 00 03 00 28 [ 32 bytes of data ] (last)
S: 00 04 00 28
Closing connection to client 127.0.0.1:59533
```

### Results

```
Client:
$ sha256sum client_file.bin server_file.bin
08d42782d6721c4c04ba5e92fb9bb730ec1d4d8683540145e9f768a82966a920  client_file.bin
c8e6eb8b22496dbae4f2e2ed4e3d4dd12df0b62bb319a0f53a0384426b37a3f0  server_file.bin

Server:
$ sha256sum client_file.bin server_file.bin
08d42782d6721c4c04ba5e92fb9bb730ec1d4d8683540145e9f768a82966a920  client_file.bin
c8e6eb8b22496dbae4f2e2ed4e3d4dd12df0b62bb319a0f53a0384426b37a3f0  server_file.bin
```

