
#include <isu-net/datagram_tcp.hpp>
#include <ex-utility/lock/autolock.hpp>

namespace
{

	bool try_easy_recover_socket_error()
	{
		bool result = true;
#ifdef _WINDOWS
		auto error_code = WSAGetLastError();
		result = false;
		return result;
		if (error_code == ENOTSOCK || error_code == WSAEINTR)
		{
			result = false;
		}
		else if (error_code == ENOMEM)
		{
		}
		else if (error_code == ENOBUFS)
		{
			result = false;
		}
		else if (error_code == WSAECONNABORTED)
		{
			result = false;
		}
#endif
		return result;
	}

struct stream_wrap
{
public:
	typedef datagram_tcp::size_type size_type;
	stream_wrap();
	stream_wrap(char* buffer, size_type max_stream);
	void set_stream(char* buffer, size_type max_stream);
	/*
	buffer:������
	buffer_size:�������Ĵ�С
	����ֵ:��master stream���пռ�����buffer_size��С�����ݵĻ�
	�򷵻�ֵΪbuffer_size,����Ϊ�����˵����ݴ�С
	*/
	size_type append(void* buffer, size_type buffer_size);
	bool is_complete() const;
	void cleanup();
	size_type stream_size() const;
private:
	char* _stream;
	size_type _was_append;
	size_type _maxinum;
};

class stream_analysis
{
public:
	typedef datagram_tcp::size_type size_type;
	typedef std::function<void(const shared_memory& memory)> analysis_callback;
	stream_analysis(analysis_callback callback);
	/*
	��һ���������������޷����һ�����ݰ���ʱ�򽫻�Ѳ�����������
	����Ὣ���е���Ч���ݰ�ȫ������callback��
	*/
	void push(void* buffer, size_type length);
private:
	analysis_callback _callback;

	shared_memory _head;
	stream_wrap _head_wrap;

	shared_memory _memory;
	stream_wrap _wrap;

	//���Խ���ͷ����ʧ�ܷ���false,�ɹ��򷵻�true
	bool _try_analysis_head(void*& buffer, size_type& length);

	//���Ը������ݵ������������ҵ���_callback
	void _copy_and_try_call(void*& buffer, size_type& length);
	//��� buffer,length�Ƿ���Ч
	bool _check_safe(void* buffer, size_type length);
	void _reset_head();
};

struct stream_header
{
	XDR_DEFINE_TYPE_1(stream_header,
		xdr_uint32, length);
};

}
datagram_tcp::datagram_tcp_overlapped::
	datagram_tcp_overlapped(datagram_tcp* tcp)
	:_tcp(tcp)
{}

datagram_tcp* datagram_tcp::datagram_tcp_overlapped::tcp()
{
	return _tcp;
}

datagram_tcp::datagram_tcp(const core_sockaddr& addr)
	:
	_socket(SOCK_STREAM, 0, addr)
{
}

datagram_tcp::datagram_tcp(socket_type sock, const core_sockaddr& address)
	:
	_socket(sock)
{	
	//�����ֱ�Ӵ�
	_recver = boost::thread(MEMBER_BIND_IN(_tcp_recv));
}

datagram_tcp::~datagram_tcp()
{
	_socket.close();
	_recver.join();
}

int datagram_tcp::connect(const core_sockaddr& dest)
{//��֤�����������̷߳���
	int ret = ::connect(_socket.socket(),
		dest.socket_addr(), sizeof(sockaddr_in));
	if (ret != SOCKET_ERROR)
	{
		_recver = boost::thread(MEMBER_BIND_IN(_tcp_recv));
	}
	return ret;
}

void datagram_tcp::disconnect() throw()
{
	::shutdown(_socket.socket(), 0);
}

void datagram_tcp::redire(const core_sockaddr& dest)
{
	disconnect();
	connect(dest);
}

void datagram_tcp::close() throw()
{
	_socket.close();
}

void datagram_tcp::send(const_memory_address buffer, size_type size)
{
#ifdef _WINDOWS

	//auto* lapped = new datagram_tcp_overlapped(this);
	const size_type buffer_count = 2;

	shared_memory header = archive(xdr_uint32(size));

	WSABUF buffers_with_header[2] =
	{
		{ header.size(), header.get() },
		{ size, reinterpret_cast<char*>(const_cast<void*>(buffer)) }
	};

	DWORD count = 0;
	WSASend(_socket.socket(), buffers_with_header, buffer_count,
		&count, 0, NULL, NULL);
#endif

#ifdef _LINUX
	::send(_socket.socket(), reinterpret_cast<char*>(&size), sizeof(size_type), 0);
	::send(_socket.socket(), reinterpret_cast<const char*>(buffer), size, 0);
#endif
}

#ifdef _WINDOWS
void CALLBACK datagram_tcp::_send_complete(
	IN DWORD dwError,
	IN DWORD cbTransferred,
	IN OVERLAPPED* lpOverlapped,
	IN DWORD dwFlags)
{
	delete lpOverlapped;
}
#endif

#ifdef _LINUX
#endif
void datagram_tcp::send(const address_type& dest,
	const_memory_address buffer, size_type size)
{
	//����dest
	send(buffer, size);
}

void datagram_tcp::when_recv_complete(const callback_type& callback)
{
	_callback = callback;
}

const core_socket datagram_tcp::native_socket() const
{
	return _socket;
}

core_sockaddr datagram_tcp::_generic_local_address()
{
	int random_port = 0;
	return core_sockaddr(AF_INET,
		htons(random_port), inet_addr("127.0.0.1"));
}

stream_wrap::stream_wrap()
	:_stream(nullptr), _maxinum(0), _was_append(0)
{
}

stream_wrap::size_type stream_wrap::stream_size() const
{
	return _maxinum;
}

stream_wrap::stream_wrap(char* buffer, size_type max_stream)
{
	set_stream(buffer, max_stream);
}

void stream_wrap::cleanup()
{
	set_stream(nullptr, 0);
}

void stream_wrap::set_stream(char* buffer, size_type max_stream)
{
	_stream = buffer;
	_maxinum = max_stream;
	_was_append = 0;
}

stream_wrap::size_type stream_wrap::
	append(void* buffer, size_type buffer_size)
{
	//�����޷���,������abs
	size_type need_copy =
		(buffer_size + _was_append) <= _maxinum ?
	buffer_size : _maxinum - _was_append;

	mymemcpy(_stream + _was_append, buffer, need_copy);
	_was_append += need_copy;
	return need_copy;
}

bool stream_wrap::is_complete() const
{
	return _maxinum == _was_append;
}

bool datagram_tcp::_try_to_recover(int length)
{
	if (!try_easy_recover_socket_error())
	{
		return false;
	}
	return true;
}

stream_analysis::stream_analysis(analysis_callback callback)
	:_callback(callback), _head(new char[4], 4)
{
	_reset_head();
}

void stream_analysis::push(void* buffer, size_type length)
{
	while (_try_analysis_head(buffer, length))
	{
		_copy_and_try_call(buffer, length);
	}
}

void stream_analysis::_reset_head()
{
	_head_wrap = stream_wrap(_head.get(), _head.size());
}

bool stream_analysis::
	_try_analysis_head(void*& buffer, size_type& length)
{
	if (!_check_safe(buffer, length))
			return false;
	if (_head_wrap.is_complete())
		return true;

	advance_in(buffer, length, _head_wrap.append(buffer, length));

	if (_head_wrap.is_complete())
	{
		//�յ���ȫ����ͷ��
		stream_header header;
		auto* ptr = reinterpret_cast<size_type*>(_head.get());
		header.length = *ptr;
		memory_block block(new char[header.length], header.length);
		_memory = shared_memory(block);
		_wrap.set_stream(reinterpret_cast<char*>(block.buffer), block.size);

		//�ص������״̬
		return true;
	}
	return false;
}

void stream_analysis::_copy_and_try_call(void*& buffer, size_type& length)
{
	if (!_check_safe(buffer, length))
			return;

	advance_in(buffer, length, _wrap.append(buffer, length));
	if (_wrap.is_complete())
	{
		_callback(_memory);
		_memory.reset();
		_wrap.cleanup();
		_reset_head();
	}
}

//��� buffer,length�Ƿ���Ч
bool stream_analysis::_check_safe(void* buffer, size_type length)
{
	return buffer&&length;
}

void datagram_tcp::_tcp_recv()
{
	const size_type buffer_length = 566;
	char buffer_block[buffer_length];

	stream_analysis analysis([&](const shared_memory& memory)
	{
		if (_callback)//����socket�����õײ���÷���
			_callback(true, 0, _socket, memory);
	});

	//��֤δ��ɵ�ֻ��һ��
	while (true)
	{
		int length = recv(_socket.socket(), buffer_block, buffer_length, 0);
		if (length == SOCKET_ERROR)
		{
			if (_try_to_recover(length))
			{
				continue;
			}
			else
				break;
		}
		using namespace std;
		analysis.push(buffer_block, length);
	}
}

//
datagram_tcp_server::datagram_tcp_server(const core_sockaddr& address)
	:_socket(SOCK_STREAM, 0, address), _lock(BOOST_DETAIL_SPINLOCK_INIT)
{
	if (::listen(_socket.socket(), -1) == SOCKET_ERROR)
	{

	}

	_accepter.reset(new boost::thread(MEMBER_BIND_IN(_tcp_accepter)));
}

void datagram_tcp_server::send(const address_type& dest,
	const_memory_address buffer, size_type size)
{
	const size_type buffer_count = 2;

	shared_memory header = archive(xdr_uint32(size));

#ifdef _WINDOWS
	WSABUF buffers_with_header[2] =
	{
		{ header.size(), header.get() },
		{ size, reinterpret_cast<char*>(const_cast<void*>(buffer)) }
	};

	DWORD count = 0;
	WSASend(dest.socket(), buffers_with_header, buffer_count,
		&count, 0, NULL, NULL);
#endif
}

void datagram_tcp_server::close()
{
	_socket.close();
	std::for_each(_clients.begin(), _clients.end(),
		[&](const std::shared_ptr<datagram_tcp>& client)
	{
		client->close();
	});
}

const datagram_tcp_server::socket_type& 
	datagram_tcp_server::native_socket() const
{
	return _socket;
}

datagram_tcp_server::~datagram_tcp_server()
{
	close();
	if (_accepter)
	{
		if (_accepter->joinable())
			_accepter->join();
	}
}

void datagram_tcp_server::when_recv_complete(const callback_type& callback)
{
	std::for_each(_clients.begin(), _clients.end(),
		[&](const std::shared_ptr<datagram_tcp>& ptr)
	{
		ptr->when_recv_complete(callback);
	});
}

void datagram_tcp_server::_tcp_accepter()
{
	while (true)
	{
		core_sockaddr client_addr;
		int length = sizeof(sockaddr_in);

		SOCKET sock = ::accept(_socket.socket(),
			client_addr.socket_addr(), &length);
		if (sock == invailed_socket)
		{
			if (!_try_recover_socket_error())
				break;
			break;
		}
		auto lock = ISU_AUTO_LOCK(_lock);
		_clients.push_back(std::shared_ptr<datagram_tcp>(
			new datagram_tcp(sock, client_addr)));
		_clients.back()->when_recv_complete(_callback);
	}
}

bool datagram_tcp_server::_try_recover_socket_error()
{
	if (!try_easy_recover_socket_error())
	{
		return false;
	}
	return true;
}