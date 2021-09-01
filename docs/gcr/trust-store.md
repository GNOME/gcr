Title: Trust store
Description: Store/Lookup bits of information used for verifying certificates.

Trust store
===========
GCR provides functions to access information about which certificates the system
and user trusts, such as certificate authority trust anchors, or overrides
to the normal verification of certificates.

These functions do not constitute a viable method for verifying certificates
used in TLS or other locations. Instead they support such verification by
providing some of the needed data for a trust decision.

The storage is provided by pluggable PKCS#11 modules.

Trust Anchors
-------------
Trust anchors are used to verify the certificate authority in a certificate
chain. Trust anchors are always valid for a given purpose. The most common
purpose is [const@PURPOSE_SERVER_AUTH] and is used for a client application to
verify that the certificate at the server side of a TLS connection is authorized
to act as such. To check if a certificate is a trust anchor, use
[func@Gcr.trust_is_certificate_anchored], or
[func@Gcr.trust_is_certificate_anchored_async] for the asynchronous version

Pinned certificates
-------------------
Pinned certificates are used when a user overrides the default trust decision
for a given certificate. They're often used with self-signed certificates.
Pinned certificates are always only valid for a single peer such as the remote
host with which TLS is being performed. To lookup pinned certificates, use
[func@Gcr.trust_is_certificate_pinned], or
[func@Gcr.trust_is_certificate_pinned_async] for the asynchronous version.

After the user has requested to override the trust decision about a given
certificate then a pinned certificates can be added by using the
[func@Gcr.trust_add_pinned_certificate] function, or
[func@Gcr.trust_add_pinned_certificate_async] for the asynchronous version.

Distrusted certificates
------------------------
Certificates can be marked _distrusted_, either by manual action of the user, or
by Certificate Authorities (CAs) that add them in a Certificate Revocation List
(CRL) or other means. To check if a certificate is distrusted, one can use
[func@Gcr.trust_is_certificate_distrusted], or
[func@Gcr.trust_is_certificate_distrusted_async] for the asynchronous version.
