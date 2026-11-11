#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVector>

namespace kestrel {

/// In-memory clipboard history and snippets, monitored from QClipboard.
class ClipboardStore : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QStringList items READ items NOTIFY itemsChanged)

public:
    explicit ClipboardStore(QObject *parent = nullptr);

    QStringList items() const { return m_history; }
    QStringList snippets() const;

    Q_INVOKABLE void addSnippet(const QString &name, const QString &text);

signals:
    void itemsChanged();

private slots:
    void onClipboardChanged();

private:
    void push(const QString &text);

    QStringList m_history;
    struct Snippet {
        QString name;
        QString text;
    };
    QVector<Snippet> m_snippets;
};

} // namespace kestrel
