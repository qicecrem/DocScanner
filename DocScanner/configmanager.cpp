#include "configmanager.h"
#include <QSqlQuery>
#include <QSqlError>
#include <QCoreApplication>
#include <QDebug>
#include <QDir>

ConfigManager& ConfigManager::instance() {
    static ConfigManager instance;
    return instance;
}

ConfigManager::ConfigManager() {
    // 将 db 存放在可执行文件目录下
    QString dbPath = QCoreApplication::applicationDirPath() + "/app_config.db";

    db = QSqlDatabase::addDatabase("QSQLITE");
    db.setDatabaseName(dbPath);

    if (!db.open()) {
        qWarning() << "无法打开 SQLite 数据库:" << db.lastError().text();
        return;
    }

    // 初始化表结构 (简单的键值对)
    QSqlQuery query;
    query.exec("CREATE TABLE IF NOT EXISTS settings (config_key TEXT PRIMARY KEY, config_value TEXT)");
}

ConfigManager::~ConfigManager() {
    if (db.isOpen()) {
        db.close();
    }
}

void ConfigManager::setValue(const QString& key, const QVariant& value) {
    QSqlQuery query;
    query.prepare("INSERT OR REPLACE INTO settings (config_key, config_value) VALUES (?, ?)");
    query.addBindValue(key);
    query.addBindValue(value.toString());
    query.exec();
}

QVariant ConfigManager::getValue(const QString& key, const QVariant& defaultValue) {
    QSqlQuery query;
    query.prepare("SELECT config_value FROM settings WHERE config_key = ?");
    query.addBindValue(key);

    if (query.exec() && query.next()) {
        return query.value(0);
    }
    return defaultValue;
}

QString ConfigManager::getString(const QString& key, const QString& defaultVal) {
    return getValue(key, defaultVal).toString();
}

int ConfigManager::getInt(const QString& key, int defaultVal) {
    return getValue(key, defaultVal).toInt();
}
