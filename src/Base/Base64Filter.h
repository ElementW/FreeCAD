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

#pragma once

#include <array>
#include <cstdint>
#include <istream>
#include <ostream>
#include <streambuf>

namespace Base
{

enum class Base64ErrorHandling
{
    throws,
    silent
};
static constexpr int base64DefaultBufferSize {80};

/** A base64 encoder that can be used as a boost iostream filter
 */
class Base64EncoderStreambuf: public std::streambuf
{
public:
    /** Constructor
     * @param dev Underlying ostream to write to.
     * @param lineSize line size for the output base64 string, 0 to
     * disable segmentation.
     */
    explicit Base64EncoderStreambuf(std::ostream& dev, std::size_t lineSize)
        : dev(dev)
        , lineSize(lineSize)
    {}

    ~Base64EncoderStreambuf() override;

protected:
    std::streambuf* setbuf(char*, std::streamsize) override
    {
        return this;
    }
    pos_type seekoff(
        off_type,
        std::ios_base::seekdir,
        std::ios_base::openmode = std::ios_base::in | std::ios_base::out
    ) override
    {
        return pos_type(off_type(-1));
    }
    pos_type seekpos(pos_type, std::ios_base::openmode = std::ios_base::in | std::ios_base::out) override
    {
        return pos_type(off_type(-1));
    }
    std::streamsize xsputn(const char_type* str, std::streamsize n) override;

private:
    std::ostream& dev;
    std::size_t lineSize;
    std::size_t pos = 0;
    std::size_t pending_size = 0;
    std::array<unsigned char, 3> pending {};
    std::string buffer;
};

class Base64EncoderStream: public std::ostream
{
public:
    explicit Base64EncoderStream(std::ostream& dev, std::size_t lineSize)
        : std::ostream(&streambuf)
        , streambuf(dev, lineSize)
    {}

private:
    Base64EncoderStreambuf streambuf;
};

/** A base64 decoder that can be used as a boost iostream filter
 */
class Base64DecoderStreambuf: public std::streambuf
{
public:
    /** Constructor
     * @param dev Underlying istream to read from.
     * @param errHandling Whether to throw on invalid non white space character.
     */
    Base64DecoderStreambuf(std::istream& dev, Base64ErrorHandling errHandling)
        : dev(dev)
        , errHandling(errHandling)
    {}

protected:
    std::streamsize xsgetn(char_type* str, std::streamsize n) override;

private:
    std::istream& dev;
    std::uint8_t pending_in = 0;
    std::array<char, 4> char_array_4 {};
    std::uint8_t pending_out = 3;
    std::uint8_t out_count = 3;
    std::array<char, 3> char_array_3 {};
    Base64ErrorHandling errHandling;
    bool eof = false;
};

class Base64DecoderStream: public std::istream
{
public:
    explicit Base64DecoderStream(std::istream& dev, Base64ErrorHandling errHandling)
        : std::istream(&streambuf)
        , streambuf(dev, errHandling)
    {}

private:
    Base64DecoderStreambuf streambuf;
};

}  // namespace Base
