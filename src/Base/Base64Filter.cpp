// SPDX-License-Identifier: LGPL-2.1-or-later
// SPDX-FileCopyrightText: 2019 Zheng Lei <realthunder.dev@gmail.com>
// SPDX-FileNotice: Part of the FreeCAD project.

/******************************************************************************
 *                                                                            *
 *   FreeCAD is free software: you can redistribute it and/or modify          *
 *   it under the terms of the GNU Lesser General Public License as           *
 *   published by the Free Software Foundation, either version 2.1            *
 *   of the License, or (at your option) any later version.                   *
 *                                                                            *
 *   FreeCAD is distributed in the hope that it will be useful,               *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty              *
 *   of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.                  *
 *   See the GNU Lesser General Public License for more details.              *
 *                                                                            *
 *   You should have received a copy of the GNU Lesser General Public         *
 *   License along with FreeCAD. If not, see https://www.gnu.org/licenses     *
 *                                                                            *
 ******************************************************************************/

#include "Base64Filter.h"

#include "Base64.h"


using namespace Base;

// NOLINTBEGIN(cppcoreguidelines-pro-bounds-pointer-arithmetic,
// cppcoreguidelines-pro-bounds-constant-array-index, cppcoreguidelines-avoid-magic-numbers,
// readability-magic-numbers)

Base64EncoderStreambuf::~Base64EncoderStreambuf()
{
    if (pending_size) {
        base64_encode(buffer, pending.data(), pending_size);
    }
    if (!buffer.empty()) {
        dev.write(buffer.c_str(), std::streamsize(buffer.size()));
        if (lineSize) {
            dev.put('\n');
        }
        buffer.clear();
    }
    else if (pos && lineSize) {
        dev.put('\n');
    }
}

std::streamsize Base64EncoderStreambuf::xsputn(const char_type* str, std::streamsize n)
{
    std::streamsize res = n;

    if (pending_size > 0) {
        while (n && pending_size < 3) {
            pending[pending_size] = *str++;
            ++pending_size;
            --n;
        }
        if (pending_size != 3) {
            return res;
        }

        base64_encode(buffer, pending.data(), 3);
    }
    pending_size = n % 3;
    n = n / 3 * 3;
    base64_encode(buffer, str, n);
    str += n;
    for (unsigned i = 0; i < pending_size; ++i) {
        pending[i] = str[i];
    }

    const char* buf = buffer.c_str();
    const char* end = buf + buffer.size();
    if (lineSize && buffer.size() >= lineSize - pos) {
        dev.write(buf, std::streamsize(lineSize - pos));
        dev.put('\n');
        buf += lineSize - pos;
        pos = 0;
        for (; end - buf >= (int)lineSize; buf += lineSize) {
            dev.write(buf, std::streamsize(lineSize));
            dev.put('\n');
        }
    }
    pos += end - buf;
    dev.write(buf, end - buf);
    buffer.clear();
    return n;
}

std::streamsize Base64DecoderStreambuf::xsgetn(char_type* str, std::streamsize n)
{
    static auto table = base64_decode_table();

    if (!n) {
        return 0;
    }

    std::streamsize count = 0;

    for (;;) {
        while (pending_out < out_count) {
            *str++ = char_array_3[pending_out++];
            ++count;
            if (--n == 0) {
                return count;
            }
        }

        if (eof) {
            return count ? count : -1;
        }

        for (;;) {
            int newChar = dev.get();
            if (newChar < 0) {
                eof = true;
                if (pending_in <= 1) {
                    if (pending_in == 1 && errHandling == Base64ErrorHandling::throws) {
                        throw std::ios_base::failure("Unexpected ending of base64 string");
                    }
                    return count ? count : -1;
                }
                out_count = pending_in - 1;
                pending_in = 4;
            }
            else {
                signed char decodedChar = table[newChar];
                if (decodedChar < 0) {
                    if (decodedChar == -2 || errHandling == Base64ErrorHandling::silent) {
                        continue;
                    }
                    throw std::ios_base::failure("Invalid character in base64 string");
                }
                char_array_4[pending_in++] = (char)decodedChar;
            }
            if (pending_in == 4) {
                pending_out = pending_in = 0;
                char_array_3[0] = static_cast<char>(
                    (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4)
                );
                char_array_3[1] = static_cast<char>(
                    ((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2)
                );
                char_array_3[2] = static_cast<char>(((char_array_4[2] & 0x3) << 6) + char_array_4[3]);
                break;
            }
        }
    }
}

// NOLINTEND(cppcoreguidelines-pro-bounds-pointer-arithmetic,
// cppcoreguidelines-pro-bounds-constant-array-index, cppcoreguidelines-avoid-magic-numbers,
// readability-magic-numbers)
