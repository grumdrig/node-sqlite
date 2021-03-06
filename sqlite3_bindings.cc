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
#include <sqlite3.h>
#include <nan.h>


#define CHECK(rc) if ((rc) != SQLITE_OK) { Nan::ThrowError(sqlite3_errmsg(db->db_)); return; }

#define SCHECK(rc) if ((rc) != SQLITE_OK) { Nan::ThrowError(sqlite3_errmsg(sqlite3_db_handle(stmt->stmt_))); return; }

#define STRING_ARG(N, NAME) \
    if (info.Length() <= (N) || !info[N]->IsString()) return Nan::ThrowTypeError("Argument " #N " must be a string"); \
    Nan::Utf8String NAME(info[N]);

#define REQ_ARGS(N) if (info.Length() < (N)) { Nan::ThrowError("Expected " #N "arguments"); return; }



class Sqlite3Db : public Nan::ObjectWrap {
public:

  static void Init(v8::Local<v8::Object> exports) {
    Nan::HandleScope scope;

    v8::Local<v8::FunctionTemplate> tpl = Nan::New<v8::FunctionTemplate>(New);
    tpl->SetClassName(Nan::New("DatabaseSync").ToLocalChecked());
    tpl->InstanceTemplate()->SetInternalFieldCount(1);

    Nan::SetPrototypeMethod(tpl, "changes", Changes);
    Nan::SetPrototypeMethod(tpl, "close", Close);
    Nan::SetPrototypeMethod(tpl, "lastInsertRowid", LastInsertRowid);
    Nan::SetPrototypeMethod(tpl, "exec", Exec);
    Nan::SetPrototypeMethod(tpl, "prepare", Prepare);

    constructor.Reset(tpl->GetFunction());
    exports->Set(Nan::New("DatabaseSync").ToLocalChecked(), tpl->GetFunction());

    Statement::Init(exports);
  }

private:

  explicit Sqlite3Db(sqlite3* db) : db_(db) {
  }

  ~Sqlite3Db() {
    sqlite3_close(db_);
  }

  sqlite3* db_;

  static Nan::Persistent<v8::Function> constructor;

protected:
  static void New(const Nan::FunctionCallbackInfo<v8::Value>& info) {
    if (!info.IsConstructCall()) return Nan::ThrowError("Call as constructor with `new` expected");

    STRING_ARG(0, filename);
    sqlite3* db;
    int rc = sqlite3_open(*filename, &db);
    if (rc) {
      Nan::ThrowError("Error opening database");
      return;
    }
    Sqlite3Db* dbo = new Sqlite3Db(db);
    dbo->Wrap(info.This());

    sqlite3_commit_hook(db, CommitHook, dbo);
    sqlite3_rollback_hook(db, RollbackHook, dbo);
    sqlite3_update_hook(db, UpdateHook, dbo);

    info.GetReturnValue().Set(info.This());
  }


  //
  // JS DatabaseSync bindings
  //

  static void Changes(const Nan::FunctionCallbackInfo<v8::Value>& info) {
    Sqlite3Db* db = ObjectWrap::Unwrap<Sqlite3Db>(info.This());
    info.GetReturnValue().Set(Nan::New(sqlite3_changes(db->db_)));
  }

  static void Close(const Nan::FunctionCallbackInfo<v8::Value>& info) {
    Sqlite3Db* db = ObjectWrap::Unwrap<Sqlite3Db>(info.This());
    CHECK(sqlite3_close(db->db_));
    db->db_ = NULL;
  }

  static void LastInsertRowid(const Nan::FunctionCallbackInfo<v8::Value>& info) {
    Sqlite3Db* db = ObjectWrap::Unwrap<Sqlite3Db>(info.This());
    info.GetReturnValue().Set(Nan::New((int32_t)sqlite3_last_insert_rowid(db->db_)));
  }

  static int CommitHook(void* v_this) {
    Nan::HandleScope scope;
    auto db = static_cast<Sqlite3Db*>(v_this)->handle();
    v8::Local<v8::Value> oncommit = db->Get(Nan::New("oncommit").ToLocalChecked());
    if (oncommit->IsFunction()) {
      Nan::MakeCallback(db, oncommit.As<v8::Function>(), 0, 0);
      // TODO: allow change in return value to convert to rollback...somehow
    }
    return 0;
  }

  static void RollbackHook(void* v_this) {
    Nan::HandleScope scope;
    auto db = static_cast<Sqlite3Db*>(v_this)->handle();
    v8::Local<v8::Value> onrollback = db->Get(Nan::New("onrollback").ToLocalChecked());
    if (onrollback->IsFunction()) {
      Nan::MakeCallback(db, onrollback.As<v8::Function>(), 0, 0);
    }
  }

  static void UpdateHook(void* v_this, int operation, const char* database,
                         const char* table, sqlite_int64 rowid) {
    Nan::HandleScope scope;
    auto db = static_cast<Sqlite3Db*>(v_this)->handle();
    v8::Local<v8::Value> onupdate = db->Get(Nan::New("onupdate").ToLocalChecked());
    v8::Local<v8::Value> args[] = {
      Nan::New(operation),
      Nan::New(database).ToLocalChecked(),
      Nan::New(table).ToLocalChecked(),
      Nan::New<v8::Integer>((uint32_t)rowid)
    };
    if (onupdate->IsFunction()) {
      Nan::MakeCallback(db, onupdate.As<v8::Function>(), 4, args);
    }
  }

  static void Exec(const Nan::FunctionCallbackInfo<v8::Value>& info) {
    Nan::HandleScope scope;
    Sqlite3Db* db = ObjectWrap::Unwrap<Sqlite3Db>(info.This());
    STRING_ARG(0, sql);
    CHECK(sqlite3_exec(db->db_, *sql, NULL/*callback*/, 0/*cbarg*/, NULL/*errmsg*/));
  }

  static void Prepare(const Nan::FunctionCallbackInfo<v8::Value>& info) {
    Nan::HandleScope scope;
    Sqlite3Db* db = ObjectWrap::Unwrap<Sqlite3Db>(info.This());
    STRING_ARG(0, sql);
    sqlite3_stmt* stmt = NULL;
    const char* tail = NULL;
    CHECK(sqlite3_prepare_v2(db->db_, *sql, -1, &stmt, &tail));
    if (stmt) {
      v8::Local<v8::Value> arg = Nan::New(stmt);
      v8::Local<v8::Object> statement(Statement::NewInstance(arg));
      ObjectWrap::Unwrap<Statement>(statement)->stmt_ = stmt;
      if (tail) {
        statement->Set(Nan::New("tail").ToLocalChecked(), Nan::New(tail).ToLocalChecked());
      }
      info.GetReturnValue().Set(statement);
    } else {
      info.GetReturnValue().Set(Nan::Null());
    }
  }


  class Statement : public Nan::ObjectWrap {
  public:

    static Nan::Persistent<v8::Function> constructor;

    static void Init(v8::Handle<v8::Object> target) {
      Nan::HandleScope scope;

      v8::Local<v8::FunctionTemplate> tpl = Nan::New<v8::FunctionTemplate>(New);
      tpl->SetClassName(Nan::New("Statement").ToLocalChecked());
      tpl->InstanceTemplate()->SetInternalFieldCount(1);

      Nan::SetPrototypeMethod(tpl, "bind", Bind);
      Nan::SetPrototypeMethod(tpl, "clearBindings", ClearBindings);
      Nan::SetPrototypeMethod(tpl, "finalize", Finalize);
      Nan::SetPrototypeMethod(tpl, "bindParameterCount", BindParameterCount);
      Nan::SetPrototypeMethod(tpl, "reset", Reset);
      Nan::SetPrototypeMethod(tpl, "step", Step);

      constructor.Reset(tpl->GetFunction());
      //exports->Set(Nan::New("Statement").ToLocalChecked(), tpl->GetFunction());
    }

    static void New(const Nan::FunctionCallbackInfo<v8::Value>& info) {
      Statement* obj = new Statement((sqlite3_stmt*)info[0]->IntegerValue());
      obj->Wrap(info.This());
      info.GetReturnValue().Set(info.This());
    }

    static v8::Local<v8::Object> NewInstance(v8::Local<v8::Value> arg) {
      Nan::EscapableHandleScope scope;

      const unsigned argc = 1;
      v8::Local<v8::Value> argv[argc] = { arg };
      v8::Local<v8::Function> cons = Nan::New<v8::Function>(constructor);
      v8::Local<v8::Object> instance = cons->NewInstance(argc, argv);

      return scope.Escape(instance);
    }

  // protected:
    explicit Statement(sqlite3_stmt* stmt) : stmt_(stmt) {}

    ~Statement() { if (stmt_) sqlite3_finalize(stmt_); }

    sqlite3_stmt* stmt_;

    operator sqlite3_stmt* () const { return stmt_; }

    //
    // JS prepared statement bindings
    //

    static void Bind(const Nan::FunctionCallbackInfo<v8::Value>& info) {
      Statement* stmt = ObjectWrap::Unwrap<Statement>(info.This());

      REQ_ARGS(2);
      if (!info[0]->IsString() && !info[0]->IsInt32()) {
        Nan::ThrowError("First argument must be a string or integer");
        return;
      }
      int index = info[0]->IsString() ?
        sqlite3_bind_parameter_index(*stmt, *v8::String::Utf8Value(info[0])) :
        info[0]->Int32Value();

      if (info[1]->IsInt32()) {
        sqlite3_bind_int(*stmt, index, info[1]->Int32Value());
      } else if (info[1]->IsNumber()) {
        sqlite3_bind_double(*stmt, index, info[1]->NumberValue());
      } else if (info[1]->IsString()) {
        v8::String::Utf8Value text(info[1]);
        sqlite3_bind_text(*stmt, index, *text, text.length(),SQLITE_TRANSIENT);
      } else if (info[1]->IsNull() || info[1]->IsUndefined()) {
        sqlite3_bind_null(*stmt, index);
      } else {
        Nan::ThrowError("Unable to bind value of this type");
      }
      info.GetReturnValue().Set(info.This());
    }

    static void BindParameterCount(const Nan::FunctionCallbackInfo<v8::Value>& info) {
      Nan::HandleScope scope;
      Statement* stmt = ObjectWrap::Unwrap<Statement>(info.This());
      info.GetReturnValue().Set(Nan::New(sqlite3_bind_parameter_count(stmt->stmt_)));
    }

    static void ClearBindings(const Nan::FunctionCallbackInfo<v8::Value>& info) {
      Nan::HandleScope scope;
      Statement* stmt = ObjectWrap::Unwrap<Statement>(info.This());
      SCHECK(sqlite3_clear_bindings(stmt->stmt_));
    }

    static void Finalize(const Nan::FunctionCallbackInfo<v8::Value>& info) {
      Nan::HandleScope scope;
      Statement* stmt = ObjectWrap::Unwrap<Statement>(info.This());
      SCHECK(sqlite3_finalize(stmt->stmt_));
      stmt->stmt_ = NULL;
      //info.This().MakeWeak();
    }

    static void Reset(const Nan::FunctionCallbackInfo<v8::Value>& info) {
      Nan::HandleScope scope;
      Statement* stmt = ObjectWrap::Unwrap<Statement>(info.This());
      SCHECK(sqlite3_reset(stmt->stmt_));
    }

    static void Step(const Nan::FunctionCallbackInfo<v8::Value>& info) {
      Nan::HandleScope scope;
      Statement* stmt = ObjectWrap::Unwrap<Statement>(info.This());


      int rc = sqlite3_step(stmt->stmt_);
      if (rc == SQLITE_ROW) {
        v8::Local<v8::Object> row = Nan::New<v8::Object>();
        for (int c = 0; c < sqlite3_column_count(stmt->stmt_); ++c) {
          v8::Handle<v8::Value> value;
          switch (sqlite3_column_type(*stmt, c)) {
          case SQLITE_INTEGER:
            value = Nan::New(sqlite3_column_int(*stmt, c));
            break;
          case SQLITE_FLOAT:
            value = Nan::New(sqlite3_column_double(*stmt, c));
            break;
          case SQLITE_TEXT:
            value = Nan::New((const char*) sqlite3_column_text(*stmt, c)).ToLocalChecked();
            break;
          case SQLITE_NULL:
          default: // We don't handle any other types just now
            value = Nan::Undefined();
            break;
          }
          row->Set(Nan::New(sqlite3_column_name(*stmt, c)).ToLocalChecked(), value);
        }
        info.GetReturnValue().Set(row);
      } else if (rc == SQLITE_DONE) {
        info.GetReturnValue().Set(Nan::Null());
      } else {
        Nan::ThrowError(sqlite3_errmsg(sqlite3_db_handle(stmt->stmt_)));
      }
    }

  };


};


Nan::Persistent<v8::Function> Sqlite3Db::constructor;
Nan::Persistent<v8::Function> Sqlite3Db::Statement::constructor;

void InitAll(v8::Local<v8::Object> exports) {
  Sqlite3Db::Init(exports);
}

NODE_MODULE(addon, InitAll)



