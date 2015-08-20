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
	����������ת������һ��������ֵת����enum,���ڿ����Զ����Լ���error_code_mapping
	��������:
	template<>
	type error_code_mapping<type>(source_type value)
	����
	type error_code_mapping(source_type value)
	��Ҫ���㲻����error_code_mapping<T>(value)��������
*/
template<class T,class ValueType>
T error_code_mapping(const ValueType& value)
{
	return static_cast<T>(value);
}

#endif