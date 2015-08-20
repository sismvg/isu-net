
#include <ex-utility/io_complete_port.hpp>
#include <exception/exhelper.hpp>

io_complete_port::io_complete_port(size_t limit_thread)
{
	_init(nullptr, limit_thread);
}

io_complete_port::io_complete_port(
	kernel_object_handle first_bind, size_t limit_thread)
{
	_init(first_bind, limit_thread);
}

io_complete_port::~io_complete_port()
{
#ifdef _WINDOWS
	if (native() != INVALID_HANDLE_VALUE)
		CloseHandle(native());
#else
#endif
}

kernel_object_handle 
	io_complete_port::native() const
{
	size_t ret = _complete_port;
	return reinterpret_cast<kernel_object_handle>(ret);
}

void io_complete_port::bind(kernel_object_handle object)
{
#ifdef _WINDOWS
	CreateIoCompletionPort(
		object, native(), NULL, _limit_thread);
	if (GetLastError() != 0)
	{
		throw complete_port_bind_fail("Try bind kernel object"\
			"to complete port fail", GetLastError());
	}
#else
#endif
}

void io_complete_port::close()
{
	if (native() != INVALID_HANDLE_VALUE)
	{
		CloseHandle(native());
		_set_handle_value(INVALID_HANDLE_VALUE);
	}
}

io_complete_port::size_t 
	io_complete_port::post(size_t key, sysptr_t argument)
{
	complete_result arg;
	arg.argument = argument;
	arg.key = key;
	arg.user_key = key;
	return post(arg);
}

io_complete_port::size_t io_complete_port::post(complete_result arg)
{

#ifdef _WINDOWS
	OVERLAPPED* lapped = new OVERLAPPED;
	ZeroMemory(lapped, sizeof(OVERLAPPED));

	lapped->hEvent = new complete_result(arg);

	size_t ret = PostQueuedCompletionStatus(
		native(), 0, arg.key, lapped);

	_try_throw_last_error();

	return ret;
#else
#endif
}

complete_result io_complete_port::get(size_t timeout)
{
#ifdef _WINDOWS
	DWORD word = 0;
	ULONG_PTR key = 0;

	OVERLAPPED* lapped = nullptr;
	BOOL accepted = GetQueuedCompletionStatus(
		native(), &word, &key, &lapped, timeout == -1 ? INFINITE : timeout);
	
	_try_throw_last_error();

	complete_result arg;
	if (accepted)
	{
		auto* ptr = reinterpret_cast<complete_result*>(lapped->hEvent);
		arg = *ptr;
		delete ptr;
	}
	else
	{
		//do some thing
		arg.key = complete_port_exit;
		arg.argument = nullptr;
	}
#else
#endif
	return arg;
}

void io_complete_port::_init(kernel_object_handle port, size_t limit)
{
	if (limit == 0)
	{
		throw std::invalid_argument("Io complete port can"\
			"not accept zero thread");
	}

	_limit_thread = limit;
	_set_handle_value(CreateIoCompletionPort(
		port == nullptr ? INVALID_HANDLE_VALUE : port, NULL,
		0, limit));
	if (native() == INVALID_HANDLE_VALUE)
	{
		throw complete_port_create_fail(
			"Try create complete fail", GetLastError());
	}
}

void io_complete_port::_try_throw_last_error()
{
	try_throw_last_error<complete_port_error>();
}

void io_complete_port::_set_handle_value(kernel_object_handle val)
{
	_complete_port = reinterpret_cast<size_t>(val);
}