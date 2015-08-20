#ifndef ISU_SOCKET_HPP
#define ISU_SOCKET_HPP

#include <string>

#include <ex-config/platform_config.hpp>
#include <ex-utility/double_ulong32.hpp>
#include <ex-utility/mpl/mpl_helper.hpp>

#ifdef _WINDOWS
#include <WinSock2.h>
#endif

#define CORE_SOCKADDR_PAIR(addr) addr.socket_addr(),addr.address_size()

class core_sockaddr
{
public:
	typedef core_sockaddr self_type;
	typedef unsigned short family_type;
	typedef unsigned short port_type;
	typedef unsigned long binary_address;

	core_sockaddr();
	/*
		创建一个地址,地址由addr指定
	*/
	core_sockaddr(family_type family, port_type port,
		const binary_address& address);

	/*
		创建一个地址,地址由ip_str指定
		格式必须为n.n.n.n
	*/
	core_sockaddr(family_type family, port_type port,
		const std::string& ip_str);

	/*
		用addr来创建一个地址
	*/
	core_sockaddr(const sockaddr_in& addr);

	/*
		一些转换
	*/
	operator const sockaddr_in&() const;
	/*
		返回抽象的地址.具体解释由tcp/ip内核
	*/
	sockaddr* socket_addr();
	const sockaddr* socket_addr() const;

	/*
		地址的长度
	*/
	int address_size() const;

	/*
		所属地址族
	*/
	family_type family() const;
	family_type& family();

	/*
		所属端口
	*/
	port_type port() const;
	port_type& port();

	/*
		二进制地址字串
	*/
	binary_address address() const;
	binary_address& address();

private:
	void _init(family_type family, port_type port, binary_address addr);
	sockaddr_in _address;
};

const int invailed_socket = -1;

class core_socket
{
public:
	typedef int system_socket;
	typedef int operation_result;
	typedef int socket_type;
	typedef core_sockaddr::family_type family_type;
	typedef core_sockaddr::port_type port_type;
	typedef int protocol_type;

	/*
		空构造,socket为invailed_socket
	*/
	core_socket();

	/*
		创建一个socket
		family:socket的地址族
		type:socket的类型
		protocol:协议
	*/
	core_socket(family_type family,
		socket_type type, protocol_type protocol);

	/*
		创建一个socket,并且绑定到addr上
		地址族必须由addr指定
	*/
	core_socket(socket_type type, protocol_type protocol,
		const core_sockaddr& addr);

	core_socket(socket_type sock);

	/*
		返回内核socket
	*/
	system_socket socket() const;

	/*
		关闭socket
	*/
	void close();

	/*
		绑定到指定socket上
	*/
	operation_result bind(const core_sockaddr& addr);

	/*
		返回绑定的地址 -未实现
	*/
	const core_sockaddr* bound();
private:
	core_sockaddr _address;
	system_socket _socket;
	void _init(family_type family, 
		socket_type type, protocol_type protocol);
};

inline bool is_multicast(const core_sockaddr& addr)
{
	const sockaddr_in& tmp = addr;
	auto value = tmp.sin_addr.S_un.S_addr;
	return value > 0xE000000F && value < 0xEFFFFFFF;
}


//几个非标准的函数

namespace
{
	auto to_ulong64(const core_sockaddr& value)
		->decltype(VOID_TYPE_CALL_CONST(double_ulong32)->to_ulong64())
	{
		double_ulong32 lv(value.port(), value.address());
		return lv.to_ulong64();
	}
}

inline bool operator<(const core_sockaddr& lhs, const core_sockaddr& rhs)
{
	return to_ulong64(lhs) < to_ulong64(rhs);
}

inline bool operator<(const core_socket& lhs, const core_socket& rhs)
{
	return lhs.socket() < rhs.socket();
}

#endif
