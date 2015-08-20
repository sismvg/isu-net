#ifndef IMOUTO_RPC_EXCEPTION_HPP
#define IMOUTO_RPC_EXCEPTION_HPP

#include <stdexcept>
#include <xstddef>

#include <ex-utility/memory/memory_block.hpp>
#include <exception/exhelper.hpp>
#include <sul-serialize/xdr/xdr_type_define.hpp>

#include <imou-rpc-kernel/imouto_rpcmsg.hpp>

/*
	������Ҫ���紫��,����û��ʹ��std::exception(const char*)
	���ڴ治��򴢴浼�µĴ��󶼵Ļ��Ͷ��Ǹ����Ե�
*/

class rpc_exception
	:public std::exception
{
public:
	/*Ϊ������rarchive����֧��Ĭ�Ϲ���
		msg:�쳣����Ϣ
		local:����Ϣ�������Ƿ��ڱ��ش�����
	*/
	typedef std::exception father_type;
	rpc_exception(const char* msg = nullptr, bool local = false);
	const xdr_string& rpc_what() const;
	~rpc_exception();
	
protected:

	XDR_DEFINE_TYPE_1(rpc_exception,xdr_string,_string_what)
};

DEFINE_HAVE_CODE_EXCEPTION(rpc_io_exception, rpc_exception);
/*
	��RPC���������͵��ó�ʱ
*/
DEFINE_EXCEPTION(rpc_local_timeout, rpc_exception);

/*
	RPC������ܵ������󣬵�û���ڹ涨ʱ���ڴ�����ɷ��س�ʱ����
*/
DEFINE_EXCEPTION(rpc_remote_timeout, rpc_exception);

/*
	���÷������쳣,������Ϣ��Ҫ�鿴�ڲ���enum
*/

template<class EnumType>
class rpc_invoke_exception
	:public rpc_exception
{
public:
	typedef EnumType enum_type;
	typedef rpc_exception father_type;

	rpc_invoke_exception()
		:father_type(nullptr,true)
	{}

	rpc_invoke_exception(enum_type code,
		const char* msg = nullptr, bool local = false)
		:
		father_type(msg,local),
		_what_code(code)
	{}

	enum_type what_code() const
	{
		return _what_code;
	}
protected:
	enum_type _what_code;
	XDR_DEFINE_TYPE_ARCHIVE_2(friend, rpc_invoke_exception<EnumType>,
		enum_type, _what_code, xdr_string, _string_what)
};

typedef rpc_invoke_exception<accept_stat> rpc_invoke_accept_exception;
typedef rpc_invoke_exception<reject_stat> rpc_invoke_reject_exception;

class rpc_local_platform_exception
	:public rpc_exception
{
public:
	typedef rpc_exception father_type;

	rpc_local_platform_exception()
	{}

	template<class T>
	rpc_local_platform_exception(const char* msg,
		const T& info, int64_t code)
		:father_type(msg), _code(code), _info(info)
	{}
	
	template<class T>
	T info() const
	{
		return boost::any_cast<T>(_info);
	}

	inline int64_t what_code() const
	{
		return _code;
	}

	virtual void raise() throw();
private:
	int64_t _code;
	boost::any _info;
};

DEFINE_EXCEPTION(rpc_platform_exception, rpc_exception);
DEFINE_EXCEPTION(rpc_remote_platform_exception, rpc_exception);
#endif
