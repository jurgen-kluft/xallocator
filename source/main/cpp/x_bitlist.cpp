#include "xbase/x_target.h"
#include "xbase/x_debug.h"
#include "xbase/x_integer.h"
#include "xbase/x_memory.h"

#include "xallocator/private/x_bitlist.h"

namespace xcore
{
    u32 xbitlist::size_in_dwords(u32 numbits)
    {
        u32 numdwords = 0;
        while (numbits > 1)
        {
            numdwords += ((numbits + 31) / 32) + 2;
            numbits = (numbits + 31) >> 5;
        }
        return numdwords * 4;
    }

    void xbitlist::init(u32* bitlist, u32 maxbits, bool setall, bool invert)
    {
        m_level0    = bitlist;
		m_numbits   = maxbits;
        m_invert    = invert ? AllBitsSet : 0;
        m_level0[0] = 0;

        // Figure out the offsets to every level
        u32  numbits = maxbits;
        u32  offset  = 0;
        u32* level   = m_level0;
        u32* prev    = m_level0;
        while (numbits > 1)
        {
            level[1] = prev[0];
            level[0] = ((numbits + 31) / 32) + 2;
            prev     = level;
            level += level[0];
            numbits = (numbits + 31) >> 5;
        }
        m_levelT = level - 3;

        reset(setall);
    }

    void xbitlist::init(xheap& heap, u32 maxbits, bool setall, bool invert)
    {
        u32  ndwords = size_in_dwords(maxbits);
        u32* bitlist = (u32*)heap.allocate(ndwords * 4, sizeof(u32));
        init(bitlist, maxbits, setall, invert);
    }

    void xbitlist::release(xheap& heap)
    {
        heap.deallocate(m_level0);
        m_level0  = nullptr;
        m_levelT  = nullptr;
        m_numbits = 0;
        m_invert  = 0;
    }

    // 5000 bits = 628 bytes = 157 u32 = (32768 bits level 0)
    // 157  bits =  20 bytes =   5 u32 = ( 1024 bits level 1)
    //   5  bits =   4 byte  =   1 u32 = (   32 bits level 2)
    // level 0, bits= 5000, dwords= 157, bytes= 628
    // level 1, bits= 157, dwords= 5, bytes= 20
    // level 2, bits= 5, dwords= 1, bytes= 4
    // total = 628 + 20 + 4 = 652 bytes

    void xbitlist::reset(bool setall)
    {
        s32 const invert = setall ? ~m_invert : m_invert;

        u32* level = m_level0;
        u32 numbits = m_numbits;
        while (numbits > 1)
        {
            u32 numdwords = ((numbits + 31) / 32);
            x_memset(level, m_invert, numdwords * 4);
            if (m_invert == 0)
            {
                u32 lastmask = (0xffffffff << ((numdwords * 32) - numbits));
                level[numdwords - 1] = level[numdwords - 1] | lastmask;
            }
            else
            {
                u32 lastmask = (0xffffffff << ((numdwords * 32) - numbits));
                level[numdwords - 1] = level[numdwords - 1] & ~lastmask;
            }
            numbits = (numbits + 31) >> 5;
            level += numdwords;
        }
    }

    void xbitlist::set(u32 bit)
    {
        ASSERT(bit < m_numbits);

        // set bit in level 0, then avalanche up if necessary
        u32* level = m_level0;
        while (level <= m_levelT)
        {
            u32  dwordIndex = (bit + 31) / 32;
            u32  dwordBit   = 1 << (bit & 31);
            u32  dword0     = level[2 + dwordIndex];
            u32  dword1;
            bool avalanche;
            if (m_invert == 0)
            {
                dword1    = dword0 | dwordBit;
                avalanche = (dword0 != dword1 && dword1 == AllBitsSet);
            }
            else
            {
                dword1    = dword0 & ~dwordBit;
                avalanche = (dword0 != dword1 && dword0 == AllBitsSet);
            }

            level[2 + dwordIndex] = dword1;

            if (!avalanche)
                break;

            level = level + level[0];
            bit   = bit >> 5;
        }
    }

    void xbitlist::clr(u32 bit)
    {
        ASSERT(bit < m_numbits);

        // clear bit in level 0, then avalanche up if necessary
        u32* level = m_level0;
        while (level <= m_levelT)
        {
            u32  dwordIndex = (bit + 31) / 32;
            u32  dwordBit   = 1 << (bit & 31);
            u32  dword0     = level[2 + dwordIndex];
            u32  dword1;
            bool avalanche;
            if (m_invert == 0)
            {
                dword1    = dword0 & ~dwordBit;
                avalanche = (dword0 != dword1 && dword0 == AllBitsSet);
            }
            else
            {
                dword1    = dword0 | dwordBit;
                avalanche = (dword0 != dword1 && dword1 == AllBitsSet);
            }

            level[2 + dwordIndex] = dword1;

            if (!avalanche)
                break;

            level = level + level[0];
            bit   = bit >> 5;
        }
    }

	bool xbitlist::is_set(u32 bit) const
    {
        u32 const* level = m_level0;
        u32 dwordIndex = bit / 32;
        u32 dwordBit = bit & 31;
        ASSERT(dwordIndex < (level[0] - 2));
        return level[2 + dwordIndex] & (1 << dwordBit);
    }

	bool xbitlist::is_full() const
    {
        if (m_invert == 0)
        {
            return m_levelT[0] == 0xfffffffff;
        }
        return m_levelT[0] == 0;
    }

    bool xbitlist::find(u32& bit) const
    {
        // Start at top level and find a '0' bit and move down
        u32        dwordIndex = 0;
        u32 const* level      = m_levelT;
        while (level >= m_level0)
        {
            u32 dword0   = level[2 + dwordIndex];
            u32 dwordBit = xfindFirstBit(~dword0);
            if (dwordBit == 32)
                return false;
            dwordIndex = (dwordIndex * 32) + dwordBit;
            level      = level - level[1];
        }
        return dwordIndex;
    }

    xbitlist::xbitlist() : m_level0(nullptr), m_levelT(nullptr), m_ndwords(0), m_invert(0) {}

}; // namespace xcore
