#ifndef IMOUTO_SIMPLE_TRANSLATION_KERNEL_HPP
#define IMOUTO_SIMPLE_TRANSLATION_KERNEL_HPP

#include <udt.hpp>

#include <thread>
#include <functional>

#include <boost/noncopyable.hpp>

#include <imou-rpc-client/imou_client_lib.hpp>

#include <mpl_helper.hpp>
inline std::ostream& operator<<(std::ostream& ost, const core_sockaddr& addr)
{
	return ost;
}
class simple_translation_kernel
	:public boost::noncopyable
{
	struct _group_info
	{
		core_sockaddr dest;
		shared_memory_block<char> memory;
	};
public:
	typedef std::size_t size_type;
	typedef std::size_t error_code;
	typedef core_sockaddr address_type;
	typedef core_socket socket_type;
	typedef udt::group_handle group_handle;
	typedef shared_memory_block<char> shared_memory;
	typedef const shared_memory_block<const char> const_shared_memory;

	typedef std::function < void(
		bool io_success, const error_code& code,
		const address_type& source, shared_memory) > callback_type;

	simple_translation_kernel(const address_type& addr)
		:_core(addr, MEMBER_BIND_IN(_bad_send))
	{
		_splock = BOOST_DETAIL_SPINLOCK_INIT;
		_group_splock = BOOST_DETAIL_SPINLOCK_INIT;
		_core.set_send_callback(MEMBER_BIND_IN(_success_send));
		_start_recver();
	}

	std::shared_ptr<boost::thread> _recver;

	~simple_translation_kernel()
	{
		close();
		if (_recver)
		{
			_recver->join();
		}
	}

	void connect(const address_type& dest)
	{//保证不能有其他线程访问
		_dest = dest;
	}

	void disconnect() throw()
	{
		this->close();
	}

	void redire(const address_type& dest)
	{
		auto lock = ISU_AUTO_LOCK(_splock);
		_dest = dest;
	}

	void close() throw()
	{
		_core.close();
		for (auto& pair :_groups)
		{
			auto& value = pair.second;
			_callback(false, 0, value.dest, value.memory);
		}
	}	

	group_handle send(const_memory_address buffer, size_type size)
	{
		auto lock = ISU_AUTO_LOCK(_splock);
		auto tmp = _dest;
		lock.unlock();
		auto handle = send(tmp, buffer, size);
	//	cancel_io(handle, true);
		return handle;
	}

	group_handle send(const core_sockaddr& dest,
		const_memory_address buffer, size_type size)
	{
		//TODO.修改掉两次复制的问题
		shared_memory memory(deep_copy(const_memory_block(buffer, size)));
		auto id = _core.alloc_group_id();

		if (!is_multicast(dest))
			_register_group(id, dest, memory);

		auto result = _core.send(dest, id, memory);

		if (!result.first.in_vaild())
		{
			_erase_group(id);
			if (_callback)
				_callback(false,
				result.first.system_socket_error(), dest, memory);
		}
		return result.second;
	}

	void when_recv_complete(const callback_type& callback)
	{
		_callback = callback;
	}

	void cancel_io(const group_handle& handle, bool active_io_failed = true)
	{
		_erase_group(handle->id());
		_core.cancel_io(handle, active_io_failed);
	}
private:

	boost::detail::spinlock _splock;

	boost::detail::spinlock _group_splock;

	address_type _dest;
	callback_type _callback;
	udt _core;
	std::map<udt::group_id_type,_group_info> _groups;
	void _register_group(udt::group_id_type id,const address_type& addr,
		const shared_memory& memory)
	{
		_group_info group;
		group.dest = addr;
		group.memory = memory;
		auto lock = ISU_AUTO_LOCK(_group_splock);
		_groups.insert(std::make_pair(id, group));
	}

	inline void _start_recver()
	{
		auto* thread=new boost::thread([&]()
		{
			while (true)
			{
				auto result = _core.recv();
				if (!result.io_state.in_vaild())
					break;
				if (_callback)
				{
					const auto& stat = result.io_state;
					_callback(true, stat.system_socket_error(),
						result.from, result.memory);
				}
			}
		});

		_recver.reset(thread);
	}

	void _erase_group(udt::group_id_type id)
	{
		auto lock = ISU_AUTO_LOCK(_group_splock);
		_groups.erase(id);

		/*测试用
		auto iter = _groups.find(id);
		if (iter == _groups.end())
		{
			mylog.log(log_debug, "wtf", id);
		}
		else
			_groups.erase(iter);
			*/
	}

	std::atomic_size_t value = 0;
	void _success_send(udt::group_id_type id, const core_sockaddr& from,
		const const_memory_block& memory)
	{
		++value;
		_erase_group(id);
	}

	void _bad_send(udt::group_id_type id, const core_sockaddr& from,
		const const_memory_block& memory)
	{
		/*
			//TODO.一个分片挂了就全挂太浪费了
		*/
		auto lock = ISU_AUTO_LOCK(_group_splock);
		auto iter = _groups.find(id);
		if (iter != _groups.end())
		{
			auto group_memory = iter->second.memory;
			_groups.erase(iter);//尽早释放锁
			lock.unlock();
			if (_callback)
				_callback(false, 0, from, group_memory);
			return;
		}
		lock.unlock();
		return;
	}
};

#endif