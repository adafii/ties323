# TIES323 Application Protocols

The assignments are [here](http://users.jyu.fi/~arjuvi/opetus/ties323/2018/demot.html).

## Application protocol implementations

Compiling requires Linux, CMake (>= 3.18), OpenSSL library (>= 3.2?), and [stand-alone Asio C++ library](https://think-async.com/Asio/) (>= 1.28).

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

## Kathará

- [x] [Web server](/kathara/web_server)
- [x] [DNS](/kathara/dns)
- [x] [Walkthrough](/kathara/walkthrough)
