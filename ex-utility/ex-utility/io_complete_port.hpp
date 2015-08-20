#ifndef ISU_IO_COMPLETE_PORT
#define ISU_IO_COMPLETE_PORT

#include <ex-utility/ex_utility_lib.hpp>

#include <ex-config/platform_config.hpp>

#ifdef _WINDOWS
#include <windows.h>
#else

#endif

#include <cstddef>
#include <cstdint>

#include <atomic>

#include <exception/exception.hpp>

struct complete_result
{
	complete_result()
		:key(0), user_key(0), argument(nullptr)
	{}

	complete_result(size_t ckey,
		size_t cuser_key, sysptr_t cargument)
		:key(ckey), user_key(cuser_key), argument(cargument)
	{}

	size_t key;
	size_t user_key;
	sysptr_t argument;
};

DEFINE_EXCEPTION(complete_port_exception, std::exception);

DEFINE_HAVE_CODE_EXCEPTION(complete_port_bind_fail, complete_port_exception);

DEFINE_HAVE_CODE_EXCEPTION(complete_port_create_fail, complete_port_exception);

enum complete_port_error
{

};

class io_complete_port
{
public:

	typedef std::size_t size_t;
	typedef std::uint64_t ulong64;

	enum complete_port_msg{ complete_port_exit = 1 };
	/*
		limit_threadָ�������̳߳ص�����
	*/
	io_complete_port(size_t limit_thread = 1);
	io_complete_port(kernel_object_handle first_bind, size_t limit_thread = 1);
	~io_complete_port();

	/*
		ƽ̨���
	*/
	kernel_object_handle native() const;

	/*
		�󶨵�ĳ���豸��,���豸�ϵ�����I/O�����Ϣ���ᱻ
		post���ܵ�
		ƽ̨��ص�
	*/
	void bind(kernel_object_handle);

	/*
		post����,�ᱻĳ��get�е��̻߳�ȡ
		����ֵ��Ϊ0ʱ�ɹ�
		Ϊ0ʱʧЧ
	*/
	size_t post(size_t key, sysptr_t);
	
	/*
		ͬ��
	*/
	size_t post(complete_result);

	/*
		�ر���ɶ˿�
	*/
	void close();
	/*
		get����,���������û�������������
			Ҳ����ʹ��timeout�����趨��ʱʱ��,��λΪ����
		-1��ʾ��Զ(��Ե�)
	*/
	complete_result get(size_t timeout = -1);

	/*
	δʵ��
	void set_dynamic_blance();
	*/
private:
	size_t _limit_thread;
#ifdef _WINDOWS
	std::atomic_size_t _complete_port;
	void _set_handle_value(kernel_object_handle);
	void _init(kernel_object_handle, size_t);
	void _try_throw_last_error();
#endif

#ifdef _LINUX

#endif
};

#endif