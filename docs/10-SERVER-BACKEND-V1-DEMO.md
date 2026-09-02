# Server Backend V1 Demo

## 1. Purpose

Backend V1 Demo is the current server baseline. Qt User and Qt Admin use one
independent Qt/C++ server to complete the basic charging and administration
flows. It is the common integration baseline for the Qt clients, the Web
dashboard, and later ML work.

## 2. Current Architecture

```text
qt-server/
├── common/                         # shared password hashing and result types
├── database/                       # shared SQLite connection manager
├── handlers/
│   ├── user/                       # USER/STATION/ORDER routes
│   └── admin/                      # ADMIN routes
├── models/                         # shared user, station and order models
├── network/                        # SocketServer, Dispatcher, SessionManager, WebSocket
├── repositories/                   # shared parameterized SQL repositories
├── services/
│   ├── user/                       # user business rules and order transactions
│   └── admin/                      # admin authentication, analytics and management
├── main.cpp                        # the sole composition root
└── qt-server.pro                   # aggregates module .pri files
```

`main.cpp` creates exactly one `DatabaseManager`, `SessionManager`,
`MessageDispatcher`, and TCP `SocketServer`. `UserBackendRegistry` and
`AdminHandlerRegistry` register their isolated handlers on that same
dispatcher. The dashboard WebSocket server is a separate protocol endpoint,
not another business SocketServer or Dispatcher.

## 3. Dependency Rules

```text
Client -> SocketServer -> MessageDispatcher -> Handler -> Service -> Repository -> Database
```

- Handlers validate JSON fields and types, obtain the trusted session context,
  call services, and return the shared response format.
- User and Admin handlers/services are isolated by directory and dispatcher
  access role.
- Repositories, `DatabaseManager`, networking, sessions, models, and common
  password hashing are shared. There is no role-specific user, station, or
  order repository.
- Handlers and services do not embed SQL; repositories use prepared statements
  and bound values.
- Clients never connect to SQLite directly.

## 4. Implemented User APIs

The following routes are registered by `UserBackendRegistry` and require a User
session unless noted otherwise:

| API | Access |
| --- | --- |
| `USER_LOGIN` | Public |
| `USER_PROFILE_GET` | User |
| `USER_PROFILE_UPDATE` | User |
| `STATION_LIST_NEARBY` | User |
| `STATION_DETAIL_GET` | User |
| `ORDER_ACTIVE_CHECK` | User |
| `ORDER_CREATE` | User |
| `ORDER_CANCEL` | User |
| `ORDER_START` | User |
| `ORDER_STOP` | User |
| `ORDER_SETTLE` | User |

## 5. Implemented Admin APIs

The following routes are registered by `AdminHandlerRegistry`; only
`ADMIN_LOGIN` is public and every other route requires an Admin session:

`ADMIN_LOGIN`, `ADMIN_REVENUE_SUMMARY`, `ADMIN_REVENUE_TREND`,
`ADMIN_PILE_STATUS_SUMMARY`, `ADMIN_PILE_LIST`, `ADMIN_PILE_RESTART`,
`ADMIN_STATION_LIST`, `ADMIN_STATION_CREATE`, `ADMIN_USER_LIST`,
`ADMIN_USER_FREEZE`, and `ADMIN_USER_UNFREEZE`.

## 6. Shared Infrastructure

- `SocketServer`: one TCP listener for both User and Admin requests.
- `MessageDispatcher`: the single route table and role gate; User sessions are
  rejected from `ADMIN_*` routes and Admin sessions from User-only routes.
- `SessionManager`: creates and validates User and Admin sessions.
- `DatabaseManager`: owns the SQLite connection configuration and supplies the
  thread-local QtSql connection used by the server.
- `repositories/`: `AdminRepository`, `UserRepository`, `StationRepository`,
  `PileRepository`, `OrderRepository`, and `OperationLogRepository`.
- `common/`: shared password hashing; User and Admin do not carry duplicate
  password helpers.

## 7. Database Access Rules

Only repositories perform database access. User and Admin share the same
repositories for shared entities: `UserRepository`, `StationRepository`, and
`OrderRepository`. Services own multi-write transactions: user order creation,
start/stop/settle/cancel flows; admin station creation; user freeze/unfreeze;
and pile restart state changes. A failure rolls back the active transaction.

## 8. Known Limitations / TODO

- ML integration and prediction routes
- Full Web dashboard data publishing
- Charging simulation and a background-worker abstraction
- Advanced authorization, concurrency, and connection-pool work
- Complete repository/model cleanup and automated integration coverage

## 8.1 Integration evolution

This Demo was assembled from three source branches.  
`codex/docs-server-backend-isolation` defined the shared Server/Session/
Database boundary and isolation documentation; `feature/user-backend` supplied
the User, station and transactional order flow; and
`fix/admin-qt-chart-namespace` supplied Admin authentication, analytics,
management, operation logs and remote-restart behavior.

First, the two role implementations converged into one Qt Server executable:
one `SocketServer`, `MessageDispatcher`, `SessionManager`, and
`DatabaseManager`. User/Admin handlers and services remain separate, while
both registries attach to the same dispatcher. Second, the old aggregated Admin
`repositories.cpp/.h` was split into the shared `AdminRepository`,
`UserRepository`, `StationRepository`, `PileRepository`, `OrderRepository`,
and `OperationLogRepository`; prepared SQL remains in repositories, never in
handlers. Third, `main.cpp`, qmake `.pri` aggregation, common password code,
transactional freeze/unfreeze, database lifetime and restart timer handling
were unified.

Finally, latest `develop` was merged and `database/` is retained byte-for-byte
from that baseline: schema, seed data, CARY/simulation and ML-history assets,
`simulation/`, `__init__.py`, and the simulation database. The canonical
contract is **12 tables and 21 indexes**, including `prediction_batch`,
`data_import_batch`, `charging_session_history`, and
`station_hourly_metric`. `DatabaseManager` enables foreign keys per connection;
startup locates repository-root `database/evcharge.db` and rejects a database
missing any of the 12 required tables. Settlement clears `current_order_id`,
sets `paid_at`, deducts `balance_fen`, and updates pile count/minutes/energy in
one transaction. Revenue uses `COMPLETED` + `paid_at`; each trend point returns
`revenueFen`, `energyKwh`, and `orderCount`.

## 9. Build and Run

Verification was executed on the project VM on 2026-09-03. `qmake6` and
top-level `make -j2` passed after installing `libqt6charts6-dev` and removing
the obsolete Qt 6 `using namespace QtCharts;` declaration in the Admin UI.

```text
cd qt-server
qmake -spec win32-g++ qt-server.pro
mingw32-make
release/evcharge-qt-server.exe --database ../database/evcharge.db
```

Initialize the selected SQLite file with `database/schema.sql` before startup.
When no `--database` is supplied, the server walks upward from its launch and
executable directories to locate the repository-root `database/evcharge.db`.
It verifies the 12-table database contract before listening. TCP uses `18080`
by default and the dashboard WebSocket uses `18081` at `/dashboard`. Use
`--tcp-port` and `--websocket-port` to override the ports.

## 10. Smoke Test Checklist

- [x] Server starts with current `database/evcharge.db`, QSQLITE, foreign keys and 12-table check
- [x] User login, profile, station lookup and active-order check
- [x] `ORDER_CREATE → START → STOP → SETTLE`
- [x] Admin login, revenue summary/trend, user list, pile list and restart
- [x] One SocketServer/DatabaseManager with both handler registries
- [x] Freeze rejects order start; frozen charging user can stop and settle
- [x] Settlement updates order, balance, `current_order_id`, pile totals and revenue
