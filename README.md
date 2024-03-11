# TIES323 Application Protocols

The assignments are [here](http://users.jyu.fi/~arjuvi/opetus/ties323/2018/demot.html).

## Application protocol implementations

Compiling requires Linux, CMake (>= 3.18), OpenSSL library (>= 3.2?), and [stand-alone Asio C++ library](https://think-async.com/Asio/) (>= 1.28). POP3 client also requires Dear ImGui library for gui, which is included as a git submodule.

### Mail protocols

SMTP, POP3, and IMAP servers are run within a single process, which allows them to access memory-based mail storage. The storage is volatile and will be lost when the server process terminates. 

- [x] [SMTP server](/protocols/mail/smtp_server)
- [x] [POP3 client](/protocols/mail/pop3_client)
- [x] [Inbox for SMTP server; POP3 server](/protocols/mail/smtp_server)
- [x] [IMAP client](/protocols/mail/imap_client)
- [x] [IMAP server](/protocols/mail/smtp_server)
- [x] Extra features
    - POP3 client has TLS option
    - POP3 client has GUI
    - Parallelized multi-user SMTP/POP3/IMAP server with transaction locking for POP3

### File transfer protocols

- [x] [FTP client](/protocols/ftp/ftp_client)
- [x] [TFTP client](/protocols/ftp/tftp_client)
- [x] [TFTP server](/protocols/ftp/tftp_server)
- [x] Both RRQ and WRQ implemented
- [x] Error handling for TFTP
    - Client and server checks block number
    - Client and server resends data if ack has wrong opcode or block number
    - Client and server resends data if ack timeouts
    - Client and server resends ack if data has wrong opcode, block number, or timeouts
    - [Random error test](/protocols/ftp/tftp_server/README.md)

## Kathará

- [x] [Web server](/kathara/web_server)
- [x] [CGI](/kathara/cgi)
- [x] [DNS](/kathara/dns)
- [x] [Walkthrough](/kathara/walkthrough)

## Web, email and DNS servers 

Notes from setting up the servers are [here](/server/README.md). I used my own servers and domain instead of lab setup.

- [x] Initial setup
- [x] [DNS](https://zonemaster.fi/result/fffd98e51a990217)
- [x] [Web server](https://ofu.fi)
- [x] SMTP
- [x] POP3 and IMAP
