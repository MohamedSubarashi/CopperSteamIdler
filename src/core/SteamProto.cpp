#include "SteamProto.h"

namespace proto {

void Writer::writeVarint(quint64 value)
{
    while (value >= 0x80) {
        m_out.append(static_cast<char>((value & 0x7F) | 0x80));
        value >>= 7;
    }
    m_out.append(static_cast<char>(value));
}

void Writer::writeTag(quint32 field, WireType type)
{
    writeVarint((static_cast<quint64>(field) << 3) | static_cast<quint32>(type));
}

void Writer::varintField(quint32 field, quint64 value)
{
    writeTag(field, Varint);
    writeVarint(value);
}

void Writer::int32Field(quint32 field, qint32 value)
{
    writeTag(field, Varint);
    // Negative int32 values are sign-extended to 64 bits on the wire.
    if (value < 0)
        writeVarint(static_cast<quint64>(static_cast<qint64>(value)));
    else
        writeVarint(static_cast<quint64>(value));
}

void Writer::boolField(quint32 field, bool value)
{
    writeTag(field, Varint);
    writeVarint(value ? 1 : 0);
}

void Writer::fixed32Field(quint32 field, quint32 value)
{
    writeTag(field, Fixed32);
    for (int i = 0; i < 4; ++i)
        m_out.append(static_cast<char>((value >> (i * 8)) & 0xFF));
}

void Writer::fixed64Field(quint32 field, quint64 value)
{
    writeTag(field, Fixed64);
    for (int i = 0; i < 8; ++i)
        m_out.append(static_cast<char>((value >> (i * 8)) & 0xFF));
}

void Writer::bytesField(quint32 field, const QByteArray& value)
{
    writeTag(field, LengthDelimited);
    writeVarint(static_cast<quint64>(value.size()));
    m_out.append(value);
}

void Writer::stringField(quint32 field, const QString& value)
{
    bytesField(field, value.toUtf8());
}

void Writer::messageField(quint32 field, const Writer& value)
{
    bytesField(field, value.data());
}

void Writer::rawMessageField(quint32 field, const QByteArray& encoded)
{
    bytesField(field, encoded);
}

bool Reader::nextTag(quint32* field, quint32* wireType)
{
    if (atEnd())
        return false;

    quint64 tag = 0;
    if (!readVarint(&tag))
        return false;

    *field = static_cast<quint32>(tag >> 3);
    *wireType = static_cast<quint32>(tag & 0x07);
    return *field != 0;
}

bool Reader::readVarint(quint64* value)
{
    quint64 result = 0;
    int shift = 0;
    while (m_pos < m_data.size()) {
        const quint8 byte = static_cast<quint8>(m_data.at(m_pos++));
        result |= static_cast<quint64>(byte & 0x7F) << shift;
        if ((byte & 0x80) == 0) {
            *value = result;
            return true;
        }
        shift += 7;
        if (shift >= 64)
            return false;
    }
    return false;
}

bool Reader::readFixed32(quint32* value)
{
    if (m_pos + 4 > m_data.size())
        return false;
    quint32 result = 0;
    for (int i = 0; i < 4; ++i)
        result |= static_cast<quint32>(static_cast<quint8>(m_data.at(m_pos + i))) << (i * 8);
    m_pos += 4;
    *value = result;
    return true;
}

bool Reader::readFixed64(quint64* value)
{
    if (m_pos + 8 > m_data.size())
        return false;
    quint64 result = 0;
    for (int i = 0; i < 8; ++i)
        result |= static_cast<quint64>(static_cast<quint8>(m_data.at(m_pos + i))) << (i * 8);
    m_pos += 8;
    *value = result;
    return true;
}

bool Reader::readBytes(QByteArray* value)
{
    quint64 length = 0;
    if (!readVarint(&length))
        return false;
    if (length > static_cast<quint64>(m_data.size() - m_pos))
        return false;
    *value = m_data.mid(m_pos, static_cast<int>(length));
    m_pos += static_cast<int>(length);
    return true;
}

bool Reader::readString(QString* value)
{
    QByteArray bytes;
    if (!readBytes(&bytes))
        return false;
    *value = QString::fromUtf8(bytes);
    return true;
}

bool Reader::skip(quint32 wireType)
{
    quint64 ignored = 0;
    QByteArray ignoredBytes;
    switch (wireType) {
    case Varint:
        return readVarint(&ignored);
    case Fixed64:
        return readFixed64(&ignored);
    case Fixed32: {
        quint32 ignored32 = 0;
        return readFixed32(&ignored32);
    }
    case LengthDelimited:
        return readBytes(&ignoredBytes);
    default:
        return false;
    }
}

} // namespace proto
