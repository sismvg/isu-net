#ifndef IMOUTO_CLIENT_STUB_HPP
#define IMOUTO_CLIENT_STUB_HPP

#include <imou-rpc-client/imou_client_lib.hpp>

#include <imou-rpc-kernel/imouto_rpcstub.hpp>

#include <ex-utility/mpl/type_list.hpp>

#include <sul-serialize/sul_serialize.hpp>
#include <sul-serialize/archive.hpp>

template<class IdentType,class SerializeObject,class R,class... Arg>
class imouto_client_stub
	:public basic_stub<IdentType>
{
public:

	typedef IdentType ident_type;
	typedef SerializeObject serialize_object;
	typedef basic_stub<IdentType> father_type;
	typedef R result_type;

	imouto_client_stub(const ident_type& ident)
		:father_type(ident)
	{}

	/*
		将参数序列化后调用real,并返回返回值和引用等符合类型数据
	*/
	//所以要求Func是可重入的
	//WAR.Arg真的没有深拷贝的问题吗？
	shared_memory push(Arg... parments) const
	{
		serialize_object core;
		return core.archive_real(type_list<Arg...>(), parments...);
	}

	/*
		单纯的反序列化数据,parments必须是传入invoke的参数
	*/
	template<class... Parments>
	result_type result(const const_memory_block& memory,Parments&&... parments) const
	{
		serialize_object core;
		return _invoke_result_impl(core,
			std::is_void<result_type>::type(), memory, parments...);
	}

	template<class Func,class... Parments>
	result_type operator()(Func real, Parments&&... parments)
	{
		return invoke_result(invoke(real, parments...), parments...);
	}

	/*
		其返回值允许用户像一个普通函数一样调用它.而不需要Client来invoke
	*/
	template<class Stub,class Address,class SharedClient>
	static std::function<result_type(Arg...)>
		make_function_stub(const Stub& stub,
		const Address& server, const SharedClient& client)
	{
		return [&](Arg... args)
			->result_type
		{
			return client->invoke(server, stub, args...);
		};
	}
private:
	template<class... Parments>
	result_type _invoke_result_impl(serialize_object& core,std::true_type,
		const const_memory_block& memory, Parments&&... parments) const
	{
		core.rarchive_real(memory, type_list<Arg...>(), parments...);
	}

	template<class... Parments>
	result_type _invoke_result_impl(serialize_object& core,std::false_type,
		const const_memory_block& memory, Parments&&... parments) const
	{
		result_type result;
		core.rarchive_real(memory, type_list<result_type&, Arg...>(),
			result, parments...);
		return result;
	}
};

template<class SerializeObject,class IdentType,class R,class... Arg>
imouto_client_stub<IdentType,SerializeObject,R,Arg...>
	basic_make_client_stub(const IdentType& ident, R(*fn)(Arg...))
{
	return imouto_client_stub<IdentType, SerializeObject, R, Arg...>(ident);
}
#endif