#ifndef IMOUTO_RPC_CLIENT_LOG_HPP
#define IMOUTO_RPC_CLIENT_LOG_HPP

#define RPC_CLIENT_INITED "rpc client inited"

#define RPC_CLIENT_DESTORYED "rpc client destoryed"

#define RPC_CLIENT_LOG_XID "rpc client log xid:%1%"

#define RPC_CLIENT_EXTERNAL_EXCEPTION \
	"rpc client has external exception type:%1% what:%2%"

#define RPC_CLIENT_INTERNAL_EXCEPTION \
	"rpc client has internal exception type:%1% what:%2%"

#define RPC_CLIENT_CALL_STUB \
	"rpc client call local stub-prog:%d proc:%d ver:%d"

#define RPC_CLIENT_CHECK_SERVER_IDENT\
	"rpc client check server ident"

#define RPC_CLIENT_CHECK_SERVER_IDENT_FAILED \
	"rpc client check server ident failed"

#define RPC_CLIENT_ERROR "rpc client error code:%1%"
#define RPC_CLIENT_THROW_USER_EXCEPTION RPC_CLIENT_EXTERNAL_EXCEPTION
/*
	server 和 client的process msg消息,用的都是同一个宏
*/
#endif