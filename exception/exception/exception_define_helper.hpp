#ifndef EXCEPTION_DEFINE_HELPER_HPP
#define EXCEPTION_DEFINE_HELPER_HPP

#define DEFINE_EXCEPTION(name,father)\
class name\
	:public father\
{\
public:\
	name()\
	:father(nullptr)\
	{}\
	name(const char* msg)\
		:father(msg)\
	{}\
}

#define DEFINE_EXCEPTION_WITH_MSG(name,father,msg)\
class name\
	:public father\
{\
public:\
	name(const char* msg)\
		:father(msg)\
	{}\
\
	name()\
		:father(msg)\
	{}\
}

#define DEFINE_HAVE_CODE_EXCEPTION(name,father)\
class name\
	:public father\
{\
public:\
	typedef std::int64_t error_code;\
	name()\
	{}\
	name(const char* msg, const error_code& code_)\
		:father(msg), code(code_)\
	{}\
\
	error_code code;\
}

/*
	类似于类型转换，但一般用于数值转换到enum,用于可以自定义自己的error_code_mapping
	做法如下:
	template<>
	type error_code_mapping<type>(source_type value)
	或者
	type error_code_mapping(source_type value)
	这要求你不能用error_code_mapping<T>(value)来调用它
*/
template<class T,class ValueType>
T error_code_mapping(const ValueType& value)
{
	return static_cast<T>(value);
}

#endif