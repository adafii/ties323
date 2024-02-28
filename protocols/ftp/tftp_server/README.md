Example of usage. See [tftp client readme](../tftp_client/README.md) for client output.

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