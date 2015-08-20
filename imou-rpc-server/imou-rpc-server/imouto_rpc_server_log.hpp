#ifndef IMOUTO_RPC_SERVER_LOG_HPP
#define IMOUTO_RPC_SERVER_LOG_HPP

/*
	�ַ�����
	�����޸�Ϊ�����ִ�
*/

#define RPC_SERVER_RUNNING "rpc server running"
#define RPC_SERVER_STOP "rpc server stopped"
#define RPC_SERVER_BLOCKING "rpc server blocking"

//��������ʼ�� debug
#define IMOUTO_RPC_SERVER_INITED "rpc server inited"

//����������һ����Ϣ�� detail
#define RPC_SERVER_PROCESSING_MSG "rpc server processing msg"

//����������һ����Ϣ�� detail
#define PROCESSING_ONE_MSG "rpc server processing call-package size:%d"

//�����������ⲿ�������������쳣 debug
#define RPC_SERVER_EXTERNAL_EXCEPTION \
	"rpc server has external exception type:%s what:%1%"

//�����������ڲ��쳣 warning
#define RPC_SERVER_INTERNAL_EXCEPTION \
	"rpc server hash internal exception type%1% what:%2%"

//��������һ����Ϣ���ﴦ���˶�����Ϣ detail
#define PROCESSED_MSG_COUNT "rpc server porcessed msg count:%d"

//replyʱI/Oʧ�� detail
#define RPC_SERVER_HAS_IO_FAILED "rpc server reply I/O failed code:%1 dest:%2%"

//����������ĳ��xid��ֵ detail
#define RPC_SERVER_XID_SET "set xid:%1 value:%2%"

//��¼xid��ֵ detail
#define RPC_SERVER_LOG_XID "logging xid:%1%"

/*
	�����������Ϣ����Ч��
	������:rpc�汾,�û����,׮���
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

//�������ⲿ����������C�쳣
#define RPC_SERVER_C_EXCEPTION "rpc server external code has c exception"

#define RPC_SERVER_CALL_STUB "rpc server call stub from:%1%"
//����������δ֪���쳣,����δ֪λ�� >=warning
#define RPC_SERVER_UNKNOW_EXCEPTION \
	"rpc server has unknow exception type:%1% what:%2%"

//��������ͻ���reply detail
#define RPC_SERVER_REPLY "rpc server reply to:%1%"

//��־���ܳ�ʼ���ɹ� debug
#define RPC_SERVER_LOG_INITED "rpc server logging inited"



#endif