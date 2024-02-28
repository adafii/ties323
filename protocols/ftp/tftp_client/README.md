Example of usage. See [tftp server readme](../tftp_server/README.md) for server output.

```
$ ./tftpc
Usage: tftpc ( -r | -w ) file host port
    -r Read file from host (RRQ)
    -w Write file to host (WRQ)
    
$ ./tftpc -w source.txt localhost 6900
Writing source.txt to 127.0.0.1:6900
C: 00 02 s  o  u  r  c  e  .  t  x  t  00 o  c  t  e  t  00
S: 00 04 00 00
C: 00 03 00 01 [ 512 bytes of data ]
S: 00 04 00 01
C: 00 03 00 02 [ 512 bytes of data ]
S: 00 04 00 02
C: 00 03 00 03 [ 258 bytes of data ] (last)
S: 00 04 00 03
Writing succeeded, disconnecting...

$ ./tftpc -r target.txt localhost 6900
Reading target.txt from 127.0.0.1:6900
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
Reading succeeded, disconnecting...

$ sha256sum target.txt source.txt
b06fb6a6eb2e0389503ff355236f57ca921b518fe65fc327e4d88df4f520cab6  target.txt
85a9d734d9897b9a53d8f871bdd452d42e330cc7f3870f55766e846b3c32f253  source.txt
```