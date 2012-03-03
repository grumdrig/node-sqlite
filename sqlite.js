/*
Copyright (c) 2009, Eric Fredricksen <e@fredricksen.net>

Permission to use, copy, modify, and/or distribute this software for any
purpose with or without fee is hereby granted, provided that the above
copyright notice and this permission notice appear in all copies.

THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/

// TODO: async

function mixin(target, source) {
  for (var name in source) {
    if (source.hasOwnProperty(name))
      target[name] = source[name];
  }
}

var bindings = require("./sqlite3_bindings");
mixin(GLOBAL, bindings);
mixin(exports, bindings);


// Conform somewhat to http://dev.w3.org/html5/webdatabase/#sql

exports.SQLITE_DELETE = 9;
exports.SQLITE_INSERT = 18;
exports.SQLITE_UPDATE = 23;


exports.openDatabaseSync = function (name, version, displayName, 
                                     estimatedSize, creationCallback) {
  // 2nd-4th parameters are ignored
  var db = new DatabaseSync(name);
  if (creationCallback) creationCallback(db);
  return db;
}


DatabaseSync.prototype.query = function (sql, bindings, callback) {
  // TODO: error callback
  if (typeof(bindings) == "function") {
    var tmp = bindings;
    bindings = callback;
    callback = tmp;
  }

  var all = [];
  
  var stmt = this.prepare(sql);
  while(stmt) {
    if (bindings) {
      if (Object.prototype.toString.call(bindings) === "[object Array]") {
        for (var i = 0; i < stmt.bindParameterCount(); ++i)
          stmt.bind(i+1, bindings.shift());
      } else {
        for (var key in bindings) 
          if (bindings.hasOwnProperty(key))
            stmt.bind(key, bindings[key]);
      }
    }
      
    var rows = [];

    while (true) {
      var row = stmt.step();
      if (!row) break;
      rows.push(row);
    }

    rows.rowsAffected = this.changes();
    rows.insertId = this.lastInsertRowid();

    all.push(rows);

    stmt.finalize();
    stmt = this.prepare(stmt.tail);
  }

  if (all.length == 0) {
    var result = null;
  } else {
    for (var i = 0; i < all.length; ++i) {
      var resultset = all[i];
      resultset.all = all;
      resultset.rows = {item: function (index) { return resultset[index]; },
                        length: resultset.length};
    }
    var result = all[0];
  }
  if (typeof(callback) == "function") {
    callback.apply(result, all);
  }
  return result;
}



// TODO: void *sqlite3_commit_hook(sqlite3*, int(*)(void*), void*);
// TODO: void *sqlite3_rollback_hook(sqlite3*, void(*)(void *), void*);


function SQLTransactionSync(db, txCallback, errCallback, successCallback) {
  this.database = db;

  this.rolledBack = false;

  this.executeSql = function(sqlStatement, arguments, callback) {
    if (this.rolledBack) return;
    var result = db.query(sqlStatement, arguments);
    if (callback) {
      var tx = this;
      callback.apply(result, [tx].concat(result.all));
    }
    return result;
  }

  var that = this;
  function unroll() {
    that.rolledBack = true;
  }
    
  db.addListener("rollback", unroll);

  this.executeSql("BEGIN TRANSACTION");
  txCallback(this);
  this.executeSql("COMMIT");

  db.removeListener("rollback", unroll);

  if (!this.rolledBack && successCallback) {
    successCallback(this);
  } else if (this.rolledBack && errCallback) {
    errCallback(this);
  }
}


DatabaseSync.prototype.transaction = function (txCallback, errCallback, 
                                               successCallback) {
  var tx = new SQLTransactionSync(this, txCallback, 
                                  errCallback, successCallback);
}

// TODO: readTransaction()

