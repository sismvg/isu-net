#ifndef IMOUTO_RPC_SERVER_LOG_HPP
#define IMOUTO_RPC_SERVER_LOG_HPP

/*
	字符串宏
	允许修改为本地字串
*/

#define RPC_SERVER_RUNNING "rpc server running"
#define RPC_SERVER_STOP "rpc server stopped"
#define RPC_SERVER_BLOCKING "rpc server blocking"

//服务器初始化 debug
#define IMOUTO_RPC_SERVER_INITED "rpc server inited"

//服务器处理一组消息包 detail
#define RPC_SERVER_PROCESSING_MSG "rpc server processing msg"

//服务器处理一个消息包 detail
#define PROCESSING_ONE_MSG "rpc server processing call-package size:%d"

//服务器遇到外部代码所引发的异常 debug
#define RPC_SERVER_EXTERNAL_EXCEPTION \
	"rpc server has external exception type:%s what:%1%"

//服务器遇到内部异常 warning
#define RPC_SERVER_INTERNAL_EXCEPTION \
	"rpc server hash internal exception type%1% what:%2%"

//服务器在一组消息包里处理了多少消息 detail
#define PROCESSED_MSG_COUNT "rpc server porcessed msg count:%d"

//reply时I/O失败 detail
#define RPC_SERVER_HAS_IO_FAILED "rpc server reply I/O failed code:%1 dest:%2%"

//服务器设置某个xid的值 detail
#define RPC_SERVER_XID_SET "set xid:%1 value:%2%"

//记录xid的值 detail
#define RPC_SERVER_LOG_XID "logging xid:%1%"

/*
	服务器检查消息的有效性
	包括了:rpc版本,用户身份,桩身份
	detail
*/
#define RPC_SERVER_CHECK_VERSION "rpc server check version"
#define RPC_SERVER_CHECK_VERSION_FAILED "rpc server check version failed ver:%1%"

#define RPC_SERVER_CHECK_USER_IDENT "rpc server check user idnet"
#define RPC_SERVER_CHECK_UER_IDENT_FAILED "rpc server check user ident failed"

#define RPC_SERVER_CHECK_STUB_EXIST "rpc server check stub"
#define RPC_SERVER_UNKNOW_STUB \
	"rpc server stub not exist-prog:%1%--proc:%2%--ver:%3%"
//---------------------------------

//遇到了外部代码引发的C异常
#define RPC_SERVER_C_EXCEPTION "rpc server external code has c exception"

#define RPC_SERVER_CALL_STUB "rpc server call stub from:%1%"
//服务器遇到未知的异常,包括未知位置 >=warning
#define RPC_SERVER_UNKNOW_EXCEPTION \
	"rpc server has unknow exception type:%1% what:%2%"

//服务器向客户端reply detail
#define RPC_SERVER_REPLY "rpc server reply to:%1%"

//日志功能初始化成功 debug
#define RPC_SERVER_LOG_INITED "rpc server logging inited"



#endif