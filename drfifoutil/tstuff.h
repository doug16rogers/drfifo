// Copyright (c) 2013-2019 Doug Rogers under the Zero Clause BSD License.
// You are free to do whatever you want with this software. See LICENSE.txt.

#ifndef __tstuff_h__
#define __tstuff_h__

#include <iostream>
#include <string>

#ifdef UNICODE

#define M_T(_s)  L ## _s
#define tstring  wstring
#define tchar_t  wchar_t
#define tcin     wcin
#define tcout    wcout
#define tcerr    wcerr
#define tclog    wclog
typedef wchar_t char_t;

inline std::string to_tstring(const std::tstring& tstr)
{
	std::string result;

	for (std::tstring::const_iterator it = tstr.begin(); it < tstr.end(); ++it)
	{
		result.push_back((char) *it);
	}

	return result;
}

#else

#define M_T(_s)  _s
#define tstring  string
#define tchar_t  char
#define tcin     cin
#define tcout    cout
#define tcerr    cerr
#define tclog    clog
#define to_string(_tstr)  (_tstr)

#endif

#endif
