// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the tlRender project.

#include <tlRender/Core/URL.h>

#include <iomanip>
#include <regex>
#include <sstream>

namespace tl
{
    namespace
    {
        //! The value of a hexadecimal digit, or -1 if it is not one.
        int fromHex(char c)
        {
            if (c >= '0' && c <= '9') return c - '0';
            if (c >= 'a' && c <= 'f') return c - 'a' + 10;
            if (c >= 'A' && c <= 'F') return c - 'A' + 10;
            return -1;
        }
    }

    std::string getURLScheme(const std::string& url)
    {
        const std::regex rx("^([A-Za-z0-9+-\\.]+://)");
        const auto rxi = std::sregex_iterator(url.begin(), url.end(), rx);
        return rxi != std::sregex_iterator() ? rxi->str() : std::string();
    }

    std::string encodeURL(const std::string& url)
    {
        // Don't encode these characters.
        const std::vector<char> chars =
        {
            '-', '.', '_', '~', ':', '/', '?',  '#',
            '[', ']', '@', '!', '$', '&', '\'', '(',
            ')', '*', '+', ',', ';', '=', '\\'
        };

        // Copy characters to the result, encoding if necessary.
        std::stringstream ss;
        ss.fill('0');
        ss << std::hex;
        for (auto i = url.begin(), end = url.end(); i != end; ++i)
        {
            const auto j = std::find(chars.begin(), chars.end(), *i);
            if (std::isalnum(*i) || j != chars.end())
            {
                ss << *i;
            }
            else
            {
                ss << '%' << std::setw(2) << int(*i);
            }
        }
        return ss.str();
    }

    std::string decodeURL(const std::string& url)
    {
        // Decoded by hand rather than with an expression. This is called for
        // every media reference of every clip each time a timeline is opened,
        // and the expression was compiled on each call: a bundle of a hundred
        // clips paid for two hundred compilations, which cost more than the
        // decoding did.
        std::string out;
        out.reserve(url.size());
        const size_t size = url.size();
        for (size_t i = 0; i < size; ++i)
        {
            // Only a complete escape is an escape. A stray percent, or one
            // with fewer than two hexadecimal digits after it, is a character
            // like any other, which is what the expression did.
            int hi = -1;
            int lo = -1;
            if ('%' == url[i] &&
                (i + 2) < size &&
                (hi = fromHex(url[i + 1])) >= 0 &&
                (lo = fromHex(url[i + 2])) >= 0)
            {
                out.push_back(static_cast<char>(hi * 16 + lo));
                i += 2;
            }
            else
            {
                out.push_back(url[i]);
            }
        }
        return out;
    }
}