// test.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"

#include <isu-net/des_verify.hpp>
#include <isu-net/datagram_tcp.hpp>

#include <imou-rpc-client/imouto_rpc_client.hpp>
#include <imou-rpc-server/imouto_rpc_server.hpp>
#include <imou-rpc-client/client_stub.hpp>

#include <sul-serialize/sul_serialize.hpp>
#include <sul-serialize/archive.hpp>

#include <boost/threadpool/pool.hpp>
#include <isu-net/des_verify.hpp>
//#include <vld.h>
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib,"sul-serialize.lib")
#pragma comment(lib,"imou-rpc-server.lib")
#pragma comment(lib,"imou-rpc-client.lib")
#pragma comment(lib,"imou-rpc-kernel.lib")
#pragma comment(lib,"ex-utility.lib")
#pragma comment(lib,"isu-net.lib")

/*
桩定义
*/

const unsigned int prog_id = 0x40010000;

auto pool = std::make_shared<stub_pool<>>(prog_id, 0);

int myfunc(int lhs, int rhs)
{
//	throw std::runtime_error("imouto no oupai prprpr xirai");
	return lhs + rhs;
}

const unsigned int ver = 0;
const unsigned int proc_id = 1;

GENERIC_SERVER_STUB((*pool), myfunc_server, prog_id, proc_id, 0, myfunc);

GENERIC_CLIENT_STUB(myfunc_client, prog_id, proc_id, 0, myfunc);

int _tmain(int argc, _TCHAR* argv[])
{
	typedef condition_filter<is_archiveable, xdr_filter> filter;
	typedef condition_filter<is_rarchiveable, xdr_filter> server_filter;

	typedef ::serialize_object<server_filter> server_object;
	typedef serialize_object<filter> serialize_object;
	typedef serialize<serialize_object> serializer;

	WSAData data;
	WSAStartup(MAKEWORD(2, 2), &data);

	core_sockaddr client_bind(AF_INET, htons(5151), inet_addr("127.0.0.1"));
	core_sockaddr server_bind(AF_INET, htons(5152), htonl(INADDR_ANY));

	core_sockaddr server_addr(AF_INET, htons(5152), inet_addr("127.0.0.1"));

	typedef datagram_tcp trans_client;
	typedef datagram_tcp_server trans_server;

	using namespace std;

	auto client_sock = make_shared<trans_client>(client_bind);
	auto server_sock = make_shared<trans_server>(server_bind);
	auto v=is_archiveable::functional<int, true>::value;
	Sleep(100);//用来等待服务器初始化完毕

	client_sock->connect(server_addr);

	typedef imouto_rpc_client<trans_client, des_verify> client_type;
	typedef imouto_rpc_server < trans_server,
		des_verify, boost::threadpool::pool > server_type;

	std::shared_ptr<client_type> client;
	std::shared_ptr<server_type> server;

	client.reset(new client_type(
		client_sock,
		make_shared<des_verify>()));

	server.reset(new server_type(
		server_sock,
		make_shared<des_verify>(),
		make_shared<boost::threadpool::pool>(2), pool));


//	cout << rvalue << endl;
	cout << "----------------------------" << endl;
	auto tick = GetTickCount();
	for (unsigned int index = 0; index != 1; ++index)
	{
		try
		{
			client->async_invoke_than(client_sock->native_socket(),
				[&](const const_shared_memory&,//异步句柄
				bool, const std::size_t&,//io状态
				const std::shared_ptr<rpc_exception>&,//rpc异常
				std::size_t)
			{
				cout << "ok" << endl;
			}, 20, 10);
		//	cout << index << endl;
		}
		catch (rpc_local_timeout& err)
		{
			cout << "Rpc timeout" << endl;
			cout << err.what() << endl;
		}
		catch (rpc_exception& err)
		{
			cout << err.rpc_what() << endl;
		}
		catch (...)
		{
			cout << "wtf" << std::endl;
		}
	}

	cout << (GetTickCount() - tick) << endl;
	system("pause");
	WSACleanup();
	return 0;
}

