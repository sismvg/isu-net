#ifndef ISU_UDT_HPP
#define ISU_UDT_HPP

#include <map>

#include <ex-utility/slice.hpp>
#include <isu-net/trans_core.hpp>
#include <ex-utility/multiplex_timer.hpp>

#include <boost/noncopyable.hpp>
//事实上这个类只是个带分片重组功能的wapper

struct udt_recv_result
{//下面的都已经抽象了,也没必要再加啥了
	io_result io_state;
	typedef shared_memory_block<char> shared_memory;
	shared_memory memory;
	core_sockaddr from;
};

/*
	自旋指针.
	当指针被修改时不可访问
*/
template<class T>
class spinpointer
{
public:
	typedef T* sysptr_t;
	typedef spinpointer<T> self_type;
	spinpointer(sysptr_t ptr = nullptr)
		:_ptr(ptr)
	{
		_locker = BOOST_DETAIL_SPINLOCK_INIT;
	}
	/*
		返回当前指针
	*/
	sysptr_t lock() const
	{
		_lock();
		return _ptr;
	}

	std::pair<bool,sysptr_t> try_lock() const
	{
		bool locked = false;
		sysptr_t value = nullptr;
		if (_locker.try_lock())
		{
			locked = true;
			value = _ptr;
		}
		return std::make_pair(locked, value);
	}

	void unlock() const
	{
		_unlock();
	}

	sysptr_t set(sysptr_t ptr)
	{
		sysptr_t ret = lock_and_set(ptr);
		unlock();
		return ret;
	}
	/*
		返回当前指针并设定
	*/
	sysptr_t lock_and_set(sysptr_t value)
	{
		_lock();
		sysptr_t tmp = _ptr;
		_ptr = value;
		return tmp;
	}

	/*
		warning no thread safe
	*/
	sysptr_t get() const
	{
		return _ptr;
	}
private:
	void _lock() const
	{
		const_cast<self_type*>(this)->_locker.lock();
	}
	void _unlock() const
	{
		const_cast<self_type*>(this)->_locker.unlock();
	}
	boost::detail::spinlock _locker;
	sysptr_t _ptr;
};

struct group_list_node
{
	typedef group_list_node self_type;
	group_list_node()
		:pre(nullptr), next(nullptr)
	{
	}

	inline void detach() const
	{
		if (pre)
		{
			pre->next = next;
		}
		if (next)
		{
			next->pre = pre;
		}
	}

	inline void connect(group_list_node* node)
	{
		next = node;
		if (node)
		{
			node->pre = this;
		}
	}
	group_list_node* pre;
	group_list_node* next;
	//WAR.
	io_mission_handle* value;
};

#define FETCH(node,func,name)\
		auto* start=node->name;\
		while(start)\
						{\
			func(start);\
			auto* tmp = start->name;\
			start->name;\
			start = tmp;\
			++ret;\
		}

template<class Func>
std::size_t forward_fetch(const group_list_node* anynode, const Func& func)
{
	std::size_t ret = 0;
	FETCH(anynode, func, pre);
	return ret;
}

template<class Func>
std::size_t back_fetch(const group_list_node* anynode, const Func& func)
{
	std::size_t ret = 0;
	FETCH(anynode, func, next);
	return ret;
}

template<class Func>
std::size_t fetch_group_list(const group_list_node* anynode, const Func& func)
{
	std::size_t node_count = 0;
	if (anynode)
	{
		func(anynode);
		++node_count;
		node_count += forward_fetch(anynode, func);
		node_count += back_fetch(anynode, func);
	}
	return node_count;
}

class udt;

class udt_group
{
	typedef std::size_t size_type;
public:
	udt_group()
		:_group_id(0), _count(0), _geted(0)
	{
		_root.value = nullptr;
		_locker = BOOST_DETAIL_SPINLOCK_INIT;
	}

	inline bool bad_handle() const
	{
		return !_root.value || (*_root.value)->retry_limit != -1;
	}

	const core_sockaddr& dest() const
	{
		return _dest;
	}

	const size_type& id() const
	{
		return _group_id;
	}
private:
	friend class udt;
	size_type _group_id;
	group_list_node _root;
	core_sockaddr _dest;
	std::atomic_size_t _count;
	std::atomic_size_t _geted;
	boost::detail::spinlock _locker;
	void _set_bad()
	{
	}
};

class udt
	:boost::noncopyable
{
public:
	typedef shared_memory_block<const char> const_shared_memory;
	typedef shared_memory_block<char> shared_memory;
	typedef char bit_type;
	typedef size_t group_id_type;
	typedef std::shared_ptr<udt_group> group_handle;
	typedef std::function<void(group_id_type, const core_sockaddr&,
		const const_memory_block&)> bad_send_functional;

	/*
		创建一个udp socket并绑定地址
		limit_thread指定了可以最多调用recv的线程数量(I/O完成端口)
		bad_send指定了在包发送失败时该如何处理
	*/
	udt()
		:_groups_lock(BOOST_DETAIL_SPINLOCK_INIT)
	{}
	udt(const core_sockaddr&, size_t limit_thread = 1);
	udt(const core_sockaddr&, bad_send_functional bad_send,
		size_t limit_thread = 1);

	/*
		接受数据,错误代码与数据全部放入udt_recv_result中
	*/
	udt_recv_result recv();

	/*
		接受数据,但由放入由用户指定的内存当中
		并且只能接受单个分片.因为无法确定发送者。
		也就没法确定buffer的长度,这回导致溢出
	*/
	io_result recv(bit_type* buffer, size_t size, core_sockaddr&);

	typedef std::pair<io_result, group_handle> send_result;
	/*
		发送一块数据,send是异步的
		io_result标记了内核socket和用户态socket的状态
	*/
	send_result send(const core_sockaddr&,
		const const_shared_memory& data,
		size_t size = 0, const retrans_control* ctrl_ptr = nullptr);

	send_result send(const core_sockaddr&, size_t id,
		const const_shared_memory& data, 
		size_t size = 0, const retrans_control* ctrl_ptr = nullptr);

	send_result send_block(const core_sockaddr& dest, size_t id,
		const_memory_block block, 
		size_t size = 0, const retrans_control* ctrl_ptr = nullptr);

	typedef bad_send_functional send_callback;
	void set_send_callback(send_callback);
	/*
		关闭socket
	*/
	void close();
	
	/*
		系统socket
	*/
	int native() const;
	/*
		
	*/
	void setsocket(const core_sockaddr&);
	size_t alloc_group_id();
	void cancel_io(const group_handle&, bool ative_io_failed = true);
private:
	send_callback _send_callback;
	//当一个组长时间收不到分片,则用这个来提醒
	//	multiplex_timer _group_timer;

	//trans_core_head-group_id-start
	typedef boost::detail::spinlock spinlock_t;
	struct _group_ident
	{
		group_id_type group_id;
		size_t slice_start;
		size_t data_size;
		spinlock_t* locker;
		group_list_node node;
	};

	struct _recv_group
	{//由于保证不会重复包.所以不需要bitmap啥的记录是否收到过
		combination _combin;
	};

	spinlock_t _groups_lock;
	std::map<group_id_type, _recv_group> _recv_groups;

	spinlock_t _sending_groups_lock;
	std::map<group_id_type,group_handle> _sending_groups;

	trans_core _core;
	bad_send_functional _bad_send;

	void _udt_bad_send(const core_sockaddr& dest,
		const const_shared_memory& memory, io_mission_handle id);
	shared_memory _insert(const _group_ident& ident, 
		const core_sockaddr& from,
		const const_memory_block& data);
	void _cancel_slice(const group_list_node* node, bool active_failed = false);
	void _cancel_group(const group_list_node* node);
	std::atomic<group_id_type> _group_id_allocer;
	group_id_type _alloc_group_id();
	void _erase_group(group_id_type id, bool tick, const core_sockaddr& from);
};
#endif