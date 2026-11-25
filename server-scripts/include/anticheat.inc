#if defined _anticheat_included
    #endinput
#endif
#define _anticheat_included

/**
 * เรียกเมื่อ client ส่ง response กลับสำเร็จ หรือ fail handshake
 * @param playerid              ID ของผู้เล่น
 * @param is_latest_version     1 = สำเร็จ (เวอร์ชันล่าสุด) | 0 = fail หรือเวอร์ชันเก่า
 */
forward OnPlayerAntiCheatConnect(playerid, bool:is_latest_version);

/**
 * เรียกเมื่อได้รับประเภทของ Anti-Cheat client
 * @param playerid      ID ของผู้เล่น
 * @param clientType    0 = เวอร์ชันปกติ | 1 = เวอร์ชัน nocleo (ตรวจ CLEO/SAMPFUNCS เพิ่ม)
 */
forward OnPlayerAntiCheatType(playerid, clientType);

/**
 * เรียกเมื่อได้รับ HWID จาก client
 * @param playerid  ID ของผู้เล่น
 * @param hwid[]    HWID ในรูปแบบ UUID (36 ตัวอักษร + null terminator)
 */
forward OnPlayerCheckHwid(playerid, const hwid[]);

forward OnPlayerCheckHwidV2(playerid, const hwidv2[]); // v2 แบบรี hwid ได้ยาก



/**
 * เรียกเมื่อ Client ส่งข้อมูลไฟล์/โปรเซสที่กำลังทำงานมาให้เซิร์ฟเวอร์ตรวจสอบ (Dynamic Check)
 * @param playerid  ID ของผู้เล่น
 * @param fileType  1 = ไฟล์ในโฟลเดอร์เกม (.asi, .cs, .dll) | 2 = โปรแกรมภายนอก (Process .exe)
 * @param filename[] ชื่อไฟล์ (เช่น Ton.asi, Ddxz.exe)
 * @param md5hash[]  MD5 Hash ของไฟล์นั้น (ความยาว 32 ตัวอักษร)
 */
forward OnPlayerDetectedDynamicFile(playerid, fileType, const filename[], const md5hash[]);



/**
 * เรียกเมื่อตรวจพบไฟล์ต้องห้ามจาก client (ผ่านการสแกน SHA-256 บนดิสก์)
 * @param playerid  ID ของผู้เล่น
 * @param cheatid   ID ของไฟล์โกงที่เจอ (ตรงกับใน client database)
 */
forward OnPlayerDetectedIllegalFile(playerid, cheatid);



/**
 * เรียกเมื่อ client ส่งโฟลเดอร์เกมมาให้เซิร์ฟเวอร์
 * @param playerid      ID ของผู้เล่น
 * @param folderPath[]  พาธเต็มของโฟลเดอร์เกม (เช่น C:\GTA TARO 7.0 by assmax)
 * @param summary[]     รายการไฟล์+โฟลเดอร์ทั้งหมดในโฟลเดอร์เกม
 */
//forward OnPlayerGameFolderReported(playerid, const folderPath[], const summary[]); (เลิกใช้ 2.3.2569)
 

/**


 * เรียกเมื่อตรวจพบ .asi หรือ .dll ที่ถูกฉีด (injected) จากภายนอกโฟลเดอร์เกม
 * @param playerid      ID ของผู้เล่น
 * @param modulePath[]  พาธเต็มของไฟล์ที่ถูกฉีดเข้ามา (เช่น C:\Cheats\Emperor.asi)
 */
forward OnPlayerDetectedInjectedModule(playerid, const modulePath[]);






/**
 * เรียกเมื่อตรวจพบโมดูล (.dll/.asi) ที่ถูกฉีดเข้ามาใหม่ใน runtime 
 * (จากการตรวจจับการเปลี่ยนแปลง module list หลังจากเกมเริ่มทำงาน)
 *
 * @param playerid      ID ของผู้เล่น
 * @param modulePath[]  พาธเต็มของโมดูลที่ถูกฉีด (เช่น C:\Cheats\Something.dll)
 * @note แตกต่างจาก OnPlayerDetectedInjectedModule ตรงที่เป็นการตรวจจับ dynamic
 */
forward OnPlayerDetectedNewInjectedModule(playerid, const modulePath[]);


/**
 * เรียกเมื่อตรวจพบการโกงประเภท memory/behavior (เช่น infinity run, game speed hack, joypad)
 * @param playerid  ID ของผู้เล่น
 * @param flags     bit flags ของการโกงที่ตรวจพบ (ดู enum CheatFlags)
 * @param speed     ค่าความเร็วพิเศษ (ใช้กับ game speed hack เป็นหลัก)
 */
forward OnPlayerDetectedCheat(playerid, flags, Float:speed);

// Cheat Flags สำหรับ OnPlayerDetectedCheat
enum CheatFlagsAC:(<<= 1)
{
    CHEAT_INFINITY_RUN_STP,
    CHEAT_GAME_SPEED_HACK_STP,
    CHEAT_USING_JOYPAD_STP
    // เพิ่มได้ตามต้องการ
};


    


