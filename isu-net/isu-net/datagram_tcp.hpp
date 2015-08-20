#ifndef DATAGRAM_TCP_HPP
#define DATAGRAM_TCP_HPP

#include <cstddef>
#include <functional>

#ifdef _WINDOWS
#include <WinSock2.h>
#endif

#include <sul-serialize/xdr/xdr.hpp>
#include <sul-serialize/serialize.hpp>

#include <isu-net/socket.hpp>
#include <ex-utility//memory/memory_helper.hpp>
#include <ex-utility/mpl/mpl_helper.hpp>
#include <ex-utility/io_complete_port.hpp>

#include <boost/threadpool/pool.hpp>
#include <boost/smart_ptr/detail/spinlock.hpp>

class datagram_tcp_server;
class datagram_tcp
	:public boost::noncopyable
{
	friend class datagram_tcp_server;

	struct datagram_tcp_overlapped
		:public OVERLAPPED
	{
		datagram_tcp_overlapped(datagram_tcp* tcp);
		datagram_tcp* tcp();
	private:
		datagram_tcp* _tcp;
	};

public:
	typedef std::size_t size_type;
	typedef std::size_t error_code;

	//这个是用来骗过rpc的 typedef
	typedef core_socket address_type;

	typedef core_socket socket_type;

	typedef std::function < void(
		bool io_success, const error_code& code,
		const address_type& source, shared_memory) > callback_type;

public:
	/*
		将该tcp绑定到目标地址上
	*/
	datagram_tcp(const core_sockaddr& address);

private:
	/*
		除了datagram_tcp_server以外，没有地方会用到
	*/
	datagram_tcp(socket_type sock, const core_sockaddr& address);
public:
	~datagram_tcp();

	/*
		链接到地址为dest的服务器
	*/
	int connect(const core_sockaddr& dest);

	/*
		断开与服务器的链接
	*/
	void disconnect() throw();
		
	/*
		断开与服务器的链接并且链接到dest
	*/
	void redire(const core_sockaddr& dest);
	
	/*
		关闭该tcp
	*/
	void close() throw();
	
	/*
		发送一块数据,这里会将整个数据当成数据报
	*/
	void send(const_memory_address buffer, size_type size);

#ifdef _WINDOWS
	static void CALLBACK _send_complete(
		IN DWORD dwError,
		IN DWORD cbTransferred,
		IN OVERLAPPED* lpOverlapped,
		IN DWORD dwFlags);
#endif

#ifdef _LINUX
#endif

	/*
		无用
	*/
	void send(const address_type& dest,
		const_memory_address buffer, size_type size);

	/*
		当接受到数据报的时候会调用callback
	*/
	void when_recv_complete(const callback_type& callback);

	/*
		平台相关，返回socket
	*/
	const core_socket native_socket() const;

protected:
	callback_type _callback;
	core_socket _socket;
	address_type _server;

private:
	boost::thread _recver;
	core_sockaddr _generic_local_address();
	bool _try_to_recover(int length);
	void _tcp_recv();
};

class datagram_tcp_server
	:public boost::noncopyable
{
public:
	typedef core_socket socket_type;
	typedef socket_type address_type;

	typedef std::size_t size_type;
	typedef size_type error_code;

	typedef std::function < void(bool io_success, const error_code&,
		const address_type& from, const shared_memory& memory) > callback_type;

	/*
		创建tcp server端，并绑定到address
	*/
	datagram_tcp_server(const core_sockaddr& address);

	/*
		发送数据到目标peer
	*/
	void send(const address_type& dest,
		const_memory_address buffer, size_type size);

	/*
		关闭该server
	*/
	void close();

	/*
		平台相关,返回原始socket
	*/
	const socket_type& native_socket() const;

	~datagram_tcp_server();

	/*
		当收到任意一个peer发来的数据包时，会调用它
	*/
	void when_recv_complete(const callback_type& callback);
private:
	callback_type _callback;
	core_socket _socket;
	std::shared_ptr<boost::thread> _accepter;

	boost::detail::spinlock _lock;
	std::vector<std::shared_ptr<datagram_tcp>> _clients;

	void _tcp_accepter();
	bool _try_recover_socket_error();
};

namespace
{
	inline std::ostream& operator<<(std::ostream& ost, const core_socket&)
	{
		return ost;
	}
}
#endif