#ifndef BASIC_RPC_SERVER_HPP
#define BASIC_RPC_SERVER_HPP

#include <boost/smart_ptr/shared_ptr.hpp>

#include <imou-rpc-kernel/imouto_rpc_kernel.hpp>

/*
	提供了消息解包功能
*/

#define PROCESS_MSG_ARGUMENT bool,const error_code&,\
	const address_type&,const memory_block&

#define PROCESS_MSG_ARGUMENT_NAME io_success,io_code,from,memory

#define PROCESS_MSG_ARGUMENT_WITH_NAME bool io_success, const error_code& io_code,\
const address_type& from, const shared_memory& memory

template<class TranslationKernel>
class basic_rpc
	:public imouto_rpc_kernel < typename TranslationKernel::address_type >
{
public:

	typedef TranslationKernel translation_kernel;
	typedef typename translation_kernel::address_type address_type;
	typedef typename translation_kernel::error_code error_code;
	typedef imouto_rpc_kernel<address_type> father_type;

protected:
	typedef std::function < void(
		const_shared_memory, PROCESS_MSG_ARGUMENT) > processor;

	void process_msg(PROCESS_MSG_ARGUMENT_WITH_NAME, processor fn)
	{
		std::size_t count = 0;
		for_each_memory_block(static_cast<memory_block>(memory),
			[&](memory_block msg_body)
		{
			fn(memory, io_success, io_code, from, msg_body);
			++count;
		});
	}
};
#endif