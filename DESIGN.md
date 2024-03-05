# Architecture

## IMAP Socket

## IMAP Parser

## IMAP Client
In fact, after IMAP parser done its part. Its not its job to parse RFC822.
This should be done somewhere else.
This should either be something line new layer called emailkit_lib.
This layer uses imap_client for imap communication and parsing of IAMP. That is the role of
IMAP client. We take IMAP socket and take IMAP parser. And combine them in IMAP client.
So by design it should be a descent IMAP client so any person who is familiar with IMAP
or with IMAP RFC should be able to use this API.

## RFC822 Parser

## EmailClient

Provides high level functions so ideally we should be able to use only these functions when 
we are writing email client and not use imap client and even lower level services.
But in the same time, we can expose unerlying object and effectively loes abstraction capabilities.
Abstraction is not that valuable in this context because we cannot imagine that we change underlying
components. If we ever do, it is OK to pay a price of leaking implementation details.
In the same time, having access to underlying instances may help to hack some email-server specific thing.

In general this is not abstract email clint. It is an email client tailored for the needs of
MailerApp. It tries to be resonable in its API but in still not an abstract email client.
One possible complication of using underlying object is that this implies that imap client
connection can be shared. IMAP supports this but I'm not sure if we support this.
If it is not going to work, then we should employ synchronization inside the email client then.

EmailClient works on top of IMAP Client, IMAP parser, RFC822 Parser, SMTP client, IMAP builder, RFC822 builder.
It also provides services like what is our server, what is its capabilities, etc..
```c++
if (client->is_connected() && client->is_authenticated()) { client->show_server_capabilities(); }
```

We may keep capabilities caches so that we can check for some additional functions anytime.
EmailClient may provide some additional functions. It is a client who should check if this function is available on the server or if non-standard field is expected to be present in responses from the server.

# MailerApp

This is a mode of our applicationi. We are not going to have anything else on of it. This is super high level already and is complete application which does everything that application does. It just does not have human interface.

# MailerAppCache

This is a class that is our local copy of remote data. It is build on initial download and then is kept actual by constantly synchronizing with the server.
The application usually does not work with server directly by only with this cache. The cache has representation on DISK so that once downlaoded it can be quickly loaded from disk after program restart. The cache has events so that we can subscribe to new items. This is similar to EventSource database where you subscribe to updates
and then you get first all items in the database and then updates if there are any.
The cache has function of local Search. Is going to be implemented with sqlite FTS. It would be nice to have fuzzy search for all languages.
We can also have online search (as first release it will be probably enough). And, one more advanced, Structured search which directly speaks to Gmail. Just in case someone is proficient in GMail search language he or she should still be able to use it.


# General Strategy

It is possible that we cannot overcome inherent problems of turning email into telegram.
In this case we may benefit from advanced features that help to deal with this problems.

Example of this problem is to write follow up. E.g. we have a group of oeople and we send them an email and ask to do some things.
Some time later we see some people did not do what they are supposed to. We want to write follow up email. How we are supposed to do that?
I guess we should have an interface which allows to create ad-hoc group and send follow up to them.

# Grouping by Projects
 It is possible that we need to group emails by chains. E.g. we have multiple projects and we want to have all this converstaions grouped.
 It is possible that it is already nicely supported by GMail and others.
 



