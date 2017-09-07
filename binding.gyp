{
  "targets": [
    {
      "target_name": "sqlite3_bindings",
	  "include_dirs" : ["<!(node -e \"require('nan')\")"],
	  "sources": [ "sqlite3_bindings.cc" ],
      "link_settings": { "libraries": ["-lsqlite3"] }
    }
  ]
}
