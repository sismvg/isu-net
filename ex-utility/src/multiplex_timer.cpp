
#include <ex-utility/multiplex_timer.hpp>

#include <algorithm>

multiplex_timer::multiplex_timer()
{
}

multiplex_timer::multiplex_timer(timer_callback callback)
{
	_init(callback);
}

multiplex_timer::multiplex_timer(timer_callback callback,cleanup clean)
	:_cleanup(clean)
{
	_init(callback);
}

multiplex_timer::~multiplex_timer()
{
	//这样就保证了没有后续的Post kill timer
	_timer_state = timer_killing;

	BOOL posted = PostQueuedCompletionStatus(
		_complete, 0, timer_all_exited, NULL);

	if (!posted)
	{
		if (GetLastError() == 0)
		{

		}
	}

	if (_complete_thread != NULL)
	{
		WaitForSingleObject(_complete_thread, INFINITE);

		CloseHandle(_complete);
		CloseHandle(_complete_thread);
	}

	stop();
}

size_t multiplex_timer::queue_size() const
{
	auto* none_const_this = const_cast<self_type*>(this);
	none_const_this->_timers_lock.lock();
	size_t result = _timers.size();
	none_const_this->_timers_lock.unlock();
	return result;
}

void multiplex_timer::start_timer(timer_handle handle)
{
	size_t ms = get_expire_interval(handle);

	BOOL is_created = CreateTimerQueueTimer(
		&(*handle)->native_timer, _systimer,
		multiplex_timer::_crack,
		reinterpret_cast<sysptr_t>(handle), ms, ms,
		WT_EXECUTEDEFAULT);

	if (!is_created)
	{
		_cleanup_handle(handle);
		_cleanup_handle_impl(handle);

		throw timer_handle_invailed("timer handle is invaild");
	}
}

multiplex_timer::timer_handle
	multiplex_timer::ready_timer(sysptr_t argument, proid_type proid)
{
	return _alloc_handle(argument, proid);
}

multiplex_timer::timer_handle 
	multiplex_timer::set_timer(sysptr_t argument, proid_type proid)
{
	timer_handle result = ready_timer(argument, proid);
	start_timer(result);
	return result;
}

void multiplex_timer::_cleanup_handle(timer_handle handle)
{
	_timers_lock.lock();
	_timers.erase((*handle));
	_timers_lock.unlock();
}

void multiplex_timer::_cleanup_handle_impl(timer_handle handle)
{
	delete handle;
}

void multiplex_timer::cancel_timer(timer_handle hand)
{
	_delete_timer((*hand)->native_timer, false);
	_cleanup_user_argument((*hand)->argument);
	_cleanup_handle(hand);
	_cleanup_handle_impl(hand);
}

void multiplex_timer::kill_timer(timer_handle hand)
{
	auto native = (*hand)->native_timer;
	_delete_timer(native, false);
}

void multiplex_timer::
	cancel_timer_when_callback_exit(timer_handle hand)
{
	InterlockedCompareExchange(
		&(*hand)->than, KICK_DOWN_THE_LADDER, 0);
}

sysptr_t multiplex_timer::get_argument(timer_handle hand) const
{
	return (*hand)->argument;
}

void multiplex_timer::advance_expire(timer_handle hand, proid_type ms)
{
	size_t newms = get_expire_interval(hand) + ms;
	InterlockedExchange(&(*hand)->interval, newms);

	if (!ChangeTimerQueueTimer(
		_systimer, (*hand)->native_timer, ms, newms))
	{

	}
}

void multiplex_timer::set_abs_expire(timer_handle, proid_type utc)
{
}

multiplex_timer::proid_type 
	multiplex_timer::get_expire_interval(timer_handle handle) const
{
	return (*handle)->interval;
}

void multiplex_timer::kill()
{
	_cleanup_all(NULL);
}

void multiplex_timer::stop()
{
	_cleanup_all(INVALID_HANDLE_VALUE);
}

multiplex_timer::timer_handle 
	multiplex_timer::_alloc_handle(sysptr_t argument, proid_type proid)
{
	timer_handle result = new iterator;
	_timer_args args;

	args.than = 0;//MSG:这里绝不会有其他线程引用
	args.argument = argument;
	args.self = this;
	args.handle = result;
	args.interval = proid;

	_timers_lock.lock();
	
	_timers.push_back(args);
	--((*result) = _timers.end());

	_timers_lock.unlock();
	return result;
}

void multiplex_timer::_delete_timer(kernel_object_handle handle, bool justkill)
{
	if (!DeleteTimerQueueTimer(_systimer, handle,
		justkill ? NULL : INVALID_HANDLE_VALUE))
	{

	}
}

void multiplex_timer::_cleanup_all(kernel_object_handle key)
{
	if (_systimer != INVALID_HANDLE_VALUE)
	{
		BOOL success = DeleteTimerQueueEx(_systimer, key);
		if (success)
		{//保证不会有抢占了
			_systimer = INVALID_HANDLE_VALUE;

			std::for_each(_timers.begin(), _timers.end(),
				[&](_timer_args& args)
			{
				_cleanup_user_argument(args.argument);
				_cleanup_handle_impl(args.handle);
			});
		}
	}
}

void CALLBACK multiplex_timer::_crack(sysptr_t parment, BOOLEAN)
{
	auto handle_ptr = reinterpret_cast<timer_handle>(parment);
	if (!handle_ptr || (*handle_ptr)->than==KICK_DOWN_PROCESSING)
	{
		return;
	}
	(*handle_ptr)->self->_crack_impl(handle_ptr, *handle_ptr);
}

void multiplex_timer::_crack_impl(timer_handle arg, iterator& handle)
{
	_callback(arg, handle->argument);

	//这样可以确定没有别的地方设定了than要做别的事情
		//假设一个_callback要kill自己,并且该_callback是可重入的话
			//必然再一次调用_callback一定会设定KICK_DOWN_THE_LADDER
	auto pre = InterlockedCompareExchange(&handle->than,
		KICK_DOWN_THE_LADDER, KICK_DOWN_THE_LADDER);

	if (pre==KICK_DOWN_THE_LADDER)
	{
		auto pre = InterlockedCompareExchange(&handle->than,
			KICK_DOWN_PROCESSING, KICK_DOWN_THE_LADDER);
		if (pre==KICK_DOWN_THE_LADDER&&_timer_state!=timer_killing)
		{//这样就能保证不会再执行一次Post了
			//MSG:hEvent本来就是干这个的
			OVERLAPPED* overlapped = new OVERLAPPED;
			ZeroMemory(overlapped, sizeof(OVERLAPPED));

			auto* self = handle->self;
			overlapped->hEvent = reinterpret_cast<kernel_object_handle>(arg);
			if (!PostQueuedCompletionStatus(
				self->_complete, 0, timer_exit, overlapped))
			{

			}
		}
	}
}

unsigned long multiplex_timer::_complete_io(sysptr_t parment)
{
	auto* self = reinterpret_cast<self_type*>(parment);
	self->_complete_io_impl(parment);
	return 0;
}

void multiplex_timer::_complete_io_impl(sysptr_t parment)
{
	//parment的结构
	kernel_object_handle port = _complete;// reinterpret_cast<kernel_object_handle>(parment);

	DWORD word = 0, key = 0;

	OVERLAPPED* overlapped = nullptr;

	while (true)
	{
		BOOL ok = GetQueuedCompletionStatus(
			port, &word, &key, &overlapped, INFINITE);

		DWORD error = GetLastError();

		if (ok)
		{
			if (key == timer_exit)
			{
				timer_handle handle_ptr =
					reinterpret_cast<timer_handle>(overlapped->hEvent);
				auto& handle = (*handle_ptr);

				_delete_timer(handle->native_timer, false);
				_cleanup_user_argument(handle->argument);
				_cleanup_handle(handle_ptr);
				_cleanup_handle_impl(handle_ptr);

				delete overlapped;
			}
			else if (key == timer_all_exited)
			{//MSG:不会管其他的了
				break;
			}
		}
		else if (overlapped == nullptr)
		{
		}
		else if (error == WAIT_TIMEOUT)
		{
		}
		else
		{
		}
	}
	return;
}

void multiplex_timer::_cleanup_user_argument(sysptr_t ptr)
{
	if (_cleanup)
	{
		_cleanup(ptr);
	}
}

void multiplex_timer::_init(timer_callback callback)
{
	_timers_lock = BOOST_DETAIL_SPINLOCK_INIT;
	_callback = callback;
	bool initok = _callback.operator bool();
	if (initok)
	{
		_timer_state = timer_running;

		_systimer = CreateTimerQueue();
		if (_systimer == INVALID_HANDLE_VALUE)
		{
			initok = false;
		}
		else
		{
			_complete = CreateIoCompletionPort(
				INVALID_HANDLE_VALUE, NULL, 0, 1);
			if (_complete == INVALID_HANDLE_VALUE)
			{
				initok = false;
			}
			else
			{
				_complete_thread = CreateThread(
					NULL, 0, _complete_io,
					reinterpret_cast<sysptr_t>(this), 0, NULL);
				if (_complete_thread == NULL)
				{
					initok = false;
				}
			}
		}
	}
	if (!initok)
	{
		throw std::exception("Unknow exception when iniialize multiplex_timer");
	}
}