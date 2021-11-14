Title: PKCS#11 configuration

PKCS#11 configuration
=====================
The GCR library maintains a global list of PKCS#11 modules to use for
its various lookups and storage operations. Each module is represented by
a [class@Gck.Module] object. You can examine this list by using
[func@pkcs11_get_modules].

The list is configured automatically by looking for system installed
PKCS#11 modules. It's not not normally necessary to modify this list. But
if you have special needs, you can use the [func@pkcs11_set_modules] and
[func@pkcs11_add_module] (or [func@pkcs11_add_module_from_file]) to do so.

Trust assertions are stored and looked up in specific PKCS#11 slots.
You can examine this list with [func@pkcs11_get_trust_lookup_slots].
