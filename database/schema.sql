-- STP-Vanguard 9.11.0 database schema
-- Import this file into the MySQL database used by your SA-MP server.
-- Review database/README.md and adapt the Pawn scripts to your own setup.

CREATE TABLE IF NOT EXISTS `dynamic_cheats` (
    `id` INT(11) NOT NULL AUTO_INCREMENT,
    `md5hash` VARCHAR(33) COLLATE utf8mb4_unicode_ci NOT NULL,
    `cheat_name` VARCHAR(64) COLLATE utf8mb4_unicode_ci NOT NULL,
    `action` INT(11) NOT NULL DEFAULT 0,
    `added_by` VARCHAR(24) COLLATE utf8mb4_unicode_ci NOT NULL,
    `added_date` DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
    `updated_by` VARCHAR(24) COLLATE utf8mb4_unicode_ci DEFAULT NULL,
    `updated_date` DATETIME DEFAULT NULL,
    PRIMARY KEY (`id`),
    UNIQUE KEY `uq_dynamic_cheats_md5hash` (`md5hash`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS `banned_hwids` (
    `id` INT(11) UNSIGNED NOT NULL AUTO_INCREMENT,
    `hwid` VARCHAR(255) COLLATE utf8mb4_unicode_ci NOT NULL,
    `Name` VARCHAR(24) COLLATE utf8mb4_unicode_ci NOT NULL,
    `reason` VARCHAR(256) COLLATE utf8mb4_unicode_ci NOT NULL,
    `location` VARCHAR(512) COLLATE utf8mb4_unicode_ci DEFAULT NULL,
    `Ban` TINYINT(1) NOT NULL DEFAULT 1,
    `banned_by` VARCHAR(24) COLLATE utf8mb4_unicode_ci NOT NULL DEFAULT 'STP_Vanguard',
    `ban_date` DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
    PRIMARY KEY (`id`),
    KEY `idx_banned_hwids_hwid` (`hwid`(191)),
    KEY `idx_banned_hwids_name` (`Name`),
    KEY `idx_banned_hwids_ban_date` (`ban_date`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

