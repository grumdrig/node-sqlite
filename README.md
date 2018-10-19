SQLite3 bindings for Node.js
=============================

Documentation lives at http://grumdrig.com/node-sqlite/
The code lives at http://github.com/grumdrig/node-sqlite/

The two files required to use these bindings are sqlite.js and
build/default/sqlite3_bindings.node. Put this directory in your
NODE_PATH or copy those two files where you need them.

Tested with Node version v8.4.0 (may not work with older versions)

### Synchronous Only

Only synchronous access is supported! For an ansynchronous and
much more full-featured library, see
  https://github.com/mapbox/node-sqlite3

### TODO

The C++ code which creates the `Statement` object uses `NewInstance`,
which is marked deprecated. I don't know what it should be replaced
with but that whole bit code is pretty ugly and could probably be
improved a lot.