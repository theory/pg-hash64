hash64 0.1.0
============

This library contains the PostgreSQL extension function, `hash64()`, that
hashes a text value and returns a unique 64-bit integer (`bigint`). The
function is similar to the built-in `hashtext()` function, but is guaranteed
not to change over major releases, so that it can be used safely for
partitioning or sharding, for example with PL/Proxy, in installations
involving multiple PostgreSQL major versions.

To build hash64, just do this:

    make
    make installcheck
    make install

If you encounter an error such as:

    "Makefile", line 8: Need an operator

You need to use GNU make, which may well be installed on your system as
`gmake`:

    gmake
    gmake install
    gmake installcheck

If you encounter an error such as:

    make: pg_config: Command not found

Be sure that you have `pg_config` installed and in your path. If you used a
package management system such as RPM to install PostgreSQL, be sure that the
`-devel` package is also installed. If necessary tell the build process where
to find it:

    env PG_CONFIG=/path/to/pg_config make && make installcheck && make install

If you encounter an error such as:

    ERROR:  must be owner of database regression

You need to run the test suite using a super user, such as the default
"postgres" super user:

    make installcheck PGUSER=postgres

Once hash64 is installed, you can add it to a database. If you're running
PostgreSQL 9.1.0 or greater, it's a simple as connecting to a database as a
super user and running:

    CREATE EXTENSION hash64;

If you've upgraded your cluster to PostgreSQL 9.1 and already had hash64
installed, you can upgrade it to a properly packaged extension with:

    CREATE EXTENSION hash64 FROM unpackaged;

For versions of PostgreSQL less than 9.1.0, you'll need to run the
installation script:

    psql -d mydb -f /path/to/pgsql/share/contrib/hash64.sql

If you want to install hash64 and all of its supporting objects into a
specific schema, use the `PGOPTIONS` environment variable to specify the
schema, like so:

    PGOPTIONS=--search_path=extensions psql -d mydb -f hash64.sql

Dependencies
------------
The `hash64` data type has no dependencies other than PostgreSQL.

Copyright and License
---------------------

* Portions Copyright (c) 2012, David E. Wheeler
* Portions Copyright (c) 2012, Peter Geoghegan
* Portions Copyright (c) 2010, Peter Eisentraut
* Portions Copyright (c) 1996-2008, PostgreSQL Global Development Group
* Portions Copyright (c) 1994, Regents of the University of California

This module is free software; you can redistribute it and/or modify it under
the [PostgreSQL License](http://www.opensource.org/licenses/postgresql).

Permission to use, copy, modify, and distribute this software and its
documentation for any purpose, without fee, and without a written agreement is
hereby granted, provided that the above copyright notice and this paragraph
and the following two paragraphs appear in all copies.

In no event shall David E. Wheeler or Sam Vilain be liable to any party for
direct, indirect, special, incidental, or consequential damages, including
lost profits, arising out of the use of this software and its documentation,
even if David E. Wheeler or Sam Vilain has been advised of the possibility of
such damage.

David E. Wheeler and Sam Vilain specifically disclaim any warranties,
including, but not limited to, the implied warranties of merchantability and
fitness for a particular purpose. The software provided hereunder is on an "as
is" basis, and David E. Wheeler and Sam Vilain have no obligations to provide
maintenance, support, updates, enhancements, or modifications.
