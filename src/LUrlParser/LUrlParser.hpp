/*
 * Lightweight URL & URI parser (RFC 1738, RFC 3986)
 * https://github.com/corporateshark/LUrlParser
 * 
 * The MIT License (MIT)
 * 
 * Copyright (C) 2015 Sergey Kosarevsky (sk@linderdaum.com)
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#pragma once

#include <string>

#include "../capi_sass.hpp"

namespace LUrlParser
{

  // LibSass change: made enum scoped
  enum class LUrlParserError
  {
    Ok = 0,
    Uninitialized = 1,
    NoUrlCharacter = 2,
    InvalidSchemeName = 3,
    NoDoubleSlash = 4,
    NoAtSign = 5,
    UnexpectedEndOfLine = 6,
    NoSlash = 7,
  };

	class clParseURL
	{
	public:
		LUrlParserError m_ErrorCode;
    sass::string m_Scheme;
    sass::string m_Host;
    sass::string m_Port;
    sass::string m_Path;
    sass::string m_Query;
    sass::string m_Fragment;
    sass::string m_UserName;
    sass::string m_Password;

		clParseURL()
			: m_ErrorCode(LUrlParserError::Uninitialized )
		{}

		/// return 'true' if the parsing was successful
		bool IsValid() const { return m_ErrorCode == LUrlParserError::Ok; }

		/// helper to convert the port number to int, return 'true' if the port is valid (within the 0..65535 range)
		bool GetPort( int* OutPort ) const;

		/// parse the URL
		static clParseURL ParseURL( const sass::string& URL );

	private:
		explicit clParseURL( LUrlParserError ErrorCode )
			: m_ErrorCode( ErrorCode )
		{}
	};

} // namespace LUrlParser
