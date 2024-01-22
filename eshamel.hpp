#pragma once

#include <type_traits>
#include <ctype.h>

namespace Eshamel
{
	template< class...Ts > struct Tuple
	{
		constexpr static std::size_t N = sizeof...(Ts);
	};
	template< char...cs >  struct MosesString
	{
		constexpr static const char* c_str()
		{
			const char str[sizeof...(cs) + 1] = { cs..., '\0' };
			return str;
		}
	};

	template< class First, class Second > struct Pair { using first = First; using second = Second; };

	template< class Name, class Str >
	struct Attr : public Pair< Name, Str >
	{
		using _super = Pair<Name, Str>;
		using name = _super::first;
		using value = _super::second;
	}; 

	template< class Name, class...Attrs >
	struct TagInfo // <TagName attr="value" attr2="value2">
	{
		using name = Name;
		using attrs = Tuple< Attrs... >;
	};

	template< class TagInfo, class...Childs >
	struct Tag
	{
		using tag_info = TagInfo;
		using child_tags = Tuple< Childs... >;
	};

	template< class...Ts > struct Document
	{
	};

namespace details	
{
	
} // details

} // Eshamel
