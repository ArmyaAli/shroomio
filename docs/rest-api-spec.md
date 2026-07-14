# Shroomio REST API v1

Status: implementation contract for issues #329, #331, #332, and #333.

## Conventions

The API is served only over HTTPS under `/v1`. Requests and responses use UTF-8 JSON with
`Content-Type: application/json`; request bodies larger than 64 KiB receive `413`. Clients send
`Accept: application/json`. Timestamps are UTC RFC 3339 strings. Resource IDs are opaque strings
and must not be interpreted by clients.

Every response includes `X-Request-ID`. A client may supply a UUID in that header; otherwise the
server creates one. Successful responses containing JSON use the documented object directly, not
a data envelope. Unknown JSON fields are ignored for forward compatibility. Missing required
fields, wrong types, and unknown enum values receive `400 invalid_request`.

Authenticated routes require:

```http
Authorization: Bearer <access_token>
```

Plain HTTP must be rejected outside an explicit development mode. Logs may contain method, route,
status, request ID, latency, and authenticated user ID. They must never contain authorization or
cookie headers, request/response bodies, passwords, tokens, email addresses, webhook signatures,
or checkout URLs.

## Errors

All non-2xx responses use one envelope:

```json
{
  "error": {
    "code": "invalid_request",
    "message": "username must contain 3 to 32 letters, numbers, underscores, or hyphens",
    "request_id": "0190f13c-8f25-7d19-a86f-90a1f5141c63"
  }
}
```

`message` is safe to show to a user and must not expose credentials, account existence during
login, SQL details, or processor payloads. Stable codes are:

| HTTP | Code | Meaning |
| --- | --- | --- |
| 400 | `invalid_request` | Malformed JSON or failed field validation. |
| 401 | `invalid_credentials` | Login credentials are incorrect. |
| 401 | `invalid_token` | Access or refresh token is missing, expired, revoked, or malformed. |
| 403 | `forbidden` | The authenticated account cannot perform the operation. |
| 404 | `not_found` | Route or referenced resource does not exist. |
| 409 | `username_taken`, `email_taken` | A unique registration field is already used. |
| 409 | `idempotency_conflict` | An idempotency key was reused with different input. |
| 413 | `payload_too_large` | Request exceeds the body limit. |
| 429 | `rate_limited` | Caller exceeded the route limit. |
| 500 | `internal_error` | Unexpected server failure. |
| 503 | `service_unavailable` | A required dependency is unavailable. |

## Tokens And Sessions

Access and refresh tokens are opaque, URL-safe values generated from at least 256 bits of a
cryptographically secure random source. Raw tokens are returned only when issued. The database
stores a SHA-256 token digest, token kind, user ID, expiry, revocation time, and refresh-family ID.

- Access tokens expire after 15 minutes.
- Refresh tokens expire after 30 days and are single use.
- Refreshing atomically revokes the presented token and creates a new token in the same family.
- Reuse of a rotated refresh token revokes the entire family.
- Login creates a new refresh family. Logout revokes the presented access token and its family.
- Passwords are hashed with Argon2id using a 16-byte random salt, 64 MiB memory, 3 iterations,
  parallelism 1, and a 32-byte output. The encoded hash records its parameters so they can be
  increased later. Passwords and raw tokens are never stored or logged.

A token pair has this shape:

```json
{
  "access_token": "opaque-access-token",
  "token_type": "Bearer",
  "expires_in": 900,
  "refresh_token": "opaque-refresh-token",
  "refresh_expires_in": 2592000
}
```

Clients keep access tokens in memory. Persistent refresh-token storage must use owner-only file
permissions or the operating system credential store. Cookies are not used by the native client.

## Rate Limits

Limits are per source IP before authentication and per user afterward. `register` and `login`
allow 5 attempts per 60 seconds; `refresh` allows 10; other authenticated routes allow 60. A
deployment may lower these values. Every limited route returns:

```http
X-RateLimit-Limit: 5
X-RateLimit-Remaining: 3
X-RateLimit-Reset: 1752537600
```

`X-RateLimit-Reset` is a Unix timestamp. A `429` also includes `Retry-After` in seconds. Failed and
successful authentication attempts both consume quota to avoid account-enumeration signals.

## Account Endpoints

### `POST /v1/account/register`

Creates a password account. `username` is 3-32 ASCII letters, numbers, underscores, or hyphens.
`email` is normalized to lowercase and has a maximum of 254 characters. `password` is 12-128 UTF-8
characters. The endpoint returns `201 Created` and a token pair so no second login is required.

```json
{
  "username": "forest_cap",
  "email": "player@example.com",
  "password": "a long unique passphrase"
}
```

```json
{
  "account": {
    "player_id": "0190f14b-87fb-70b4-aef7-55dadfab1818",
    "username": "forest_cap",
    "email": "player@example.com",
    "created_at": "2026-07-14T22:00:00Z"
  },
  "session": {
    "access_token": "opaque-access-token",
    "token_type": "Bearer",
    "expires_in": 900,
    "refresh_token": "opaque-refresh-token",
    "refresh_expires_in": 2592000
  }
}
```

Expected failures: `400 invalid_request`, `409 username_taken`, `409 email_taken`, and `429
rate_limited`.

### `POST /v1/account/login`

Accepts either username or email in `identity`. The response is deliberately identical for an
unknown identity and a wrong password. Returns `200 OK` with a token pair.

```json
{
  "identity": "forest_cap",
  "password": "a long unique passphrase"
}
```

```json
{
  "access_token": "opaque-access-token",
  "token_type": "Bearer",
  "expires_in": 900,
  "refresh_token": "opaque-refresh-token",
  "refresh_expires_in": 2592000
}
```

Expected failures: `400 invalid_request`, `401 invalid_credentials`, and `429 rate_limited`.

### `POST /v1/account/refresh`

Atomically rotates a refresh token. It does not accept an access token and returns `200 OK` with a
new token pair. Concurrent use has one winner; every later use of the old token fails.

```json
{
  "refresh_token": "opaque-refresh-token"
}
```

Expected failures: `400 invalid_request`, `401 invalid_token`, and `429 rate_limited`.

### `POST /v1/account/logout`

Requires an access token. The optional refresh token lets the server identify the refresh family;
when omitted, the family associated with the access token is revoked. Revocation is idempotent.
Returns `204 No Content` with an empty body.

```json
{
  "refresh_token": "opaque-refresh-token"
}
```

Expected failure: `401 invalid_token` when the bearer token cannot identify a known session. A
known token for an already-revoked session returns `204`, making retries idempotent.

### `GET /v1/account/me`

Returns `200 OK` for the bearer token's profile. `unlocked_species` contains stable catalog IDs;
an empty array is valid.

```json
{
  "player_id": "0190f14b-87fb-70b4-aef7-55dadfab1818",
  "username": "forest_cap",
  "email": "player@example.com",
  "created_at": "2026-07-14T22:00:00Z",
  "unlocked_species": [0, 2, 5]
}
```

Expected failure: `401 invalid_token`.

## Billing Endpoints

Shroomio does not accept card or bank details. Checkout responses redirect the user to a hosted
payment-processor page. Processor secrets come from runtime secret storage and never from request
JSON or the repository.

### `POST /v1/billing/checkout`

Requires a bearer token and an `Idempotency-Key` UUID header. `sku` must match a server-side
catalog entry; clients cannot provide price, currency, or entitlement values. `quantity` is an
integer from 1 through 10. Returns `201 Created`; replaying the same key and body returns the
original response.

```json
{
  "sku": "spore-pack-small",
  "quantity": 1,
  "success_url": "shroomio://checkout/success",
  "cancel_url": "shroomio://checkout/cancel"
}
```

```json
{
  "checkout_id": "checkout_0190f16d",
  "checkout_url": "https://payments.example/hosted/opaque",
  "expires_at": "2026-07-14T22:30:00Z"
}
```

Expected failures: `400 invalid_request`, `401 invalid_token`, `404 not_found`, `409
idempotency_conflict`, and `503 service_unavailable`.

### `POST /v1/billing/webhook`

This processor-facing route has no bearer token. It verifies the raw request bytes against the
processor secret before parsing JSON. The adapter normalizes provider headers to
`X-Webhook-ID`, `X-Webhook-Timestamp` (Unix seconds), and `X-Webhook-Signature` (`v1=<hex HMAC>`).
The HMAC covers `<timestamp>.<raw-body>`. Signatures older than five minutes are rejected.
Processor event IDs are unique and make delivery idempotent. Supported event types are
`checkout.completed`, `checkout.failed`, and `checkout.refunded`.

```json
{
  "id": "event_0190f178",
  "type": "checkout.completed",
  "created_at": "2026-07-14T22:15:00Z",
  "data": {
    "checkout_id": "checkout_0190f16d",
    "status": "paid",
    "processor_transaction_id": "transaction_opaque"
  }
}
```

A valid supported event returns `204 No Content`, including duplicate deliveries. An authenticated
but unsupported event returns `204` and is audit logged by event ID and type. Invalid JSON returns
`400 invalid_request`; a missing, expired, or invalid signature returns `401 invalid_token`; an
unavailable database returns `503 service_unavailable`. Bodies, signatures, checkout URLs, and
processor customer data are never logged.

## Health Endpoint

`GET /health` is unversioned, unauthenticated, and rate limited. It returns `200 OK` when the HTTPS
listener and database are ready, otherwise `503 service_unavailable`.

```json
{
  "status": "ok",
  "service": "shroomio-server"
}
```

It exposes no build secrets, filesystem paths, database names, or player data.

## Versioning And Deprecation

Breaking request or response changes require a new path prefix such as `/v2`; additive fields and
new error codes are non-breaking. Servers support an old major version for at least one published
client release after its replacement ships. Deprecated responses include the RFC 9745
`Deprecation` header, the RFC 8594 `Sunset` header, and a `Link` header pointing to migration
documentation. Clients must ignore unknown response fields and must not retry non-idempotent
requests without an idempotency key.

## Implementation Boundaries

- #329 owns TLS termination, routing, `/health`, body limits, request IDs, and access logging.
- #331 owns account validation, Argon2id, token persistence/rotation, and auth rate limits.
- #332 owns the server-side product catalog, hosted checkout adapter, webhook verification,
  idempotency records, and billing audit events.
- #333 owns HTTPS-only client calls and protected refresh-token storage.
- #328 remains the parent acceptance tracker. Gameplay stays on the existing UDP/ENet interface.
