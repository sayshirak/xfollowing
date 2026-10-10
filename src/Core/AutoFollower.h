#ifndef AUTOFOLLOWER_H
#define AUTOFOLLOWER_H

#include <QObject>
#include <QString>

class AutoFollower : public QObject {
    Q_OBJECT

public:
    explicit AutoFollower(QObject* parent = nullptr);

    // 获取关注脚本（含语言过滤 + 粉丝/关注数比值过滤）
    QString getFollowScript(const QString& selectedLang, double maxFollowerRatio = 1.5);

    // 获取回关检查脚本（检查对方是否关注我）
    QString getCheckFollowBackScript();

    // 获取取消关注脚本
    QString getUnfollowScript();

    // 获取导出关注列表脚本：定位当前账号的 /following 页面，滚动采集全部用户
    QString getExportFollowingScript();
};

#endif // AUTOFOLLOWER_H
