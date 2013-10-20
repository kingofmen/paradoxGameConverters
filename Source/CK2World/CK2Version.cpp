/*Copyright (c) 2013 The CK2 to EU3 Converter Project
 
 Permission is hereby granted, free of charge, to any person obtaining
 a copy of this software and associated documentation files (the
 "Software"), to deal in the Software without restriction, including
 without limitation the rights to use, copy, modify, merge, publish,
 distribute, sublicense, and/or sell copies of the Software, and to
 permit persons to whom the Software is furnished to do so, subject to
 the following conditions:
 
 The above copyright notice and this permission notice shall be included
 in all copies or substantial portions of the Software.
 
 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.*/



#include "CK2Version.h"
#include "..\Parsers\Object.h"


CK2Version::CK2Version(string versionString)
{
	int vPos			= versionString.find_first_of('v');
	int periodPos	= versionString.find_first_of('.');

	if (vPos != string::npos)
	{
		vPos++;
		major = atoi( versionString.substr(vPos, periodPos - vPos).c_str() );
	
		periodPos++;
		minor = atoi( versionString.substr(periodPos, 2).c_str() );
		if (versionString.size() > (unsigned int)(periodPos + 3))
		{
			revision = atoi( versionString.substr(periodPos + 3, 1).c_str() );
		}
		else
		{
			revision = 0;
		}
	}
	else
	{
		major = atoi( versionString.substr(0, periodPos).c_str() );
	
		periodPos++;
		minor = atoi( versionString.substr(periodPos, 2).c_str() );
		if (versionString.size() > (unsigned int)(periodPos + 3))
		{
			revision = atoi( versionString.substr(periodPos + 3, 1).c_str() );
		}
		else
		{
			revision = 0;
		}
	}
}