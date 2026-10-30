#pragma once

#include <QQuickImageProvider>

namespace kestrel {

/// Resolves `image://icon/<theme-name>` (or `image://icon/<absolute-path>`) via the icon theme.
/// Missing icons resolve to a drawn placeholder instead of a failed image.
class IconImageProvider : public QQuickImageProvider
{
public:
    IconImageProvider();

    QPixmap requestPixmap(const QString &id, QSize *size, const QSize &requestedSize) override;
};

} // namespace kestrel
