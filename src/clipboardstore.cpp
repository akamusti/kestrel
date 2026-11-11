#include "clipboardstore.h"

#include <QClipboard>
#include <QGuiApplication>

namespace kestrel {

ClipboardStore::ClipboardStore(QObject *parent)
    : QObject(parent)
{
    if (QClipboard *clipboard = QGuiApplication::clipboard()) {
        connect(clipboard, &QClipboard::dataChanged, this, &ClipboardStore::onClipboardChanged);
    }
}

void ClipboardStore::onClipboardChanged()
{
    if (QClipboard *clipboard = QGuiApplication::clipboard()) {
        push(clipboard->text());
    }
}

void ClipboardStore::push(const QString &text)
{
    const QString trimmed = text.trimmed();
    if (trimmed.isEmpty()) {
        return;
    }
    m_history.removeAll(trimmed);
    m_history.prepend(trimmed);
    while (m_history.size() > 50) {
        m_history.removeLast();
    }
    emit itemsChanged();
}

QStringList ClipboardStore::snippets() const
{
    QStringList result;
    for (const Snippet &snippet : m_snippets) {
        result.append(snippet.text);
    }
    return result;
}

void ClipboardStore::addSnippet(const QString &name, const QString &text)
{
    if (text.trimmed().isEmpty()) {
        return;
    }
    m_snippets.append({name.trimmed(), text.trimmed()});
    emit itemsChanged();
}

} // namespace kestrel
