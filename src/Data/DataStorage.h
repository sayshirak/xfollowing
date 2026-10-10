#ifndef DATASTORAGE_H
#define DATASTORAGE_H

#include <QObject>
#include <QString>
#include <QList>
#include <QJsonObject>
#include "Post.h"
#include "Keyword.h"

class DataStorage : public QObject {
    Q_OBJECT

public:
    explicit DataStorage(QObject* parent = nullptr);

    // 关键词管理
    QList<Keyword> loadKeywords();
    void saveKeywords(const QList<Keyword>& keywords);
    void addKeyword(const Keyword& keyword);
    void removeKeyword(const QString& keywordId);
    void updateKeyword(const Keyword& keyword);

    // 黑名单关键词（名称/ID 包含则不监控、不关注；默认空）
    QList<Keyword> loadBlacklistKeywords();
    void saveBlacklistKeywords(const QList<Keyword>& keywords);

    // 帖子管理
    QList<Post> loadPosts();
    void savePosts(const QList<Post>& posts);
    void addPost(const Post& post);
    void updatePost(const Post& post);
    bool postExists(const QString& postId);

    // 配置管理
    QJsonObject loadConfig();
    void saveConfig(const QJsonObject& config);
    // 粉丝数/关注数 上限比值；config.json 数组项 key=
    // "Ratio of followers to accounts followed"；缺省返回 defaultValue
    double loadFollowerFollowingRatio(double defaultValue = 1.5);

    // 获取存储路径
    QString getDataPath() const { return m_dataPath; }
    QString getProfilePath() const { return m_profilePath; }
    QString getBackupPath() const { return m_backupPath; }

    // 数据迁移和备份
    void migrateOldData();                // 迁移老版本数据
    void createDailyBackup();             // 创建每日备份
    void cleanOldBackups(int keepDays=30); // 清理超过指定天数的备份

private:
    void ensureDataDir();

    QString m_dataPath;      // 数据目录 (exe目录/data)
    QString m_profilePath;   // 浏览器配置目录 (exe目录/userdata/default)
    QString m_backupPath;    // 备份目录 (exe目录/backups)
};

#endif // DATASTORAGE_H
