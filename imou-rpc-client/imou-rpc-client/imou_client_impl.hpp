#ifndef IMOU_RPC_CLIENT_IMPL_HPP
#define IMOU_RPC_CLIENT_IMPL_HPP

template<class Func>
void _skip_exception_call(exception_type& exp, Func fn)
{
	__try
	{
		return _skip_cpp_exception_call(exp, fn);
	}
	__except (_myfilter(exp, GetExceptionCode(),
		GetExceptionInformation()))
	{
	}
}

unsigned int _myfilter(
	exception_type& exp, unsigned int code, EXCEPTION_POINTERS* info)
{
	/*
	允许在这里引发的异常将直接导致程序崩溃
	*/
	exp.reset(ptr);

	return EXCEPTION_EXECUTE_HANDLER;
}

template<class Func>
void _skip_cpp_exception_call(exception_type& exp, Func fn)
{
	try
	{
		fn();
	}
	catch (std::bad_alloc&)
	{
		/*
		WAR.当是一个内存不足导致的异常的时候将无法恢复
		也无法记录日志(日志要求内存分配)
		*/
	}
	catch (std::runtime_error& error)
	{
		log_runtime_error("std::runtime_error:", error, false);
		exp = std::make_shared<std::runtime_error>(error);
	}
	catch (...)
	{
		exp = std::make_shared<
			std::runtime_error>("unknow exception");
		log_runtime_error("unknow:", *exp);
	}
}

class event_type
{
public:

	typedef std::size_t size_type;
	typedef const_shared_memory async_handle;
	typedef boost::mutex lock_kernel;

	static const size_type time_infinte = -1;

	event_type();
	void operator()(RPCMSG_CALLBACK_ARG);
	bool lock(size_type ms = time_infinte);
	void unlock();

	template<class Func>
	void timed_wait(size_type ms, Func& fn)
	{
		if (!lock(ms))
		{
			throw rpc_timeout(ms);
		}

		unlock();
		fn(_handle, _io_success, _io_code, _value, _memory);
	}

private:
	//well... 的确很恶心
	bool _io_success;
	size_type _io_code;
	size_type _value;
	const_memory_block _memory;
	async_handle _handle;

	std::shared_ptr<lock_kernel> _locker;
};

template<class AddressType>
class imou_client_impl
{
protected:
	typedef AddressType address_type;
	typedef std::functional<void(const memory_block&)> rarchiver;
	typedef std::shared_ptr<rpc_exception> exception_pointer;
	/*
		rarchiver:用来反序列化返回值的东西
	*/
	void timed_invoke_impl(rarchiver&,const address_type& dest,
		const const_shared_memory& memory,const stub_ident& ident)
	{
		auto wait_handle = event_type();
		wait_handle.lock();

		size_type xid = _call(dest, ms, ident,
			memory, [&](RPCMSG_CALLBACK_ARG)->RPCMSG_CALLBACK_RET
		{
			wait_handle(RPCMSG_CALLBACK_USE);
			wait_handle.unlock();
		});

		try
		{
			wait_handle.timed_wait(ms, [&](RPCMSG_CALLBACK_ARG)
				->RPCMSG_CALLBACK_RET
			{
				if (!io_success)
				{
					throw rpc_invoke_exception(io_code,
						"Rpc call I/O fail", true);
				}
				auto pair = _async_callback(RPCMSG_CALLBACK_USE);
				if (pair.first == invalid_length)
				{
					//这个函数一定会抛出异常
					_make_rpc_exception_and_raise(RPCMSG_CALLBACK_USE);
				}
				else
				{
					rarchiver(pair.second);
				}
			});
		}
		catch (rpc_local_timeout& error)
		{
			if (verify_xid(dest, xid))//防止扔出异常的这段时间收到了消息
				throw error;
		}
	}

	/*
	*/
	void async_timed_invoke_impl(rarchiver&)
	{
		auto fn = [&, callback](RPCMSG_CALLBACK_ARG)
			->RPCMSG_CALLBACK_RET
		{
			auto pair = _async_callback(RPCMSG_CALLBACK_USE);

			if (pair.first != invalid_length)
			{
				exception_pointer exception;
				_skip_exception_call(exp, [&]()
				{
					rarchiveer(pair.second);
				});

				if (callback)
				{
					callback(handle, io_success, io_code, exception);
					return;
				}
			}

			if (callback)
			{
				//是否需要放到别的线程以及加上防崩溃?
				callback(handle, io_success, io_code,
					_make_exception(RPCMSG_CALLBACK_USE));
			}
		};
	}

	void _rpc_timeout(timer_handle handle, sysptr_t arg)
	{
		auto* info = reinterpret_cast<_timer_arg*>(arg);
		if (verify_xid(info->dest, info->xid))
		{
			//表明在这一瞬间没有被set过
			(*info->callback)(async_handle(), 0, 0,
				session_timeout, memory_block(nullptr, 0));
		}
		_timeout_timer.cancel_timer_when_callback_exit(handle);
	}

	std::shared_ptr<rpc_exception>
		_make_exception(RPCMSG_CALLBACK_ARG, bool do_throw = false)
	{
		if (rpc_code == static_cast<size_type>(session_timeout))
		{
			return make_shared<rpc_timeout>("rpc call respone to long");
		}
		const_memory_block exdata;
		rarchive(memory, exdata);
		if (rpc_code == static_cast<size_type>(session_cpp_exception))
		{
			return _make_exception_impl<cpp_style_runtime_error>(
				RPCMSG_CALLBACK_USE, exdata, do_throw);
		}
		return _make_exception_impl<c_style_runtime_error>(
			RPCMSG_CALLBACK_USE, exdata, do_throw);
	}

	template<class T>
	exception_pointer _make_exception_impl(RPCMSG_CALLBACK_ARG,
		const const_memory_block& expmemory, bool do_throw)
	{
		auto ptr = make_shared<rpc_runtime_error>(exception);
		rarchive(expmemory, *ptr);
		exception.stat = rpc_code;
		if (do_throw)
		{
			throw *ptr;
		}
		return ptr;
	}

	void _make_rpc_exception_and_raise(RPCMSG_CALLBACK_ARG)
	{
		if (value <= static_cast<size_type>(enum_value_boundary))
			_make_exception(RPCMSG_CALLBACK_USE, true);
	}

	static size_type _to_session_stat(replay_stat rpstat, size_type stat)
	{
		size_type mapping_number =
			rpstat == replay_stat::msg_accepted ?
		session_success : session_rpc_mismatch;

		return stat + mapping_number;
	}

	/*
	非success的时候memory_block为空
	*/
	std::pair<size_type, const_memory_block>
		_async_callback(RPCMSG_CALLBACK_ARG)
	{
		memory_block alterd_arguments;
		auto stat = static_cast<session_stat>(rpc_code);
		switch (stat)
		{
		case session_success:
		{
			size_type size = rarchive(memory, alterd_arguments);
			return std::make_pair(size, alterd_arguments);
			break;
		}
		default:
			break;
		}
		return std::make_pair(invalid_length, const_memory_block());
	}

};

#endif