#ifndef ISU_XDR_CONVERT_HPP
#define ISU_XDR_CONVERT_HPP

#include <string>
#include <type_traits>

#include <sul-serialize/extent/string_serialize.hpp>

//最小对齐包装
//保证小于xdr最小对齐的数据能得到填充
//仅对静态size的对象有效

#define XDR_MININUM_ALIGN 4
#define XDR_MININUM_ALIGN_TYPE long

#define XDR_MAXINUM_FIXED_LENGTH_OPAQUE_DATA_LENGTH 8192

#define XDR_MAXINUM_VARIABLE_LENGTH_OPAQUE_DATA_LENGTH \
	XDR_MAXINUM_FIXED_LENGTH_OPAQUE_DATA_LENGTH

#define XDR_MAXINUM_STRING_LENGTH 255

template<class T>
struct is_fixed_length_data
{
	typedef std::true_type type;
	static const bool value = true;
};

//强制单字节字符串
class xdr_string
	:public std::basic_string<char>
{
public:
	typedef std::basic_string<char> father_type;

	typedef xdr_string self_type;

	xdr_string()
	{}

	template<class Iter>
	xdr_string(const Iter& begin,const Iter& end)
		:father_type(begin,end)
	{}

	xdr_string(const father_type& father)
		:father_type(father)
	{}

	xdr_string(const xdr_string& rhs)
		:father_type(rhs)
	{}

	template<class Iter>
	xdr_string(const Iter& begin, size_t count)
		: father_type(begin, count)
	{}

	xdr_string(const std::initializer_list<char>& vars)
		: father_type(vars)
	{}

	xdr_string& operator=(const father_type& rhs)
	{
		static_cast<father_type&>(*this) = rhs;
		return *this;
	}
	xdr_string& operator=(const xdr_string& rhs)
	{
		father_type::operator=(rhs);
		return *this;
	}
	template<class T>
	xdr_string& operator=(const T& var)
	{
		*this = xdr_string(var);
		return *this;
	}
};

template<>
struct is_fixed_length_data < const xdr_string >
{
	typedef std::false_type type;
	static const bool value = false;
};

template<>
struct is_fixed_length_data < xdr_string >
{
	typedef std::false_type type;
	static const bool value = false;
};


template<class T>
struct xdr_object_maxinum_length
{
	static const size_t value = XDR_MAXINUM_FIXED_LENGTH_OPAQUE_DATA_LENGTH;
};

template<>
struct xdr_object_maxinum_length < xdr_string >
{
	static const size_t value = XDR_MAXINUM_STRING_LENGTH;
};

/*
template<class Dest,class T>
struct to_be_same_singled_impl
{
	typedef Dest type;
};

template<class Dest>
struct to_be_same_singled_impl < Dest,std::true_type >
{
	typedef unsigned Dest type;
};

template<class Dest,class T>
struct to_be_same_singled
{
	typedef
		typename to_be_same_singled_impl < Dest,
		std::is_unsigned<T>::type > ::type type;
};*/

template <class T>
struct mininum_align_xdr_wapper
{
/*	static_assert(sizeof(value_type) <= XDR_MININUM_ALIGN,
		"mininum_align_xdr_wapper's"\
		" template argument T's size must less than XDR_MININUM_ALIGN"); */

	//	typedef typename
	//	to_be_same_singled<XDR_MININUM_ALIGN_TYPE, T>::type type;

	typedef T value_type;

	mininum_align_xdr_wapper(T& value)
		:_value(&value)//不设定_size为0,支持及时崩溃
	{
	}

	value_type& value()
	{
		return *_value;
	}

	const value_type& value() const
	{
		return *_value;
	}

	size_t real_size() const
	{
		return _size;
	}
	void set_real_size(size_t size)
	{
		_size = size;
	}
	size_t _size;
	value_type* _value;
};

//优先匹配模板,而不是转换。。。
inline size_t archived_size(const xdr_string& string)
{
	return archived_size(static_cast<const xdr_string::father_type&>(string));
}

inline size_t archive_to_with_size(memory_address buffer, size_t bufsize,
	const xdr_string& value, size_t result)
{
	return archive_to_with_size(buffer, bufsize,
		static_cast<const xdr_string::father_type&>(value), result);
}

inline size_t rarchive_with_size(const_memory_address buffer, size_t bufsize,
	xdr_string& value, size_t value_size)
{
	return rarchive_with_size(buffer, bufsize,
		static_cast<xdr_string::father_type&>(value), value_size);
}

template<class T>
mininum_align_xdr_wapper<T> xdr_convert(T& value)
{
	return mininum_align_xdr_wapper<T>(value);
}

template<class T>
size_t ex_length(const T& value)
{
	bool fixed = is_fixed_length_data<
		typename std::remove_cv<T>::type > ::value;
	return (fixed ? 0 : 1)*XDR_MININUM_ALIGN;
}

template<class T>
size_t archived_size(const mininum_align_xdr_wapper<T>& wapper)
{//WARNING:const
	size_t size = archived_size(wapper.value());
	//变长数据要写入一个size参数并对齐到XDR_MININUM_ALIGIN
	size = xdr_align(size);
	if (size > xdr_object_maxinum_length<T>::value)
	{
		throw std::invalid_argument("xdr object length too long");
	}
	return size + ex_length(wapper.value());
}

template<class T>
size_t rarchived_size(const mininum_align_xdr_wapper<T>& wapper)
{
	//不需要对齐
	return ex_length(wapper.value()) + archived_size(wapper.value());
}

template<class T>
size_t archive_to_with_size(memory_address buffer, size_t bufsize,
	const mininum_align_xdr_wapper<T>& value, size_t result)
{
	//result是对齐后的字节
	bool is_fixed_length = is_fixed_length_data<
		typename std::remove_cv<T>::type>::value;
	size_t ex = 0;
	if (!is_fixed_length)
	{
		//要写入一个size,result中包含了size
		size_t unalign_size = archived_size(value.value());
		size_t adv = archive_to_with_size(buffer, bufsize,
			unalign_size, sizeof(size_t));
		ex = adv;
		incerment<1>(buffer, adv);
		result = unalign_size;
		//warning:未填充
	}
	archive_to_with_size(buffer, bufsize,
			value.value(), result);
	return result + ex;
}

template<class T>
size_t archive_to(memory_address buffer, size_t bufsize, 
	const mininum_align_xdr_wapper<T>& value)
{
	return archive_to_with_size(buffer, bufsize,
		value.value(), archived_size(value));
}

template<class T>
size_t archive_to(const memory_block& buffer,
	const mininum_align_xdr_wapper<T>& value)
{
	return archive_to(buffer.buffer, buffer.size, value);
}

template<class T>
size_t rarchive_with_size(const_memory_address buffer, size_t bufsize,
	mininum_align_xdr_wapper<T>& value,size_t value_size)
{
	bool fixed_length = is_fixed_length_data<T>::value;
	if (!fixed_length)
	{
		size_t count = 0;
		rarchive_with_size(buffer, bufsize, count,sizeof(size_t));
		incerment<1>(buffer, sizeof(size_t));
		value_size = count;
		bufsize -= sizeof(size_t);
	}
	return (fixed_length ? 0 : sizeof(size_t)) + xdr_align(
		rarchive_with_size(buffer, bufsize,
		value.value(), value_size));
}

template<class T>
size_t rarchive(const_memory_address buffer, size_t bufsize, 
	mininum_align_xdr_wapper<T>& value)
{//强制忽略未用字节
	size_t size = archived_size(value);
	rarchive_with_size(buffer,
		bufsize, value.value(), size);
	return size;
}

template<class T>
size_t rarchive(const const_memory_block& buffer, 
	mininum_align_xdr_wapper<T>& value)
{
	return rarchive(buffer.buffer, buffer.size, value);
}
#endif