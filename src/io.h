//----------------------------------------------------------------------------------------------------------------------
// Emulates the IO ports and peripherals
//----------------------------------------------------------------------------------------------------------------------

#pragma once

class Memory;

enum class Key
{
    Shift, Z, X, C, V,
    A, S, D, F, G,
    Q, W, E, R, T,
    _1, _2, _3, _4, _5,
    _0, _9, _8, _7, _6,
    P, O, I, U, Y,
    Enter, L, K, J, H,
    Space, SymShift, M, N, B,

    COUNT
};

class Io
{
public:
    Io(Memory& memory, std::vector<u8>& keys);

    void contend(u16 port, i64 tStates, int num, i64& inOutTStates);
    void out(u16 port, u8 data, i64& inOutTStates);
    u8 in(u16 port, i64& inOutTStates);

    u8 getBorder() const { return m_border; }

private:
    u8                  m_border;
    Memory&             m_memory;
    std::vector<u8>&    m_keys;
};

//----------------------------------------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------------------------------------

#ifdef NX_IMPL

#include "memory.h"

Io::Io(Memory& memory, std::vector<u8>& keys)
    : m_border(7)
    , m_memory(memory)
    , m_keys(keys)
{
    for (auto& k : m_keys) k = 0;
}

void Io::contend(u16 port, i64 tStates, int num, i64& inOutTStates)
{
    if (m_memory.isContended(port))
    {
        for (int i = 0; i < num; ++i)
        {
            inOutTStates += m_memory.contention(inOutTStates) + tStates;
        }
    }
    else
    {
        inOutTStates += tStates * num;
    }
}

void Io::out(u16 port, u8 data, i64& inOutTStates)
{
    if (m_memory.isContended(port))
    {
        contend(port, 1, 1, inOutTStates);
    }
    else
    {
        ++inOutTStates;
    }

    bool isUlaPort = ((port & 1) == 0);
    if (isUlaPort)
    {
        // Deal with $fe
        m_border = data & 7;

        contend(port, 3, 1, inOutTStates);
    }
    else
    {
        if (m_memory.isContended(port))
        {
            contend(port, 1, 3, inOutTStates);
        }
        else
        {
            inOutTStates += 3;
        }
    }
}

u8 Io::in(u16 port, i64& inOutTStates)
{
    u8 x = 0;
    bool isUlaPort = ((port & 1) == 0);

    if (m_memory.isContended(port))
    {
        contend(port, 1, 1, inOutTStates);
    }
    else
    {
        ++inOutTStates;
    }

    if (isUlaPort)
    {
        contend(port, 3, 1, inOutTStates);
    }
    else
    {
        if (m_memory.isContended(port))
        {
            contend(port, 1, 3, inOutTStates);
        }
        else
        {
            inOutTStates += 3;
        }
    }

    //
    // Fetch the actual value from the port
    //

    if (isUlaPort)
    {
        if (LO(port) == 0xfe)
        {
            x = 0xff;
            u8 row = HI(port);
            for (int i = 0; i < 8; ++i)
            {
                if ((row & 1) == 0)
                {
                    // This row is required to be read
                    u8 bit = 1;
                    u8 keys = 0;
                    for (int j = 0; j < 5; ++j)
                    {
                        if (m_keys[i * 5 + j])
                        {
                            // Key is down!
                            keys |= bit;
                        }
                        bit <<= 1;
                    }
                    x &= ~keys;
                }
                row >>= 1;
            }
        }
    }
    return x;
}

//----------------------------------------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------------------------------------

#endif // NX_IMPL