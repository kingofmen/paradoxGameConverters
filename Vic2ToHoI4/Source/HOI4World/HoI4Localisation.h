/*Copyright (c) 2016 The Paradox Game Converters Project

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



#ifndef HoI4LOCALISATION_H_
#define HoI4LOCALISATION_H_



#include <array>
#include <iostream>
#include <string>
#include <vector>



class V2Country;



// Holds translations for attributes of a specific HoI4 country.
class HoI4Localisation
{
	public:
		// Sets the tag to use for creating the name and adjective key to use in the localisation output.
		void SetTag(const std::wstring& tag);
		// Populates the localised names and adjectives using information from the V2 country.
		void ReadFromCountry(const V2Country&);

		// Sets the key to use for the specified party in the localisation output.
		void SetPartyKey(size_t partyIndex, const std::wstring& partyKey);
		// Sets the localised party name for the specified party in the given language, e.g. "english".
		void SetPartyName(size_t partyIndex, const std::wstring& language, const std::wstring& name);

		// Writes a HoI4-formatted localisation info for all localised elements as:
		// key;translation0;translation1;...;;;x
		void WriteToStream(std::wostream&) const;

	private:
		static const size_t numLanguages = 14;
		static const std::array<std::wstring, numLanguages> languages;

		typedef std::array<std::wstring, numLanguages> Localisations;

		std::wstring tag;
		Localisations name;
		Localisations adjective;

		struct Party
		{
			std::wstring key;
			Localisations name;
		};
		std::vector<Party> parties;
};

#endif