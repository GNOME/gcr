Contributing to GCR, GCK libraries
==================================

Code merging
------------
This is the work flow for modifying the gcr repository:

1. File a bug for the given flaw or feature (if it does not already exist) at
   https://gitlab.gnome.org/GNOME/gcr/issues.

2. Create a fork of the main repository on gitlab.gnome.org (if you haven't
   already) and commit your work there.

3. If this is a non-trivial flaw or feature, write test cases. We won't accept
   significant changes without adequate test coverage.

4. Write code to fix the flaw or add the feature. In the case that tests are
   necessary, the new tests must pass consistently.

5. All code must follow the project coding style (see below).

6. Make a Merge Request from your fork to the main gcr repository.

7. The project must remain buildable with all meson options and pass all
   tests on all platforms. It should also pass the CI.

8. Rework your code based on suggestions in the review and submit new patches.
   Return to the review step as necessary.

9. Finally, if everything is reviewed and approved, a maintainer will merge it
   for you. Thanks!


Repository Structure
--------------------
* egg: Various bits of code shared with other modules (private)
* gck: A public library for accessing PKCS#11 modules.
* gcr: A public library for bits of crypto and parsing etc...
* ui: A public library that provides a GTK UI for several objects in gcr.
* schema: Desktop settings schemas for crypto stuff
* testing: Testing CA, gnupg and other mock setups
* po: Contains translation files

Coding style
------------
Our coding style is very similar to the linux coding style:

  http://lxr.linux.no/linux/Documentation/CodingStyle

+ Space between function name and parentheses.

```
my_function_call (arg1, arg2);
```

* Braces on the same line as conditional with spaces around braces:

```
if (test) {
	do_y ();
	do_z ();
}

switch (value) {
case CONSTANT:
	do_z ();
	break;
default:
	break;
}
```

* Braces around functions on a separate line from function name,
  return value on a separate line, arguments on separate lines.

```
static void
my_special_function (int arg1,
                     int arg2)
{
	/* body of function */
}
```

* Don't use braces unnecessarily:

```
	if (test)
		do_this_thing ();
```

* But use braces here, when one section has more than a line:

```
	if (test) {
		do_this_thing ();
	} else {
		do_other_thing ();
		smile_nicely ();
	}
```

* Use of tabs for 8 char indent.

```
------->if (test) {
------->------->Value;
------->------->Value;
------->}
```

* No trailing whitespace on lines.

* The '*' in a pointer declaration belongs with the variable name:

```
	char *name;
```

+ Extra long wrapped lines should wrap to function opening brace
  using spaces past indentation point.

```
------>my_function_call ("this is a very long argument here",
------>                  "wrapped argument is indented with spaces");
```

* Function names are in lower case with _ separators.

```
this_is_a_long_function_name ();
```

* Constants are all in upper case with _ separators.

```
THIS_IS_A_CONSTANT
```

+ Structures should be typedefed to avoid saying 'struct' and names
  are CamelCase:

```
ThisIsAStruct
```

* One line comments should look like:

```
/* This is a one line comment */
```

* Multi line comments should look like:

```
/*
 * This is a multiline comment.
 * And it has a useless second line.
 */
```

When in doubt, adapt to the style of the code around your patch.
