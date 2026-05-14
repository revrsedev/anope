-- chanstats_plus schema (MySQL/MariaDB)
--
-- NOTE: The module [modules/third/chanstats_plus.cpp](modules/third/chanstats_plus.cpp)
-- will automatically CREATE TABLE IF NOT EXISTS at startup/reload.
-- You only need to create the database and grant privileges.
--
-- This script sets up a dedicated database: `dbstats`.

-- Generated password for user `anope`:
--   1jiBMAmgGR0mTQtfZ6+9VNy/CNXpPoQcKZQXOZtyJ8o=
-- Store this somewhere safe and (optionally) change it before running.

-- 1) Create database
CREATE DATABASE IF NOT EXISTS `dbstats` CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci;

-- 2) Create a SQL user
CREATE USER IF NOT EXISTS 'anope'@'localhost' IDENTIFIED BY '1jiBMAmgGR0mTQtfZ6+9VNy/CNXpPoQcKZQXOZtyJ8o=';
CREATE USER IF NOT EXISTS 'anope'@'127.0.0.1' IDENTIFIED BY '1jiBMAmgGR0mTQtfZ6+9VNy/CNXpPoQcKZQXOZtyJ8o=';

-- 3) Grant privileges (must include at least CREATE/INSERT/UPDATE/SELECT)
GRANT SELECT, INSERT, UPDATE, CREATE, ALTER, INDEX ON `dbstats`.* TO 'anope'@'localhost';
GRANT SELECT, INSERT, UPDATE, CREATE, ALTER, INDEX ON `dbstats`.* TO 'anope'@'127.0.0.1';
FLUSH PRIVILEGES;

-- 4) Create table
-- If you changed the module prefix (prefix="anope_"), update the table name below.
USE `dbstats`;

CREATE TABLE IF NOT EXISTS `anope_chanstatsplus` (
  `chan` varchar(64) NOT NULL DEFAULT '',
  `nick` varchar(64) NOT NULL DEFAULT '',
  `period` ENUM('total','monthly','weekly','daily') NOT NULL,
  `period_start` date NOT NULL,
  `letters` bigint unsigned NOT NULL DEFAULT '0',
  `words` bigint unsigned NOT NULL DEFAULT '0',
  `lines` int unsigned NOT NULL DEFAULT '0',
  `actions` int unsigned NOT NULL DEFAULT '0',
  `smileys_happy` int unsigned NOT NULL DEFAULT '0',
  `smileys_sad` int unsigned NOT NULL DEFAULT '0',
  `smileys_other` int unsigned NOT NULL DEFAULT '0',
  `kicks` int unsigned NOT NULL DEFAULT '0',
  `kicked` int unsigned NOT NULL DEFAULT '0',
  `modes` int unsigned NOT NULL DEFAULT '0',
  `topics` int unsigned NOT NULL DEFAULT '0',
  PRIMARY KEY (`chan`,`nick`,`period`,`period_start`),
  KEY `nick_idx` (`nick`),
  KEY `chan_idx` (`chan`),
  KEY `period_idx` (`period`,`period_start`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;
