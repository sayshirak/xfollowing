#ifndef KEYWORDPANEL_H
#define KEYWORDPANEL_H

#include <QWidget>
#include <QListWidget>
#include <QPushButton>
#include <QLineEdit>
#include <QList>
#include "Data/Keyword.h"

class KeywordPanel : public QWidget {
    Q_OBJECT

public:
    // protectHuguan: 禁止删除默认「互关」；enableDoubleClick: 双击跳转搜索
    explicit KeywordPanel(const QString& title = QStringLiteral("关键词"),
                          bool protectHuguan = false,
                          bool enableDoubleClick = false,
                          QWidget* parent = nullptr);

    void setKeywords(const QList<Keyword>& keywords);
    QList<Keyword> getKeywords() const;

signals:
    void keywordsChanged();
    void keywordDoubleClicked(const QString& keyword);

private slots:
    void onAddClicked();
    void onDeleteClicked();
    void onItemDoubleClicked(QListWidgetItem* item);

private:
    void updateList();

    QString m_title;
    bool m_protectHuguan;
    bool m_enableDoubleClick;

    QLineEdit* m_inputEdit;
    QPushButton* m_addBtn;
    QPushButton* m_deleteBtn;
    QListWidget* m_listWidget;

    QList<Keyword> m_keywords;
};

#endif // KEYWORDPANEL_H
