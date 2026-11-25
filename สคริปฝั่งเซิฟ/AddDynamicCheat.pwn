// ============================================================================
// [ระบบ Dynamic Cheat Scan ผ่าน SQL - เวอร์ชันแก้ไขใหม่ทั้งหมด]
// ============================================================================

#define MAX_SQL_CHEATS 500
#define CHEATS_PER_PAGE 20

enum E_SQL_CHEAT {
    CheatDBID,
    CheatHash[33],
    CheatName[64],
    CheatAction,
    CheatAddedBy[24],
    CheatAddedDate[32],
    CheatUpdatedBy[24],
    CheatUpdatedDate[32]
};

new SQLCheats[MAX_SQL_CHEATS][E_SQL_CHEAT];
new TotalSQLCheats = 0;

// ตัวแปรสำหรับแอดมิน
new AdminPage[MAX_PLAYERS];
new AdminEditIndex[MAX_PLAYERS];
new AdminTempHash[MAX_PLAYERS][33];
new AdminTempName[MAX_PLAYERS][64];

// ===================================================================
// Load Dynamic Cheats จาก SQL
// ===================================================================
forward LoadDynamicCheats(); 
public LoadDynamicCheats()
{
    mysql_tquery(g_SQL, "SELECT * FROM `dynamic_cheats`", "OnDynamicCheatsLoaded", "");
    return 1;
}

forward OnDynamicCheatsLoaded();
public OnDynamicCheatsLoaded()
{
    TotalSQLCheats = 0;
    new rows = cache_num_rows();

    if(rows > 0)
    {
        for(new i = 0; i < rows && i < MAX_SQL_CHEATS; i++)
        {
            cache_get_value_name_int(i, "id", SQLCheats[i][CheatDBID]);
            cache_get_value_name(i, "md5hash", SQLCheats[i][CheatHash], 33);
            cache_get_value_name(i, "cheat_name", SQLCheats[i][CheatName], 64);
            cache_get_value_name_int(i, "action", SQLCheats[i][CheatAction]);
            
            // ข้อมูลผู้เพิ่ม
            cache_get_value_name(i, "added_by", SQLCheats[i][CheatAddedBy], 24);
            cache_get_value_name(i, "added_date", SQLCheats[i][CheatAddedDate], 32);
            
            // ข้อมูลผู้แก้ไขล่าสุด (เช็คป้องกันค่า NULL)
            new tempStr[32];
            cache_get_value_name(i, "updated_by", tempStr, 24);
            if(strlen(tempStr) < 1 || !strcmp(tempStr, "NULL", true)) strcopy(SQLCheats[i][CheatUpdatedBy], "-");
            else strcopy(SQLCheats[i][CheatUpdatedBy], tempStr);

            cache_get_value_name(i, "updated_date", tempStr, 32);
            if(strlen(tempStr) < 1 || !strcmp(tempStr, "NULL", true)) strcopy(SQLCheats[i][CheatUpdatedDate], "-");
            else strcopy(SQLCheats[i][CheatUpdatedDate], tempStr);

            TotalSQLCheats++;
        }
    }
    printf("[STP-Vanguard] Loaded %d dynamic cheats from database.", TotalSQLCheats);
    return 1;
}

// ===================================================================
// ตรวจซ้ำ MD5 (ทั้ง Script + SQL)
// ===================================================================
stock bool:IsHashExists(const md5hash[])
{
    for(new i = 0; i < sizeof(DynamicCheats); i++) {
        if(!strcmp(md5hash, DynamicCheats[i][CheatHash], true)) return true;
    }
    for(new i = 0; i < TotalSQLCheats; i++) {
        if(!strcmp(md5hash, SQLCheats[i][CheatHash], true)) return true;
    }
    return false;
}

// ===================================================================
// Detection
// ===================================================================
public OnPlayerDetectedDynamicFile(playerid, fileType, const filename[], const md5hash[])
{
    if (!IsPlayerConnected(playerid)) return 1;

    if (!gPlayerHasSpawned{playerid})
    {
        for (new i = 0; i < sizeof(DynamicCheats); i++)
        {
            if (!strcmp(md5hash, DynamicCheats[i][CheatHash], true))
            {
                gPendingDynamicCheat[playerid] = i;
                strcopy(gPendingDynamicFileName[playerid], filename);
                strcopy(gPendingDynamicMD5[playerid], md5hash);
                return 1;
            }
        }
        
        for (new i = 0; i < TotalSQLCheats; i++)
        {
            if (SQLCheats[i][CheatAction] > 0 && !strcmp(md5hash, SQLCheats[i][CheatHash], true))
            {
                gPendingDynamicCheat[playerid] = (i + 1000);
                strcopy(gPendingDynamicFileName[playerid], filename);
                strcopy(gPendingDynamicMD5[playerid], md5hash);
                return 1;
            }
        }
        return 1;
    }

    ProcessDynamicCheatDetection(playerid, fileType, filename, md5hash);
    return 1;
}

stock ProcessDynamicCheatDetection(playerid, fileType, const filename[], const md5hash[])
{
    if (!IsPlayerConnected(playerid)) return 0;
    
    new cheatAction = 0;
    new cheatName[64];
    new bool:found = false;

    for (new i = 0; i < sizeof(DynamicCheats); i++)
    {
        if (!strcmp(md5hash, DynamicCheats[i][CheatHash], true))
        {
            cheatAction = DynamicCheats[i][CheatAction];
            strcopy(cheatName, DynamicCheats[i][CheatName]);
            found = true;
            break;
        }
    }

    if(!found)
    {
        for (new i = 0; i < TotalSQLCheats; i++)
        {
            if (SQLCheats[i][CheatAction] > 0 && !strcmp(md5hash, SQLCheats[i][CheatHash], true))
            {
                cheatAction = SQLCheats[i][CheatAction];
                strcopy(cheatName, SQLCheats[i][CheatName]);
                found = true;
                break;
            }
        }
    }

    if(found && cheatAction > 0)
    {
        new str[256];

        if (cheatAction == 2)
        {
            format(str, sizeof(str), "{B22222}[{DCDCDC}STP-Vanguard{DCDCDC}{B22222}]{FFFFFF} %s ถูกแบน ข้อหา: ตรวจพบไฟล์อันตราย %s (%s)", 
                GetPlayerNameEx(playerid), cheatName, filename);
            SendClientMessageToAll(-1, str);

            new ip[16]; GetPlayerIp(playerid, ip, sizeof(ip));
            format(str, sizeof(str), "[STP-Vanguard (Dynamic)] : ผู้เล่น %s ถูกแบน สาเหตุ: %s\n:file_folder: ไฟล์ที่ตรวจพบ: %s\n[:gear:] : (ID: %d)\n[:round_pushpin:] : ไอพีเน็ต : %s\n[:timer:] : ถูกแบนตอนเวลา : %s",
                GetPlayerNameEx(playerid), cheatName, filename, playerData[playerid][pID], ip, ReturnDateTime(0));
            DCC_SendChannelMessage(DCC_FindChannelById("1455499307384832074"), str);

            new escReason[128], escName[MAX_PLAYER_NAME * 2], query[1024];
            mysql_escape_string(cheatName, escReason);
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
        else if (cheatAction == 1)
        {
            format(str, sizeof(str), "{B22222}[{DCDCDC}STP-Vanguard{DCDCDC}{B22222}]{FFFFFF} %s ถูกเตะออกจากเซิร์ฟเวอร์ ข้อหา: %s", 
                GetPlayerNameEx(playerid), cheatName);
            SendClientMessageToAll(-1, str);

            new ip[16]; GetPlayerIp(playerid, ip, sizeof(ip));
            format(str, sizeof(str), "[STP-Vanguard (Dynamic)] : ผู้เล่น %s ถูกเตะ สาเหตุ: %s\n:file_folder: ไฟล์ที่ตรวจพบ: %s\n[:gear:] : (ID: %d)",
                GetPlayerNameEx(playerid), cheatName, filename, playerData[playerid][pID]);
            DCC_SendChannelMessage(DCC_FindChannelById("1455499187759218790"), str);

            DelayedKick(playerid);
        }
    }
    return 1;
}

// ===================================================================
// CMD + Main Menu
// ===================================================================
CMD:acmanage(playerid, params[])
{
    if (playerData[playerid][pAdmin] < 20) 
        return SendClientMessage(playerid, -1, "ไม่มีสิทธิ์ใช้งานคำสั่งนี้");

    ShowDynamicAdminMenu(playerid);
    return 1;
}

stock ShowDynamicAdminMenu(playerid)
{
    Dialog_Show(playerid, DIALOG_DYNAMIC_MENU, DIALOG_STYLE_LIST, 
        "STP-Vanguard | จัดการไฟล์โปรต้องห้าม",
        "1. ดูไฟล์โปรฝังในสคริปต์ (Hardcoded - แก้ไขไม่ได้)\n\
2. ดู/จัดการ ไฟล์โปร Dynamic (SQL)\n\
3. เพิ่มไฟล์โปรใหม่ (Add Hash)\n\
4. รีโหลดข้อมูลใหม่จากฐานข้อมูล", 
        "เลือก", "ยกเลิก");
}

Dialog:DIALOG_DYNAMIC_MENU(playerid, response, listitem, inputtext[])
{
    if(!response) return 1;

    switch(listitem)
    {
        case 0: ShowScriptCheats(playerid);
        case 1: 
        {
            AdminPage[playerid] = 0;
            ShowSQLCheatList(playerid);
        }
        case 2: 
            Dialog_Show(playerid, DIALOG_DYNAMIC_ADD_HASH, DIALOG_STYLE_INPUT, 
                "เพิ่มไฟล์โปร | ขั้นตอน 1/3", 
                "กรุณาใส่ค่า MD5 Hash (32 ตัวอักษรพิมพ์ใหญ่):", "ต่อไป", "ยกเลิก");
        case 3: 
        {
            LoadDynamicCheats();
            SendClientMessage(playerid, -1, "{00FF00}[STP-Vanguard] ทำการรีโหลดข้อมูล Dynamic Cheats จากฐานข้อมูลเรียบร้อยแล้ว!");
            ShowDynamicAdminMenu(playerid);
        }
    }
    return 1;
}

// ===================================================================
// 1. แสดง Script Hardcoded
// ===================================================================
stock ShowScriptCheats(playerid)
{
    new string[2048];
    format(string, sizeof(string), "ประเภท\tชื่อ\tบทลงโทษ\n");

    for(new i = 0; i < sizeof(DynamicCheats); i++)
    {
        new actStr[32];
        if(DynamicCheats[i][CheatAction] == 0) actStr = "{AAAAAA}ปิดใช้งาน";
        else if(DynamicCheats[i][CheatAction] == 1) actStr = "{FFFF00}เตะ (Kick)";
        else actStr = "{FF0000}แบน (Ban)";

        format(string, sizeof(string), "%s{Script}\t%s\t%s\n", string, DynamicCheats[i][CheatName], actStr);
    }

    Dialog_Show(playerid, DIALOG_SCRIPT_CHEATS, DIALOG_STYLE_TABLIST_HEADERS, 
        "STP-Vanguard | ไฟล์โปรฝังในสคริปต์ (แก้ไขไม่ได้)", 
        string, "กลับ", "");
}

Dialog:DIALOG_SCRIPT_CHEATS(playerid, response, listitem, inputtext[])
{
    ShowDynamicAdminMenu(playerid);
    return 1;
}

// ===================================================================
// 2. แสดง + จัดการ SQL
// ===================================================================
stock ShowSQLCheatList(playerid)
{
    if(TotalSQLCheats == 0) 
        return SendClientMessage(playerid, -1, "{FF0000}ยังไม่มีข้อมูลไฟล์โปร Dynamic ใน SQL");

    new string[2048], count = 0;
    new startIndex = AdminPage[playerid] * CHEATS_PER_PAGE;
    new endIndex = startIndex + CHEATS_PER_PAGE;

    if(endIndex > TotalSQLCheats) endIndex = TotalSQLCheats;

    format(string, sizeof(string), "ID\tชื่อ\tบทลงโทษ\n");

    for(new i = startIndex; i < endIndex; i++)
    {
        new actStr[32];

        if(SQLCheats[i][CheatAction] == 0) actStr = "{AAAAAA}ปิดใช้งาน";
        else if(SQLCheats[i][CheatAction] == 1) actStr = "{FFFF00}เตะ (Kick)";
        else actStr = "{FF0000}แบน (Ban)";

        format(string, sizeof(string), "%s%d\t%s\t%s\n", string, SQLCheats[i][CheatDBID], SQLCheats[i][CheatName], actStr);
        count++;
    }

    if(endIndex < TotalSQLCheats) strcat(string, "{00FFFF}>>> หน้าถัดไป >>>\n");
    if(AdminPage[playerid] > 0) strcat(string, "{00FFFF}<<< ย้อนกลับ <<<\n");

    new title[128];
    format(title, sizeof(title), "จัดการไฟล์โปร Dynamic (SQL) - หน้า %d", AdminPage[playerid] + 1);

    Dialog_Show(playerid, DIALOG_DYNAMIC_LIST, DIALOG_STYLE_TABLIST_HEADERS, title, string, "เลือก", "กลับ");
}

Dialog:DIALOG_DYNAMIC_LIST(playerid, response, listitem, inputtext[])
{
    if(!response) return ShowDynamicAdminMenu(playerid);

    new startIndex = AdminPage[playerid] * CHEATS_PER_PAGE;
    new endIndex = startIndex + CHEATS_PER_PAGE;
    if(endIndex > TotalSQLCheats) endIndex = TotalSQLCheats;

    new count = endIndex - startIndex;

    if(listitem < count)
    {
        AdminEditIndex[playerid] = startIndex + listitem;
        ShowDynamicEditMenu(playerid);
    }
    else if(listitem == count && endIndex < TotalSQLCheats)
    {
        AdminPage[playerid]++;
        ShowSQLCheatList(playerid);
    }
    else
    {
        AdminPage[playerid]--;
        ShowSQLCheatList(playerid);
    }
    return 1;
}

// ===================================================================
// Edit Menu (เวอร์ชันแสดงรายละเอียดครบ)
// ===================================================================
stock ShowDynamicEditMenu(playerid)
{
    new index = AdminEditIndex[playerid];
    new actStr[32], info[1024];

    if(SQLCheats[index][CheatAction] == 0) actStr = "ปิดใช้งาน";
    else if(SQLCheats[index][CheatAction] == 1) actStr = "เตะ (Kick)";
    else actStr = "แบน (Ban)";

    // จัดเรียง List ให้สามารถกดได้เฉพาะ 3 บรรทัดแรก
    format(info, sizeof(info), "1. {FFFF00}แก้ไขชื่อไฟล์ {FFFFFF}(ปัจจุบัน: %s)\n", SQLCheats[index][CheatName]);
    format(info, sizeof(info), "%s2. {FFFF00}เปลี่ยนบทลงโทษ {FFFFFF}(ปัจจุบัน: %s)\n", info, actStr);
    format(info, sizeof(info), "%s3. {FF0000}ลบข้อมูลนี้ทิ้ง\n", info);
    
    // ส่วนรายละเอียด ไม่ได้ออกแบบมาให้กด (ถ้ากดจะเด้งกลับหน้าเดิม)
    format(info, sizeof(info), "%s{AAAAAA}---------------------------------------------------\n", info);
    format(info, sizeof(info), "%s{FFFFFF}ค่า MD5: {00FFFF}%s\n", info, SQLCheats[index][CheatHash]);
    format(info, sizeof(info), "%s{FFFFFF}เพิ่มโดย: {00FF00}%s {FFFFFF}(%s)\n", info, SQLCheats[index][CheatAddedBy], SQLCheats[index][CheatAddedDate]);
    
    if(strcmp(SQLCheats[index][CheatUpdatedBy], "-", true) != 0) {
        format(info, sizeof(info), "%s{FFFFFF}อัพเดทล่าสุดโดย: {FFA500}%s {FFFFFF}(%s)", info, SQLCheats[index][CheatUpdatedBy], SQLCheats[index][CheatUpdatedDate]);
    } else {
        format(info, sizeof(info), "%s{FFFFFF}อัพเดทล่าสุดโดย: {AAAAAA}- ยังไม่มีประวัติการอัพเดท -", info);
    }

    Dialog_Show(playerid, DIALOG_DYNAMIC_EDIT_MENU, DIALOG_STYLE_LIST, 
        "STP-Vanguard | รายละเอียดและการจัดการ", 
        info, "เลือก", "ย้อนกลับ");
}

Dialog:DIALOG_DYNAMIC_EDIT_MENU(playerid, response, listitem, inputtext[])
{
    if(!response) return ShowSQLCheatList(playerid);

    switch(listitem)
    {
        case 0: Dialog_Show(playerid, DIALOG_DYNAMIC_EDIT_NAME, DIALOG_STYLE_INPUT, "แก้ไขชื่อ", "ใส่ชื่อใหม่ที่ต้องการ:", "บันทึก", "ยกเลิก");
        case 1: Dialog_Show(playerid, DYNAMIC_EDIT_ACTION, DIALOG_STYLE_LIST, "เปลี่ยนบทลงโทษ", "0 - ปิดใช้งาน\n1 - เตะ (Kick)\n2 - แบน (Ban)", "บันทึก", "ยกเลิก");
        case 2: Dialog_Show(playerid, DIALOG_DYNAMIC_DELETE, DIALOG_STYLE_MSGBOX, "ยืนยันการลบ", "{FF0000}คุณแน่ใจหรือไม่ว่าต้องการลบข้อมูลนี้?", "ยืนยัน", "ยกเลิก");
        default: ShowDynamicEditMenu(playerid); // กรณีเผลอกดบรรทัดที่เป็นข้อมูลแจ้งเตือน ให้เด้งแสดงหน้าต่างเดิม
    }
    return 1;
}

// ===================================================================
// เพิ่มไฟล์ใหม่ (3 ขั้นตอน)
// ===================================================================
Dialog:DIALOG_DYNAMIC_ADD_HASH(playerid, response, listitem, inputtext[])
{
    if(!response) return ShowDynamicAdminMenu(playerid);

    if(strlen(inputtext) != 32) 
        return Dialog_Show(playerid, DIALOG_DYNAMIC_ADD_HASH, DIALOG_STYLE_INPUT, 
            "เพิ่มไฟล์โปร | ขั้นตอน 1/3", 
            "{FF0000}MD5 ต้องมีความยาว 32 ตัวอักษรเท่านั้น!\n{FFFFFF}กรุณาใส่ค่า MD5 Hash ใหม่:", "ต่อไป", "ยกเลิก");

    if(IsHashExists(inputtext)) 
        return Dialog_Show(playerid, DIALOG_DYNAMIC_ADD_HASH, DIALOG_STYLE_INPUT, 
            "เพิ่มไฟล์โปร | ขั้นตอน 1/3", 
            "{FF0000}ค่า MD5 นี้มีอยู่ในระบบแล้ว (ซ้ำ)!\n{FFFFFF}กรุณาใส่ค่า MD5 Hash ใหม่:", "ต่อไป", "ยกเลิก");

    strcopy(AdminTempHash[playerid], inputtext);
    Dialog_Show(playerid, DIALOG_DYNAMIC_ADD_NAME, DIALOG_STYLE_INPUT, 
        "เพิ่มไฟล์โปร | ขั้นตอน 2/3", 
        "กรุณาตั้งชื่อเพื่อใช้แจ้งเตือน (เช่น Aimbot.exe, Godmode.asi):", "ต่อไป", "ย้อนกลับ");
}

Dialog:DIALOG_DYNAMIC_ADD_NAME(playerid, response, listitem, inputtext[])
{
    if(!response) return Dialog_Show(playerid, DIALOG_DYNAMIC_ADD_HASH, DIALOG_STYLE_INPUT, 
        "เพิ่มไฟล์โปร | ขั้นตอน 1/3", "กรุณาใส่ค่า MD5 Hash (32 ตัวอักษรพิมพ์ใหญ่):", "ต่อไป", "ยกเลิก");

    if(strlen(inputtext) < 2) 
        return Dialog_Show(playerid, DIALOG_DYNAMIC_ADD_NAME, DIALOG_STYLE_INPUT, 
            "เพิ่มไฟล์โปร | ขั้นตอน 2/3", 
            "{FF0000}ชื่อสั้นเกินไป!\n{FFFFFF}กรุณาตั้งชื่อเพื่อใช้แจ้งเตือน:", "ต่อไป", "ย้อนกลับ");

    strcopy(AdminTempName[playerid], inputtext);
    Dialog_Show(playerid, DYNAMIC_ADD_ACTION, DIALOG_STYLE_LIST, 
        "เพิ่มไฟล์โปร | ขั้นตอน 3/3", 
        "0 - ปิดใช้งาน (ตรวจเจอแต่ไม่ทำอะไร)\n1 - เตะออกจากเซิร์ฟเวอร์ (Kick)\n2 - แบนบัญชี + HWID (Ban)", 
        "บันทึก", "ย้อนกลับ");
}

Dialog:DYNAMIC_ADD_ACTION(playerid, response, listitem, inputtext[])
{
    if(!response)
    {
        return Dialog_Show(playerid, DIALOG_DYNAMIC_ADD_NAME, DIALOG_STYLE_INPUT,
            "เพิ่มไฟล์โปร | ขั้นตอน 2/3", 
            "กรุณาตั้งชื่อเพื่อใช้แจ้งเตือน (เช่น Aimbot.exe, Godmode.asi):", 
            "ต่อไป", "ย้อนกลับ");
    }

    switch(listitem)
    {
        case 0, 1, 2:
        {
            new action = listitem;
            new query[512], escName[64], escAdmin[MAX_PLAYER_NAME];
            
            mysql_escape_string(AdminTempName[playerid], escName);
            mysql_escape_string(GetPlayerNameEx(playerid), escAdmin);

            mysql_format(g_SQL, query, sizeof(query),
                "INSERT INTO `dynamic_cheats` (`md5hash`, `cheat_name`, `action`, `added_by`, `added_date`) VALUES ('%e', '%e', %d, '%e', NOW())",
                AdminTempHash[playerid], escName, action, escAdmin);

            mysql_tquery(g_SQL, query, "LoadDynamicCheats", "");

            new str[128];
            format(str, sizeof(str), "{00FF00}เพิ่ม %s (MD5: %s) เรียบร้อยแล้ว!", 
                AdminTempName[playerid], AdminTempHash[playerid]);
            SendClientMessage(playerid, -1, str);
            
            ShowDynamicAdminMenu(playerid);
        }
    }
    return 1;
}

// ===================================================================
// Edit / Delete ข้อมูล
// ===================================================================
Dialog:DIALOG_DYNAMIC_EDIT_NAME(playerid, response, listitem, inputtext[])
{
    if(!response) return ShowDynamicEditMenu(playerid);
    new index = AdminEditIndex[playerid];
    new query[512], escName[64], escAdmin[MAX_PLAYER_NAME];

    if(strlen(inputtext) < 2) {
        SendClientMessage(playerid, -1, "{FF0000}ชื่อสั้นเกินไป!");
        return ShowDynamicEditMenu(playerid);
    }

    mysql_escape_string(inputtext, escName);
    mysql_escape_string(GetPlayerNameEx(playerid), escAdmin);

    mysql_format(g_SQL, query, sizeof(query), 
        "UPDATE `dynamic_cheats` SET `cheat_name` = '%e', `updated_by` = '%e', `updated_date` = NOW() WHERE `id` = %d", 
        escName, escAdmin, SQLCheats[index][CheatDBID]);
    
    mysql_tquery(g_SQL, query, "LoadDynamicCheats", "");

    SendClientMessage(playerid, -1, "{00FF00}อัพเดทชื่อสำเร็จ!");
    ShowSQLCheatList(playerid);
}

Dialog:DYNAMIC_EDIT_ACTION(playerid, response, listitem, inputtext[])
{
    if(!response) return ShowDynamicEditMenu(playerid);
    new index = AdminEditIndex[playerid];
    new query[512], escAdmin[MAX_PLAYER_NAME];

    mysql_escape_string(GetPlayerNameEx(playerid), escAdmin);

    mysql_format(g_SQL, query, sizeof(query), 
        "UPDATE `dynamic_cheats` SET `action` = %d, `updated_by` = '%e', `updated_date` = NOW() WHERE `id` = %d", 
        listitem, escAdmin, SQLCheats[index][CheatDBID]);
    
    mysql_tquery(g_SQL, query, "LoadDynamicCheats", "");

    SendClientMessage(playerid, -1, "{00FF00}อัพเดทบทลงโทษสำเร็จ!");
    ShowSQLCheatList(playerid);
}

Dialog:DIALOG_DYNAMIC_DELETE(playerid, response, listitem, inputtext[])
{
    if(!response) return ShowDynamicEditMenu(playerid);
    new index = AdminEditIndex[playerid];
    new query[256];

    mysql_format(g_SQL, query, sizeof(query), 
        "DELETE FROM `dynamic_cheats` WHERE `id` = %d", 
        SQLCheats[index][CheatDBID]);

    mysql_tquery(g_SQL, query, "LoadDynamicCheats", "");

    SendClientMessage(playerid, -1, "{00FF00}ลบข้อมูลออกจากฐานข้อมูลเรียบร้อยแล้ว!");
    ShowSQLCheatList(playerid);
}