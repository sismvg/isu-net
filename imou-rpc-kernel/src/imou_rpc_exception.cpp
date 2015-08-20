
#include <imou-rpc-kernel/imouto_rpc_exception.hpp>

#include <boost/any.hpp>
#include <sul-serialize/archive.hpp>

#ifdef _WINDOWS
#include <windows.h>

rpc_exception::rpc_exception(const char* msg /* = nullptr */, bool local /* = false */)
	:father_type(msg)
{
	if (msg)
	{
		_string_what = std::string(msg);
	}
}

const xdr_string& rpc_exception::rpc_what() const
{
	return _string_what;
}

rpc_exception::~rpc_exception()
{
}

void rpc_local_platform_exception::raise()
{
	const auto& record = this->info<EXCEPTION_POINTERS>().ExceptionRecord;
	RaiseException(record->ExceptionCode, record->ExceptionFlags,
		record->NumberParameters, record->ExceptionInformation);
}

#endif