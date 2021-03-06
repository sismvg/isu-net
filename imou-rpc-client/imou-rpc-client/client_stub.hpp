#ifndef ISU_CLIENT_STUB_HPP
#define ISU_CLIENT_STUB_HPP

#include <imou-rpc-client/imou_client_lib.hpp>

#include <imou-rpc-kernel/stub_ident.hpp>
#include <imou-rpc-client/imouto_client_stub.hpp>

#include <ex-utility/memory/memory_helper.hpp>

typedef serialize <
	serialize_object <
	condition_filter <
	is_archiveable, xdr_filter > > > client_default_serialize;

template<class IdentType, class R, class... Arg>
auto make_client_stub(const IdentType& ident, R(*fn)(Arg...))
->decltype(basic_make_client_stub<client_default_serialize>(ident, fn))
{
	return basic_make_client_stub<client_default_serialize>(ident, fn);
}

template<std::size_t Prog,std::size_t Proc>
struct client_stub_holder
{
};

#define ALLOC_CLIENT_STUB_IDENT(prog,proc)\
template<> struct client_stub_holder < prog, proc > {\
static_assert(prog >= 0x40000000, "progid must be less ge than 0x40000000");};

#define GENERIC_CLIENT_STUB(stub_name,prog,proc,ver,func)\
	ALLOC_CLIENT_STUB_IDENT(prog,proc)\
	auto stub_name=make_client_stub(stub_ident(prog,ver,proc),\
reinterpret_cast<real_type<decltype(func)>::type*>(nullptr))

#define GENERIC_CLIENT_STUB_IN_HEAP(stub_name,prog,proc,ver,func)\
	ALLOC_CLIENT_STUB_IDENT(prog,proc)\
	auto stub_name=std::make_shared(make_client_stub(stub_ident(prog,ver,proc),\
	reinterpret_cast<real_type<decltype(func)>::type*>(nullptr))

#endif