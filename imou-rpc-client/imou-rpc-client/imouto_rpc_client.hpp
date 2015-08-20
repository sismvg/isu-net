#ifndef ISU_IMOUTO_RPC_CLIENT_HPP
#define ISU_IMOUTO_RPC_CLIENT_HPP

//��3���������
//����ģ��,У��ģ��,rpcϸ�ڿ���
//���ɻ���rpcͨ������->�����û�У��->����ģ�鷢��

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
	��ȫ�رտͻ���
	*/
	virtual void close() throw() = 0;

};

//���ھ���Ҫadvance_in(adv,memory),���ԾͲ���const&��
#define RPCMSG_CALLBACK_ARG const async_handle& handle,bool io_success,size_type io_code,\
	size_type rpc_code,const_memory_block memory

#define RPCMSG_CALLBACK_USE handle,io_success,io_code,rpc_code,memory
#define RPCMSG_CALLBACK_RET void

/*
����֧��imouto_rpc_client���ڲ�ʵ��,��Ҫ�ڱ�ĵط�ʹ����
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
	//well... ��ȷ�ܶ���
	bool _io_success;
	size_type _io_code;
	size_type _value;
	const_memory_block _memory;
	async_handle _handle;

	std::shared_ptr<lock_kernel> _locker;
};

template<class TranslationKernel, class Verify>
class imouto_rpc_client
	://���õ�������ģ�͵�����
	public imouto_typedef<TranslationKernel, Verify>,
	public basic_rpc<TranslationKernel>,
	public basic_imouto_client < typename TranslationKernel::address_type >
{
public:
	static const size_type invalid_length = 0;

	typedef imouto_rpc_client self_type;

	typedef std::shared_ptr<rpc_exception> exception_pointer;

	typedef std::function < void(const async_handle&,//�첽���
		bool, const error_code&,//io״̬
		const exception_pointer&,//rpc�쳣
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
	�����ĳ���źͰ汾��.
	translation:�������ģ��
	verify:�����û�У���ģ��
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
		//���Կ�ʼ������
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
	����һ������
	sync:�Ƿ�ͬ������
	stub:��������ǰ���id��׮
	args:��������
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
		���ﲻ�ö�stub�Ĵ�����з���������
		��Ϊ���ڵ������߳�ִ�еĴ�������������������ֲ�����
		������Ǻõ�
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

		//TODO.����result����֧�ֿյĹ���,�����а취��������Ҫ
		typename Stub::result_type result;

		try
		{
			/*
			û��д���� lock.wait() dosomething without lambda;
			��Ϊ��ҪRPCMSG_CALLBACK_ARG�������������߳�.
			������һ���Ժ��ٴ�wait_handle���ȡ̫�鷳��
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
					//�������һ�����׳��쳣
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
			if(_have(dest,xid))//��ֹ�ӳ��쳣�����ʱ���յ�����Ϣ
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
				//�Ƿ���Ҫ�ŵ�����߳��Լ����Ϸ�����?
				callback(handle, io_success, io_code,
					_make_exception(RPCMSG_CALLBACK_USE), 0);
			}
		};

		/*
		����Ҫ������,���ɼ�timed_invoke
		*/
		auto memory = stub.push(args...);
		const_memory_block stack = memory;
		_call(addr, ms, stub.ident(), stack, fn, false);
	}

	/*
	�첽����һ������
	stub:��������ǩ����id��׮
	callback:��rpc����ִ����ϻᱻ����
	args:�����Ĳ���
	*/
	template<class Stub, class Func, class... Arg>
	void async_invoke_than(const Stub& stub, const Func& callback, Arg&&... args)
	{
		//-1 forever;
		async_timed_invoke_than(-1, stub, callback, args...);
	}

	/*
	��û��callback,������async_invoke_than��ͬ
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
	�ض��������
	*/
	virtual void redire(const address_type& address)
	{
		//	_translation->redire(address);
	}

	/*
	�رտͻ��˺ͷ�����������
	*/
	virtual void close() throw()
	{
		_translation->close();
	}
private:
	/*
	�����첽invoke�ĳ�ʱ,ͬ��invoke����timed_wait
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
		//WAR.����
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
	�������͸���memory���������ڿ���
	����֧���첽��������rpc��Ϣ
	*/
	void _process_package_impl(const async_handle& handle,
		bool io_success, const error_code& io_code,
		const address_type& from, const_memory_block memory)
	{
		//io�쳣�Ļ���һ�����е�
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
		data:����ֵ���û��޸ĵĲ��� or �쳣�ṹ
		*/

		reply_body reply;
		advance_in(memory, rarchive(memory, reply));

		/*
		���:xid,�û����
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
				//��Щʲô
			}
			else
			{
				//������,Ȼ��ֱ��return
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
		�����������������쳣��ֱ�ӵ��³������
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
			WAR.����һ���ڴ治�㵼�µ��쳣��ʱ���޷��ָ�
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
	��success��ʱ��memory_blockΪ��
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
		/* ��ʱ����ʲô
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
			//��������һ˲��û�б�set��
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
		Ϊ��Ϊ�첽callback�ṩ�쳣��Ϣ�������ָ�����������
		�������ָ����Ҫthrow�Ļ�����Ҫ��ָ��
		*/
		if (rpc_code == static_cast<size_type>(session_timeout))
		{
			//WAR.��ʱ����������.�Լ����û��throw
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
	{//TODO.������������������

		T exception;
		rarchive(expmemory, exception);
		if (do_throw)
		{
			throw exception;
		}
		return std::make_shared<T>(exception);
	}

	void _make_rpc_exception_and_raise(RPCMSG_CALLBACK_ARG)
	{//���뱣֤����success

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
//���exceptino��Ч,���dynamic_cast exceptionȻ���׳�
void throw_rpc_exception(const std::shared_ptr<rpc_exception>& exception);

#endif