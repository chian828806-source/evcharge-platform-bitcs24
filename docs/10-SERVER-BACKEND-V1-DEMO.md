# Server Backend V1 Demo

## Purpose

This is the Backend V1 Demo baseline. Qt User and Qt Admin share one independent Qt/C++ server, providing a common baseline for UI, Web and later ML integration.

## Current architecture

```text
Client -> SocketServer -> MessageDispatcher -> Handler -> Service -> Repository -> SQLite
qt-server/
  handlers/user, handlers/admin
  services/user, services/admin
  repositories, models, database, network, common
  main.cpp, qt-server.pro
```

`main.cpp` is the only composition root. It creates one `DatabaseManager`, `SessionManager`, `MessageDispatcher`, TCP `SocketServer`, and optional dashboard WebSocket server. User and Admin handlers are separately registered on that same dispatcher. Repositories, database and common password hashing are shared infrastructure; clients never access SQLite.

Handlers validate payload shape and map responses, services implement business rules and transactions, and repositories contain parameterized SQL. User services retain the order write transaction path; station creation also uses a transaction.

## Implemented User APIs

`USER_LOGIN`, `USER_PROFILE_GET`, `USER_PROFILE_UPDATE`, `STATION_LIST_NEARBY`, `STATION_DETAIL_GET`, `ORDER_ACTIVE_CHECK`, `ORDER_CREATE`, `ORDER_CANCEL`, `ORDER_START`, `ORDER_STOP`, `ORDER_SETTLE`.

## Implemented Admin APIs

`ADMIN_LOGIN`, `ADMIN_REVENUE_SUMMARY`, `ADMIN_REVENUE_TREND`, `ADMIN_PILE_STATUS_SUMMARY`, `ADMIN_PILE_LIST`, `ADMIN_PILE_RESTART`, `ADMIN_STATION_LIST`, `ADMIN_STATION_CREATE`, `ADMIN_USER_LIST`, `ADMIN_USER_FREEZE`, `ADMIN_USER_UNFREEZE`.

Admin routes require an Admin session; User routes require a User session. A User session is rejected for `ADMIN_*` routes by `MessageDispatcher`.

## Build and run

From `qt-server`, run `qmake -spec win32-g++ qt-server.pro` then `mingw32-make`. Start `release/evcharge-qt-server.exe --database ../database/evcharge.db`. TCP defaults to `18080`; the dashboard WebSocket defaults to `18081` at `/dashboard`. Initialize the SQLite file with `database/schema.sql` before startup.

## Smoke-test checklist

- [ ] Server starts and database initializes
- [ ] `USER_LOGIN` and a station/order request reach User handlers
- [ ] `ADMIN_LOGIN`, revenue, and freeze/unfreeze reach Admin handlers
- [ ] User/Admin share one SocketServer, Dispatcher, SessionManager and DatabaseManager
- [ ] Frozen-user restriction, order transaction and pile restart work

## Known limitations / TODO

ML integration, full Web dashboard integration, charging simulation, background-worker abstraction, advanced authorization/concurrency, repository/model cleanup, and end-to-end automated integration tests remain V1.x work.
