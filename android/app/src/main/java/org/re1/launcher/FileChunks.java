package org.re1.launcher;

import java.io.EOFException;
import java.io.IOException;
import java.io.InputStream;

/** Bounded, exact reads shared by the Android bridge and HTTP fallback. */
final class FileChunks {
    static final int MAX_LENGTH = 256 * 1024;

    static void validate(long size, long offset, int length) throws IOException {
        if (size < 0 || offset < 0 || length <= 0 || length > MAX_LENGTH || offset > size - length)
            throw new IOException("Invalid file range: offset=" + offset + " length=" + length + " size=" + size);
    }

    // The caller owns the stream. Providers may implement skip partially or
    // not at all, and reads need not fill the requested buffer in one call.
    static byte[] read(InputStream source, long size, long offset, int length) throws IOException {
        validate(size, offset, length);
        byte[] discard = null;
        long left = offset;
        while (left > 0) {
            long skipped = source.skip(left);
            if (skipped > 0) { left -= skipped; continue; }
            if (discard == null) discard = new byte[8192];
            int count = source.read(discard, 0, (int)Math.min(left, discard.length));
            if (count < 0) throw new EOFException("File ends before offset " + offset);
            if (count == 0) {
                if (source.read() < 0) throw new EOFException("File ends before offset " + offset);
                count = 1;
            }
            left -= count;
        }
        byte[] bytes = new byte[length];
        int done = 0;
        while (done < length) {
            int count = source.read(bytes, done, length - done);
            if (count < 0) throw new EOFException("Short file: offset=" + offset + " expected=" + length + " got=" + done);
            if (count == 0) {
                int value = source.read();
                if (value < 0) throw new EOFException("Short file: offset=" + offset + " expected=" + length + " got=" + done);
                bytes[done++] = (byte)value;
            } else done += count;
        }
        return bytes;
    }
}
