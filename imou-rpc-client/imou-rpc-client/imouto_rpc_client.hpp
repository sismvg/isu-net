#ifndef ISU_IMOUTO_RPC_CLIENT_HPP
#define ISU_IMOUTO_RPC_CLIENT_HPP

//由3个部分组成
//传输模块,校验模块,rpc细节控制
//生成基本rpc通信数据->生成用户校验->传输模块发送

#include <imou-rpc-client/imou_client_lib.hpp>

#include <imou-rpc-kernel/imouto_rpc_kernel.hpp>

#include <ex-utility/utility.hpp>

#include <boost/thread/mutex.hpp>
#include <boost/smart_ptr/detail/spinlock.hpp>

#include <imou-rpc-client/imouto_rpc_client_log.hpp>
#include <imou-rpc-kernel/basic_rpc.hpp>

#include <sul-serialize/sul_serialize.hpp>
#include <sul-serialize/archive.hpp>
#include <sul-serialize/xdr/xdr.hpp>

template<class AddressType>
class basic_imouto_client
{
public:
	/*
	安全关闭客户端
	*/
	virtual void close() throw() = 0;

};

//由于经常要advance_in(adv,memory),所以就不传const&了
#define RPCMSG_CALLBACK_ARG const async_handle& handle,bool io_success,size_type io_code,\
	size_type rpc_code,const_memory_block memory

#define RPCMSG_CALLBACK_USE handle,io_success,io_code,rpc_code,memory
#define RPCMSG_CALLBACK_RET void

/*
用来支持imouto_rpc_client的内部实现,不要在别的地方使用它
*/

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
			throw rpc_local_timeout("Rpc invoke timeout");
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

template<class TranslationKernel, class Verify>
class imouto_rpc_client
	://不用担心数据模型的问题
	public imouto_typedef<TranslationKernel, Verify>,
	public basic_rpc<TranslationKernel>,
	public basic_imouto_client < typename TranslationKernel::address_type >
{
public:
	static const size_type invalid_length = 0;

	typedef imouto_rpc_client self_type;

	typedef std::shared_ptr<rpc_exception> exception_pointer;

	typedef std::function < void(const async_handle&,//异步句柄
		bool, const error_code&,//io状态
		const exception_pointer&,//rpc异常
		size_type) > callback_type;//xid

private:
	typedef std::function < void(const async_handle&,
		bool, const error_code&,
		const size_type rpc_code,
		const_memory_block) > inside_callback_type;

	struct _timer_arg
	{
		size_type xid;
		address_type dest;
		inside_callback_type* callback;
	};

	void _relase_timer_arg(void* arg_ptr)
	{
		auto* arg = reinterpret_cast<_timer_arg*>(arg_ptr);
		//	delete arg->callback;
		delete arg;
	}
public:
	/*
	不关心程序号和版本号.
	translation:负责传输的模块
	verify:负责用户校验的模块
	*/
	template<class Translation, class Verify>
	imouto_rpc_client(
		const std::shared_ptr<Translation>& translation,
		const std::shared_ptr<Verify>& verify)
		:
		_verify(verify),
		_translation(translation),
		_timeout_timer(MEMBER_BIND_IN(_rpc_timeout),
		MEMBER_BIND_IN(_relase_timer_arg))
	{
		_xidmap_lock = BOOST_DETAIL_SPINLOCK_INIT;
		if (!(_verify&&_translation))
		{
			throw rpc_exception("translation or verify is nullptr");
		}
		else
		{
			_translation->when_recv_complete(
				[&](PROCESS_MSG_ARGUMENT_WITH_NAME)
			{
				process_msg(PROCESS_MSG_ARGUMENT_NAME,
					MEMBER_BIND_IN(_process_package_impl));
			});
		}
	}

	~imouto_rpc_client()
	{
		_disable_more_mission();
		_abort_all_mission();
		//可以开始析构了
	}

	enum client_state
	{
		normal=0,
		disable_more_mission=1
	};

	void _disable_more_mission()
	{
		_state = static_cast<std::size_t>(disable_more_mission);
	}

	void _abort_all_mission()
	{
		auto lock = ISU_AUTO_LOCK(_xidmap_lock);
		_missions.clear();
	}

	/*
	调用一个过程
	sync:是否同步调用
	stub:包含函数前面和id的桩
	args:函数参数
	*/
	template<class Stub, class... Arg>
	typename Stub::result_type invoke(const address_type& dest,
		const Stub& stub, Arg&&... args)
	{
		//-1 mean Forever
		return timed_invoke(dest, -1, stub, args...);
	}

	template<class Stub, class... Arg>
	typename Stub::result_type timed_invoke(const address_type& dest, size_type ms,
		const Stub& stub, Arg&&... args)
	{
		/*
		这里不用对stub的代码进行防崩溃处理
		因为是在调用者线程执行的打包，如果出错而调用者又不关心
		则崩溃是好的
		*/

		auto memory_ptr = stub.push(args...);

		const_memory_block memory = memory_ptr;

		auto wait_handle = event_type();
		wait_handle.lock();

		size_type xid = _call(dest, ms, stub.ident(),
			memory, [&](RPCMSG_CALLBACK_ARG)->RPCMSG_CALLBACK_RET
		{
			wait_handle(RPCMSG_CALLBACK_USE);
			wait_handle.unlock();
		});

		//TODO.这样result必须支持空的构造,但是有办法让他不需要
		typename Stub::result_type result;

		try
		{
			/*
			没法写形如 lock.wait() dosomething without lambda;
			因为需要RPCMSG_CALLBACK_ARG来自于其他的线程.
			而保存一份以后再从wait_handle类读取太麻烦了
			*/
			wait_handle.timed_wait(ms, [&](RPCMSG_CALLBACK_ARG)
				->RPCMSG_CALLBACK_RET
			{
				if (!io_success)
				{
					throw rpc_io_exception("Rpc call has I/O fail", io_code);
				}
				auto pair = _async_callback(RPCMSG_CALLBACK_USE);
				if (pair.first == invalid_length)
				{
					//这个函数一定会抛出异常
					_make_rpc_exception_and_raise(RPCMSG_CALLBACK_USE);
				}
				else
				{
					result = stub.result(pair.second, args...);
				}
				delete pair.second.buffer;
			});
		}
		catch (rpc_local_timeout& error)
		{
			if(_have(dest,xid))//防止扔出异常的这段时间收到了消息
				throw error;
		}
		return result;
	}

	template<class Address, class Stub, class... Arg>
	void async_timed_invoke_than(const Address& addr, size_type ms,
		const Stub& stub, const callback_type& callback, Arg&&... args)
	{
		auto fn = [&, callback](RPCMSG_CALLBACK_ARG)
			->RPCMSG_CALLBACK_RET
		{
			auto pair = _async_callback(RPCMSG_CALLBACK_USE);

			if (pair.first != invalid_length)
			{
				exception_pointer exp;
				_skip_exception_call(exp, [&]()
				{
					stub.result(pair.second, args...);
				});
				if (callback)
				{
					callback(handle, io_success, io_code, exp, 0);
					return;
				}
			}

			if (callback)
			{
				//是否需要放到别的线程以及加上防崩溃?
				callback(handle, io_success, io_code,
					_make_exception(RPCMSG_CALLBACK_USE), 0);
			}
		};

		/*
		不需要防崩溃,理由见timed_invoke
		*/
		auto memory = stub.push(args...);
		const_memory_block stack = memory;
		_call(addr, ms, stub.ident(), stack, fn, false);
	}

	/*
	异步调用一个过程
	stub:包含函数签名和id的桩
	callback:当rpc函数执行完毕会被调用
	args:函数的参数
	*/
	template<class Stub, class Func, class... Arg>
	void async_invoke_than(const Stub& stub, const Func& callback, Arg&&... args)
	{
		//-1 forever;
		async_timed_invoke_than(-1, stub, callback, args...);
	}

	/*
	仅没有callback,其他与async_invoke_than相同
	*/
	template<class Address, class Stub, class... Arg>
	void async_invoke(const Address& addr, const Stub& stub, Arg&&... args)
	{
		//-1 forever
		async_timed_invoke(addr, -1, stub, args...);
	}

	template<class Address, class Stub, class... Arg>
	void async_timed_invoke(const Address& addr, size_type ms,
		const Stub& stub, Arg&&... args)
	{
		async_timed_invoke_than(addr, ms, stub, callback_type(), args...);
	}

	/*
	重定向服务器
	*/
	virtual void redire(const address_type& address)
	{
		//	_translation->redire(address);
	}

	/*
	关闭客户端和服务器的链接
	*/
	virtual void close() throw()
	{
		_translation->close();
	}
private:
	/*
	用于异步invoke的超时,同步invoke则用timed_wait
	*/
	multiplex_timer _timeout_timer;

	std::shared_ptr<translation_kernel> _translation;

	std::shared_ptr<Verify> _verify;

	template<class Stack>
	size_type _call(const address_type& dest, size_type timeout_ms,
		const ident_type& stub, const Stack& stack,
		const inside_callback_type& callback, bool sync = true)
	{
		rpcstub_ident ident(stub);
		ident.head.magic = msg_type::call;
		ident.head.xid = _alloc_xid(dest, callback);

		rpc_call_body body;
		//WAR.鬼畜
		body.body = ident;
		body.ident = _verify->generic_verify(stub);

		if (!sync)
		{
			_timer_arg* arg = new _timer_arg;
		//	arg->callback = reinterpret_cast<inside_callback_type*>(body.trunk);
			arg->xid = ident.head.xid;
			body.handle = _timeout_timer.set_timer(arg, timeout_ms);
		}

		auto size = archived_size(body) + archived_size(stack);
		auto memory_ptr = archive(size, body, stack);

		const_memory_block memory = memory_ptr;
		_translation->send(dest, memory.buffer, memory.size);
		return ident.head.xid;
	}

	/*
	持有它就负责memory的生命周期控制
	用于支持异步和批处理rpc消息
	*/
	void _process_package_impl(const async_handle& handle,
		bool io_success, const error_code& io_code,
		const address_type& from, const_memory_block memory)
	{
		//io异常的话包一定是有的
		if (!io_success)
		{
			const auto* body = reinterpret_cast<const rpc_call_body*>(memory.buffer);
			_if_have_call(from, body->body.head.xid, [&](inside_callback_type& callback)
			{
				callback(handle, io_success, io_code, session_io_failed, memory);
				_timeout_timer.cancel_timer(
					reinterpret_cast<timer_handle>(body->handle));
			}, true);
			return;
		}
		/*
		memory:_reply_body-stat-trunk-data
		data:返回值和用户修改的参数 or 异常结构
		*/

		reply_body reply;
		advance_in(memory, rarchive(memory, reply));

		/*
		检查:xid,用户身份
		*/
		if (reply.head.magic == msg_type::replay&&_have(from, reply.head.xid))
		{
			if (reply.what == replay_stat::msg_accepted)
			{
				if (!_verify->check_verify(reply.ident))
				{
					//TODO:do something
					return;
				}
			}
			else if (reply.what == replay_stat::msg_denied)
			{
				//做些什么
			}
			else
			{
				//错误处理,然后直接return
				return;
			}

			auto lock = ISU_AUTO_LOCK(_xidmap_lock);

			auto iter = _missions.find(from);
			auto xid_iter = iter->second.find(reply.head.xid);

			(xid_iter->second)(handle, io_success, io_code,
				_to_session_stat(reply.what, reply.stat), memory);
			iter->second.erase(xid_iter);
		}
		else
		{
			//do something
		}
	}


	bool _have(const address_type& from, const xid_type xid)
	{

		auto lock = ISU_AUTO_LOCK(_xidmap_lock);
		auto iter = _missions.find(from);
		if (iter == _missions.end())
			return false;
		auto xid_iter = iter->second.find(xid);
		if (xid_iter != iter->second.end())
		{
			return true;
		}
		return false;
	}

	template<class Func>
	void _skip_exception_call(exception_pointer& exp, Func fn)
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
		exception_pointer& exp, unsigned int code, EXCEPTION_POINTERS* info)
	{

		/*
		允许在这里引发的异常将直接导致程序崩溃
		*/
		exp = std::make_shared<rpc_local_platform_exception>(
			"Rpc invoke has exception from local", *info, code);

		return EXCEPTION_EXECUTE_HANDLER;
	}

	template<class Func>
	void _skip_cpp_exception_call(exception_pointer& exp, Func fn)
	{

		try
		{
			fn();
		}
		catch (std::bad_alloc&)
		{
			/*
			WAR.当是一个内存不足导致的异常的时候将无法恢复
			*/
		}
		catch (std::runtime_error& error)
		{
			exp = std::make_shared<rpc_exception>(error.what());
		}
		catch (...)
		{
			exp = std::make_shared<rpc_exception>(
				"Local inovke has unknow exception");
		}
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
		/* 暂时不做什么
		case session_prog_unavail:
		break;
		case session_prog_mismatch:
		break;
		case session_proc_unavail:
		break;
		case session_garbage_args:
		break;
		case session_rpc_mismatch:
		break;
		case session_auth_error:
		break;
		case session_rpc_failed:
		break;
		*/
		default:
			break;
		}
		return std::make_pair(invalid_length, const_memory_block());
	}

	typedef multiplex_timer::timer_handle timer_handle;
	void _rpc_timeout(timer_handle handle, sysptr_t arg)
	{

		auto* info = reinterpret_cast<_timer_arg*>(arg);
		if (_have(info->dest, info->xid))
		{
			//表明在这一瞬间没有被set过
			(*info->callback)(async_handle(), 0, 0,
				session_timeout, memory_block(nullptr, 0));
		}
		_timeout_timer.cancel_timer_when_callback_exit(handle);
	}

	void _log_rpc_error(RPCMSG_CALLBACK_ARG)
	{
	}

	exception_pointer _make_exception(
		RPCMSG_CALLBACK_ARG, bool do_throw = false)
	{

		/*
		为了为异步callback提供异常信息则必须用指针来保存对象
		但是如果指定了要throw的话则不需要用指针
		*/
		if (rpc_code == static_cast<size_type>(session_timeout))
		{
			//WAR.超时参数的问题.以及这个没法throw
			return std::make_shared<rpc_local_timeout>(
				"Rpc invoke time out");
		}
		const_memory_block exdata;
		rarchive(memory, exdata);
		if (rpc_code == static_cast<size_type>(session_cpp_exception))
		{
			return _make_exception_impl<rpc_exception>(
				RPCMSG_CALLBACK_USE, exdata, do_throw);
		}
		return _make_exception_impl<rpc_remote_platform_exception>(
			RPCMSG_CALLBACK_USE, exdata, do_throw);
	}

	template<class T>
	exception_pointer _make_exception_impl(RPCMSG_CALLBACK_ARG,
		const const_memory_block& expmemory, bool do_throw)
	{//TODO.如何消除掉这个拷贝？

		T exception;
		rarchive(expmemory, exception);
		if (do_throw)
		{
			throw exception;
		}
		return std::make_shared<T>(exception);
	}

	void _make_rpc_exception_and_raise(RPCMSG_CALLBACK_ARG)
	{//必须保证不是success

		if (rpc_code <= static_cast<size_type>(enum_value_boundary))
			_make_exception(RPCMSG_CALLBACK_USE, true);
	}

	std::atomic_size_t _state;
	std::atomic_size_t _last_xid;
	std::map < address_type,
		std::map < xid_type, inside_callback_type >> _missions;

	//xid
	xid_type _alloc_xid(const address_type& dest, const inside_callback_type& call)
	{

		auto lock = ISU_AUTO_LOCK(_xidmap_lock);
		auto ret = ++_last_xid;
		_missions[dest].insert(std::make_pair(ret, call));
		return ret;
	}

	void _force_check_it(const address_type& dest, const xid_type& xid)
	{

		auto lock = ISU_AUTO_LOCK(_xidmap_lock);
		auto iter = _missions.find(dest);
		if (iter != _missions.end())
		{
			iter->second.erase(xid);
		}
	}

	template<class Func>
	bool _if_have_call(const address_type& dest,
		const xid_type xid, Func fn,bool erase=false)
	{

		auto lock = ISU_AUTO_LOCK(_xidmap_lock);
		bool value = false;
		auto iter = _missions.find(dest);
		if (iter != _missions.end())
		{
			auto xid_iter = iter->second.find(xid);
			if (xid_iter != iter->second.end())
			{
				value = true;
				fn(xid_iter->second);
				if (erase)
				{
					iter->second.erase(xid_iter);
				}
			}
		}
		return value;
	}
};
//如果exceptino有效,则会dynamic_cast exception然后抛出
void throw_rpc_exception(const std::shared_ptr<rpc_exception>& exception);

#endif