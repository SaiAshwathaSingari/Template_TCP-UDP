## Test Cases (Positive + Negative)

These test cases cover both the **HTTP API** and **WebSocket** flows, including positive, negative, and edge scenarios.

### Authentication (HTTP)

- **TC-A1 Register (valid)**: `POST /api/register` with `{"username":"abc","password":"pass"}` → **200 OK**, returns `token`.
- **TC-A2 Register (missing fields)**: missing `username` or `password` → **400** `Missing fields`.
- **TC-A3 Register (short username)**: username length < 3 → **400**.
- **TC-A4 Register (short password)**: password length < 4 → **400**.
- **TC-A5 Register (duplicate username)**: register same username twice → **400** `Username already taken`.
- **TC-A6 Login (valid)**: `POST /api/login` correct credentials → **200 OK**, returns `token`.
- **TC-A7 Login (wrong password)** → **401** `Invalid credentials`.
- **TC-A8 Login (unknown username)** → **401** `Invalid credentials`.
- **TC-A9 Wrong method**: `GET /api/login` or `GET /api/register` → **400** `POST required`.
- **TC-A10 Incomplete body**: `Content-Length` says N, but fewer bytes sent → **400** `Missing or incomplete body`.

### Authentication (WebSocket)

- **TC-AW1 WS register (valid)**: `{"type":"auth_register","username":"abc","password":"pass"}` → `auth_response success:true`.
- **TC-AW2 WS login (valid)**: `{"type":"auth_login","username":"abc","password":"pass"}` → `auth_response success:true`.
- **TC-AW3 WS auth (missing fields)** → `auth_response success:false`.
- **TC-AW4 Protected WS message without auth**: send `doc_list` before login → `error Not authenticated`.

### Documents (HTTP) — requires `Authorization: Bearer <token>`

- **TC-D1 List docs (valid)**: `GET /api/docs` with token → **200 OK**, returns array.
- **TC-D2 List docs (no token)** → **401 Unauthorized**.
- **TC-D3 Create doc (valid)**: `POST /api/docs` with `{"name":"MyDoc"}` → **200 OK**, returns `doc_id`.
- **TC-D4 Create doc (missing name)** → **400**.
- **TC-D5 Get doc (valid)**: `GET /api/docs/<id>` existing → **200 OK**.
- **TC-D6 Get doc (not found)** → **404**.
- **TC-D7 Update doc (valid)**: `PUT /api/docs/<id>` with correct `version` and new `content` → **200 OK**, version increments.
- **TC-D8 Update doc (version conflict)**: send stale `version` → **409 Conflict**, returns latest `version` and `content`.
- **TC-D9 Update doc (missing fields)**: missing `content` or `version` → **400**.
- **TC-D10 Delete doc (owner)**: `DELETE /api/docs/<id>` by owner → **200 OK**.
- **TC-D11 Delete doc (non-owner)** → **401** `Only owner can delete`.
- **TC-D12 Delete doc (not found)** → **404**.

### Documents (WebSocket) — requires authenticated session

- **TC-DW1 Create doc**: `{"type":"doc_create","name":"X"}` → `doc_created`.
- **TC-DW2 List docs**: `{"type":"doc_list"}` → `doc_list` with documents JSON.
- **TC-DW3 Join doc (valid)**: `{"type":"doc_join","doc_id":1}` → `doc_sync` + `user_list`.
- **TC-DW4 Join doc (missing doc_id)** → `error Missing doc_id`.
- **TC-DW5 Edit doc (valid)**: `{"type":"doc_edit","doc_id":1,"content":"...","cursor":10}` → broadcast to other clients, version increments.
- **TC-DW6 Cursor update**: `{"type":"doc_cursor","doc_id":1,"cursor":12}` → broadcast to others.
- **TC-DW7 Leave doc**: `{"type":"doc_leave"}` → user list updates.

### Chat (HTTP) — requires token

- **TC-C1 Post chat (valid)**: `POST /api/chat` with `{"doc_id":1,"message":"hi"}` → **200 OK**.
- **TC-C2 Post chat (missing fields)** → **400**.
- **TC-C3 Get chat (valid)**: `GET /api/chat?doc_id=1&since=0` → **200 OK**, messages array.
- **TC-C4 Get chat (missing doc_id)** → **400** `doc_id required`.

### Chat (WebSocket)

- **TC-CW1 Send chat (valid)**: `{"type":"chat","doc_id":1,"message":"hi"}` → broadcast to doc subscribers.
- **TC-CW2 Send chat (missing fields)** → no broadcast / ignored (negative validation case).

### Network / Robustness / Negative Scenarios

- **TC-N1 Invalid HTTP endpoint**: `GET /api/unknown` → **404**.
- **TC-N2 Malformed request line** → **400/connection close**.
- **TC-N3 WebSocket close**: client sends close frame → server responds close + cleanup.
- **TC-N4 Max clients**: open > `MAX_CLIENTS` connections → extra connection gets **503**.
- **TC-N5 Large WS payload**: attempt payload > server limits → decode error / disconnect expected (verify).
- **TC-N6 SQL injection attempt**: username like `a' OR '1'='1` should not bypass login (prepared statements).
- **TC-N7 Invalid/expired token**: random token in Authorization header → **401**.
