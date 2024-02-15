Example of usage:

``` shell
$ ./server &
[1] 70762
$ nc -C server localhost 2500
220 localhost Simple Mail Transfer Service ready
helo localhost
250 localhost
mail from:<admin@localhost>
250 OK
rcpt to:<liia@localhost>
250 OK
rcpt to:<test@localhost>
250 OK
data
354 Start recipients input; end with <CRLF>.<CRLF>
Date: now
From: admin@localhost
To: liia@localhost
Subject: Test

Hello!
Hello from admin!
.
250 OK
quit
221 localhost Service closing transmission channel

$ nc -C localhost 1100
+OK POP3 server ready
user liia@localhost
+OK
pass secret
+OK
^Z
zsh: suspended  nc -C localhost 1100

$ nc -C localhost 1430
* OK localhost IMAP4 Server
1 login liia@localhost secret
1 OK
2 list "" *
* LIST () "/" "Sent"
* LIST () "/" "INBOX"
2 OK
3 logout
* BYE
3 OK

$ nc -C localhost 1100
+OK POP3 server ready
user liia@localhost
+OK
pass secret
-ERR mail-drop already locked
quit
+OK POP3 server signing off

$ fg
[2]  - continued  nc -C localhost 1100
list
+OK 2 messages (165 octets)
1 84
2 81
.
quit
+OK POP3 server signing off

$ kill -1 70762
[1]  + done       ./server
```
