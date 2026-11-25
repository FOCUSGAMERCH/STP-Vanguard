#if defined _STP_VANGUARD_INCLUDED
    #endinput
#endif
#define _STP_VANGUARD_INCLUDED

forward OnPlayerAntiCheatConnect(playerid, bool:is_latest_version);
forward OnPlayerAntiCheatType(playerid, clientType);
forward OnPlayerCheckHwid(playerid, const hwid[]);
forward OnPlayerCheckHwidV2(playerid, const hwid[]);
forward OnPlayerDetectedIllegalFile(playerid, cheatId);
forward OnPlayerDetectedInjectedModule(playerid, const modulePath[]);
forward OnPlayerDetectedNewInjectedModule(playerid, const modulePath[]);
forward OnPlayerDetectedDynamicFile(playerid, fileType, const filename[], const md5hash[]);
forward OnPlayerAntiCheatTimeout(playerid, status);
forward OnPlayerAntiCheatRecovered(playerid, status);

// Optional callbacks: OnPlayerAntiCheatTimeout status=0 after 45 seconds without
// a valid challenge response; OnPlayerAntiCheatRecovered status=1 on recovery.
// Apply your server's reconnect / kick policy here. A timeout is not proof of cheating.
