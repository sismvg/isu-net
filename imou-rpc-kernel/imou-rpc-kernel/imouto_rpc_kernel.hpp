#ifndef IMOUTO_RPC_KERNEL_HPP
#define IMOUTO_RPC_KERNEL_HPP

#include <map>
#include <string>

#include <boost/smart_ptr/detail/spinlock.hpp>

#include <ex-utility/dynamic_bitmap.hpp>

#include <imou-rpc-kernel/imouto_rpcstub.hpp>
#include <imou-rpc-kernel/imouto_rpcmsg.hpp>
#include <imou-rpc-kernel/imouto_rpc_exception.hpp>
#include <ex-utility/lock/autolock.hpp>

template<class AddressType>
class imouto_rpc_kernel
	:boost::noncopyable
{
public:

	typedef AddressType address_type;
	typedef std::size_t xid_type;
	typedef std::string string_type;

	imouto_rpc_kernel()
		:_xidmap_lock(BOOST_DETAIL_SPINLOCK_INIT)
	{
	}

	~imouto_rpc_kernel()
	{}

protected:
	/*
		����[xid],����δ����verify_xidǰ��xidֵ
		����xid�������,������׳�std::invalied_argument�쳣
	*/

	xid_type alloc_xid(const address_type& dest)
	{
		auto lock = ISU_AUTO_LOCK(_xidmap_lock);
		auto& map = _xidmap[dest];
		map.push_back(false);
		return map.size()-1;
	}

	/*
		ȷ���յ�xid,������Ϊtrue
	*/
	bool verify_xid(const address_type& from, const xid_type& xid)
	{
		auto lock = ISU_AUTO_LOCK(_xidmap_lock);
		auto iter = _get_iter(from, xid);
		return !iter->second.set(xid, true);
	}

	/*
		����[xid]��ֵΪvalue,����δ����set_xidǰ��ֵ
		[xid]����Ҫ����.��[xid]�������򴴽���,Ĭ��ֵΪfalse
	*/
	bool set_xid(const address_type& from,
		const xid_type& xid, bool value)
	{
		auto lock = ISU_AUTO_LOCK(_xidmap_lock);
		return _xidmap[from].set(xid, value);
	}

	bool get_xid(const address_type& from, const xid_type& xid)
	{
		auto lock = ISU_AUTO_LOCK(_xidmap_lock);
		auto iter = _get_iter(from, xid);
		return iter->second.test(xid);
	}

protected:
	boost::detail::spinlock _xidmap_lock;
	std::map<address_type, dynamic_bitmap> _xidmap;
private:
	auto _get_iter(const address_type& from, const xid_type& xid)
		->decltype(_xidmap.begin())
	{
		auto iter = _xidmap.find(from);
		if (iter == _xidmap.end() || !iter->second.have_bit(xid))
		{
			throw std::invalid_argument("imouto rpc kernel not found xid");
		}
		return iter;
	}
};

#endif