#ifndef IMOUTO_RPC_SERVER_HPP
#define IMOUTO_RPC_SERVER_HPP

#include <ex-config/platform_config.hpp>

#include <ex-utility/memory/memory_helper.hpp>
#include <ex-utility/dynamic_bitmap.hpp>

#include <imou-rpc-kernel/imouto_rpc_kernel.hpp>
#include <imou-rpc-server/imouto_rpc_server_log.hpp>
#include <imou-rpc-server/server_stub_pool.hpp>
#include <imou-rpc-server/basic_rpc_server.hpp>

#ifdef _WINDOWS
#include <excpt.h>
#endif

/*
//今天要完成的
	性能数据接口
	接口的修改
	任务控制和重启控制
	代码解耦-非模板代码提取

	//明天
	对小型函数的本线程调用(可能有危险)
	xid的问题
	虚拟连接的问题
	des验证完成编写
	压力测试
	外网测试
	和rpcbind一起测试

	//决议事项,支持这两个的话会用掉太多的内存还有对批处理的支持不完全
	批处理 - ex_lambda fn-<-<-... <-怎么在只分配一次内存的情况下保存多个桩?
	同步超时取消I/O
	由于I/O问题导致的封装打破如何修复
*/

template<
	class TranslationKernel,
	class Verify,
	class ThreadPool>
class imouto_rpc_server
	:
		public imouto_typedef<TranslationKernel,Verify>,
		public basic_rpc<TranslationKernel>
{
public:
	typedef basic_server_stub<stub_ident> server_stub;
	typedef stub_pool<> stub_pool_type;
	typedef ThreadPool thread_pool;

	template<class Trans,class Verify,class ThreadPool,class StubPool>
	imouto_rpc_server(const Trans& trans,const Verify& verify,
		const ThreadPool& pool,const StubPool& stubs)
	{
		_init(trans, verify, pool, stubs);
	}

	~imouto_rpc_server()
	{
		if (_translation)
			_translation->close();
		_threads->wait();
	}

	void run()
	{
	}

	void stop()
	{
	}

	void blocking()
	{
	}
private:
	/*
		传输模块
	*/
	std::shared_ptr<translation_kernel> _translation;
	/*
		身份验证
	*/
	std::shared_ptr<verify> _verify;
	/*
		负责实际函数的调用还有一些其他工作
	*/
	std::shared_ptr<thread_pool> _threads;
	/*
		保存了指向实际函数的桩
	*/
	std::shared_ptr<stub_pool_type> _stubs;

	template<class Trans,class Verify,class ThreadPool,class StubPool>
	void _init(const Trans& trans,const Verify& verify,
		const ThreadPool& pool, const StubPool& stubs)
	{
		//_xidmap_lock = BOOST_DETAIL_SPINLOCK_INIT;
		_translation = trans;
		_verify = verify;
		_threads = pool;
		 _stubs = stubs;
		//一个都不能少!
		if (!(trans&&verify&&pool&&stubs))
			return;//throw something
		
		trans->when_recv_complete([&](PROCESS_MSG_ARGUMENT_WITH_NAME)
		{
			process_msg(PROCESS_MSG_ARGUMENT_NAME,
				MEMBER_BIND_IN(_process_rpcmsg_impl));
		});
	}

	//和client长得很像..


	void _process_rpcmsg_impl(const async_handle& handle,
		bool io_success, const error_code& io_code,
		const address_type& from, memory_block memory)
	{
		//检测io错误-检测版本-检测用户-检测桩是否存在-调用-捕捉异常和参数数据
		//-回送

		if (!io_success)
		{
			//没啥好做的
			return;
		}

		auto* body = reinterpret_cast<rpc_call_body*>(memory.buffer);
		auto& call_body = body->body.body;

		reply_body reply;

		reply.head = body->body.head;
		reply.head.magic = msg_type::replay;
		reply.trunk = body->trunk;

		//按照rpcmsg.hpp中指定的格式
		advance_in(memory, archived_size(*body));

		//只要遇到一次失败就会由函数发送回馈
		_check_xid(from, reply.head.xid)
		&&
		_check_version(from, reply, call_body.rpcvers)
		&&
		_check_user(from, reply, body->ident)
		&&
		_check_stub(from, handle, reply, memory, body);
	}

	bool _check_xid(const address_type& from,const xid_type& xid)
	{
		try
		{
			return !set_xid(from, xid, true);
		}
		catch (std::invalid_argument&)
		{
			return false;
		}
		return false;
	}

	bool _check_version(const address_type& from,
		reply_body& reply, const xdr_uint32& vers)
	{
		bool ret = true;
		if (vers != IMOUTO_RPC_VERS)
		{
			ret = false;
			reply.what = msg_denied;
			reply.stat = reject_stat::rpc_mismatch;

			mismatch_info info(0, IMOUTO_RPC_VERS);
			_reply(from, reply, info);
			
		}
		return ret;
	}

	bool _check_user(const address_type& from,
		reply_body& reply, verify_ident& ident)
	{
		bool ret = true;
		auto check_result = _verify->check_verify(ident);
		if (!check_result)
		{
			reply.what = msg_denied;
			reply.stat = reject_stat::auth_error;
			ret = false;
			_reply(from, reply, check_result);
		}
		const auto& reply_ident = _verify->reply_verify(ident);
		reply.ident = reply_ident;
		return ret;
	}

	bool _check_stub(
		const address_type& from,
		const async_handle& handle,reply_body& reply,
		const memory_block& memory,rpc_call_body* body)
	{

		find_stat ret = find_success;
		auto& call_body = body->body;
		auto ident=body->body.body.ident;
		auto pair = _stubs->find(ident);

		if ((ret = pair.second)==find_success)
		{
			memory_block arguments;
			rarchive(memory, arguments);
			shared_memory tmp(arguments);

			verify_xid(from, body->body.head.xid);
			_threads->schedule([&,pair,body,arguments,from,reply,handle,tmp]()
			{
				_call_stub(from, reply, *pair.first->second,
					handle, arguments, body);
			});
		}
		else
		{
			reply.what = msg_accepted;
			reply.stat = pair.second;
			_reply(from, reply);

		}
		return ret == find_success;
	}

	void _call_stub(const address_type& from,reply_body reply,const server_stub& stub,
		const async_handle& handle,const memory_block& memory, rpc_call_body* body)
	{
		reply.what = msg_accepted;
#ifdef _WINDOWS
		__try
		{
#endif
			/*
				由于[同一个函数只能有一个异常处理块],所以只能把cpp异常放到这里
			*/
			_call_stub_with_cppexp(from, reply, stub,
				handle, memory, body);
			
#ifdef _WINDOWS
		}
		__except (_my_filter(from,reply,
			GetExceptionCode(), GetExceptionInformation()))
		{
			//没法上日志..因为有destructor
		}
#endif
	}

	int _my_filter(const address_type& from,reply_body& reply,
		unsigned int code, _EXCEPTION_POINTERS* info)
	{
		//WAR.考虑到平台问题只能给个code
		rpc_remote_platform_exception error(
			"Rpc invoke fail,because has platform exception");
		auto data = archive(error);
		reply.stat = proc_c_exception;
		_reply(from, reply, data);
		return EXCEPTION_EXECUTE_HANDLER;
	}

	void _call_stub_with_cppexp(const address_type& from, 
		reply_body& reply, const server_stub& stub,
		const async_handle& handle, const memory_block& memory, rpc_call_body* body)
	{
		shared_memory exdata;
		try
		{
			auto result_and_arg = stub(memory);

			exdata = result_and_arg;
			reply.stat = success;
			_reply(from, reply, static_cast<memory_block>(exdata));
			return;
		}
		catch (std::runtime_error& error)
		{
			exdata = archive(rpc_exception(error.what(), true));
		}
		catch (...)
		{
			exdata = archive(rpc_exception(
				RPC_SERVER_UNKNOW_EXCEPTION, true));
		}
		//WAR.enum值不对
		reply.stat = proc_cpp_exception;
		_reply(from, reply, static_cast<memory_block>(exdata));
	}

	template<class... Arg>
	void _reply(const address_type& from, const Arg&... args)
	{
		shared_memory mem_ptr = archive(archived_size(args...), args...);
		const_memory_block mem = mem_ptr;

		_translation->send(from, mem.buffer, mem.size);
	}


};

#endif