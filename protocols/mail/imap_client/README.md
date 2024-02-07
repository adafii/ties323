Example of usage. TLS tunnel to IMAP server was established prior to fetching mails, because the client lacks TLS support.

```
$ grep '^[^;]' /etc/stunnel/stunnel.conf
[imap]
client = yes
accept = 127.0.0.1:143
connect = imap.zoho.eu:993
verifyChain = yes
CApath = /etc/ssl/certs
checkHost = imap.zoho.eu
OCSPaia = yes

$ ./imapc
Usage: ./imapc <user> <pass> <host> <port>

$ ./imapc adafiii <redacted> localhost 143 > out 2> err

$ cat err
S: * OK svwall.zoho.com IMAP4 Server (Zoho Mail IMAP4rev1 Server version 1.0)
C: 1 LOGIN adafiii <redacted>
S: * CAPABILITY IMAP4rev1 UNSELECT CHILDREN XLIST NAMESPACE IDLE MOVE ID AUTH=PLAIN SASL-IR UIDPLUS ESEARCH LIST-EXTENDED LIST-STATUS WITHIN LITERAL- APPENDLIMIT=20971520 CONDSTORE
S: 1 OK Success
C: 2 LIST "" *
S: * LIST (\HasNoChildren) "/" "INBOX"
S: * LIST (\Noinferiors \Drafts) "/" "Drafts"
S: * LIST (\Noinferiors) "/" "Templates"
S: * LIST (\HasNoChildren) "/" "Snoozed"
S: * LIST (\Noinferiors \Sent) "/" "Sent"
S: * LIST (\Noinferiors \Junk) "/" "Spam"
S: * LIST (\HasNoChildren \Trash) "/" "Trash"
S: * LIST (\HasNoChildren) "/" "Notification"
S: * LIST (\HasNoChildren) "/" "Newsletter"
S: 2 OK Success
C: 3 EXAMINE INBOX
S: * 3 EXISTS
S: * 1 RECENT
S: * OK [UNSEEN 3]
C: 4 FETCH 1 BODY[HEADER.FIELDS (TO FROM DATE SUBJECT)]
S: * 1 FETCH (BODY[HEADER.FIELDS ("TO" "FROM" "DATE" "SUBJECT")] {127}
S: From: welcome@zoho.eu
S: Subject: Welcome to Zoho Mail
S: To: Liia <adafiii@zohomail.eu>
S: Date: Wed, 07 Feb 2024 15:14:19 +0100
S:
S: )
S: 4 OK Success
C: 5 FETCH 2 BODY[HEADER.FIELDS (TO FROM DATE SUBJECT)]
S: * 2 FETCH (BODY[HEADER.FIELDS ("TO" "FROM" "DATE" "SUBJECT")] {137}
S: From: welcome@zoho.eu
S: Subject: Access from anywhere, anytime!
S: To: Liia <adafiii@zohomail.eu>
S: Date: Wed, 07 Feb 2024 15:14:20 +0100
S:
S: )
S: 5 OK Success
C: 6 FETCH 3 BODY[HEADER.FIELDS (TO FROM DATE SUBJECT)]
S: * 3 FETCH (BODY[HEADER.FIELDS ("TO" "FROM" "DATE" "SUBJECT")] {156}
S: Date: Wed, 7 Feb 2024 21:03:37 +0100 (CET)
S: From: Zoho Team <noreply@zohoaccounts.eu>
S: To: adafiii@zohomail.eu
S: Subject: Your Zoho password was changed.
S:
S: )
S: 6 OK Success
C: 7 EXAMINE Drafts
S: * 0 EXISTS
S: * 0 RECENT
S: * OK [UIDVALIDITY 1] UIDs valid
C: 8 EXAMINE Templates
S: * 0 EXISTS
S: * 0 RECENT
S: * OK [UIDVALIDITY 1] UIDs valid
C: 9 EXAMINE Snoozed
S: * 0 EXISTS
S: * 0 RECENT
S: * OK [UIDVALIDITY 1] UIDs valid
C: 10 EXAMINE Sent
S: * 0 EXISTS
S: * 0 RECENT
S: * OK [UIDVALIDITY 1] UIDs valid
C: 11 EXAMINE Spam
S: * 0 EXISTS
S: * 0 RECENT
S: * OK [UIDVALIDITY 1] UIDs valid
C: 12 EXAMINE Trash
S: * 0 EXISTS
S: * 0 RECENT
S: * OK [UIDVALIDITY 1] UIDs valid
C: 13 EXAMINE Notification
S: * 0 EXISTS
S: * 0 RECENT
S: * OK [UIDVALIDITY 1] UIDs valid
C: 14 EXAMINE Newsletter
S: * 0 EXISTS
S: * 0 RECENT
S: * OK [UIDVALIDITY 1] UIDs valid
C: 15 LOGOUT
S: * BYE IMAP4rev1 Server logging out

$ cat out 
Connected to localhost:143
INBOX (3 mails)
      From                                To                                  Date                                     Subject     
      welcome@zoho.eu                     Liia <adafiii@zohomail.eu>          Wed, 07 Feb 2024 15:14:19 +0100          Welcome to Zoho Mail
      welcome@zoho.eu                     Liia <adafiii@zohomail.eu>          Wed, 07 Feb 2024 15:14:20 +0100          Access from anywhere, anytime!
      Zoho Team <noreply@zohoaccounts.eu> adafiii@zohomail.eu                 Wed, 7 Feb 2024 21:03:37 +0100 (CET)     Your Zoho password was changed.
Drafts (0 mails)
Templates (0 mails)
Snoozed (0 mails)
Sent (0 mails)
Spam (0 mails)
Trash (0 mails)
Notification (0 mails)
Newsletter (0 mails)
```