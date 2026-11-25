#include <a_samp>
#include <YSI_Coding\y_hooks>
#include <sscanf2>
#include <a_mysql>
#include <anticheat>  // ac.inc

// --- [ การตั้งค่าตัวแปร ] ---
new gPlayerAntiCheatStatus[MAX_PLAYERS char];   // 0 = ไม่มี, 1 = เก่า, 2 = ล่าสุด
new bool:gPlayerGotACMessage[MAX_PLAYERS char];
new bool:gPlayerHasSpawned[MAX_PLAYERS char];
new gPendingCheatID[MAX_PLAYERS];
new gPlayerHWID[MAX_PLAYERS][65];
new gPlayerHWIDV2[MAX_PLAYERS][65];
new gPlayerACType[MAX_PLAYERS];                 // 0 = ปกติ, 1 = nocleo, -1 = ยังไม่รู้

new bool:gPlayerHWIDSaved[MAX_PLAYERS];
new bool:gPlayerHWIDSavedV2[MAX_PLAYERS];
new gPlayerACStatusString[MAX_PLAYERS][128];

// ============================================================================
// [ระบบ Dynamic Cheat Scan] กำหนดไฟล์แบน/เตะ ฝั่งเซิร์ฟเวอร์ได้ทันที
// ============================================================================

enum E_DYNAMIC_CHEAT {
    CheatHash[33],   // MD5 ของไฟล์ (ต้อง 32 ตัวอักษรพิมพ์ใหญ่)
    CheatName[64],   // ชื่อโปรที่ใช้แจ้งเตือน
    CheatAction      // 1 = เตะ (Kick), 2 = แบนบัญชี+HWID (Ban)
};

// เพิ่มไฟล์ที่ต้องการลงตรงนี้ได้เลย (ไม่ต้องอัพเดต Client ใหม่)
new const DynamicCheats[][E_DYNAMIC_CHEAT] = {
    {"479F790D850AA208A73787411C1DF00", "NormalMapFix.asidd", 1}, // 2 = แบน
    {"42C9AA8BE4B490E1F9CDC31D115C2444", "Ddxz.exe (External)", 1} // 1 = เตะ // ที่ต้องแก้คือแสดงชื่อที่เตะไม่ถูกและแสดง ไฟล์ที่ตรวจพบ:  ไม่ถูกต้องค่อยทำ ว่างต้องทำ เช่นเราใส่อันนี้แต่จะขึ้นอันแรก NormalMapfix.asi งงมากก
};

// ตัวแปรใหม่สำหรับเก็บ Dynamic Cheat ที่ตรวจพบ (คล้าย gPendingCheatID)
new gPendingDynamicCheat[MAX_PLAYERS];        // เก็บ index ใน DynamicCheats
new gPendingDynamicFileName[MAX_PLAYERS][128]; // เก็บชื่อไฟล์จริง
new gPendingDynamicMD5[MAX_PLAYERS][33];       // เก็บ MD5

// ============================================================================
// Callback ใหม่ - OnPlayerDetectedDynamicFile
// ============================================================================
/*public OnPlayerDetectedDynamicFile(playerid, fileType, const filename[], const md5hash[])
{
    if (!IsPlayerConnected(playerid)) 
        return 1;

    // ถ้าผู้เล่นยังไม่ spawn ? เก็บไว้ก่อน (เหมือนระบบ cheatid เดิม)
    if (!gPlayerHasSpawned{playerid})
    {
        for (new i = 0; i < sizeof(DynamicCheats); i++)
        {
            if (!strcmp(md5hash, DynamicCheats[i][CheatHash], true))
            {
                gPendingDynamicCheat[playerid] = i;                    // เก็บ index
                strcopy(gPendingDynamicFileName[playerid], filename);
                strcopy(gPendingDynamicMD5[playerid], md5hash);
                return 1;
            }
        }
        return 1;
    }

    // ถ้า spawn แล้ว ? ประมวลผลทันที
    ProcessDynamicCheatDetection(playerid, fileType, filename, md5hash);
    return 1;
}

stock ProcessDynamicCheatDetection(playerid, fileType, const filename[], const md5hash[])
{
    if (!IsPlayerConnected(playerid)) return 0;

    for (new i = 0; i < sizeof(DynamicCheats); i++)
    {
        if (!strcmp(md5hash, DynamicCheats[i][CheatHash], true))
        {
            new str[256];
            new acStatus[128];
            strcopy(acStatus, gPlayerACStatusString[playerid]);

            if (DynamicCheats[i][CheatAction] == 2) // แบน
            {
                format(str, sizeof(str), "{B22222}[{DCDCDC}STP-Vanguard{DCDCDC}{B22222}]{FFFFFF} %s ถูกแบน ข้อหา: ตรวจพบไฟล์อันตราย %s (%s)", 
                    GetPlayerNameEx(playerid), DynamicCheats[i][CheatName], filename);
                SendClientMessageToAll(-1, str);

                // Discord Log
                new ip[16]; GetPlayerIp(playerid, ip, sizeof(ip));
                format(str, sizeof(str), "[STP-Vanguard (Dynamic)] : ผู้เล่น %s ถูกแบน สาเหตุ: %s\n:file_folder: ไฟล์ที่ตรวจพบ: %s\n[:gear:] : (ID: %d)\n[:round_pushpin:] : ไอพีเน็ต : %s\n[:timer:] : ถูกแบนตอนเวลา : %s",
                    GetPlayerNameEx(playerid), DynamicCheats[i][CheatName], filename, playerData[playerid][pID], ip, ReturnDateTime(0));
                DCC_SendChannelMessage(DCC_FindChannelById("1455499307384832074"), str);

                // แบน HWID + บัญชี
                new escReason[128], escName[MAX_PLAYER_NAME * 2], query[1024];
                mysql_escape_string(DynamicCheats[i][CheatName], escReason);
                mysql_escape_string(GetPlayerNameEx(playerid), escName);

                mysql_format(g_SQL, query, sizeof(query),
                    "INSERT INTO `banned_hwids` (`hwid`, `Name`, `reason`, `location`, `Ban`, `banned_by`, `ban_date`) VALUES ('%e', '%e', 'Dynamic: %e', NULL, '1', 'STP-Vanguard', NOW())",
                    gPlayerHWID[playerid], escName, escReason);
                mysql_tquery(g_SQL, query);

                playerData[playerid][pBan] = 1;
                mysql_format(g_SQL, query, sizeof(query), 
                    "UPDATE `players` SET `playerBan` = 1, `playerBanHWID` = 1, `playerBanReason` = 'Dynamic: %e' WHERE `playerID` = %d", 
                    escReason, playerData[playerid][pID]);
                mysql_tquery(g_SQL, query);

                DelayedKick(playerid);
            }
            else if (DynamicCheats[i][CheatAction] == 1) // เตะ
            {
                /*format(str, sizeof(str), "{B22222}[{DCDCDC}STP-Vanguard{DCDCDC}{B22222}]{FFFFFF} %s ถูกเตะออกจากเซิร์ฟเวอร์ ข้อหา: ตรวจพบโปรแกรม %s (%s)", 
                    GetPlayerNameEx(playerid), DynamicCheats[i][CheatName], filename);
                SendClientMessageToAll(-1, str);*/

                //SendClientMessage(playerid, 0xFF0000FF, "โปรเพื่อ");


                /*format(str, sizeof(str), "{B22222}[{DCDCDC}STP-Vanguard{DCDCDC}{B22222}]{FFFFFF} %s ถูกเตะออกจากเซิร์ฟเวอร์ ข้อหา: %s", 
                    GetPlayerNameEx(playerid), DynamicCheats[i][CheatName]);
                SendClientMessageToAll(-1, str);

                // Discord Log (เตะ)
                new ip[16]; GetPlayerIp(playerid, ip, sizeof(ip));
                format(str, sizeof(str), "[STP-Vanguard (Dynamic)] : ผู้เล่น %s ถูกเตะ สาเหตุ: %s\n:file_folder: ไฟล์ที่ตรวจพบ: %s\n[:gear:] : (ID: %d)",
                    GetPlayerNameEx(playerid), DynamicCheats[i][CheatName], filename, playerData[playerid][pID]);
                DCC_SendChannelMessage(DCC_FindChannelById("1455499187759218790"), str);

                DelayedKick(playerid);
            }
            break;
        }
    }
    return 1;
}*/

// --- [ Callbacks จาก Plugin ] ---
public OnPlayerAntiCheatConnect(playerid, bool:is_latest_version)
{
    gPlayerAntiCheatStatus{playerid} = is_latest_version ? 2 : 1;
    return 1;
}

public OnPlayerAntiCheatType(playerid, clientType)
{
    gPlayerACType[playerid] = clientType;

    if(!gPlayerGotACMessage{playerid})
    {
        if(gPlayerAntiCheatStatus{playerid} == 1)
        {
            SendClientMessage(playerid, 0xFF0000FF, "{B22222}[{DCDCDC}STP-Vanguard{DCDCDC}{B22222}]{FFFFFF} {CD5C5C}ไฟล์กันโปรของคุณเป็นเวอร์ชันเก่า{FFFFFF} กรุณาดาวน์โหลดใหม่จาก {4169E1}Discord ");
            strcopy(gPlayerACStatusString[playerid], "เวอร์ชันไฟล์กันโปรเก่า (แจ้งให้อัปเดต)");
        }
        else if(gPlayerAntiCheatStatus{playerid} == 2)
        {
            if(clientType == 1)
            {
                SendClientMessage(playerid, 0x00FF00FF, "{B22222}[{DCDCDC}STP-Vanguard{DCDCDC}{B22222}]{FFFFFF} ระบบป้องกันทำงานสมบูรณ์ ({00FF00}เวอร์ชันล่าสุด{FFFFFF}) ({00FF7F}v.nocleo{FFFFFF})");
                strcopy(gPlayerACStatusString[playerid], "ระบบป้องกันทำงานสมบูรณ์ (เวอร์ชันล่าสุด) (nocleo)");
            }
            else
            {
                SendClientMessage(playerid, 0x00FF00FF, "{B22222}[{DCDCDC}STP-Vanguard{DCDCDC}{B22222}]{FFFFFF} ระบบป้องกันทำงานสมบูรณ์ ({00FF00}เวอร์ชันล่าสุด{FFFFFF})");
                strcopy(gPlayerACStatusString[playerid], "ระบบป้องกันทำงานสมบูรณ์ (เวอร์ชันล่าสุด)");
            }
        }
        gPlayerGotACMessage{playerid} = true;
    }
    return 1;
}



/*
public OnPlayerCheckHwid(playerid, const hwid[])
{
    format(gPlayerHWID[playerid], 65, "%s", hwid);

    new query[128];
    mysql_format(g_SQL, query, sizeof(query), "SELECT * FROM `banned_hwids` WHERE `hwid` = '%e' LIMIT 1", hwid);
    mysql_tquery(g_SQL, query, "CheckBannedHWID", "is", playerid, hwid);

    //printf("[AC] Player %d sent HWID: %s", playerid, hwid); // ปิดรก printf
    return 1;
}*/

/*
public OnPlayerCheckHwid(playerid, const hwid[]) // 11.1/2569
{
    format(gPlayerHWID[playerid], 65, "%s", hwid);

    // ตรวจสอบ HWID แบนก่อน
    new query[128];
    mysql_format(g_SQL, query, sizeof(query), "SELECT * FROM `banned_hwids` WHERE `hwid` = '%e' AND `Ban` = 1 LIMIT 1", hwid); 
    mysql_tquery(g_SQL, query, "CheckBannedHWID", "i", playerid);  // ส่งแค่ playerid เพราะไม่ต้องใช้ hwid ใน callback อีก

    // อัพเดท HWID ของผู้เล่นในตาราง players
    new playerName[MAX_PLAYER_NAME];
    GetPlayerName(playerid, playerName, sizeof(playerName));

    mysql_format(g_SQL, query, sizeof(query), "UPDATE `players` SET `playerHWID` = '%e' WHERE `playerName` = '%e' LIMIT 1", hwid, playerName);
    mysql_tquery(g_SQL, query);                                                         

    return 1;
}*/

public OnPlayerCheckHwid(playerid, const hwid[])
{
    format(gPlayerHWID[playerid], 65, "%s", hwid);
    CheckHWIDBan(playerid);
    // ตรวจสอบแบนก่อน (อันนี้ยังคงทำทันทีได้เลย เพราะเป็นเรื่องความปลอดภัย)
    new query[128];
    mysql_format(g_SQL, query, sizeof(query), "SELECT * FROM `banned_hwids` WHERE `hwid` = '%e' AND `Ban` = 1 LIMIT 1", hwid);
    mysql_tquery(g_SQL, query, "CheckBannedHWID", "i", playerid);

    // อย่าบันทึกตรงนี้แล้ว!!
    return 1;
}


public OnPlayerCheckHwidV2(playerid, const hwidv2[]) // การเก็บค่าใหม่ 
{
    format(gPlayerHWIDV2[playerid], 65, "%s", hwidv2);
    CheckHWIDBanv2(playerid); //  HWIDv2 (กัน bypass มโน)
    // อย่าบันทึกตรงนี้แล้ว!!
    return 1;
}







stock CheckHWIDBan(playerid)
{
    if(strlen(gPlayerHWID[playerid]) < 10) // ยังไม่มี HWID หรือ HWID ไม่สมบูรณ์
        return 0;

    new query[256];
    mysql_format(g_SQL, query, sizeof(query),
        "SELECT `playerName`, `playerHWID`, `playerBanHWID`, `playerBan`, `playerBanReason`, `playerBanDate` \
        FROM `players` \
        WHERE `playerHWID` = '%e' AND `playerBanHWID` = 1 AND `playerBan` = 1 \
        LIMIT 1",
        gPlayerHWID[playerid]);

    mysql_tquery(g_SQL, query, "HWIDBans_Load", "i", playerid);
    return 1;
}
new bool:g_HwidBanMsgSent[MAX_PLAYERS];

forward HWIDBans_Load(playerid);
public HWIDBans_Load(playerid)
{
    if (cache_num_rows() > 0)
    {
        new
            banHWID[65],
            banName[MAX_PLAYER_NAME],
            banReason[128],
            banDate[24],
            string[600];

        cache_get_value_name(0, "playerName", banName, sizeof(banName));
        cache_get_value_name(0, "playerHWID", banHWID, sizeof(banHWID));
        cache_get_value_name(0, "playerBanReason", banReason, sizeof(banReason));
        cache_get_value_name(0, "playerBanDate", banDate, sizeof(banDate));

        format(string, sizeof(string),
            "{FF6600}คุณถูกแบนออกจากเซิฟเวอร์\n\n\
            {FFFFFF}ชื่อผู้ถูกแบน: {FFFF00}%s\n\
            {FFFFFF}HWID ที่ถูกแบน: {FFFF00}%s\n\
            {FFFFFF}สาเหตุ: {FFFF00}%s\n\
            {FFFFFF}วันที่แบน: {FFFF00}%s",
            banName, banHWID, banReason, banDate);

        // ? ส่งข้อความแค่ครั้งเดียว
        if (!g_HwidBanMsgSent[playerid])
        {
            g_HwidBanMsgSent[playerid] = true;

            SendClientMessageEx(playerid, COLOR_WHITE, "");
            SendClientMessageEx(playerid, COLOR_WHITE,
                "{66FFFF}[{FFFFFF}{FF0000}Training Park{FFFFFF}{66FFFF}]\
{FFFFFF}{FFFF00} คอมของคุณถูกแบน โปรดติดต่อแอดมินเพื่อปลดแบน!");
            SendClientMessageEx(playerid, COLOR_WHITE,
                "{66FFFF}[{FFFFFF}{FF0000}Training Park{FFFFFF}{66FFFF}]\
{FFFFFF}{FFFF00} ไม่สามารถเข้าสู่เซิฟเวอร์ได้ เนื่องจากไอดีของคุณถูกแบน");
        }

        Dialog_Show(playerid, DIALOG_BANCHECK, DIALOG_STYLE_MSGBOX,
            "STP.Vanguard-Ban",
            string,
            "ปิด", "");

        DelayedKick(playerid);
        return 1;
    }
    return 1;
}


///

/*
stock CheckHWIDBanv2(playerid)
{
    if(strlen(gPlayerHWIDV2[playerid]) < 10) // ยังไม่มี HWID หรือ HWID ไม่สมบูรณ์
        return 0;

    new query[256];
    mysql_format(g_SQL, query, sizeof(query),
        "SELECT `playerName`, `playerHWIDV2`, `playerBanHWID`, `playerBan`, `playerBanReason`, `playerBanDate` \
        FROM `players` \
        WHERE `playerHWIDV2` = '%e' AND `playerBanHWID` = 1 AND `playerBan` = 1 \
        LIMIT 1",
        gPlayerHWIDV2[playerid]);

    mysql_tquery(g_SQL, query, "HWIDBans_Loadv2", "i", playerid);
    return 1;
}
new bool:g_HwidBanMsgSentv2[MAX_PLAYERS];

forward HWIDBans_Loadv2(playerid);
public HWIDBans_Loadv2(playerid)
{
    if (cache_num_rows() > 0)
    {
        new
            banHWID[65],
            banName[MAX_PLAYER_NAME],
            banReason[128],
            banDate[24],
            string[600];

        cache_get_value_name(0, "playerName", banName, sizeof(banName));
        cache_get_value_name(0, "playerHWID", banHWID, sizeof(banHWID));
        cache_get_value_name(0, "playerBanReason", banReason, sizeof(banReason));
        cache_get_value_name(0, "playerBanDate", banDate, sizeof(banDate));

        format(string, sizeof(string),
            "{FF6600}คุณถูกแบนออกจากเซิฟเวอร์\n\n\
            {FFFFFF}ชื่อผู้ถูกแบน: {FFFF00}%s\n\
            {FFFFFF}HWID ที่ถูกแบน: {FFFF00}%s\n\
            {FFFFFF}สาเหตุ: {FFFF00}%s\n\
            {FFFFFF}วันที่แบน: {FFFF00}%s",
            banName, banHWID, banReason, banDate);

        // ? ส่งข้อความแค่ครั้งเดียว
        if (!g_HwidBanMsgSentv2[playerid])
        {
            g_HwidBanMsgSentv2[playerid] = true;

            SendClientMessageEx(playerid, COLOR_WHITE, "");
            SendClientMessageEx(playerid, COLOR_WHITE,
                "{66FFFF}[{FFFFFF}{FF0000}Training Park{FFFFFF}{66FFFF}]\
{FFFFFF}{FFFF00} คอมของคุณถูกแบน โปรดติดต่อแอดมินเพื่อปลดแบน!");
            SendClientMessageEx(playerid, COLOR_WHITE,
                "{66FFFF}[{FFFFFF}{FF0000}Training Park{FFFFFF}{66FFFF}]\
{FFFFFF}{FFFF00} ไม่สามารถเข้าสู่เซิฟเวอร์ได้ เนื่องจากไอดีของคุณถูกแบน");


            /*SendClientMessageEx(playerid, COLOR_WHITE, "");
            SendClientMessageEx(playerid, COLOR_RED,
                "{B22222}[{DCDCDC}STP-Vanguard{DCDCDC}{B22222}]{FFFFFF} เองคิดจะทำอะไรของเองน้องชาย ของมโนของเองอะเก็บไว้ใช่คนเดียวเถอะ");*/

       /* }


        Dialog_Show(playerid, DIALOG_BANCHECK, DIALOG_STYLE_MSGBOX,
            "STP.Vanguard-Ban",
            string,
            "ปิด", "");

        DelayedKick(playerid);
        return 1;
    }
    return 1;
}*/


stock CheckHWIDBanv2(playerid)
{
    if(strlen(gPlayerHWIDV2[playerid]) < 10)
        return 0;

    new query[256];
    mysql_format(g_SQL, query, sizeof(query),
        "SELECT `playerName`, `playerHWID`, `playerBanHWID`, `playerBan`, `playerBanReason`, `playerBanDate` \
        FROM `players` \
        WHERE `playerHWIDV2` = '%e' AND `playerBanHWID` = 1 AND `playerBan` = 1 \
        LIMIT 1",
        gPlayerHWIDV2[playerid]);

    mysql_tquery(g_SQL, query, "HWIDBans_Loadv2", "i", playerid);
    return 1;
}
new bool:g_HwidBanMsgSentv2[MAX_PLAYERS];

// ====================== ส่วนที่แก้ไขหลัก ======================
forward HWIDBans_Loadv2(playerid);
public HWIDBans_Loadv2(playerid)
{
    if (cache_num_rows() > 0)
    {
        new
            banHWID[65],           // ใช้เก็บ playerHWID ปกติ
            banName[MAX_PLAYER_NAME],
            banReason[128],
            banDate[24],
            string[600];

        cache_get_value_name(0, "playerName", banName, sizeof(banName));
        cache_get_value_name(0, "playerHWID", banHWID, sizeof(banHWID));     // ? เปลี่ยนเป็น playerHWID
        cache_get_value_name(0, "playerBanReason", banReason, sizeof(banReason));
        cache_get_value_name(0, "playerBanDate", banDate, sizeof(banDate));

        format(string, sizeof(string),
            "{FF6600}คุณถูกแบนออกจากเซิฟเวอร์\n\n\
            {FFFFFF}ชื่อผู้ถูกแบน: {FFFF00}%s\n\
            {FFFFFF}HWID ที่ถูกแบน: {FFFF00}%s\n\
            {FFFFFF}สาเหตุ: {FFFF00}%s\n\
            {FFFFFF}วันที่แบน: {FFFF00}%s",
            banName, banHWID, banReason, banDate);

        // ข้อความแจ้งเตือน (ส่งครั้งเดียว)
        if (!g_HwidBanMsgSentv2[playerid])
        {
            g_HwidBanMsgSentv2[playerid] = true;

            SendClientMessageEx(playerid, COLOR_WHITE, "");
            SendClientMessageEx(playerid, COLOR_WHITE,
                "{66FFFF}[{FFFFFF}{FF0000}Training Park{FFFFFF}{66FFFF}]\
{FFFFFF}{FFFF00} คอมของคุณถูกแบน โปรดติดต่อแอดมินเพื่อปลดแบน!");
            SendClientMessageEx(playerid, COLOR_WHITE,
                "{66FFFF}[{FFFFFF}{FF0000}Training Park{FFFFFF}{66FFFF}]\
{FFFFFF}{FFFF00} ไม่สามารถเข้าสู่เซิฟเวอร์ได้ เนื่องจากไอดีของคุณถูกแบน");


        }

        Dialog_Show(playerid, DIALOG_BANCHECK, DIALOG_STYLE_MSGBOX,
            "STP.Vanguard-Ban",
            string,
            "ปิด", "");

        DelayedKick(playerid);
        return 1;
    }
    return 1;
}

//





public OnPlayerDetectedIllegalFile(playerid, cheatid)
{
    if(!gPlayerHasSpawned{playerid})
    {
        gPendingCheatID[playerid] = cheatid;
        return 1;
    }
    ProcessCheatDetection(playerid, cheatid);
    return 1;
}

new gInjectQuery[1536];
new gEscFileName[128];
new gEscModulePath[1024];
new gEscName[64];
new gReasonStr[256];
new gEscReason[512];

// เพิ่มตัวแปร global ด้านบน (นอกฟังก์ชัน) เพื่อป้องกันการซ้ำ
new gLastInjectedDetectTime[MAX_PLAYERS];
new gLastInjectedModuleHash[MAX_PLAYERS];

/*
public OnPlayerDetectedInjectedModule(playerid, const modulePath[])
{
    if(!IsPlayerConnected(playerid)) return 1;

    // ป้องกันตรวจจับไฟล์เดียวกันซ้ำภายใน 10 วินาที
    new hash = 0;
    for(new i = 0, len = strlen(modulePath); i < len; i++)
    {
        hash = (hash * 31 + modulePath[i]) & 0xFFFFFFFF;
    }

    new currentTime = gettime();
    if(gLastInjectedModuleHash[playerid] == hash && gLastInjectedDetectTime[playerid] > currentTime - 10)
    {
        return 1;
    }

    gLastInjectedModuleHash[playerid] = hash;
    gLastInjectedDetectTime[playerid] = currentTime;

    new name[MAX_PLAYER_NAME], str[512];
    GetPlayerName(playerid, name, sizeof(name));

    // ดึงชื่อไฟล์จาก path
    new fileName[128];
    new len = strlen(modulePath);
    new lastSlash = -1;

    for (new i = len - 1; i >= 0; i--)
    {
        if (modulePath[i] == '\\' || modulePath[i] == '/')
        {
            lastSlash = i;
            break;
        }
    }

    if (lastSlash != -1)
    {
        strmid(fileName, modulePath, lastSlash + 1, len, sizeof(fileName));
    }
    else
    {
        strcopy(fileName, modulePath, sizeof(fileName));
    }

    if (strlen(fileName) == 0)
        format(fileName, sizeof(fileName), "Unknown.dll");

    new ip[16];
    GetPlayerIp(playerid, ip, sizeof(ip));

    // ดึงสถานะกันโปรที่เก็บไว้
    new acStatus[128];
    strcopy(acStatus, gPlayerACStatusString[playerid]);

    // ประกาศในเกม
    format(str, sizeof(str), "{B22222}[{DCDCDC}STP-Vanguard{DCDCDC}{B22222}]{FFFFFF} %s ถูกแบน (HWID) ข้อหา: ฉีดไฟล์จากภายนอกเกม (%s)", name, fileName);
    SendClientMessageToAll(-1, str);

    // บันทึก HWID Ban
    if (strlen(gPlayerHWID[playerid]) > 10)
    {
        new query[1024];
        new escName[MAX_PLAYER_NAME * 2];
        new escReason[512];
        new escLocation[1024];

        mysql_escape_string(name, escName);

        format(escReason, sizeof(escReason), "Injected external module: %s", fileName);
        mysql_escape_string(escReason, escReason);

        mysql_escape_string(modulePath, escLocation);

        mysql_format(g_SQL, query, sizeof(query),
            "INSERT INTO `banned_hwids` (`hwid`, `Name`, `reason`, `location`, `Ban`, `banned_by`, `ban_date`) VALUES ('%e', '%e', '%e', '%e', '1', 'STP-Vanguard', NOW())",
            gPlayerHWID[playerid], escName, escReason, escLocation);
        mysql_tquery(g_SQL, query);
    }

    // แบนบัญชี
    new reasonStr[256];
    format(reasonStr, sizeof(reasonStr), "Injected external module: %s", fileName);

    playerData[playerid][pBan] = 1;
    strcopy(playerData[playerid][pBanReason], reasonStr, 256);

    new query2[1024];
    mysql_format(g_SQL, query2, sizeof(query2),
        "UPDATE `players` SET `playerBan` = 1, `playerBanReason` = '%e' WHERE `playerID` = %d",
        reasonStr, playerData[playerid][pID]);
    mysql_tquery(g_SQL, query2);

    // === Log Discord พร้อมแสดง path เต็มของไฟล์ ===
    format(str, sizeof(str), "[STP-Vanguard] : ผู้เล่น %s ถูกแบน (HWID) สาเหตุ: ฉีดไฟล์จากภายนอก\n:file_folder: ไฟล์ที่ตรวจพบ: **%s**\n:link: Path เต็ม: `%s`\n[:gear:] : (ID: %d)\n[:globe_with_meridians:] : อุปกรณ์ที่ใช้เล่น : %s\n[:shield:] : กันโปรที่ลง : %s\n[:round_pushpin:] : ไอพีเน็ต : %s\n[:timer:] : ถูกแบนตอนเวลา : %s",
        name, fileName, modulePath, playerData[playerid][pID], IsPlayerAndroidv2(playerid) ? "โทรศัพท์มือถือ" : "คอมพิวเตอร์", acStatus, ip, ReturnDateTime(0));

    DCC_SendChannelMessage(DCC_FindChannelById("1455499435885990023"), str);

    DelayedKick(playerid);
    return 1;
}*/




// ============================================================================
// ประกาศตัวแปร (ใส่ในส่วน global หรือ enum ของผู้เล่น)
// ============================================================================

#define MAX_INJECTED_FILES_PER_PLAYER   (32)

new gInjectedFiles[MAX_PLAYERS][MAX_INJECTED_FILES_PER_PLAYER][32];
new gInjectedCount[MAX_PLAYERS];

// ============================================================================
// Whitelist DLL ที่ไม่ต้องแจ้ง (ระบบ / โปรแกรมถูกกฎหมาย)
// ============================================================================

static const WHITELIST_INJECTED[][32] = {
    "d3d9.dll",
    "d3d10.dll",
    "d3d11.dll",
    "dxgi.dll",
    "dxgi10.dll",
    "dxgi11.dll",
    "dinput8.dll",
    "xinput1_3.dll",
    "xinput1_4.dll",
    "xinput9_1_0.dll",
    "version.dll",
    "winmm.dll",
    "kernel32.dll",
    "user32.dll",
    "advapi32.dll",
    "msvcrt.dll",
    "ntdll.dll",
    "sechost.dll",
    "rpcrt4.dll",
    "cryptbase.dll",
    "bcrypt.dll",
    "OWExplorer.dll",           // Overwolf overlay / streaming
    "aswhook.dll",              // Avast Antivirus hook
    "aswhooka.dll",
    "nvngx.dll",                // NVIDIA
    "nvapi64.dll",
    "nvapi.dll",
    "EAX.DLL",                  // Fix GTA San Andreas
    "eax.dll",                  // Fix GTA San Andreas
    "bink2w64.dll",             // Bink Video (เกมหลายเกม)
    "bink2w32.dll",
    "steam_api.dll",
    "steam_api64.dll",
    "gameoverlayrenderer.dll",
    "gameoverlayrenderer64.dll",
    "discord_game_sdk.dll",
    "mc-sec-plugin-x86.dll",
    "aswAMSI.dll",
    "AMSIExt_x86.dll",
    "idmmkb.dll",
    "action_x86.dll",
    "std.asi.dll", // modloader\.data\plugins\gta3
    "std.bank.dll", // modloader\.data\plugins\gta3
    "std.data.dll", // modloader\.data\plugins\gta3
    "std.fx.dll", // modloader\.data\plugins\gta3
    "std.movies.dll", // modloader\.data\plugins\gta3
    "std.scm.dll", // modloader\.data\plugins\gta3
    "std.sprites.dll", // modloader\.data\plugins\gta3
    "std.stream.dll", // modloader\.data\plugins\gta3
    "std.tracks.dll", // modloader\.data\plugins\gta3
    "discord-rpc.dll"
};

// ============================================================================
// Reset เมื่อผู้เล่นออกเกม
// ============================================================================
hook OnPlayerDisconnect(playerid, reason)
{
    gInjectedCount[playerid] = 0;
    
    for(new i = 0; i < MAX_INJECTED_FILES_PER_PLAYER; i++)
    {
        gInjectedFiles[playerid][i][0] = EOS;
    }
    
    return 1;
}

// ============================================================================
// ฟังก์ชันช่วย
// ============================================================================
stock bool:HasAlreadyReportedInjectedFile(playerid, const fileName[])
{
    for(new i = 0; i < gInjectedCount[playerid]; i++)
    {
        if(!strcmp(gInjectedFiles[playerid][i], fileName, true))
        {
            return true;
        }
    }
    return false;
}

stock AddReportedInjectedFile(playerid, const fileName[])
{
    if(gInjectedCount[playerid] >= MAX_INJECTED_FILES_PER_PLAYER) return;

    format(gInjectedFiles[playerid][gInjectedCount[playerid]], 32, "%s", fileName);
    gInjectedCount[playerid]++;
}

stock bool:IsSuspectASI(const fileName[])
{
    new len = strlen(fileName);
    if(len < 4) return false;

    new temp[32];
    strcopy(temp, fileName, sizeof(temp));

    for(new i = 0; i < len; i++)
    {
        if(temp[i] >= 'A' && temp[i] <= 'Z')
            temp[i] += 32;
    }

    return (
        temp[len-4] == '.' &&
        temp[len-3] == 'a' &&
        temp[len-2] == 's' &&
        temp[len-1] == 'i'
    );
}

stock bool:IsWhitelistedInjectedFile(const fileName[])
{
    new temp[32];
    strcopy(temp, fileName, sizeof(temp));

    // lowercase เพื่อเทียบแบบไม่สน case
    for(new i = 0; temp[i] != EOS; i++)
    {
        if(temp[i] >= 'A' && temp[i] <= 'Z')
            temp[i] += 32;
    }

    for(new i = 0; i < sizeof(WHITELIST_INJECTED); i++)
    {
        if(!strcmp(temp, WHITELIST_INJECTED[i], true))
        {
            return true;
        }
    }
    return false;
}






// ============================================================================
// Callback หลัก
// ============================================================================
public OnPlayerDetectedInjectedModule(playerid, const modulePath[])
{
    if(!IsPlayerConnected(playerid)) return 1;

    // ????????????????????????????????????????????????
    // ดึงเฉพาะชื่อไฟล์ (ไม่รวม path)
    // ????????????????????????????????????????????????
    new fileName[128];
    new len = strlen(modulePath);
    new lastSlash = -1;

    for(new i = len - 1; i >= 0; i--)
    {
        if(modulePath[i] == '\\' || modulePath[i] == '/')
        {
            lastSlash = i;
            break;
        }
    }

    if(lastSlash != -1)
    {
        strmid(fileName, modulePath, lastSlash + 1, len, sizeof(fileName));
    }
    else
    {
        strcopy(fileName, modulePath, sizeof(fileName));
    }

    if(strlen(fileName) == 0)
        format(fileName, sizeof(fileName), "Unknown.dll");

    // ????????????????????????????????????????????????
    // 1. กรอง whitelist ก่อน (ระบบ / Overwolf / Avast ฯลฯ)
    // ????????????????????????????????????????????????
    if(IsWhitelistedInjectedFile(fileName))
    {
        // printf("[Vanguard] Ignored whitelisted: %s (player %d)", fileName, playerid); // debug ถ้าต้องการ
        return 1;
    }

    // ????????????????????????????????????????????????
    // 2. ถ้าเคยแจ้งไฟล์นี้แล้ว ? ข้าม
    // ????????????????????????????????????????????????
    if(HasAlreadyReportedInjectedFile(playerid, fileName))
    {
        return 1;
    }

    // ????????????????????????????????????????????????
    // ไฟล์ที่น่าสงสัยจริง ๆ ? ส่งแจ้งเตือน
    // ????????????????????????????????????????????????
    new name[MAX_PLAYER_NAME];
    GetPlayerName(playerid, name, sizeof(name));

    new ip[16];
    GetPlayerIp(playerid, ip, sizeof(ip));

    new acStatus[128];
    strcopy(acStatus, gPlayerACStatusString[playerid]); // สมมติว่ามีตัวแปรนี้อยู่แล้ว

    new str[700];
    
    format(str, sizeof(str),
        "[STP-Vanguard] : ผู้เล่น %s ถูกสงสัย สาเหตุ: ฉีดไฟล์จากภายนอก%s\n" \
        ":file_folder: ไฟล์ที่ตรวจพบ: **%s**\n" \
        ":link: Path เต็ม: `%s`\n" \
        "[:gear:] ID: %d\n" \
        "[:globe_with_meridians:] อุปกรณ์ที่ใช้เล่น: %s\n" \
        "[:shield:] กันโปรที่ลง: %s\n" \
        "[:round_pushpin:] ไอพีเน็ต: %s\n" \
        "[:timer:] ถูกตรวจพบตอนเวลา: %s",
        name,
        IsSuspectASI(fileName) ? " **(สงสัยเป็นพิเศษ)**" : "",
        fileName,
        modulePath,
        playerData[playerid][pID],                          // สมมติว่ามี playerData อยู่
        IsPlayerAndroidv2(playerid) ? "โทรศัพท์มือถือ" : "คอมพิวเตอร์",
        acStatus,
        ip,
        ReturnDateTime(0)                                   // สมมติว่ามีฟังก์ชันนี้
    );

    DCC_SendChannelMessage(DCC_FindChannelById("1464731488317866187"), str); // log สงสัย-โปรฉีดinjector

    // บันทึกว่าส่งแจ้งไฟล์นี้ไปแล้ว
    AddReportedInjectedFile(playerid, fileName);

    return 1;
}


/*
new bool:g_GameFolderReported[MAX_PLAYERS char];
public OnPlayerGameFolderReported(playerid, const folderPath[], const summary[])
{
    if (g_GameFolderReported{playerid}) return 1;
    g_GameFolderReported{playerid} = true;

    if (!IsPlayerConnected(playerid)) return 1;

    new name[MAX_PLAYER_NAME];
    GetPlayerName(playerid, name, sizeof(name));

    new DCC_Channel:channel = DCC_FindChannelById("1457034612915179776"); // เปลี่ยนเป็น channel ของคุณ

    // ส่วนหัว (เหมือนภาพเลย)
    new header[600];
    format(header, sizeof(header),
        "**[Game Folder Report]** ผู้เล่น: **%s**\n**Path:** `%s`",
        name, folderPath);

    DCC_SendChannelMessage(channel, header);

    // รายการไฟล์+โฟลเดอร์ทั้งหมดใน code block (สวยเหมือนภาพ)
    if (strlen(summary) > 0)
    {
        new msg[2048];

        format(msg, sizeof(msg),
            "รายการไฟล์/โฟลเดอร์ในโฟลเดอร์เกม:\n```\n%s\n```",
            summary);

        // ป้องกันข้อความเกิน Discord limit
        if (strlen(msg) > 1950)
        {
            msg[1940] = '\0';                     // ตัดให้พอดี
            strcat(msg, "\n... (ถูกตัดเนื่องจากยาวเกิน)");
        }

        DCC_SendChannelMessage(channel, msg);
    }
    else
    {
        DCC_SendChannelMessage(channel, "*(ไม่มีไฟล์หรือโฟลเดอร์ในโฟลเดอร์เกม)*");
    }

    return 1;
}*/


// เพิ่มตัวแปร global เหมือนเดิม (ถ้ายังไม่มี)
new gLastNewInjectedDetectTime[MAX_PLAYERS];
new gLastNewInjectedModuleHash[MAX_PLAYERS];

// Callback ใหม่สำหรับ runtime injected
/*
public OnPlayerDetectedNewInjectedModule(playerid, const modulePath[])
{
    if(!IsPlayerConnected(playerid)) return 1;

    // ป้องกัน flood ตรวจจับไฟล์เดียวกันซ้ำ
    new hash = 0; 
    for(new i = 0, len = strlen(modulePath); i < len; i++)
    {
        hash = (hash * 31 + modulePath[i]) & 0xFFFFFFFF;
    }

    new currentTime = gettime();
    if(gLastNewInjectedModuleHash[playerid] == hash && 
       gLastNewInjectedDetectTime[playerid] > currentTime - 12)
    {
        return 1;
    }

    gLastNewInjectedModuleHash[playerid] = hash;
    gLastNewInjectedDetectTime[playerid] = currentTime;

    new name[MAX_PLAYER_NAME], fileName[128], str[512], ip[16];
    GetPlayerName(playerid, name, sizeof(name));
    GetPlayerIp(playerid, ip, sizeof(ip));

    // แยกชื่อไฟล์จาก path
    new lastSlash = -1;
    for(new i = strlen(modulePath)-1; i >= 0; i--)
    {
        if(modulePath[i] == '\\' || modulePath[i] == '/')
        {
            lastSlash = i;
            break;
        }
    }

    if(lastSlash != -1)
        strmid(fileName, modulePath, lastSlash+1, strlen(modulePath), sizeof(fileName));
    else
        strcopy(fileName, modulePath, sizeof(fileName));

    if(strlen(fileName) == 0)
        format(fileName, sizeof(fileName), "Unknown_Injected.dll");

    // สร้างข้อความแจ้งเตือน
    format(str, sizeof(str), 
        "{B22222}[{DCDCDC}STP-Vanguard{DCDCDC}{B22222}]{FFFFFF} %s ถูกแบน (HWID) - ตรวจพบการฉีดโมดูล runtime: %s",
        name, fileName);
    SendClientMessageToAll(-1, str);

    new reason[128];
    format(reason, sizeof(reason), "Runtime Injected Module: %s (Path: %s)", fileName, modulePath);

    // สถานะกันโปร
    new acStatus[128];
    strcopy(acStatus, gPlayerACStatusString[playerid], sizeof(acStatus));

    // บันทึกแบน HWID (เหมือนระบบเดิม)
    if(strlen(gPlayerHWID[playerid]) > 20)
    {
        new query[1024], escName[MAX_PLAYER_NAME*2], escReason[256];

        //mysql_escape_string(name, escName, sizeof(escName), g_SQL);
        //mysql_escape_string(reason, escReason, sizeof(escReason), g_SQL);

        //mysql_format(g_SQL, query, sizeof(query),
        //    "INSERT INTO `banned_hwids` (`hwid`, `Name`, `reason`, `location`, `Ban`, `banned_by`, `ban_date`) \
        //    VALUES ('%e', '%e', '%e', '%e', '1', 'STP-Vanguard', NOW())",
        //    gPlayerHWID[playerid], escName, escReason, modulePath);
       // mysql_tquery(g_SQL, query);

        // แบนในตาราง players (ถ้ามี)
       // mysql_format(g_SQL, query, sizeof(query),
       //     "UPDATE `players` SET `playerBan` = 1, `playerBanReason` = '%e', `playerBanIP` = '%e', `playerBanDate` = NOW() \
        //    WHERE `playerName` = '%e'",
        //    reason, ip, name);
       // mysql_tquery(g_SQL, query);
    }

    // Log Discord (ปรับ channel ตามของคุณ)
    format(str, sizeof(str),
        "[STP-Vanguard Runtime Inject] ผู้เล่น %s ถูกแบน\nไฟล์: **%s**\nPath: `%s`\nIP: %s\nกันโปร: %s\nเวลา: %s",
        name, fileName, modulePath, ip, acStatus, ReturnDateTime(0));
    DCC_SendChannelMessage(DCC_FindChannelById("1455499307384832074"), str);

    //DelayedKick(playerid);

    return 1;
}*/

// ประกาศตัวแปร global หรือ player variable เพื่อป้องกันการส่งซ้ำ
new bool:g_HWIDBanShown[MAX_PLAYERS];
/* ปิดใช้งานชั่วคราว
forward CheckBannedHWID(playerid);
public CheckBannedHWID(playerid)
{
    // ถ้าเคยแสดงแล้ว ให้ข้ามไปเลย (ป้องกันการเรียกซ้ำ)
    if(g_HWIDBanShown[playerid]) 
        return 1;

    if(cache_num_rows() > 0)  // พบ HWID ที่ถูกแบน
    {
        // ตั้ง flag ว่าส่งแล้ว เพื่อไม่ให้ส่งซ้ำอีก
        g_HWIDBanShown[playerid] = true;

        new reason[64], banned_by[64], player_name[MAX_PLAYER_NAME], ban_date[32], str[256];

        cache_get_value_name(0, "reason", reason, sizeof(reason));
        cache_get_value_name(0, "banned_by", banned_by, sizeof(banned_by));
        cache_get_value_name(0, "Name", player_name, sizeof(player_name));
        cache_get_value_name(0, "ban_date", ban_date, sizeof(ban_date));

        // แสดงข้อความในแชท (ส่งแค่ครั้งเดียว)
        format(str, sizeof(str), "{B22222}[{DCDCDC}STP-Vanguard{DCDCDC}{B22222}]{FFFFFF} เครื่องนี้ถูกแบน (HWID)");
        SendClientMessage(playerid, -1, str);

        format(str, sizeof(str), "{FFFFFF}แบนโดย: {FF6347}%s {FFFFFF}| เหตุผล: {FF6347}%s {FFFFFF}| ชื่อที่ถูกแบน: {FF6347}%s", 
            banned_by, reason, player_name);
        SendClientMessage(playerid, -1, str);

        // เตรียมข้อความสำหรับ Dialog
        format(str, sizeof(str), 
            "{FF6600}เครื่องของคุณถูกแบน (HWID Ban)\n\n\
            {FFFFFF}ชื่อบัญชีที่ถูกแบน: {FF6347}%s\n\
            {FFFFFF}แบนโดย: {FF6347}%s\n\
            {FFFFFF}สาเหตุ: {FF6347}%s\n\n\
            {FFFFFF}วันที่แบน: {FF6347}%s",
            player_name, banned_by, reason, ban_date);

        // แสดง Dialog (แค่ครั้งเดียว)
        Dialog_Show(playerid, DIALOG_BANCHECK, DIALOG_STYLE_MSGBOX, "\t{FFFFFF}Training Park", str, "ปิด", "");

        DelayedKick(playerid);
    }
    // ถ้าไม่แบน ก็ไม่ต้องทำอะไรเพิ่ม (หรืออาจ reset flag ถ้าต้องการ)
    return 1;
}*/



// เพิ่มตัวแปร global เหล่านี้ด้านบน (นอกฟังก์ชัน) เพื่อป้องกันการตรวจจับ cheatid ซ้ำ
new gLastCheatDetectTime[MAX_PLAYERS];
new gLastCheatID[MAX_PLAYERS] = {-1, ...}; // ตั้งค่าเริ่มต้นเป็น -1 ทุกคน

stock ProcessCheatDetection(playerid, cheatid)
{
    if(!IsPlayerConnected(playerid)) return 0;
    new currentTime = gettime();
    if(gLastCheatID[playerid] == cheatid && gLastCheatDetectTime[playerid] > currentTime - 10)
    {
        return 1;
    }
    gLastCheatID[playerid] = cheatid;
    gLastCheatDetectTime[playerid] = currentTime;

    new name[MAX_PLAYER_NAME], str[512], reason[128];
    GetPlayerName(playerid, name, sizeof(name));

    switch(cheatid)
    {
        case 1: reason = "Emperor.asi";
        case 2: reason = "SprintHookByDarkP1xel.ASI";
        case 3: reason = "InfinityRun_SA.asi";
        case 4: reason = "anti_stun insert+X.cs (AntishotPoint.cs)";
        case 5: reason = "MoonLoader.asi";
        case 6: reason = "lua51.dll (MoonLoader)";
        case 7: reason = "IREPLAYv3.2.asi (โปรเมนู)";
        case 8: reason = "StealthRemastered.asi (โปรเมนู)";
        case 9: reason = "clockweatherset.asi";
        case 10: reason = "HeadShotAimBot.cs (Aimbot Alt+1)";
        case 11: reason = "iReplay v.5 by SampHack.asi (โปรเมนู)";
        case 12: reason = "tr.cs (/tr โปรแกล้งคนตัวเด้ง)";
        case 13: reason = "Radarzoom.cs (hack minimap)";
        case 14: reason = "2in1.cs";
        case 15: reason = "asfist_1.cs";
        case 16: reason = "fightSpeed.cs";
        case 17: reason = "OPSpeedHack.cs";
        case 18: reason = "Weaponhack.cs";
        case 19: reason = "5Ctroll.cs";
        case 20: reason = "Bigtroll.cs";
        case 21: reason = "cboom.cs";
        case 22: reason = "freeze.cs";
        case 23: reason = "lagpanic.cs";
        case 24: reason = "LagTroll.cs";
        case 25: reason = "slap O+U.cs";
        case 26: reason = "takecar.cs";
        case 27: reason = "tbkick.cs";
        case 28: reason = "SbivNaR.cs (R รีตัว)";
        case 29: reason = "FastPedSyal.cs";
        case 30: reason = "SPREAD.cs";
        case 31: reason = "bladeSprint.asi";
        case 32: reason = "arcane.asi (โปรเมนู)";
        case 33: reason = "SKINCHANGER.cs (FakeSkin)";
        case 34: reason = "CrashRpt1402.dll";
        case 35: reason = "FakeMobile_For_GTA_VIET_NAM_GVN.sf (Wanabe Moblie)";
        case 36: reason = "UltraFuck 2.36.luac";
        case 37: reason = "sprintbind.cs";
        case 38: reason = "Infinite Run.cs";
        case 39: reason = "ddmg.sf (ดาเมจ x2)";
        case 40: reason = "CLEO.asi (Forbidden Mod)";
        case 41: reason = "SAMPFUNCS.asi (Forbidden Mod)";
        case 42: reason = "fast-enter-exit.cs";
        case 43: reason = "2in1.asi (R รีตัว)";
        case 44: reason = "2in1_sbiv_na_x_1.cs (R รีตัว)";
        case 45: reason = "STP.VannguardNightmare.asi (ฝันร้าย Bypass)";
        case 46: reason = "ultrawh.asi";
        case 47: reason = "cacmods.asi";
        case 48: reason = "leet.dll";
        case 49: reason = "crack.asi";
        case 50: reason = "Anti-Stunshot(num4-5).cs"; // ban
        case 51: reason = "antistun chanced.asi"; // ban
        case 52: reason = "Godmode-OPS-ON-OFF.cs"; // ban
        case 53: reason = "Godmode202.cs"; // ban
        case 54: reason = "HAKU.cs"; // ban              
        case 55: reason = "HUEVIY AIM 0.3.7 by p1cador.cs"; // ban
        case 56: reason = "LEGION.asi"; // ban 
        case 57: reason = "MHACK.sf"; //ban
        case 58: reason = "Motherfuckerz.cs"; // ban
        case 59: reason = "NT037.cs"; // ban
        case 60: reason = "SAIM.asi"; // ban   
        case 61: reason = "SAMP 0.3.7 MultiHack.cs"; // ban
        case 62: reason = "WH ALT+F2.cs"; //ban
        case 63: reason = "LEGION v2.asi"; //ban
        case 64: reason = "MixSets.asi"; //kick
        case 65: reason = "Udrakoloader.asi"; //ban
        case 101: reason = "Aimbot.exe"; // ส่วน exe พวกทำงานนอกไฟล์เกม แบบส่งผลกับเกม 
        case 102: reason = "Emperor Cheat AB.exe"; // ส่วน exe พวกทำงานนอกไฟล์เกม แบบส่งผลกับเกม 
        case 103: reason = "Emperor Cheat AB.exe";  // ส่วน exe พวกทำงานนอกไฟล์เกม แบบส่งผลกับเกม 
        case 104: reason = "Extreme Injector v3.exe";  // ส่วน exe พวกทำงานนอกไฟล์เกม แบบส่งผลกับเกม 
        case 105: reason = "loader.exe (dump.dll)";  // ส่วน exe พวกทำงานนอกไฟล์เกม แบบส่งผลกับเกม 
        case 106: reason = "วิ่งไม่เหนื่อย (Infinite Run)";  // ส่วน exe พวกทำงานนอกไฟล์เกม แบบส่งผลกับเกม 
        case 107: reason = "Aimbot.exe"; // ส่วน exe พวกทำงานนอกไฟล์เกม แบบส่งผลกับเกม 
        case 108: reason = "Emperor Injector.exe"; // ส่วน exe พวกทำงานนอกไฟล์เกม แบบส่งผลกับเกม 
        case 109: reason = "Aim po pingu.exe"; // ส่วน exe พวกทำงานนอกไฟล์เกม แบบส่งผลกับเกม 
        case 110: reason = "NT037.exe"; // ส่วน exe พวกทำงานนอกไฟล์เกม แบบส่งผลกับเกม 
        case 111: reason = "AutoHotkey"; // ส่วน exe พวกทำงานนอกไฟล์เกม แบบส่งผลกับเกม 
        case 112: reason = "Megajump"; // โปร menu
        case 113: reason = "Saiyasart.exe (ของมโน Samp)"; // ส่วน exe พวกทำงานนอกไฟล์เกม แบบส่งผลกับเกม 
        case 114: reason = "AIMBOT by Borg_Grown.exe (ของมโน Samp)"; // ส่วน exe พวกทำงานนอกไฟล์เกม แบบส่งผลกับเกม 
        case 115: reason = "PyAIM.exe"; // ส่วน exe พวกทำงานนอกไฟล์เกม แบบส่งผลกับเกม 
        case 116: reason = "cstlnsoft.exe"; // ส่วน exe พวกทำงานนอกไฟล์เกม แบบส่งผลกับเกม 
        case 117: reason = "ProcessHacker"; // ส่วน exe พวกทำงานนอกไฟล์เกม แบบส่งผลกับเกม 
        default: format(reason, sizeof(reason), "Illegal File ID: %d", cheatid);
    }

    new bool:isHWIDBan = (
        cheatid == 7 || cheatid == 8 || cheatid == 11 || cheatid == 32 ||
        cheatid == 10 || cheatid == 16 || cheatid == 4 || cheatid == 17 ||
        cheatid == 36 || cheatid == 12 || cheatid == 28 || cheatid == 37 ||
        cheatid == 14 || cheatid == 42 || cheatid == 43 || cheatid == 44 || 
        cheatid == 45 || cheatid == 39 || cheatid == 46 || cheatid == 101 || 
        (cheatid >= 50 && cheatid <= 63) ||
        cheatid == 102 || cheatid == 103 || cheatid == 105 || cheatid == 107 || cheatid == 108 || 
        cheatid == 109 || cheatid == 110 || cheatid == 112 || cheatid == 113 || cheatid == 114 || cheatid == 115 || cheatid == 116 || cheatid == 65
    );

    new escReason[256 * 2];
    mysql_escape_string(reason, escReason);

    new ip[16], ban_date[24];
    GetPlayerIp(playerid, ip, sizeof(ip));
    format(ban_date, sizeof(ban_date), "%s", ReturnDate());

    // ใช้ตัวแปรที่เก็บสถานะกันโปรไว้แล้ว
    new acStatus[128];
    strcopy(acStatus, gPlayerACStatusString[playerid]);

    if(isHWIDBan && strlen(gPlayerHWID[playerid]) > 10)
    {
        new escName[MAX_PLAYER_NAME * 2];
        mysql_escape_string(name, escName);

        new query[1024];
        mysql_format(g_SQL, query, sizeof(query),
            "INSERT INTO `banned_hwids` (`hwid`, `Name`, `reason`, `location`, `Ban`, `banned_by`, `ban_date`) VALUES ('%e', '%e', '%e', NULL, '1', 'STP-Vanguard', NOW())",
            gPlayerHWID[playerid], escName, escReason);
        mysql_tquery(g_SQL, query);

        playerData[playerid][pBan] = 1;
        
        mysql_format(g_SQL, query, sizeof(query), 
        "UPDATE `players` SET `playerBan` = 1, `playerBanHWID` = 1, `playerBanReason` = '%e', `playerBanIP` = '%e', `playerBanDate` = '%e' WHERE `playerID` = %d",
        escReason,
        ip,
        ban_date,
        playerData[playerid][pID]);
        mysql_tquery(g_SQL, query);

        format(str, sizeof(str), "{B22222}[{DCDCDC}STP-Vanguard{DCDCDC}{B22222}]{FFFFFF} %s ถูกแบน ข้อหา: %s", name, reason);
        SendClientMessageToAll(-1, str);

        format(str, sizeof(str), "[STP-Vanguard] : ผู้เล่น %s ถูกแบน สาเหตุ: %s\n[:gear:] : (ID: %d)\n[:globe_with_meridians:] : อุปกรณ์ที่ใช้เล่น : %s\n[:shield:] : กันโปรที่ลง : %s\n[:round_pushpin:] : ไอพีเน็ต : %s\n[:timer:] : ถูกแบนตอนเวลา : %s",
            name, reason, playerData[playerid][pID], IsPlayerAndroidv2(playerid) ? "โทรศัพท์มือถือ" : "คอมพิวเตอร์", acStatus, ip, ReturnDateTime(0));
        DCC_SendChannelMessage(DCC_FindChannelById("1455499307384832074"), str);
    }
    else
    {
        format(str, sizeof(str), "{B22222}[{DCDCDC}STP-Vanguard{DCDCDC}{B22222}]{FFFFFF} %s ถูกเตะออกจากเซิร์ฟเวอร์ ข้อหา: %s", name, reason);
        SendClientMessageToAll(-1, str);

        format(str, sizeof(str), "[STP-Vanguard] : ผู้เล่น %s ถูกเตะออกจากเซิร์ฟเวอร์ สาเหตุ: %s\n[:gear:] : (ID: %d)\n[:globe_with_meridians:] : อุปกรณ์ที่ใช้เล่น : %s\n[:shield:] : กันโปรที่ลง : %s\n[:round_pushpin:] : ไอพีเน็ต : %s\n[:timer:] : ถูกเตะตอนเวลา : %s",
            name, reason, playerData[playerid][pID], IsPlayerAndroidv2(playerid) ? "โทรศัพท์มือถือ" : "คอมพิวเตอร์", acStatus, ip, ReturnDateTime(0));
        DCC_SendChannelMessage(DCC_FindChannelById("1455499187759218790"), str);
    }

    DelayedKick(playerid);
    return 1;
}




hook OnPlayerConnect(playerid)
{
    g_HWIDBanShown[playerid] = false; // รีเซ็ตทุกครั้งที่เชื่อมต่อ แบน
    gPlayerAntiCheatStatus{playerid} = 0;
    gPlayerGotACMessage{playerid} = false;
    gPlayerHasSpawned{playerid} = false;
    gPendingCheatID[playerid] = 0;
    gPlayerHWID[playerid][0] = EOS;
    gPlayerHWIDV2[playerid][0] = EOS;
    gPlayerACType[playerid] = -1;
    gPlayerHWIDSaved[playerid] = false; // รีเซ็ตทุกครั้งที่เข้าใหม่
    gPlayerHWIDSavedV2[playerid] = false; // รีเซ็ตทุกครั้งที่เข้าใหม่

    gPendingDynamicCheat[playerid] = -1;           // สำคัญ!
    gPendingDynamicFileName[playerid][0] = EOS;
    gPendingDynamicMD5[playerid][0] = EOS;

    //g_GameFolderReported{playerid} = false; (เลิกใช้ 2.3.2569)
    return 1;
}

/*
hook OnPlayerSpawn(playerid) // 11/1/2569 ก่อนย้าย hwid มาเซฟตอน spawn
{
    gPlayerHasSpawned{playerid} = true;

    // เช็คเลเวลแอดมิน ถ้าเท่ากับหรือมากกว่า 19 ให้ข้ามการตรวจสอบกันโปรทั้งหมด
    if(playerData[playerid][pAdmin] < 19)
    {
        if(!IsPlayerAndroidv2(playerid))
        {
            if(gPlayerAntiCheatStatus{playerid} == 0)
            {
                //SendClientMessage(playerid, 0xFF0000FF, "{B22222}[{DCDCDC}STP-Vanguard{DCDCDC}{B22222}]{FFFFFF} {CD5C5C}คุณไม่ได้ติดตั้งระบบป้องกันการโกง{FFFFFF} กรุณาดาวน์โหลดจาก {4169E1}Discord{FFFFFF} ก่อนเข้าเล่น!");
                SendClientMessage(playerid, 0xFF0000FF, "{B22222}[{DCDCDC}STP-Vanguard{DCDCDC}{B22222}]{FFFFFF} {CD5C5C}คุณไม่ได้ติดตั้งระบบป้องกันการโกง (ตัวกันโปร)");
                SendClientMessage(playerid, 0xFF0000FF, "{FFFFFF}ดาวน์โหลดจาก >{4169E1}Discord: discord.gg/fyyMhXJS9A{FFFFFF} ก่อนเข้าเล่น!");
                DelayedKick(playerid);
                return 1;
            }
            else if(gPlayerAntiCheatStatus{playerid} == 1)
            {
                SendClientMessage(playerid, 0xFF0000FF, "{B22222}[{DCDCDC}STP-Vanguard{DCDCDC}{B22222}]{FFFFFF} {CD5C5C}ไฟล์กันโปรของคุณเป็นเวอร์ชันเก่า {FFFFFF}(อัพเดทล่าสุด > 2.1.2569)");
                SendClientMessage(playerid, 0xFF0000FF, "{FFFFFF}กรุณาดาวน์โหลดใหม่จาก {4169E1}Discord!");
                DelayedKick(playerid);
                return 1;
            }
        }
        else
        {
            if(gPlayerAntiCheatStatus{playerid} == 0 && !gPlayerGotACMessage{playerid})
            {
                //SendClientMessage(playerid, 0xFFFF00FF, "[STP-Vanguard] คุณเล่นผ่านมือถือ จึงไม่จำเป็นต้องติดตั้งระบบป้องกัน");
                gPlayerGotACMessage{playerid} = true;
            }
        }
    }
    // ถ้าเป็นแอดมินเลเวล 19+ จะไม่เข้าเงื่อนไขด้านบนเลย ? ข้ามการตรวจกันโปรทั้งหมด

    if(gPendingCheatID[playerid] != 0)
    {
        ProcessCheatDetection(playerid, gPendingCheatID[playerid]);
        gPendingCheatID[playerid] = 0;
    }

    return 1;
}*/


// ย้ายการบันทึกมาทำตอน spawn + ตรวจสอบว่ายังไม่เคย save
hook OnPlayerSpawn(playerid)
{
    gPlayerHasSpawned{playerid} = true;

    // =====================================
    //       ส่วนบันทึก HWID ลงฐานข้อมูล
    // =====================================
    if(!gPlayerHWIDSaved[playerid] && strlen(gPlayerHWID[playerid]) > 10) // มีค่า HWID จริง ๆ
    {
        new query[256], playerName[MAX_PLAYER_NAME];
        GetPlayerName(playerid, playerName, sizeof(playerName));

        mysql_format(g_SQL, query, sizeof(query), 
            "UPDATE `players` SET `playerHWID` = '%e' WHERE `playerName` = '%e' LIMIT 1", 
            gPlayerHWID[playerid], playerName);

        mysql_tquery(g_SQL, query, "", ""); // ไม่ต้อง callback ก็ได้

        gPlayerHWIDSaved[playerid] = true;
        printf("[HWID] Saved HWID for %s (ID:%d) -> %s", playerName, playerid, gPlayerHWID[playerid]);
    }


        // =====================================
    //       ส่วนบันทึก HWID ลงฐานข้อมูล V2
    // =====================================
    if(!gPlayerHWIDSavedV2[playerid] && strlen(gPlayerHWIDV2[playerid]) > 10) // มีค่า HWID จริง ๆ
    {
        new query[256], playerName[MAX_PLAYER_NAME];
        GetPlayerName(playerid, playerName, sizeof(playerName));

        mysql_format(g_SQL, query, sizeof(query), 
            "UPDATE `players` SET `playerHWIDV2` = '%e' WHERE `playerName` = '%e' LIMIT 1", 
            gPlayerHWIDV2[playerid], playerName);

        mysql_tquery(g_SQL, query, "", ""); // ไม่ต้อง callback ก็ได้

        gPlayerHWIDSavedV2[playerid] = true;
        printf("[HWID] Saved HWIDV2 for %s (ID:%d) -> %s", playerName, playerid, gPlayerHWIDV2[playerid]);
    }

        // === ประมวลผล Dynamic Cheat ที่ค้างไว้ ===
    if (gPendingDynamicCheat[playerid] != -1)  // ต้องกำหนดค่าเริ่มต้นเป็น -1 ใน OnPlayerConnect
    {
        ProcessDynamicCheatDetection(playerid, 0, gPendingDynamicFileName[playerid], gPendingDynamicMD5[playerid]);
        gPendingDynamicCheat[playerid] = -1;   // รีเซ็ต
    }

    // =====================================
    //       ระบบกันโปรเดิม (ไม่เปลี่ยน)
    // =====================================
    if(playerData[playerid][pAdmin] < 19)
    {
        if(!IsPlayerAndroidv2(playerid))
        {
            if(gPlayerAntiCheatStatus{playerid} == 0)
            {
                SendClientMessage(playerid, 0xFF0000FF, "{B22222}[{DCDCDC}STP-Vanguard{DCDCDC}{B22222}]{FFFFFF} {CD5C5C}คุณไม่ได้ติดตั้งระบบป้องกันการโกง (ตัวกันโปร)");
                SendClientMessage(playerid, 0xFF0000FF, "{FFFFFF}ดาวน์โหลดจาก > {4169E1}Discord: discord.gg/fyyMhXJS9A{FFFFFF} ก่อนเข้าเล่น!");
                SendClientMessage(playerid, COLOR_WHITE, ""); 
                SendClientMessage(playerid, COLOR_LIGHTBLUE, "{FF6600}[!]{FFFFFF} ถ้าเกิดว่าลงกันโปรแล้วขึ้นไม่ได้ลง ให้อัพเดทลงกันโปรตัวล่าสุดในดิสคอส");
                SendClientMessage(playerid, COLOR_LIGHTBLUE, "{FF6600}[!]{FFFFFF} (อัพเดทล่าสุด > 17.5.2569)");
                DelayedKick(playerid);  
                return 1;
            }
            else if(gPlayerAntiCheatStatus{playerid} == 1)
            {
                SendClientMessage(playerid, 0xFF0000FF, "{B22222}[{DCDCDC}STP-Vanguard{DCDCDC}{B22222}]{FFFFFF} {CD5C5C}ไฟล์กันโปรของคุณเป็นเวอร์ชันเก่า {FFFFFF}(อัพเดทล่าสุด > 17.5.2569)");
                SendClientMessage(playerid, 0xFF0000FF, "{FFFFFF}กรุณาดาวน์โหลดใหม่จาก {4169E1}Discord!");
                DelayedKick(playerid);
                return 1;
            }
        }
    }

    if(gPendingCheatID[playerid] != 0)
    {
        ProcessCheatDetection(playerid, gPendingCheatID[playerid]);
        gPendingCheatID[playerid] = 0;
    }

    return 1;
}

Logdiscord_AntiCheatStatus(playerid)
{
    if (!IsPlayerConnected(playerid)) return 0;

    // --- เช็คเงื่อนไขก่อนส่ง: ต้องเป็น PC และติดตั้งกันโปรเวอร์ชันล่าสุด ---
    if (IsPlayerAndroidv2(playerid)) // ถ้าเป็นมือถือ ? ไม่ส่ง log
        return 0;

    if (gPlayerAntiCheatStatus{playerid} != 2) // ถ้าไม่ได้ลงหรือลงเวอร์ชันเก่า ? ไม่ส่ง log
        return 0;

    // --- ถ้าผ่านเงื่อนไขทั้งหมด ค่อยสร้างและส่ง Embed ---
    new DCC_Embed:embed = DCC_CreateEmbed("Admin Login Log", "", "", "", 0x00FF00);

    new str[256], playerIP[16], query[128], playerName[MAX_PLAYER_NAME], skinURL[128];
    new country[64], isp[64], city[64], skinID, playerWhitelist;

    new acStatus[128];
    strcopy(acStatus, gPlayerACStatusString[playerid]);

    // --- ดึงข้อมูลผู้เล่น ---
    GetPlayerName(playerid, playerName, sizeof(playerName));
    GetPlayerIp(playerid, playerIP, sizeof(playerIP));
    GetPlayerCountry(playerid, country, sizeof(country));
    GetPlayerISP(playerid, isp, sizeof(isp));
    GetPlayerCity(playerid, city, sizeof(city));

    // --- ดึงข้อมูล Whitelist จาก SQL ---
    cache_get_value_name_int(0, "playerWhitelist", playerWhitelist);

    // --- สกิน ---
    skinID = GetPlayerSkin(playerid);
    if (skinID >= 0 && skinID <= 311)
        format(skinURL, sizeof(skinURL), "http://trainingpark.ddns.net/assets/skins/skin-%d_stp.png", skinID);
    else
        format(skinURL, sizeof(skinURL), "http://trainingpark.ddns.net/assets/skins/cat.png");

    DCC_SetEmbedThumbnail(embed, skinURL);

    // --- ตั้งหัว Embed ---
    format(str, sizeof(str), "[Log-Login (STP-Vanguard) :green_circle: ] ข้อมูลผู้เล่น: %s", playerName);
    DCC_SetEmbedTitle(embed, str);

    // --- ฟิลด์ข้อมูล ---
    new playerField[128], whitelistStatus[32];
    format(whitelistStatus, sizeof(whitelistStatus), "(%s Whitelist)", playerWhitelist == 1 ? "มี" : "ไม่มี");
    format(playerField, sizeof(playerField), ":bust_in_silhouette: ชื่อผู้เล่น %s %s", playerName, whitelistStatus);
    DCC_AddEmbedField(embed, playerField, "");

    DCC_AddEmbedField(embed, ":jigsaw: อุปกรณ์", "คอมพิวเตอร์"); // แน่นอนว่าเป็น PC เพราะเช็คมาแล้ว

    DCC_AddEmbedField(embed, ":flag_white: ประเทศ", country);

    DCC_AddEmbedField(embed, ":cityscape: เมือง", city);

    DCC_AddEmbedField(embed, ":wireless: ผู้ให้บริการอินเทอร์เน็ต", isp);

    DCC_AddEmbedField(embed, "IP", playerIP);

    DCC_AddEmbedField(embed, "กันโปรที่ลง", acStatus);

    DCC_AddEmbedField(embed, ":timer: เข้าสู่ระบบเมื่อ", ReturnDateTime(0));

    // --- ส่งไป Discord ---
    DCC_SendChannelEmbedMessage(DCC_FindChannelById("1455555958356054107"), embed);

    return 1;
}

// --- [ คำสั่งแอดมินและผู้เล่นทั่วไป ] ---
CMD:checkhwid(playerid, params[])
{
    if (playerData[playerid][pAdmin] < 20)
        return 1;

    new targetid;
    if(sscanf(params, "u", targetid)) return SendClientMessage(playerid, -1, "การใช้งาน: /checkhwid [ID]");

    if(!IsPlayerConnected(targetid)) return SendClientMessage(playerid, -1, "ไม่พบผู้เล่น");

    new str[128];
    format(str, sizeof(str), "* HWID ของ %d (%s) คือ: %s", targetid, GetPlayerNameEx(targetid), gPlayerHWID[targetid]);
    SendClientMessage(playerid, -1, str);
    return 1;
}
alias:rg("ac")
CMD:rg(playerid, params[])
{
    if(playerData[playerid][IsLoggedIn] == false)
	return 1;

    new targetid;
    if(sscanf(params, "u", targetid)) return SendClientMessage(playerid, -1, "{FF6600}[CMD]:{04c0f0} /rg หรือ /ac (id)");

    if(!IsPlayerConnected(targetid)) return SendClientMessage(playerid, -1, "{FF6600}[!]{FFFFFF} ผู้เล่นนั้นไม่ออนไลน์");

    new targetName[MAX_PLAYER_NAME], str[256];
    GetPlayerName(targetid, targetName, sizeof(targetName));

    // เช็คก่อนเลยว่าถ้าเป็นมือถือ ให้แสดงข้อความพิเศษ
    if(IsPlayerAndroidv2(targetid))
    {
        format(str, sizeof(str), "{f9f900}%s | {00FF00}เล่นผ่านมือถือ - ไม่ต้องติดตั้งกันโปร", targetName);
        SendClientMessage(playerid, -1, str);
        return 1;
    }

    switch(gPlayerAntiCheatStatus{targetid})
    {
        case 0, 1: // รวมทั้งไม่ได้ลงเลย และลงแล้วแต่เวอร์ชันเก่า
        {
            format(str, sizeof(str), "{f9f900}%s | {FF0000}ไม่ได้ติดตั้งตัวกันโปร", targetName);
        }
        case 2:
        {
            if(gPlayerACType[targetid] == 1)
            {
                format(str, sizeof(str), "{f9f900}%s | {00FF00}STP-Vanguard (v.nocleo)", targetName);
            }
            else // == 0
            {
                format(str, sizeof(str), "{f9f900}%s | {00FF00}STP-Vanguard (ปกติ)", targetName);
            }
        }
        default:
        {
            format(str, sizeof(str), "{f9f900}%s | {00FF00}สถานะกันโปรไม่ทราบ (อาจเกิดข้อผิดพลาด)", targetName);
        }
    }

    SendClientMessage(playerid, -1, str);
    return 1;
}

//


