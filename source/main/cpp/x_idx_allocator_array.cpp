#include "xbase/x_target.h"
#include "xbase/x_integer.h"
#include "xbase/x_allocator.h"
#include "xbase/x_idx_allocator.h"
#include "xbase/x_memory_std.h"

namespace xcore
{
	class x_indexed_array_allocator : public xfsadexed
	{
	public:
		x_indexed_array_allocator(xalloc* allocator) : mAllocator(allocator)	{ }
		void				initialize(void* object_array, u32 size_of_object, u32 object_alignment, u32 size);
		void				initialize(xalloc* allocator, u32 size_of_object, u32 object_alignment, u32 size);

		XCORE_CLASS_PLACEMENT_NEW_DELETE

	protected:
		enum { NILL_IDX = 0xffffffff };

		void				init_freelist();

		virtual void		init();
		virtual void		clear();

		virtual u32			size() const		{ return mAllocCount; }
		virtual u32			max_size() const	{ return mObjectArraySize; }

		virtual void*		allocate(xsize_t size, u32 alignment);
		virtual void*		reallocate(void* p, xsize_t new_size, u32 new_alignment);
		virtual void		deallocate(void* p);

		virtual u32			iallocate(void*& p);
		virtual void		ideallocate(u32 idx);

		virtual void*		to_ptr(u32 idx) const;
		virtual u32			to_idx(void const* p) const;

		virtual void		release();

	private:
		xalloc*		mAllocator;
		u32*				mFreeObjectList;
		u32					mAllocCount;
		xalloc*		mObjectArrayAllocator;
		u32					mObjectArraySize;
		xbyte*				mObjectArray;
		xbyte*				mObjectArrayEnd;
		u32					mSizeOfObject;
		u32					mAlignOfObject;
	};

	x_iidx_allocator*		gCreateArrayIdxAllocator(xalloc* allocator, xalloc* object_array_allocator, u32 size_of_object, u32 object_alignment, u32 size)
	{
		void* mem = allocator->allocate(sizeof(x_indexed_array_allocator), 4);
		x_indexed_array_allocator* array_allocator = new (mem) x_indexed_array_allocator(allocator);
		array_allocator->initialize(object_array_allocator, size_of_object, object_alignment, size);
		return array_allocator;
	}

	x_iidx_allocator*		gCreateArrayIdxAllocator(xalloc* allocator, void* object_array, u32 size_of_object, u32 object_alignment, u32 size)
	{
		void* mem = allocator->allocate(sizeof(x_indexed_array_allocator), 4);
		x_indexed_array_allocator* array_allocator = new (mem) x_indexed_array_allocator(allocator);
		array_allocator->initialize(object_array, size_of_object, object_alignment, size);
		return array_allocator;
	}


	void		x_indexed_array_allocator::init_freelist()
	{
		u32* object_array = (u32*)mObjectArray;
		for (u32 i=1; i<mObjectArraySize; ++i)
		{
			*object_array = i;
			object_array += mSizeOfObject / 4;
		}
		*object_array = NILL_IDX;
		mFreeObjectList = (u32*)mObjectArray;
	}

	void		x_indexed_array_allocator::initialize(void* object_array, u32 size_of_object, u32 object_alignment, u32 size)
	{
		object_alignment = xalignUp(object_alignment, 4);

		mFreeObjectList = NULL;
		mAllocCount = 0;
		mObjectArrayAllocator = NULL;
		mObjectArraySize = size;
		mSizeOfObject = xalignUp(size_of_object, object_alignment);
		mObjectArray = (xbyte*)object_array;
		mObjectArrayEnd = mObjectArray + (mObjectArraySize * mSizeOfObject);
	}

	void		x_indexed_array_allocator::initialize(xalloc* allocator, u32 size_of_object, u32 object_alignment, u32 size)
	{
		mFreeObjectList = NULL;
		mAllocCount = 0;
		mObjectArray = NULL;
		mObjectArrayAllocator = allocator;
		mObjectArraySize = size;

		mAlignOfObject = xalignUp(object_alignment, 4);
		mSizeOfObject = xalignUp(size_of_object, mAlignOfObject);
	}

	void		x_indexed_array_allocator::init()
	{
		clear();

		if (mObjectArray==NULL)
		{
			mObjectArray = (xbyte*)mObjectArrayAllocator->allocate(mSizeOfObject * mObjectArraySize, mAlignOfObject);
			mObjectArrayEnd = mObjectArray + (mObjectArraySize * mSizeOfObject);
		}
		init_freelist();
	}

	void		x_indexed_array_allocator::clear()
	{
		ASSERT(mAllocCount==0);
		if (mObjectArrayAllocator!=NULL)
		{
			if (mObjectArray!=NULL)
			{
				mObjectArrayAllocator->deallocate(mObjectArray);
				mObjectArray = NULL;
				mObjectArrayEnd = NULL;
			}
		}

		mFreeObjectList = NULL;
	}

	void*		x_indexed_array_allocator::allocate(xsize_t size, u32 alignment)
	{
		ASSERT(size < mSizeOfObject);
		void* p;
		iallocate(p);
		return p;
	}

	u32			x_indexed_array_allocator::iallocate(void*& p)
	{
		if (mFreeObjectList == NULL)
		{
			p = NULL;
			return NILL_IDX;
		}

		u32 idx = (u32)((uptr)mFreeObjectList - (uptr)mObjectArray) / mSizeOfObject;
		p = (void*)mFreeObjectList;

		u32 next_object = *mFreeObjectList;
		if (next_object != NILL_IDX)
			mFreeObjectList = (u32*)((uptr)mObjectArray + (next_object * mSizeOfObject));
		else
			mFreeObjectList = NULL;

		++mAllocCount;
		return idx;
	}

	void*		x_indexed_array_allocator::reallocate(void* old_ptr, xsize_t new_size, u32 new_alignment)
	{
		ASSERT(new_size <= mSizeOfObject);
		ASSERT(new_alignment <= mAlignOfObject);
		return old_ptr;
	}

	void		x_indexed_array_allocator::deallocate(void* ptr)
	{
		u32 idx = to_idx(ptr);
		ideallocate(idx);
	}

	void		x_indexed_array_allocator::ideallocate(u32 idx)
	{
		if (idx < mObjectArraySize)
		{
			u32* free_object = (u32*)(mObjectArray + (mSizeOfObject * idx));
			*free_object = to_idx(mFreeObjectList);
			mFreeObjectList = free_object;
			--mAllocCount;
		}
	}

	void*		x_indexed_array_allocator::to_ptr(u32 idx) const
	{
		if (idx == NILL_IDX)
			return NULL;
		ASSERT(mObjectArray!=NULL && idx < mObjectArraySize);
		void* p = (void*)((uptr)mObjectArray + (mSizeOfObject * idx));
		return p;
	}

	u32			x_indexed_array_allocator::to_idx(void const* p) const
	{
		ASSERT(mObjectArray!=NULL && mObjectArrayEnd!=NULL);
		if ((xbyte*)p >= mObjectArray && (xbyte*)p < mObjectArrayEnd)
		{
			u32 idx = (u32)((uptr)p - (uptr)mObjectArray) / mSizeOfObject;
			return idx;
		}
		else
		{
			return NILL_IDX;
		}
	}

	void		x_indexed_array_allocator::release()
	{
		clear();
		this->~x_indexed_array_allocator();
		mAllocator->deallocate(this);
	}
}
