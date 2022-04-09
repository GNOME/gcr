Title: About PKCS#11

About PKCS#11
=============
PKCS#11 is an API for storing and using crypto objects, and performing crypto
operations on them.

It is specified at the [RSA website] and a [handy PKCS#11 reference] is also
available.

## PKCS#11 URIs
[PKCS#11 URIs] are a standard for referring to PKCS#11 modules, tokens, or
objects. What the PKCS#11 URI refers to depends on the context in which it is
used.

A PKCS#11 URI can always resolve to more than one object, token or module. A
PKCS#11 URI that refers to a token, would (when used in a context that expects
objects) refer to all the token on that module.

To parse a PKCS#11 URI, use the [func@Gck.UriData.parse] function passing in the type of
context in which you're using the URI. To build a URI, use the [method@Gck.UriData.build]
function.

In most cases, the parsing or building of URIs is already handled for you in the
GCK library. For example: to enumerate objects that match a PKCS#11 URI use the
[func@modules_enumerate_uri] function.



[RSA website]: http://www.rsa.com/rsalabs/node.asp?id=2133
[handy PKCS#11 reference]: http://www.cryptsoft.com/pkcs11doc/
[PKCS#11 URIs]: http://tools.ietf.org/html/draft-pechanec-pkcs11uri-03
