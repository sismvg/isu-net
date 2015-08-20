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
		����һ����ַ,��ַ��addrָ��
	*/
	core_sockaddr(family_type family, port_type port,
		const binary_address& address);

	/*
		����һ����ַ,��ַ��ip_strָ��
		��ʽ����Ϊn.n.n.n
	*/
	core_sockaddr(family_type family, port_type port,
		const std::string& ip_str);

	/*
		��addr������һ����ַ
	*/
	core_sockaddr(const sockaddr_in& addr);

	/*
		һЩת��
	*/
	operator const sockaddr_in&() const;
	/*
		���س���ĵ�ַ.���������tcp/ip�ں�
	*/
	sockaddr* socket_addr();
	const sockaddr* socket_addr() const;

	/*
		��ַ�ĳ���
	*/
	int address_size() const;

	/*
		������ַ��
	*/
	family_type family() const;
	family_type& family();

	/*
		�����˿�
	*/
	port_type port() const;
	port_type& port();

	/*
		�����Ƶ�ַ�ִ�
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
		�չ���,socketΪinvailed_socket
	*/
	core_socket();

	/*
		����һ��socket
		family:socket�ĵ�ַ��
		type:socket������
		protocol:Э��
	*/
	core_socket(family_type family,
		socket_type type, protocol_type protocol);

	/*
		����һ��socket,���Ұ󶨵�addr��
		��ַ�������addrָ��
	*/
	core_socket(socket_type type, protocol_type protocol,
		const core_sockaddr& addr);

	core_socket(socket_type sock);

	/*
		�����ں�socket
	*/
	system_socket socket() const;

	/*
		�ر�socket
	*/
	void close();

	/*
		�󶨵�ָ��socket��
	*/
	operation_result bind(const core_sockaddr& addr);

	/*
		���ذ󶨵ĵ�ַ -δʵ��
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


//�����Ǳ�׼�ĺ���

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
