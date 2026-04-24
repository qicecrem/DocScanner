#ifndef CONFIGMANAGER_H
#define CONFIGMANAGER_H

#include <QString>
#include <QVariant>
#include <QSqlDatabase>

class ConfigManager {
public:
    // 单例模式，全局访问
    static ConfigManager& instance();

    // 通用读写方法
    void setValue(const QString& key, const QVariant& value);
    QVariant getValue(const QString& key, const QVariant& defaultValue = QVariant());

    // 类型安全的便捷方法
    QString getString(const QString& key, const QString& defaultVal = "");
    int getInt(const QString& key, int defaultVal = 0);

private:
    ConfigManager();
    ~ConfigManager();
    QSqlDatabase db;
};

#endif // CONFIGMANAGER_H
