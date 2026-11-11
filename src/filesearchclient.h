#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QProcess>

namespace kestrel {

class FileSearchClient : public QObject
{
    Q_OBJECT

public:
    explicit FileSearchClient(QObject *parent = nullptr);

    void search(const QString &query);

signals:
    void resultsReady(const QString &query, const QStringList &paths);

private:
    void start(const QString &query);

    QString m_query;
    QProcess *m_process = nullptr;
};

} // namespace kestrel
