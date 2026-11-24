# STP-Vanguard

**STP-Vanguard** is an anti-cheat system for **GTA San Andreas Multiplayer (SA-MP)**. It is designed to work through a client component and a server plugin, helping server owners detect suspicious software or game modifications and review reports from connected players.

## What it does

The client performs periodic checks for potentially unauthorized files, loaded modules, and processes, along with selected in-game values. It reports its status and findings to the server.

The server validates client messages and tracks the verification session using fresh challenges, heartbeats, and acknowledgements. It then exposes callbacks to Pawn scripts so the game mode can decide how to handle timeouts, recoveries, and reports.

Two client variants are available: one that allows CLEO/SAMPFUNCS and one that blocks them.

STP-Vanguard is a detection and reporting layer. It does not guarantee that a player's computer is completely free of cheats, and server-side gameplay checks should remain in place.

> **Source code has not been published yet.** This repository currently contains project information only.

---

# STP-Vanguard (ภาษาไทย)

**STP-Vanguard** คือระบบ Anti-Cheat สำหรับ **GTA San Andreas Multiplayer (SA-MP)** ออกแบบให้ทำงานร่วมกันระหว่างโปรแกรมฝั่งผู้เล่นและปลั๊กอินฝั่งเซิร์ฟเวอร์ เพื่อช่วยตรวจจับซอฟต์แวร์หรือการดัดแปลงเกมที่น่าสงสัย และส่งข้อมูลให้ผู้ดูแลเซิร์ฟเวอร์ตรวจสอบ

## ระบบทำงานอย่างไร

Client จะตรวจสอบเป็นระยะ เช่น ไฟล์ โมดูล และโปรเซสที่อาจเกี่ยวข้องกับการโกง รวมถึงค่าบางอย่างภายในเกม จากนั้นจะส่งสถานะและผลการตรวจไปยัง Server

Server ตรวจสอบรูปแบบและความถูกต้องของข้อมูล พร้อมติดตาม session ด้วย challenge ที่สร้างใหม่ การส่ง heartbeat และการตอบรับรายงาน จากนั้นส่ง callback ให้สคริปต์ Pawn เพื่อให้เกมโหมดกำหนดวิธีจัดการกรณีขาดการเชื่อมต่อ การกลับมาเชื่อมต่อ และรายงานการตรวจพบ

มี Client สองรูปแบบให้เลือก ได้แก่รุ่นที่อนุญาต CLEO/SAMPFUNCS และรุ่นที่ไม่อนุญาต

STP-Vanguard เป็นระบบช่วยตรวจจับและรายงาน ไม่สามารถรับประกันได้ว่าคอมพิวเตอร์ของผู้เล่นปลอดจากโปรแกรมโกงทั้งหมด เซิร์ฟเวอร์ควรคงการตรวจสอบพฤติกรรมการเล่นไว้ด้วย

> **ขณะนี้ยังไม่ได้เผยแพร่ Source Code** รีโปนี้มีเฉพาะข้อมูลแนะนำโปรเจกต์
