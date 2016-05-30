#include <vpp/transfer.hpp>
#include <vpp/vk.hpp>
#include <algorithm>

namespace vpp
{

//TransferBuffer
TransferManager::TransferBuffer::TransferBuffer(const Device& dev, std::size_t size, std::mutex& mtx)
	: mutex_(mtx)
{
	vk::BufferCreateInfo info;
	info.size = size;
	info.usage = vk::BufferUsageBits::transferDst | vk::BufferUsageBits::transferSrc;

	buffer_ = Buffer(dev, info, vk::MemoryPropertyBits::hostVisible);
}

TransferManager::TransferBuffer::~TransferBuffer()
{
	auto rc = ranges_.size();
	if(rc > 0) std::cerr << "vpp::TransferManager::~TransferBuffer: " << rc << " allocations left\n";
}

Allocation TransferManager::TransferBuffer::use(std::size_t size)
{
	static const Allocation start = {0, 0};
	auto old = start;

	for(auto it = ranges_.begin(); it != ranges_.end(); ++it)
	{
		if(it->offset - old.end() > size)
		{
			Allocation range = {old.end(), size};

			//inserts the tested range before the higher range, if there is any
			ranges_.insert(it, range);
			return range;
		}

		old = *it;
	}

	return {};
}

bool TransferManager::TransferBuffer::release(const Allocation& alloc)
{
	std::lock_guard<std::mutex> guard(mutex_);
	for(auto it = ranges_.begin(); it < ranges_.end(); ++it)
	{
		if(it->offset == alloc.offset && it->size == alloc.size)
		{
			ranges_.erase(it);
			return true;
		}
	}

	return false;
}

//BufferRange
TransferManager::BufferRange::~BufferRange()
{
	if(buffer_) buffer_->release(allocation());
}

TransferManager::BufferRange::BufferRange(BufferRange&& other) noexcept
{
	swap(*this, other);
}

TransferRange& TransferManager::BufferRange::operator=(BufferRange&& other) noexcept
{
	if(buffer_) buffer_->release(allocation());
	buffer_ = {};
	allocation_ = {};
	swap(*this, other);

	return *this;
}

void swap(TransferRange& a, TransferRange& b) noexcept
{
	using std::swap;

	swap(a.buffer_, b.buffer_);
	swap(a.allocation_, b.allocation_);
}

//TransferManager
TransferManager::TransferManager(const Device& dev) : Resource(dev)
{
}

TransferRange TransferManager::buffer(std::size_t size)
{
	std::lock_guard<std::mutex> guard(mutex_);
	for(auto& buffp : buffers_)
	{
		auto alloc = buffp->use(size);
		if(alloc.size > 0) return BufferRange(*buffp, alloc);
	}

	//Allocate new buffer
	buffers_.emplace_back(new TransferBuffer(device(), size, mutex_));
	return BufferRange(*buffers_.back(), buffers_.back()->use(size));
}

std::size_t TransferManager::totalSize() const
{
	std::lock_guard<std::mutex> guard(mutex_);
	std::size_t ret {};
	for(auto& bufp : buffers_) ret += bufp->buffer().memoryEntry().size();
	return ret;
}

std::size_t TransferManager::activeRanges() const
{
	std::lock_guard<std::mutex> guard(mutex_);
	std::size_t ret {};
	for(auto& bufp : buffers_) ret += bufp->rangesCount();
	return ret;
}

void TransferManager::reserve(std::size_t size)
{
	std::lock_guard<std::mutex> guard(mutex_);
	buffers_.emplace_back(new TransferBuffer(device(), size, mutex_));
}

void TransferManager::shrink()
{
	std::lock_guard<std::mutex> guard(mutex_);
	for(auto it = buffers_.begin(); it < buffers_.end();)
	{
		if((*it)->rangesCount() == 0) it = buffers_.erase(it);
		else ++it;
	}
}

void TransferManager::optimize()
{
	std::lock_guard<std::mutex> guard(mutex_);
	std::size_t size;
	for(auto it = buffers_.begin(); it < buffers_.end();)
	{
		if((*it)->rangesCount() == 0)
		{
			size += (*it)->buffer().memoryEntry().size();
			it = buffers_.erase(it);
		}
		else
		{
			++it;
		}
	}

	reserve(size);
}

}