#pragma once

#include <QByteArray>
#include <QString>
#include <QtGlobal>

// Minimal Protocol Buffers wire-format support.
//
// The standalone idling backend only needs a dozen hand-written Steam client
// messages, so pulling in protoc and the protobuf runtime would be far more
// machinery than the job needs. This implements just the wire format:
// varints, fixed32/fixed64 and length-delimited fields.
//
// Reference: https://protobuf.dev/programming-guides/encoding/
namespace proto {

enum WireType : quint32 {
    Varint = 0,
    Fixed64 = 1,
    LengthDelimited = 2,
    Fixed32 = 5,
};

class Writer
{
public:
    QByteArray data() const { return m_out; }
    bool isEmpty() const { return m_out.isEmpty(); }

    void varintField(quint32 field, quint64 value);
    void int32Field(quint32 field, qint32 value);
    void boolField(quint32 field, bool value);
    void fixed32Field(quint32 field, quint32 value);
    void fixed64Field(quint32 field, quint64 value);
    void bytesField(quint32 field, const QByteArray& value);
    void stringField(quint32 field, const QString& value);
    void messageField(quint32 field, const Writer& value);
    void rawMessageField(quint32 field, const QByteArray& encoded);

private:
    void writeTag(quint32 field, WireType type);
    void writeVarint(quint64 value);

    QByteArray m_out;
};

class Reader
{
public:
    explicit Reader(const QByteArray& data) : m_data(data) {}

    bool atEnd() const { return m_pos >= m_data.size(); }
    int position() const { return m_pos; }

    // Reads the next tag. Returns false at end of buffer or on malformed input.
    bool nextTag(quint32* field, quint32* wireType);

    bool readVarint(quint64* value);
    bool readFixed32(quint32* value);
    bool readFixed64(quint64* value);
    bool readBytes(QByteArray* value);
    bool readString(QString* value);

    bool skip(quint32 wireType);

private:
    const QByteArray& m_data;
    int m_pos = 0;
};

} // namespace proto
