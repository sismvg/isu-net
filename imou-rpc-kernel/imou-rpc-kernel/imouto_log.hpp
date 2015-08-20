#ifndef IMOUTO_LOG_HPP
#define IMOUTO_LOG_HPP

#include <fstream>

#include <loghelper.hpp>
#include <boost/format.hpp>

//用来生成字符串
#define LOG_TEXT(text) text

#ifndef _CLOSE_ALL_RPC_LOG

//#define MYMSGL(lev) BOOST_LOG_SEV(_logger,lev)
#include <iostream>

struct myobj_class
{
	template<class T>
	const myobj_class& operator<<(const T&) const
	{
		return *this;
	}
};

const myobj_class myobj_1;

#define MYMSGL(lev) std::cout
#define MYMSG_LOG_FUNCTION BOOST_LOG_FUNCTION
#else

struct nop_stream
{};

template<class T>
const nop_stream& operator<<(const nop_stream& ost, const T& value)
{
	return ost;
}

#define MYMSGL(lev) nop_stream stream; stream
#define MYMSG_LOG_FUNCTION
#endif

#define MYMSG MYMSGL(debug)


template<class T>
void format_impl(T&)
{}

template<class T, class V, class... Arg>
void format_impl(T& obj, const V& arg, const Arg&... args)
{
	obj%arg;
	format_impl(obj, args...);
}

#ifdef _CLOSE_ALL_RPC_LOG
struct nop_struct
{};

inline std::ostream& operator<<(std::ostream& ost, const nop_struct& nop)
{
	return ost;
}

struct nop_str_struct
{
	inline nop_struct str() const
	{
		return nop_struct();
	}
};
#endif

template<class Str, class... Arg>
auto format(const Str& expr, const Arg&... args)
->decltype(boost::format(expr))
{
#ifndef _CLOSE_ALL_RPC_LOG
	auto obj = boost::format(expr);
	format_impl(obj, args...);
	return obj;
#else
	return nop_str_struct();
#endif
}
#endif