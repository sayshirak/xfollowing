#include "KeywordPanel.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QMessageBox>

KeywordPanel::KeywordPanel(const QString& title, bool protectHuguan, bool enableDoubleClick, QWidget* parent)
    : QWidget(parent)
    , m_title(title)
    , m_protectHuguan(protectHuguan)
    , m_enableDoubleClick(enableDoubleClick) {

    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);

    QGroupBox* groupBox = new QGroupBox(m_title, this);
    QVBoxLayout* layout = new QVBoxLayout(groupBox);

    QHBoxLayout* inputLayout = new QHBoxLayout();
    m_inputEdit = new QLineEdit(groupBox);
    m_inputEdit->setPlaceholderText("输入关键词...");
    m_addBtn = new QPushButton("添加", groupBox);
    inputLayout->addWidget(m_inputEdit);
    inputLayout->addWidget(m_addBtn);
    layout->addLayout(inputLayout);

    m_listWidget = new QListWidget(groupBox);
    m_listWidget->setMaximumHeight(80);
    layout->addWidget(m_listWidget);

    m_deleteBtn = new QPushButton("删除选中", groupBox);
    layout->addWidget(m_deleteBtn);

    mainLayout->addWidget(groupBox);

    connect(m_addBtn, &QPushButton::clicked, this, &KeywordPanel::onAddClicked);
    connect(m_deleteBtn, &QPushButton::clicked, this, &KeywordPanel::onDeleteClicked);
    connect(m_inputEdit, &QLineEdit::returnPressed, this, &KeywordPanel::onAddClicked);
    if (m_enableDoubleClick) {
        connect(m_listWidget, &QListWidget::itemDoubleClicked, this, &KeywordPanel::onItemDoubleClicked);
    }
}

void KeywordPanel::setKeywords(const QList<Keyword>& keywords) {
    m_keywords = keywords;
    updateList();
}

QList<Keyword> KeywordPanel::getKeywords() const {
    return m_keywords;
}

void KeywordPanel::updateList() {
    m_listWidget->clear();
    for (const auto& kw : m_keywords) {
        QListWidgetItem* item = new QListWidgetItem(kw.text);
        item->setData(Qt::UserRole, kw.id);
        if (!kw.isEnabled) {
            item->setForeground(Qt::gray);
        }
        m_listWidget->addItem(item);
    }
}

void KeywordPanel::onAddClicked() {
    QString text = m_inputEdit->text().trimmed();
    if (text.isEmpty()) {
        return;
    }

    for (const auto& kw : m_keywords) {
        if (kw.text.compare(text, Qt::CaseInsensitive) == 0) {
            QMessageBox::warning(this, "提示", "该关键词已存在");
            return;
        }
    }

    Keyword kw(text);
    m_keywords.append(kw);
    updateList();
    m_inputEdit->clear();

    emit keywordsChanged();
}

void KeywordPanel::onDeleteClicked() {
    QListWidgetItem* item = m_listWidget->currentItem();
    if (!item) {
        return;
    }

    QString keyword = item->text();
    if (m_protectHuguan && keyword == "互关") {
        QMessageBox::warning(this, "提示", "默认关键词\"互关\"不能删除");
        return;
    }

    QString id = item->data(Qt::UserRole).toString();
    for (int i = 0; i < m_keywords.size(); ++i) {
        if (m_keywords[i].id == id) {
            m_keywords.removeAt(i);
            break;
        }
    }

    updateList();
    emit keywordsChanged();
}

void KeywordPanel::onItemDoubleClicked(QListWidgetItem* item) {
    QString keyword = item->text();
    if (!keyword.isEmpty()) {
        emit keywordDoubleClicked(keyword);
    }
}
