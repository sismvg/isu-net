#ifndef ISU_TRANS_CORE_HPP
#define ISU_TRANS_CORE_HPP

#include <isu-net/socket.hpp>
#include <ex-utility/memory/memory_helper.hpp>
#include <ex-utility/multiplex_timer.hpp>

class trans_core_impl;

typedef unsigned long package_id_type;

enum trans_error_code{
	//��ͷ��Ԥ�����ڴ治��
	not_enough_for_head = 1,
	//������߳��˳�
	recv_thread_exit,
	//�ں�socket����
	has_system_socket_error,
	//�ڴ治��
	memory_not_enough,
	//�������ڴ治�������C++�쳣
	have_cpp_exception,
	//�ش���ι���,������û�д���ɹ�
	send_faild,
};

typedef multiplex_timer::timer_handle timer_handle;

struct trans_trunk
{
	trans_trunk()
		:local_handle(nullptr)
	{}
	timer_handle local_handle;
};

struct _trans_group
{
	uint32_t magic;
	package_id_type id;
	uint16_t rtt, window;
	trans_trunk trunk;
};

struct retrans_trunk
{
	retrans_trunk(const _trans_group& group,
		const shared_memory& buf, const core_sockaddr& addr)
		:id(group.id), buffer(buf), address(addr), retry_limit(0)
	{
	}

	package_id_type id;

	uint32_t retry_limit;
	std::atomic_size_t retry_count;
	timer_handle timer;

	shared_memory buffer;
	core_sockaddr address;
};

typedef std::shared_ptr<retrans_trunk> io_mission_handle;

struct io_result
{//BE RULE3:�����ϻᱻinline�ĺ�������д��ͷ�ļ���
	typedef int error_code;
	/*
	����ʵ�����,��Ҫ����
	*/
	io_result()
		:_code(0), _system_error(0), _id(nullptr)
	{}

	io_result(const io_mission_handle& id,
		error_code local_code = 0, error_code system_code = 0)
		:_id(id), _code(local_code), _system_error(system_code)
	{}

	/*
	I/O�����Ƿ�ɹ�����
	*/
	inline bool in_vaild() const
	{
		return _code == 0 && _system_error == 0;
	}

	/*
	trans_core��I/O����������ʲô����
	*/
	inline error_code what() const
	{
		return _code;
	}

	/*
	�ں�socket������ʲô����
	*/
	inline error_code system_socket_error() const
	{
		return _system_error;
	}

	/*
	��������ݵı�ʶ
	*/
	inline const io_mission_handle& id() const
	{
		return _id;
	}
private:
	error_code _code;
	error_code _system_error;
	io_mission_handle _id;
};

#define WORK_DEFAULT 0x0
#define POST_SEND_COMPLETE 0x1

#define SEND_COMPLETE 0x0
#define FAILED_SEND_COMPLETE 0x1
#define RECV_COMPLETE 0x10
#define EXIT_COMPLETE 0x100

struct complete_io_result
{
public:
	typedef std::size_t size_t;
	typedef char bit;
	typedef const char const_bit;
	typedef bit* iterator;
	typedef const_bit* const_iterator;
	typedef void* io_mission_handle;
	typedef complete_io_result self_type;

	typedef shared_memory_block<const char> const_shared_memory;
	typedef shared_memory_block<char> shared_memory;

	/*
		����,ʵ�����
	*/
	complete_io_result();
	complete_io_result(const io_result&, const core_sockaddr& from,size_t io_type,
		const shared_memory& memory, size_t size);

	/*
		˭���͹�����
	*/
	const core_sockaddr& from() const;

	/*
		����I/O����ʱ��I/O״̬
	*/
	const io_result& get_io_result() const;

	/*
		��Ч��������,������䲻����ͷ��
	*/
	iterator begin();
	iterator end();
	const_iterator begin() const;
	const_iterator end() const;

	/*
		udp�����е�ͷ��Ԥ��
	*/
	size_t reserve();

	/*
		���io������
	*/
	size_t io_type() const;
	/*
		����ͷ����ԭʼ�ڴ�
	*/
	shared_memory raw_memory();
	const_shared_memory raw_memory() const;
private:
	size_t _io_type;
	io_result _io_error;
	core_sockaddr _from;
	shared_memory _memory;
	size_t _size;
	iterator _begin();
	const_iterator _begin() const;
};

typedef complete_io_result recv_result;
#define DEFAULT_ALLOCATOR std::allocator<char>


struct retrans_control
{
	typedef std::size_t size_t;
	retrans_control(size_t first_time = 64, size_t retry = 15)
		:first_timeout(first_time), retry_limit(retry)
	{
	}
	size_t first_timeout, retry_limit;
};


class trans_core
{
	//����֤˳����
	//�ڳ����ش�������ǰ,���ᶪ��
public:
	/*
		trans_core_implԤ����ͷ������
	*/
	static size_t trans_core_reserve();
	/*
		һ�����ݷ�Ƭ��߿����ж೤(������ͷ��)
	*/
	static size_t maxinum_slice_size();

	typedef shared_memory_block<const char> const_shared_memory;
	typedef shared_memory_block<char> shared_memory;

	typedef sockaddr_in core_sockaddr;
	typedef std::function < void(const core_sockaddr&,
		const const_shared_memory&, io_mission_handle) > bad_send_functional;
	/*
		���ص�ַ
		limit_thread:���ڴ���recv�ķ���ֵ������߳�����
		���Ե���windows��I/O��ɶ˿�ʹ��
	*/
	trans_core()
	{}
	trans_core(const core_sockaddr&, size_t limit_thread = 1);
	trans_core(const core_sockaddr&,
		bad_send_functional bad_send, size_t limit_thread = 1);
	//���ͷ�����Ԥ��trans_core_reserve���ֽڵ�ͷ��

	/*
		װ��Ҫ���͵����ݵ�ͷ��
		�����memory.size()С��Ԥ����ͷ������
		io_resul��waht�᷵��not_enough_for_head
	*/
	io_result assemble_send(const shared_memory& memory,
		const core_sockaddr& dest, const retrans_control* ctrl_ptr = nullptr);

	/*
		����һ���Ѿ�װ���ͷ��������
	*/
	io_result send_assembled(const shared_memory& memory, const core_sockaddr& dest);

	/*
		��������,ͷ��װ�����ڲ�����
	*/
	io_result send(const shared_memory&, const core_sockaddr&,
		const retrans_control* ctrl_ptr = nullptr);

	/*
		�������ݲ�����,�������޶����ܵ�ַ
	*/
	recv_result recv();

	void set_flag(size_t flag);

	void close();

	bool cancel_io(const io_mission_handle& handle, bool active_io_faild);

	typedef int raw_socket;
	
	/*
		�ں�socket
	*/
	raw_socket native() const;
private:
	std::shared_ptr<trans_core_impl> _impl;

	static size_t trans_core_reserve_size;
	static size_t maxinum_slice_data_size;
};
#endif