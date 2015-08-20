
#include <ex-utility/memory/memory_block.hpp>

#include <sul-serialize/archive.hpp>
#include <ex-utility/memory/memory_block_utility.hpp>

size_t archived_size(const const_memory_block& blk)
{
	return blk.size + sizeof(blk.size);
}

size_t archived_size(const memory_block& blk)
{
	return archived_size(static_cast<const const_memory_block&>(blk));
}

size_t archive_to_with_size(memory_address buffer,
	size_t bufsize, const const_memory_block& blk, size_t size)
{
	archive_to_with_size(buffer, bufsize, blk.size, sizeof(blk.size));
	advance_in(buffer, bufsize, sizeof(blk.size));
	mymemcpy(buffer, blk.buffer, blk.size);
	return blk.size + sizeof(blk.size);
}

size_t archive_to_with_size(memory_address buffer,
	size_t bufsize, const memory_block& blk, size_t size)
{
	return archive_to_with_size(buffer, bufsize,
		static_cast<const const_memory_block&>(blk), size);
}

size_t rarchive(const_memory_block src, const_memory_block& blk)
{
	advance_in(src, rarchive(src, blk.size));
	blk.buffer = src.buffer;
	return sizeof(blk.size) + blk.size;
}

size_t rarchive(const_memory_block src, memory_block& blk)
{
	advance_in(src, rarchive(src, blk.size));
	src.size = blk.size;
	blk = deep_copy(src);
	return blk.size + sizeof(blk.size);
}