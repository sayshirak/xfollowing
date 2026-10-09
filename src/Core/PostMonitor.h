#ifndef POSTMONITOR_H
#define POSTMONITOR_H

#include <QObject>
#include <QString>
#include <QList>
#include "Data/Keyword.h"

class PostMonitor : public QObject {
    Q_OBJECT

public:
    explicit PostMonitor(QObject* parent = nullptr);

    // 获取监控脚本（含语言过滤）
    QString getMonitorScript(const QList<Keyword>& keywords, const QString& selectedLang);

    // 获取粉丝页面监控脚本（含语言过滤）
    QString getFollowersMonitorScript(const QString& selectedLang);

private:
    QString buildKeywordsArray(const QList<Keyword>& keywords);
};

#endif // POSTMONITOR_H
