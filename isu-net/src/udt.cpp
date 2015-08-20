
#include <isu-net/udt.hpp>
#include <ex-utility/mpl/mpl_helper.hpp>

#include <ex-utility/lock/autolock.hpp>

udt::udt(const core_sockaddr& local, size_t limit_thread)
	:
		_core(local,MEMBER_BIND_IN(_udt_bad_send),limit_thread),
		_groups_lock(BOOST_DETAIL_SPINLOCK_INIT),
		_sending_groups_lock(BOOST_DETAIL_SPINLOCK_INIT)
{

}

udt::udt(const core_sockaddr& local, bad_send_functional bad_send,
	size_t limit_thread)
	: 
		_core(local, MEMBER_BIND_IN(_udt_bad_send),limit_thread),
		_bad_send(bad_send),
		_groups_lock(BOOST_DETAIL_SPINLOCK_INIT),
		_sending_groups_lock(BOOST_DETAIL_SPINLOCK_INIT)
{

}

void udt::set_send_callback(send_callback callback)
{
	_send_callback = callback;
	_core.set_flag(POST_SEND_COMPLETE);
}

void udt::close()
{
	_core.close();
}

int udt::native() const
{
	return _core.native();
}

void udt::setsocket(const core_sockaddr& addr)
{
	_core.close();
	_core = trans_core(addr);
}

udt_recv_result udt::recv()
{
	udt_recv_result result;
	bool runable = true;
	do
	{
		recv_result data = _core.recv();
		const io_result& error = data.get_io_result();

		bool not_recv_complete = (data.io_type() == SEND_COMPLETE 
			|| data.io_type() == FAILED_SEND_COMPLETE);

		auto* group = reinterpret_cast<const _group_ident*>(data.begin());
		if (not_recv_complete)
		{
			if (group->locker)
			{
				group->locker->lock();
				_cancel_slice(&group->node);
				group->locker->unlock();
			}
			_erase_group(group->group_id, true, data.from());
			continue;
		}

		if (error.in_vaild())
		{//没有复制到标准缓冲区中...
			const _group_ident* ptr =
				reinterpret_cast<const _group_ident*>(data.begin());

			recv_result::iterator begin =
				data.begin() + sizeof(_group_ident);

			shared_memory memory = _insert(*ptr, data.from(),
				const_memory_block(begin, data.end() - begin));

			if (memory.get() != nullptr)
			{
				result.io_state = io_result(nullptr,
					error.what(), error.system_socket_error());
				result.memory = memory;
				result.from = data.from();
				runable = false;
			}
		}
		else
		{
			result.io_state = error;
			runable = false;
		}
	} while (runable);
	return result;
}

io_result udt::recv(bit_type* buffer, size_t size, core_sockaddr& from)
{
	recv_result data = _core.recv();
	if (data.get_io_result().in_vaild())
	{
		recv_result::iterator begin = data.begin() + sizeof(_group_ident);
		deep_copy_to(memory_block(buffer, size),
			const_memory_block(begin, data.end() - begin));
		from = data.from();
	}
	return data.get_io_result();
}

size_t udt::alloc_group_id()
{
	return _alloc_group_id();
}

udt::send_result udt::send_block(const core_sockaddr& dest,size_t id,
	const_memory_block block, size_t size,const retrans_control* ptrc)
{
	size_t slice_size =
		trans_core::maxinum_slice_size() + trans_core::trans_core_reserve();

	size_t group_id = id;
	slicer::user_head_genericer slicer_head_gen
		= [&](size_t slice_id, size_t data_start, archive_packages& pac)
	{
		pac.fill(trans_core::trans_core_reserve());

		_group_ident ident;
		ident.group_id = group_id;
		ident.slice_start = data_start;
		ident.data_size = block.size;
		pac.push_back(make_block(ident));
	};
	//头部大小不对
	size_t user_keep = trans_core::trans_core_reserve() +
		sizeof(_group_ident) + sizeof(slicer_head);

	slicer slice_core(block, slice_size, slicer_head_gen,
		user_keep, concrete);

	bool is_multicst_addr = is_multicast(dest);

	group_handle handle;
	io_result result(nullptr);

	if (!is_multicst_addr)
	{
		auto lock2 = ISU_AUTO_LOCK(_sending_groups_lock);
		auto pair = _sending_groups.emplace(group_id, group_handle(new udt_group));
		if (pair.second)
		{
			handle = pair.first->second;
			handle->_group_id = group_id;
		}
		lock2.unlock();
	}

	handle->_count = -1;

	size_t count = 0;
	while (!slice_core.slice_end())
	{
		auto iter = slice_core.get_next_slice();
		size_t size = iter->first.size() + iter->second.size();
		dynamic_memory_block<char> buffer(size);

		buffer.push(iter->first);
		buffer.push(iter->second.data());

		++count;

		auto* group = reinterpret_cast<_group_ident*>(
			buffer.memory().get()+trans_core::trans_core_reserve());

		result = _core.assemble_send(buffer.memory(), dest, ptrc);

		if (handle)
		{
			handle->_locker.lock();
			group->locker = &handle->_locker;

			auto& node = group->node;
			node.value = new io_mission_handle(result.id());
			handle->_root.connect(&node);

			handle->_locker.unlock();
		}

		_core.send_assembled(buffer.memory(), dest);

		if (!result.in_vaild())
			break;
	}
	//这个时候才可以
	handle->_count = count;
	if (handle->_geted == handle->_count)
		_erase_group(handle->_group_id, false, dest);
	return std::make_pair(result, handle);
}

udt::send_result udt::send(const core_sockaddr& dest,
	const const_shared_memory& data, size_t size,const retrans_control* ptr)
{
	return send(dest, alloc_group_id(), data, size, ptr);
}

udt::send_result udt::send(const core_sockaddr& dest, size_t id,
	const const_shared_memory& data, size_t size,const retrans_control* ptr)
{
	return send_block(dest, id,
		static_cast<const_memory_block>(data), size, ptr);
}

void udt::cancel_io(const group_handle& handle, bool active_io_failed /* = true */)
{
	if (!handle->bad_handle())
	{
		_cancel_group(&handle->_root);
		if (active_io_failed&&_bad_send)
		{
			_bad_send(handle->_group_id, handle->dest(), const_memory_block(nullptr, 0));
		}
		handle->_set_bad();
	}
}
//
udt::group_id_type udt::_alloc_group_id()
{
	return _group_id_allocer++;
}
//依旧内存泄漏或保存不当
udt::shared_memory udt::_insert(const _group_ident& ident,
	const core_sockaddr& from,
	const const_memory_block& data)
{
	shared_memory result(memory_block(nullptr, 0));

	auto lock = ISU_AUTO_LOCK(_groups_lock);
	auto iter = _recv_groups.find(ident.group_id);
	if (iter == _recv_groups.end())
	{
		slicer_head head;
		head.data_size = ident.data_size;

		_recv_group group;
		group._combin = combination(head);
		iter = _recv_groups.emplace(ident.group_id,group).first;
	}	

	auto& combin = iter->second._combin;

	combin.insert(ident.slice_start, data);

	if (combin.accepted_all())
	{
		result = combin.data().first;
		_recv_groups.erase(iter);
	}
	return result;
}

void udt::_udt_bad_send(const core_sockaddr& dest,
	const const_shared_memory& memory, io_mission_handle id)
{
	//会导致cancel掉所有的组	
	const auto* begin = memory.get() +
	trans_core::trans_core_reserve();
	auto* ptr =
		const_cast<_group_ident*>(
			reinterpret_cast<const _group_ident*>(begin));

	ptr->locker->lock();
	_cancel_group(&ptr->node);
	ptr->locker->unlock();

	if (_bad_send)
	{
		const char* end = memory.get() + memory.size();
		_bad_send(ptr->group_id, dest,
			const_memory_block(begin, end - begin));
	}
	auto lock = ISU_AUTO_LOCK(_sending_groups_lock);
	_sending_groups.erase(ptr->group_id);
}

void udt::_cancel_slice(const group_list_node* node,bool active_failed)
{
	node->detach();
	delete node->value;
}

void udt::_cancel_group(const group_list_node* node)
{
	auto v=fetch_group_list(node, [&](const group_list_node* node)
	{
		if (node->value)
		{
			_core.cancel_io(*node->value, true);
			delete node->value;
		}
	});
	int value = v;
} 

void udt::_erase_group(group_id_type id, bool tick,const core_sockaddr& from)
{
	auto lock = ISU_AUTO_LOCK(_sending_groups_lock);
	auto iter = _sending_groups.find(id);
	if (iter != _sending_groups.end())
	{
		auto& handle = iter->second;
		if (!tick||++handle->_geted == handle->_count)
		{
			_send_callback(id, from,
				const_memory_block(nullptr, 0));
			_sending_groups.erase(iter);
		}
	}
}