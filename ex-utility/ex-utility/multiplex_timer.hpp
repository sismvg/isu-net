#ifndef ISU_MULTIPLEX_TIMER_HPP
#define ISU_MULTIPLEX_TIMER_HPP

#include <list>
#include <atomic>
#include <functional>

#include <ex-utility/ex_utility_lib.hpp>

#include <ex-config/platform_config.hpp>
#include <exception/exception.hpp>

#include <boost/smart_ptr/detail/spinlock.hpp>

#ifdef _WINDOWS
#include <windows.h>
#endif

#define KICK_DOWN_THE_LADDER 0x1
#define KICK_DOWN_PROCESSING 0x10
#define ADVANCE_EXPIRE 0x100
#define SET_ABD_EXPIRE 0x1000

DEFINE_EXCEPTION(timer_handle_invailed, std::exception);

class multiplex_timer
{//It is thread safe

public:
	typedef void* sysptr_t;
	typedef unsigned long dure_type;
	typedef unsigned long proid_type;

private:
	typedef multiplex_timer self_type;

	struct _timer_args
	{
#ifdef _DEBUG
		size_t _debug_unique_ident;
#endif
		kernel_object_handle native_timer;
		self_type* self;
		volatile long than;
		volatile long interval;
		sysptr_t argument;
		//WARNING:这里必须这么写
		std::list<_timer_args>::iterator* handle;
	};
	typedef std::list<_timer_args>::iterator iterator;
public:

	typedef iterator* timer_handle;
	typedef std::function<void(timer_handle, sysptr_t)> timer_callback;
	typedef std::function<void(sysptr_t)> cleanup;

	enum timer_key{
		timer_all_exited=1,
		timer_exit=2,
		timer_exited_by_api=3
	};

	enum timer_state
	{
		timer_killing,
		timer_running
	};

	/*
	dure is interval of timer.
	*/
	multiplex_timer();
	multiplex_timer(timer_callback callback);
	multiplex_timer(timer_callback callback, cleanup);

	~multiplex_timer();

	/*
	set a timer.
	you can use cancel_timer give timer_handle
	to end.
	*/
	timer_handle set_timer(sysptr_t argument, proid_type proid);

	/*
		ready timer and get handle
	*/
	timer_handle ready_timer(sysptr_t argument, proid_type proid);

	/*
		start timer if it was started
		it will throw
	*/
	void start_timer(timer_handle);

	/*
	cancel_timer in next callback doen
	*/
	void cancel_timer(timer_handle hand);

	/*
	Don't wait callback done.
	Don't call it before cancel_timer_when_callback_exit
	*/
	void kill_timer(timer_handle hand);

	/*
	when callback exit you can use that
	*/
	void cancel_timer_when_callback_exit(timer_handle hand);

	/*
		next timeout how many ms from now
	*/
	proid_type get_expire_interval(timer_handle) const;

	/*
		get reserve argumetn
	*/
	sysptr_t get_argument(timer_handle) const;

	/*
		timer expire exchange
	*/
	void advance_expire(timer_handle, proid_type ms);

	/*
		timer expire change to abs time
		use the UTC count
	*/
	void set_abs_expire(timer_handle, proid_type utc);

	/*
		Kill all sub timer ,not wait.
	*/
	void kill();

	/*
		Wait all sub timer stop
	*/
	void stop();

	/*
		how many timer
	*/
	size_t queue_size() const;
private:
	/*
		Just callback
	*/
	timer_callback _callback;
	/*
		If it is not empty,use that to cleanup user argument.
	*/
	cleanup _cleanup;

	/*
		To mark multiplex_timer's state,usually mark exit
	*/
	std::atomic_size_t _timer_state;

	/*
		System's timer,support from operation system.
	*/
	kernel_object_handle _systimer;
	kernel_object_handle _complete;

	kernel_object_handle _complete_thread;

	boost::detail::spinlock _timers_lock;
	/*
		Use to save timer argument
	*/
	std::list<_timer_args> _timers;
	//impl

	void _init(timer_callback);
	void _cleanup_handle(timer_handle handle);
	timer_handle _alloc_handle(sysptr_t argument, proid_type proid);

	void _cleanup_all(kernel_object_handle key);
	void _cleanup_handle_impl(timer_handle handle);
	void _cleanup_user_argument(sysptr_t);

	void _delete_timer(kernel_object_handle handle, bool justkill);

	void _clear();
	void _erase(iterator iter);

	static void SYS_CALLBACK _crack(sysptr_t parment, BOOLEAN);
	void _crack_impl(timer_handle arg, iterator& handle);

	struct _complete_io_argument
	{
		self_type* self;
	};

	static SYSTEM_THREAD_RET SYS_CALLBACK _complete_io(sysptr_t parment);

	void _complete_io_impl(sysptr_t parment);

};

template<class T>
T get_argument(const multiplex_timer& timer,
	const multiplex_timer::timer_handle& handle)
{
	return reinterpret_cast<T>(timer.get_argument(handle));
}

#endif