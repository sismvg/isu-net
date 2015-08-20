

#include <imou-rpc-client/imouto_rpc_client.hpp>

event_type::event_type()
	:_locker(new lock_kernel)//TODO.可以不用mutex
{
}

void event_type::operator()(RPCMSG_CALLBACK_ARG)
{
	_io_success = io_success;
	_io_code = io_code;
	_value = rpc_code;
	_memory = memory;
	_handle = handle;
}

bool event_type::lock(size_type ms)
{
	if (ms == time_infinte)
	{
		_locker->lock();
		return true;
	}
	return _locker->timed_lock(boost::posix_time::milliseconds(ms));
}

void event_type::unlock()
{
	_locker->unlock();
}
