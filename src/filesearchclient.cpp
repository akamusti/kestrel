#include "filesearchclient.h"

#include <QDir>
#include <QFileInfo>
#include <QProcess>
#include <QStandardPaths>

namespace kestrel {

FileSearchClient::FileSearchClient(QObject *parent)
    : QObject(parent)
{
}

void FileSearchClient::search(const QString &query)
{
    m_query = query.trimmed();
    if (m_process) {
        m_process->kill();
        m_process->deleteLater();
        m_process = nullptr;
    }

    if (m_query.isEmpty()) {
        emit resultsReady(m_query, {});
        return;
    }

    start(m_query);
}

void FileSearchClient::start(const QString &query)
{
    const QString home = QDir::homePath();
    m_process = new QProcess(this);

    connect(m_process, &QProcess::finished, this, [this, query](int, QProcess::ExitStatus) {
        QStringList paths;
        if (m_process) {
            const QString output = QString::fromUtf8(m_process->readAllStandardOutput());
            for (const QString &line : output.split(u'\n', Qt::SkipEmptyParts)) {
                const QString trimmed = line.trimmed();
                if (!trimmed.isEmpty() && QFileInfo::exists(trimmed)) {
                    paths.append(trimmed);
                    if (paths.size() >= 25) {
                        break;
                    }
                }
            }
            m_process->deleteLater();
            m_process = nullptr;
        }
        emit resultsReady(query, paths);
    });

    const QString fd = QStandardPaths::findExecutable(QStringLiteral("fd"));
    if (!fd.isEmpty()) {
        m_process->start(fd, {QStringLiteral("-F"), QStringLiteral("-i"),
                              QStringLiteral("-t"), QStringLiteral("f"),
                              QStringLiteral("--max-results"), QStringLiteral("25"),
                              QStringLiteral("--maxdepth"), QStringLiteral("6"),
                              query, home});
        return;
    }

    const QString find = QStandardPaths::findExecutable(QStringLiteral("find"));
    if (!find.isEmpty()) {
        m_process->start(find, {home, QStringLiteral("-maxdepth"), QStringLiteral("6"),
                                QStringLiteral("-type"), QStringLiteral("f"),
                                QStringLiteral("-iname"),
                                QStringLiteral("*") + query + QStringLiteral("*")});
        return;
    }

    m_process->deleteLater();
    m_process = nullptr;
    emit resultsReady(query, {});
}

} // namespace kestrel
