#include <cstring>
#include <ios>
#include <iostream>
#include <string>
#include <type_traits>
#include <variant>
#include <strstream>
#include <concepts>

#include <boost/static_string.hpp>
#include <boost/fusion/support/pair.hpp>
#include <boost/fusion/sequence.hpp>
#include <boost/fusion/container.hpp>
#include <boost/xpressive/xpressive.hpp>
#include <boost/xpressive/regex_algorithms.hpp>
#include <boost/xpressive/regex_primitives.hpp>
#include <boost/xpressive/xpressive_fwd.hpp>
#include <boost/xpressive/regex_actions.hpp>
#include <boost/format.hpp>
#include <boost/lexical_cast.hpp>

#include "eshamel.hpp"

using namespace boost;
using namespace boost::xpressive;

template< class...T > struct Tuple
{
	constexpr static const size_t N = sizeof...(T);
};

template< class Sym, class ValT >
struct Attribute : public fusion::pair< Sym, ValT > { };

template< class Sym, class ValT >
struct OptionalAttribute : public Attribute< Sym, ValT > { };

// a macro that generates metafunctions of the form HasX<T> which computes whether type T has an inner type named "X".
#define HAS_X(inner) \
template< class X, class E = void > struct _Has_##inner : false_type { };\
template< class X > struct _Has_##inner< X, void_t<typename X::inner> > : true_type { };\
template< class X > struct _Has_##inner< X, void_t<decltype(X::inner)> > : true_type { }; \
template< class X > using Has_##inner##T = typename _Has_##inner<X>::type;\
template< class X > using Has_##inner##V = typename _Has_##inner<X>::value;

HAS_X(Attrs);
HAS_X(Childs);
HAS_X(ChildrenAreOrdered);
HAS_X(AttributesAreOrdered);
HAS_X(as_str);

template< class N > concept IsNode = Has_as_strT< N >::value;
////////////////////////////////////////////////////

#define LET_THERE_BE_FIELD(ident) public: struct ident { constexpr static const std::string_view as_str = #ident; }; 

#define START_ATTRS() using Attrs = boost::fusion::map<
#define ATTR(ty, ident) Attribute< ident, ty > 
#define OPT_ATTR(ty, ident) Attribute< ident, std::optional< ty > > 
#define END_ATTRS() > ; Attrs attrs; \
public: template< class Field > auto& attr(Field){ return boost::fusion::at_key<Field>(attrs); };

#define START_CHILDS() using Childs = boost::fusion::vector<
#define CHILD(ident) ident
#define OPT_CHILD(ident) std::optional< ident >
#define END_CHILDS() > ; Childs childs; \
public: template< class Child > auto& child(Child) { return boost::fusion::at_key<Child>(childs); }


#define START_XML_STRUCT(name) struct name {\
	constexpr static const std::string_view as_str = #name;
#define END_XML_STRUCT() };

#define _META_ATTR_ORDERED_CHILDREN() using ChildrenAreOrdered = void ;
#define _META_ATTR_ORDERED_ATTRS() using AttributesAreOrdered = void ;

template< class T > struct IsOptional : false_type { };
template< class T > struct IsOptional< std::optional<T> > : true_type { using inner = T; };

struct Marshal
{
public:
	template< class S >
	static std::string marshal(S const& s)
	{
		std::stringstream ss;
		marsh_node_impl(ss, s);
		return ss.str();
	}

private:

	template< class S >
	static void marsh_node_impl(std::stringstream& ss, S const& s)
		requires (!IsOptional<S>::value)
	{
		static_assert(Has_as_strT<S>::value, "This might not be a node since it lacks `as_str`.");
		ss << "<" << S::as_str ;
		if constexpr (Has_AttrsT<S>::value)
		{
			boost::fusion::for_each(s.attrs,
				[&]< class A >(A const& attr) {
					ss << " " << A::first_type::as_str
					   << "=" << "\"" << attr.second << "\"";
				});
		}
		ss << ">";
		if constexpr (Has_ChildsT<S>::value)
		{
			boost::fusion::for_each(s.childs,
				[&]< class C >(C const& child){ Marshal::marsh_node_impl< C >(ss, child); });
		}
		ss << "</" << S::as_str << ">" ;
	}

	template< class S >
	static void marsh_node_impl(std::stringstream& ss, S const& s)
		requires (IsOptional<S>::value)
	{
		static_assert(Has_as_strT<typename S::value_type>::value, "This might not be a node since it lacks `as_str`.");
		if (s.has_value())
		{
			return marsh_node_impl(ss, s.value());
		}
	}
};

template< class T, class E = void > struct TypeMap{ using type = T; };
template< class T > struct TypeMap< T, std::enable_if_t<std::is_pointer_v<T>> >
{
	using type = int64_t;
	static_assert(sizeof(type) == sizeof(void*), "!!");
};

template< class S >
struct Unmarshal
{
public:
	static S unmarshal(std::string_view sv)
	{
		S s;
		unmarshal_node_impl(s, sv);
		return s;
	}

private:

	static void unmarshal_node_impl(S& s, std::string_view& sv)
	{
		std::cout << "F: " << sv << std::endl;
		mark_tag node_ident(1), attr_assignments(2);
		cregex node_start_tag = as_xpr('<')
			>> (node_ident= (alpha >> *set[ alnum | '-' | '.' ])) ;
	    cregex node_end_tag = as_xpr("</") >> S::as_str >> '>';

		cmatch what;
		if (regex_search( sv, what, node_start_tag ))
		{
			// std::cout << what[node_ident] << "%@%%" << S::as_str << std::endl;
			assert(0 == what[node_ident].compare(S::as_str.begin()));
			auto attrs_str = std::string_view( what[node_ident].second, sv.end() );
			unmarshal_attrs_impl(s.attrs, attrs_str);
			// assume matching '>' is present
			attrs_str = attrs_str.substr(1 + attrs_str.find('>'));
			assert(attrs_str.begin() <= sv.end());
			sv = std::string_view(attrs_str.begin(), sv.end());

			fusion::for_each(s.childs, [&]< class C >(C& child) {
					child = Unmarshal< C >::unmarshal(sv);
				});

			cmatch node_end_match;
			if (regex_search( sv, node_end_match, node_end_tag ))
			{
				sv = std::string_view( node_end_match[0].first, node_end_match[0].second );
				std::cout << "End: " << sv << std::endl;
			}
		}
		else return ; 
	}

	template< class T > static T _StrToType(std::string const& s)
	{
		if constexpr (std::is_pointer_v<T>)
		{
			return std::bit_cast<T>(std::stoull(s, nullptr, 16));
		}
		else {
			return boost::lexical_cast<T>(s);
		}
	}

	template< class Vec >
	static void unmarshal_attrs_impl(Vec& attrs, std::string_view& sv)
		requires (Has_AttrsT< S >::value && Has_AttributesAreOrderedT< S >::value) 
	{
		// std::cout << "attr: " << sv << std::endl;
		mark_tag attr_ident(1), attr_value(2);
		cregex attr_assign_re 
			= (attr_ident= (alpha >> *set[ alnum | '-' | '.' ]))
			>> '='
			>> '\"' >> (attr_value= *~(set= '\"')) >> '\"';

		cregex_iterator cur( sv.begin(), sv.end(), attr_assign_re ), end;
		fusion::for_each(attrs, [&]< class A >(A& attr) {
				if ( cur != end )
				{
					std::cout << "\t1attr: " << (*cur)[0] << std::endl;
					auto ident = (*cur)[attr_ident];
					if (0 == ident.compare(A::first_type::as_str.data()))
						attr.second = _StrToType<typename A::second_type>(
							(*cur)[attr_value]);
					else
						if constexpr (!IsOptional< typename A::second_type >::value)
						{
							assert(false); // TODO: throw exception or sum'.
						}
						else
						{
							attr.second = std::nullopt;
						}

					sv = std::string_view((*cur)[0].second, sv.end());
					++cur;
				}
			});
	}

	template< class Vec >
	static void unmarshal_attrs_impl(Vec& attrs, std::string_view& sv)
		requires (Has_AttrsT< S >::value && !Has_AttributesAreOrderedT< S >::value)
	{
        // std::cout << "attr: " << sv << std::endl;
		mark_tag attr_ident(1), attr_value(2);
		cregex attr_assign_re 
			= (attr_ident= (alpha >> *set[ alnum | '-' | '.' ]))
			>> '='
			>> '\"' >> (attr_value= *~(set= '\"')) >> '\"';

		cregex_iterator cur( sv.begin(), sv.end(), attr_assign_re ), end;
		for (; cur != end; ++cur)
		{
			auto ident = (*cur)[attr_ident]; 
			// std::cout << "F!!" << (*cur)[attr_value] << std::endl;
			bool extra = true;
			fusion::for_each(attrs, [&]< class A >(A& attr) {
					if (0 == (ident.compare(A::first_type::as_str.data())))
					{
						attr.second = _StrToType<typename A::second_type>(
							(*cur)[attr_value]);
						extra &= false;
					}
					else extra &= true;
				});
			if (extra) assert(false && "Has extra attribute");
			sv = std::string_view((*cur)[0].second, sv.end());
		}
	}
};

template< class...S >
struct UniversalUnmarshaller
{
public:
	using V = std::variant< S... >;
	static V unmarshal(std::string_view sv)
	{
		V v;
		unmarshaller_dispatch< S... >(v, sv);
		return v;
	}

private:

	struct _UnmarshFn
	{
		using result_type = void;

		template< class Value, class Var >
		void operator()(Value*, Var& v, std::string_view sv) const
		{
			v = Unmarshal<Value>::unmarshal(sv);
		}
	};

	template< class...Tags > requires ( IsNode<Tags> && ... )
	static void unmarshaller_dispatch(V& v, std::string_view sv)
	{
		mark_tag tag_ident(1);
		auto unmarsh_fn = typename xpressive::function<_UnmarshFn>::type {{}};

		cregex first_tag_re = as_xpr('<')
			>> (as_xpr(Tags::as_str)
				[
					unmarsh_fn(
						(Tags*)nullptr,
						xpressive::ref(v),
						xpressive::ref(sv))
				] |... );
	    cregex_iterator cur( sv.begin(), sv.end(), first_tag_re ), end;
	}
};


//////////////////////////////////////////////
START_XML_STRUCT(ProtocolStarting)

    START_ATTRS()
    END_ATTRS()
    
    START_CHILDS()
    END_CHILDS()

END_XML_STRUCT()
//////////////////////////////////////////////

//////////////////////////////////////////////
START_XML_STRUCT(ProtocolStop)
    LET_THERE_BE_FIELD(height)
    LET_THERE_BE_FIELD(auditable)
    LET_THERE_BE_FIELD(skipped)
    
    START_ATTRS()
        ATTR(double, height) ,
        ATTR(bool, auditable) ,
        ATTR(bool, skipped)
    END_ATTRS()

	START_CHILDS()
	    CHILD(ProtocolStarting)
	END_CHILDS()
END_XML_STRUCT()
//////////////////////////////////////////////

//////////////////////////////////////////////
START_XML_STRUCT(JSStartup)
// _META_ATTR_ORDERED_CHILDREN()

    LET_THERE_BE_FIELD(runtime)
    LET_THERE_BE_FIELD(context)
    LET_THERE_BE_FIELD(timestamp)

    START_ATTRS()
        ATTR(void*, runtime) , 
	    ATTR(void*, context) ,
	    ATTR(double, timestamp) 
	END_ATTRS()

	START_CHILDS()
	// CHILD(ProtocolStop)  ,
	// CHILD(ProtocolStarting) 
	END_CHILDS()
END_XML_STRUCT()
//////////////////////////////////////////////


int main()
{
	using namespace boost;
	std::cout << "Hello World" << std::endl;
	// auto p = Point(5, 5, true);
	// fusion::for_each(p, UniversalUnmarshaller<>::UnmarshalImpl());
	// auto prts = ProtocolStop();

	// auto m = Marshal();
	// // std::cout << Marshal::marshal<ProtocolStop>(ProtocolStop()) << std::endl;
	// prts.attr(ProtocolStop::height()) = 5;
	// auto s = Marshal::marshal<ProtocolStop>(prts);
	// // auto um = Unmarshal<ProtocolStop>();
	// auto _prts = Unmarshal<ProtocolStop>::unmarshal(s);
	// std::cout << "height:= " << _prts.attr(ProtocolStop::height()) << std::endl;

	{
		auto js = JSStartup();
		js.attr(JSStartup::runtime()) = (void*)0xdededede;
		js.attr(JSStartup::context()) = (void*)0xdeadbeef;
		// js.attr(JSStartup::runtime()) = (void*)0xdededede;
		js.attr(JSStartup::timestamp()) = 053.5351;
		std::cout << "jscontext: " << Marshal::marshal(js) << std::endl;

		{
			auto _js = Unmarshal< JSStartup >::unmarshal(Marshal::marshal(js));
			std::cout << _js.attr(JSStartup::context()) << std::endl;
			std::cout << _js.attr(JSStartup::runtime()) << std::endl;
			std::cout << _js.attr(JSStartup::timestamp()) << std::endl;
		}

		{
			auto _var = UniversalUnmarshaller<
				ProtocolStop, ProtocolStarting, JSStartup>::unmarshal(Marshal::marshal(js));
			auto& _js = std::get<JSStartup>(_var);
			std::cout <<"Variant-----------------" << std::endl;
			std::cout << _js.attr(JSStartup::context()) << std::endl;
			std::cout << _js.attr(JSStartup::runtime()) << std::endl;
			std::cout << _js.attr(JSStartup::timestamp()) << std::endl;
		}
	}
	return 0;
}
