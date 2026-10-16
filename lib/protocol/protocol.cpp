#include "protocol.h"

namespace protocol
{
    uint16_t crc16(const uint8_t *data, size_t length)
    {
        uint16_t crc = 0xFFFF;
        for (size_t i = 0; i < length; i++)
        {
            crc ^= (uint16_t)data[i] << 8;
            for (uint8_t bit = 0; bit < 8; bit++)
            {
                crc = (crc & 0x8000) ? (uint16_t)((crc << 1) ^ 0x1021) : (uint16_t)(crc << 1);
            }
        }
        return crc;
    }

    size_t cobsEncode(const uint8_t *data, size_t length, uint8_t *out)
    {
        // Index of the code byte for the block being built, patched once the block ends.
        size_t codeIndex = 0;
        size_t write = 1;
        uint8_t code = 1;

        for (size_t i = 0; i < length; i++)
        {
            if (data[i] == 0)
            {
                out[codeIndex] = code;
                codeIndex = write++;
                code = 1;
                continue;
            }
            out[write++] = data[i];
            code++;
            if (code == 0xFF)
            {
                // A full block of 254 non-zero bytes ends without an implied zero.
                out[codeIndex] = code;
                codeIndex = write++;
                code = 1;
            }
        }
        out[codeIndex] = code;
        return write;
    }

    bool cobsDecode(const uint8_t *in, size_t length, uint8_t *out, size_t &outLength)
    {
        size_t read = 0;
        size_t write = 0;

        while (read < length)
        {
            const uint8_t code = in[read];
            if (code == 0)
            {
                return false;
            }
            const size_t blockEnd = read + code;
            if (blockEnd > length)
            {
                // The block claims more bytes than the frame holds: something was lost.
                return false;
            }
            read++;
            while (read < blockEnd)
            {
                out[write++] = in[read++];
            }
            // Every block but a full one implies a zero, except at the very end of the frame.
            if (code != 0xFF && read < length)
            {
                out[write++] = 0;
            }
        }
        outLength = write;
        return true;
    }

    size_t encodeFrame(const uint8_t *body, size_t length, uint8_t *out)
    {
        if (length > MAX_BODY)
        {
            return 0;
        }

        uint8_t checked[MAX_BODY + 2];
        for (size_t i = 0; i < length; i++)
        {
            checked[i] = body[i];
        }
        const uint16_t crc = crc16(body, length);
        checked[length] = crc >> 8;
        checked[length + 1] = crc & 0xFF;

        const size_t encoded = cobsEncode(checked, length + 2, out);
        out[encoded] = FRAME_DELIMITER;
        return encoded + 1;
    }

    bool decodeFrame(uint8_t *frame, size_t length, size_t &bodyLength)
    {
        size_t decoded = 0;
        if (!cobsDecode(frame, length, frame, decoded) || decoded <= 2)
        {
            // Broken encoding, or nothing but a CRC: a body needs at least its message code.
            return false;
        }
        bodyLength = decoded - 2;
        const uint16_t received = ((uint16_t)frame[bodyLength] << 8) | frame[bodyLength + 1];
        return crc16(frame, bodyLength) == received;
    }
}
