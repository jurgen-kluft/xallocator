//==============================================================================
//  x_dl_allocator.h
//==============================================================================
#ifndef __X_DL_ALLOCATOR_H__
#define __X_DL_ALLOCATOR_H__
#include "xbase/x_target.h"
#ifdef USE_PRAGMA_ONCE 
#pragma once 
#endif

//==============================================================================
// xCore namespace
//==============================================================================
namespace xcore
{
	/// Forward declares
	class xalloc;

	/// A custom allocator; Doug Lea malloc
	extern xalloc*		gCreateDlAllocator(void* mem, u32 memsize);

};

#endif	/// __X_DL_ALLOCATOR_H__

