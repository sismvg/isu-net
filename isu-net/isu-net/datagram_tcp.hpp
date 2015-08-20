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

	//���������ƭ��rpc�� typedef
	typedef core_socket address_type;

	typedef core_socket socket_type;

	typedef std::function < void(
		bool io_success, const error_code& code,
		const address_type& source, shared_memory) > callback_type;

public:
	/*
		����tcp�󶨵�Ŀ���ַ��
	*/
	datagram_tcp(const core_sockaddr& address);

private:
	/*
		����datagram_tcp_server���⣬û�еط����õ�
	*/
	datagram_tcp(socket_type sock, const core_sockaddr& address);
public:
	~datagram_tcp();

	/*
		���ӵ���ַΪdest�ķ�����
	*/
	int connect(const core_sockaddr& dest);

	/*
		�Ͽ��������������
	*/
	void disconnect() throw();
		
	/*
		�Ͽ�������������Ӳ������ӵ�dest
	*/
	void redire(const core_sockaddr& dest);
	
	/*
		�رո�tcp
	*/
	void close() throw();
	
	/*
		����һ������,����Ὣ�������ݵ������ݱ�
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
		����
	*/
	void send(const address_type& dest,
		const_memory_address buffer, size_type size);

	/*
		�����ܵ����ݱ���ʱ������callback
	*/
	void when_recv_complete(const callback_type& callback);

	/*
		ƽ̨��أ�����socket
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
		����tcp server�ˣ����󶨵�address
	*/
	datagram_tcp_server(const core_sockaddr& address);

	/*
		�������ݵ�Ŀ��peer
	*/
	void send(const address_type& dest,
		const_memory_address buffer, size_type size);

	/*
		�رո�server
	*/
	void close();

	/*
		ƽ̨���,����ԭʼsocket
	*/
	const socket_type& native_socket() const;

	~datagram_tcp_server();

	/*
		���յ�����һ��peer���������ݰ�ʱ���������
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