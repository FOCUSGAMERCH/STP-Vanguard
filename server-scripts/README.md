# SA-MP Server Scripts

This folder contains the Pawn-side examples for connecting STP-Vanguard to a San Andreas Multiplayer (SA-MP) server. These files are intended to be reviewed and integrated with the server's existing game mode and plugin setup.

## Files

- `STP-Vanguard.pwn` — example game-mode integration. It tracks client verification state and handles server-side responses to client checks and reports.
- `AddDynamicCheat.pwn` — SQL-backed dynamic-cheat administration and detection data handling.
- `include/anticheat.inc` — callback declarations used by the Pawn integration to receive client verification, client type, and HWID events.

## Setup overview

1. Use the matching STP-Vanguard client and server-plugin build from this release.
2. Add the files from `include/` to your Pawn compiler's include path, and make the existing YSI, sscanf, and MySQL Pawn dependencies available.
3. Integrate the example code with your SA-MP game mode. The scripts expect the MySQL connection handle `g_SQL` to be initialized by your server.
4. Create the database tables using [`database/schema.sql`](../database/schema.sql), then review the SQL and enforcement actions for your server before enabling them.

Do not commit database credentials. Keep configuration in your server's private configuration files.

## Source encoding

The Pawn files contain Thai comments and use CP874 in the local working tree for compatibility with classic Pawn tools. The repository uses `.gitattributes` so a Git clone checks them out in the expected encoding. If you download a ZIP directly from GitHub and Thai text looks garbled in your editor, convert the `.pwn` and `.inc` files to Windows-874/CP874 before editing or compiling.

For the full protocol, data-flow explanation, screenshots, limitations, and license terms, see the [main project README](../README.md).
