# Database setup

1. Back up your existing database.
2. Select the database used by the SA-MP game mode in phpMyAdmin or your MySQL client.
3. Import [schema.sql](schema.sql).
4. Ensure the Pawn scripts use the same existing MySQL connection handle, `g_SQL`, and include/plugin version as your server.
5. Adapt the game mode's MySQL initialization, include paths, and callbacks before compiling.

## Tables

- `dynamic_cheats`: stores MD5 signature text, a display name, action, and add/update metadata. `id` is auto-increment because the included Pawn script inserts rows without providing an ID. Keep signature data to MD5 hex strings and review `action` handling in your script before enabling an entry.
- `banned_hwids`: stores the HWID, player name, reason, optional location, active flag, operator, and ban timestamp. The `hwid` index supports the lookup used by the example Pawn code. Set `Ban` to `0` to disable a record while retaining its history.

The schema reflects the project screenshots and Pawn queries. It is a starting point; reconcile it with any existing `players`, `g_SQL`, table naming, and moderation logic before importing into a production database. The repository intentionally contains no live player records or database credentials.

The stored HWID and player moderation history are sensitive. Restrict database permissions, protect backups, tell players what is collected, and define an appropriate retention/deletion policy.
