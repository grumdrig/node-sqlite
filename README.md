= SQLite bindings for Node.js =

Interface to an [http://sqlite.org/ SQLite] (version 3) database
within [http://nodejs.org/ Node]. 

The semantics mostly follow those of the
[http://dev.w3.org/html5/webdatabase/#sql HTML5 Web SQL API], with
some extensions. Also, only the synchronous API is implemented; the
asynchronous API is a big TODO item.

== Quick Example ==

{{{
var sqlite = require("./sqlite");
var db = sqlite.openDatabaseSync("test.db");
db.query("INSERT INTO test (column) VALUES ($value)", {$value: 10});
db.query("SELECT column FROM test WHERE rowid < ?", [5], function
(records) {
  process.assert(records[0].column == 10);
  process.assert(records.rows.item(0).column == 10);  // HTML5
  semantics
});
// Multiple statement queries are supported:
db.query("UPDATE test SET column=20; SELECT column FROM test;",
function (update, select) {
  process.assert(update.rowsAffected == 1);
  process.assert(select[0].column == 20);
});
// HTML5 semantics
db.transaction(function(tx) {
  tx.executeSql("SELECT * FROM test WHERE column = ?", [10], function
  (tx,res) {
    process.assert(res.rows.item(0).row.column == 10);
  });
});
db.close();
}}}



== Functions ==

=== `var db = sqlite.openDatabaseSync(filename)` ===

  Returns an object representing the sqlite3 database with given
  filename.

=== `db.query(sql [,bindings] [,callback])` ===

  Executes the query `sql`, with variables bound from `bindings`. The
  variables can take the form 
    * *`?` or `?N`, where `N` is a number*, in which case `bindings`
  should be an array of values, or 
    * *`$id`, where `$id` is an identifier*, in which case `bindings`
  should be an object with keys matching the variable names (e.g. `{
  $name: "Rumplestiltskin" }`.

  If provided the `callback` is called with a number of arguments
  equal
  to the number of statements in the query. Each argument is a result
  set which is an array of objects mapping column names to values.

  Each result set `r` also has these accessors:
    * *`r.rowsAffected`* is the number of rows affected by an `UPDATE`
  query.   
    * *`r.insertId`* is the `ROWID` of the new row in an `INSERT`
  query    
    * *`r.rows.length`* (also just `r.length`) is the number of rows
  in a
       `SELECT` result  
    * *`r.all`* is an array of result sets, one for each statement in
  the
       query

  The return value is the first (often only) result set.

== Install ==

Install node. http://nodejs.org/

{{{
$ git clone git://github.com/grumdrig/node-sqlite.git
$ cd node_sqlite
$ node-waf configure
$ node-waf build
}}}

== Test ==

{{{
$ node test.js
$ node doc/examples.js
}}}


== V8 ==

The bindings are somewhat specific to Node but are probably fairly
easily adaptable to work, more generally, with
[http://code.google.com/p/v8/ V8].

