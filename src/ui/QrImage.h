#pragma once

#include <QPixmap>
#include <QString>

// Renders a QR code for the given text using the bundled qrcodegen library
// (src/third_party/qrcodegen, MIT). Returns a null pixmap when the text cannot
// be encoded.
namespace QrImage {

QPixmap pixmap(const QString& text, int modulePixels = 6, int quietModules = 4);

} // namespace QrImage
