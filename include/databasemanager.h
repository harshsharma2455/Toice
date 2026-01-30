#ifndef DATABASEMANAGER_H
#define DATABASEMANAGER_H

#include <QObject>
#include <QtSql/QSqlDatabase>
#include <QtSql/QSqlQuery>
#include <QtSql/QSqlError>
#include <QStandardPaths>
#include <QDir>
#include <QDebug>
#include <QDateTime>

class DatabaseManager : public QObject {
    Q_OBJECT
public:
    static DatabaseManager& instance() {
        static DatabaseManager _instance;
        return _instance;
    }

    bool init() {
        QString dbPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
        QDir().mkpath(dbPath);
        
        m_db = QSqlDatabase::addDatabase("QSQLITE");
        m_db.setDatabaseName(dbPath + "/toice.db");

        if (!m_db.open()) {
            qCritical() << "Database Error:" << m_db.lastError().text();
            return false;
        }

        QSqlQuery query;
        // History Table
        query.exec("CREATE TABLE IF NOT EXISTS history ("
                   "id INTEGER PRIMARY KEY AUTOINCREMENT,"
                   "text TEXT,"
                   "timestamp DATETIME DEFAULT CURRENT_TIMESTAMP)");
        
        // Settings Table
        query.exec("CREATE TABLE IF NOT EXISTS settings ("
                   "key TEXT PRIMARY KEY,"
                   "value TEXT)");

        return true;
    }

    void addHistory(const QString &text) {
        QSqlQuery query;
        query.prepare("INSERT INTO history (text) VALUES (:text)");
        query.bindValue(":text", text);
        query.exec();
    }

    QList<QPair<QString, QString>> getHistory(int limit = 50) {
        QList<QPair<QString, QString>> results;
        QSqlQuery query;
        // Select timestamp formatted as HH:mm AM/PM for display
        query.prepare("SELECT text, strftime('%I:%M %p', timestamp, 'localtime') as time_str FROM history ORDER BY timestamp ASC LIMIT :limit");
        query.bindValue(":limit", limit);
        if (query.exec()) {
            while (query.next()) {
                QString txt = query.value(0).toString();
                QString time = query.value(1).toString();
                results.append(qMakePair(txt, time));
            }
        }
        return results;
    }

    void setSetting(const QString &key, const QString &value) {
        QSqlQuery query;
        query.prepare("INSERT OR REPLACE INTO settings (key, value) VALUES (:key, :value)");
        query.bindValue(":key", key);
        query.bindValue(":value", value);
        query.exec();
    }

    QString getSetting(const QString &key, const QString &defaultValue = "") {
        QSqlQuery query;
        query.prepare("SELECT value FROM settings WHERE key = :key");
        query.bindValue(":key", key);
        if (query.exec() && query.next()) {
            return query.value(0).toString();
        }
        return defaultValue;
    }

private:
    DatabaseManager() {}
    QSqlDatabase m_db;
};

#endif
