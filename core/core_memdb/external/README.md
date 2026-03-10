# core_memdb external

This directory holds the vendored SQLite amalgamation required by `core_memdb`.

Current contents:
- `sqlite3.c`
- `sqlite3.h`

Current status:
- the amalgamation is staged locally and compiled into `libcore_memdb.a`
- the `core_memdb` runtime is fully wired to the SQLite backend via `shared/core/core_memdb/src/core_memdb.c`
