#pragma once

#include <stddef.h>
#include <stdint.h>

/// Wire framing shared with the desktop application: COBS with a CRC-16, delimited by 0x00.
///
/// A frame on the wire is COBS(body ++ crc16(body)) ++ 0x00, where the body is the message code
/// followed by its payload and the CRC is big-endian.
///
/// COBS removes every 0x00 from the encoded bytes, so the delimiter never appears inside a frame
/// and payload bytes may take the full 0-255 range. The CRC turns a lost or corrupted byte into a
/// rejected frame instead of a different, valid-looking message.
///
/// This has no Arduino dependency so the same code runs in the native unit tests, against the
/// same byte vectors the application tests use.
namespace protocol
{
    /// Ends every frame. COBS guarantees it appears nowhere else.
    const uint8_t FRAME_DELIMITER = 0x00;

    /// Largest body (code + payload) this firmware sends or accepts.
    const size_t MAX_BODY = 24;

    /// Bytes a frame of MAX_BODY can occupy on the wire: CRC, COBS overhead and delimiter.
    const size_t MAX_ENCODED_FRAME = MAX_BODY + 2 + 1 + 1;

    /// CRC-16/CCITT-FALSE: polynomial 0x1021, initial value 0xFFFF, no reflection, no final XOR.
    uint16_t crc16(const uint8_t *data, size_t length);

    /// COBS-encodes `data` into `out`, which must hold length + length / 254 + 1 bytes. Returns
    /// the encoded length. The delimiter is not added.
    size_t cobsEncode(const uint8_t *data, size_t length, uint8_t *out);

    /// Decodes one COBS frame, delimiter excluded. `out` may be `in`: decoding never writes ahead
    /// of what it has read. Returns false when the encoding does not hold together.
    bool cobsDecode(const uint8_t *in, size_t length, uint8_t *out, size_t &outLength);

    /// Builds the wire bytes for a body into `out` (MAX_ENCODED_FRAME bytes), delimiter included.
    /// Returns the number of bytes to send, or 0 if the body is longer than MAX_BODY.
    size_t encodeFrame(const uint8_t *body, size_t length, uint8_t *out);

    /// Decodes a frame in place, delimiter excluded. On success the body occupies the start of
    /// `frame` and its length is in `bodyLength`. Fails on broken COBS, a missing body or a CRC
    /// that does not match.
    bool decodeFrame(uint8_t *frame, size_t length, size_t &bodyLength);
}
