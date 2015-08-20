#ifndef ISU_UDT_HPP
#define ISU_UDT_HPP

#include <map>

#include <ex-utility/slice.hpp>
#include <isu-net/trans_core.hpp>
#include <ex-utility/multiplex_timer.hpp>

#include <boost/noncopyable.hpp>
//��ʵ�������ֻ�Ǹ�����Ƭ���鹦�ܵ�wapper

struct udt_recv_result
{//����Ķ��Ѿ�������,Ҳû��Ҫ�ټ�ɶ��
	io_result io_state;
	typedef shared_memory_block<char> shared_memory;
	shared_memory memory;
	core_sockaddr from;
};

/*
	����ָ��.
	��ָ�뱻�޸�ʱ���ɷ���
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
		���ص�ǰָ��
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
		���ص�ǰָ�벢�趨
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
		����һ��udp socket���󶨵�ַ
		limit_threadָ���˿���������recv���߳�����(I/O��ɶ˿�)
		bad_sendָ�����ڰ�����ʧ��ʱ����δ���
	*/
	udt()
		:_groups_lock(BOOST_DETAIL_SPINLOCK_INIT)
	{}
	udt(const core_sockaddr&, size_t limit_thread = 1);
	udt(const core_sockaddr&, bad_send_functional bad_send,
		size_t limit_thread = 1);

	/*
		��������,�������������ȫ������udt_recv_result��
	*/
	udt_recv_result recv();

	/*
		��������,���ɷ������û�ָ�����ڴ浱��
		����ֻ�ܽ��ܵ�����Ƭ.��Ϊ�޷�ȷ�������ߡ�
		Ҳ��û��ȷ��buffer�ĳ���,��ص������
	*/
	io_result recv(bit_type* buffer, size_t size, core_sockaddr&);

	typedef std::pair<io_result, group_handle> send_result;
	/*
		����һ������,send���첽��
		io_result������ں�socket���û�̬socket��״̬
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
		�ر�socket
	*/
	void close();
	
	/*
		ϵͳsocket
	*/
	int native() const;
	/*
		
	*/
	void setsocket(const core_sockaddr&);
	size_t alloc_group_id();
	void cancel_io(const group_handle&, bool ative_io_failed = true);
private:
	send_callback _send_callback;
	//��һ���鳤ʱ���ղ�����Ƭ,�������������
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
	{//���ڱ�֤�����ظ���.���Բ���Ҫbitmapɶ�ļ�¼�Ƿ��յ���
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