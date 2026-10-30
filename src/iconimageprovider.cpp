#include "iconimageprovider.h"

#include <QFileInfo>
#include <QIcon>
#include <QPainter>
#include <QPixmap>

namespace kestrel {
namespace {

constexpr int kDefaultSize = 64;

/// Neutral stand-in for icons the theme cannot resolve. Drawn rather than bundled so it
/// always matches the requested size and never fails.
QPixmap placeholderPixmap(int width, int height)
{
    QPixmap pixmap(width, height);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(255, 255, 255, 34));

    const qreal side = std::min(width, height);
    const qreal radius = side * 0.22;
    painter.drawRoundedRect(QRectF(0, 0, side, side), radius, radius);
    return pixmap;
}

} // namespace

IconImageProvider::IconImageProvider()
    : QQuickImageProvider(QQuickImageProvider::Pixmap)
{
}

QPixmap IconImageProvider::requestPixmap(const QString &id, QSize *size, const QSize &requestedSize)
{
    const int width = requestedSize.width() > 0 ? requestedSize.width() : kDefaultSize;
    const int height = requestedSize.height() > 0 ? requestedSize.height() : kDefaultSize;

    QPixmap pixmap;
    if (id.startsWith(u'/') && QFileInfo::exists(id)) {
        pixmap = QPixmap(id);
    } else if (!id.isEmpty()) {
        pixmap = QIcon::fromTheme(id).pixmap(width, height);
    }
    if (pixmap.isNull()) {
        pixmap = placeholderPixmap(width, height);
    }

    if (size) {
        *size = pixmap.size();
    }
    return pixmap;
}

} // namespace kestrel
