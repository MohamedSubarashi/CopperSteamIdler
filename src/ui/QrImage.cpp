#include "QrImage.h"

#include "../third_party/qrcodegen/qrcodegen.h"

#include <QImage>
#include <QPainter>
#include <QVector>

QPixmap QrImage::pixmap(const QString& text, int modulePixels, int quietModules)
{
    const QByteArray utf8 = text.toUtf8();
    if (utf8.isEmpty() || modulePixels <= 0)
        return QPixmap();

    QVector<uint8_t> qr(qrcodegen_BUFFER_LEN_MAX);
    QVector<uint8_t> temp(qrcodegen_BUFFER_LEN_MAX);
    if (!qrcodegen_encodeText(utf8.constData(), temp.data(), qr.data(),
                              qrcodegen_Ecc_MEDIUM, qrcodegen_VERSION_MIN,
                              qrcodegen_VERSION_MAX, qrcodegen_Mask_AUTO, true)) {
        return QPixmap();
    }

    const int size = qrcodegen_getSize(qr.data());
    const int total = size + quietModules * 2;

    QImage image(total * modulePixels, total * modulePixels, QImage::Format_RGB32);
    image.fill(Qt::white);

    QPainter painter(&image);
    painter.setPen(Qt::NoPen);
    painter.setBrush(Qt::black);
    for (int y = 0; y < size; ++y) {
        for (int x = 0; x < size; ++x) {
            if (qrcodegen_getModule(qr.data(), x, y)) {
                painter.drawRect((x + quietModules) * modulePixels,
                                 (y + quietModules) * modulePixels,
                                 modulePixels, modulePixels);
            }
        }
    }
    painter.end();

    QPixmap pixmap = QPixmap::fromImage(image);
    return pixmap;
}
