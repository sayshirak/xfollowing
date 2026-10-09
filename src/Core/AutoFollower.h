#ifndef AUTOFOLLOWER_H
#define AUTOFOLLOWER_H

#include <QObject>
#include <QString>

class AutoFollower : public QObject {
    Q_OBJECT

public:
    explicit AutoFollower(QObject* parent = nullptr);

    // 获取关注脚本（含语言过滤）
    QString getFollowScript(const QString& selectedLang);

    // 获取回关检查脚本（检查对方是否关注我）
    QString getCheckFollowBackScript();

    // 获取取消关注脚本
    QString getUnfollowScript();
};

#endif // AUTOFOLLOWER_H
