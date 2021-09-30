Title: Non-pageable memory

Non-pageable memory
===================
Normal allocated memory can be paged to disk at the whim of the operating
system. This can be a problem for sensitive information like passwords, keys
and secrets.

The Gcr library holds passwords and keys in *non-pageable*, or *locked* memory.
This is only possible if the OS contains support for it.

The set of `gcr_secure_memory_*()` functions allow applications to use secure
memory to hold passwords and other sensitive information.

* [func@Gcr.secure_memory_alloc]
* [func@Gcr.secure_memory_try_alloc]
* [func@Gcr.secure_memory_realloc]
* [func@Gcr.secure_memory_try_realloc]
* [func@Gcr.secure_memory_is_secure]
* [func@Gcr.secure_memory_strdup]
* [func@Gcr.secure_memory_strfree]
