//----------------------------------------------------------------------------------------------------------------------
//! @file       editor/data.cc
//! @brief      Implements the editor buffer.
//----------------------------------------------------------------------------------------------------------------------

#include <editor/data.h>
#include <emulator/nxfile.h>
#include <utils/tinyfiledialogs.h>

static const int kInitialGapSize = 4096;

//----------------------------------------------------------------------------------------------------------------------

EditorData::EditorData()
    : m_fileName()
    , m_buffer()
    , m_gapStart(0)
    , m_gapEnd(kInitialGapSize)
{
    m_buffer.resize(kInitialGapSize);
}

//----------------------------------------------------------------------------------------------------------------------

EditorData::EditorData(string fileName)
    : m_fileName(fileName)
    , m_buffer()
    , m_gapStart(0)
    , m_gapEnd(0)
{
    if (NxFile::loadTextFile(fileName, m_buffer))
    {
        // Move the text to the end of an enlarged buffer.
        ensureGapSize(kInitialGapSize);
    }
    else
    {
        tinyfd_messageBox("LOADING ERROR", stringFormat("Unable to load {0}!", fileName).c_str(), "ok", "error", 0);
        m_buffer.resize(kInitialGapSize);
        m_gapEnd = kInitialGapSize;
    }
}

//----------------------------------------------------------------------------------------------------------------------

Pos EditorData::bufferPosToPos(BufferPos p)
{
    return p > m_gapStart ? (p - (m_gapEnd - m_gapStart)) : p;
}

//----------------------------------------------------------------------------------------------------------------------

BufferPos EditorData::posToBufferPos(Pos p)
{
    return p > m_gapStart ? m_gapStart + (p - m_gapStart) : p;
}

//----------------------------------------------------------------------------------------------------------------------

void EditorData::setInsertPoint(Pos pos)
{
    BufferPos bp = posToBufferPos(pos);
    if (pos < m_gapStart)
    {
        // Shift all data between pos and gap to the end of the gap, moving the gap to the left at the same time.
        i64 delta = m_gapStart - bp;
        move(m_buffer.begin() + pos, m_buffer.begin() + m_gapStart, m_buffer.begin() + m_gapEnd - delta);
        m_gapStart -= delta;
        m_gapEnd -= delta;
    }
    else
    {
        // Shift all data between the gap end and position to the beginning of the gap, moving the gap right.
        i64 delta = bp - m_gapEnd;
        move_backward(m_buffer.begin() + m_gapEnd, m_buffer.begin() + bp, m_buffer.begin() + m_gapStart);
        m_gapStart += delta;
        m_gapEnd += delta;
    }
}

//----------------------------------------------------------------------------------------------------------------------

void EditorData::ensureGapSize(i64 size)
{
    if ((m_gapEnd - m_gapStart) < size)
    {
        // We need to resize the buffer to allow the extra size.
        i64 oldSize = (i64)m_buffer.size();
        i64 newSize = oldSize * 3 / 2;
        i64 minSize = oldSize + (size - (m_gapEnd - m_gapStart));
        i64 finalSize = std::max(newSize, minSize);
        i64 delta = oldSize - m_gapEnd;
        m_buffer.resize(finalSize);
        move_backward(m_buffer.begin() + m_gapEnd, m_buffer.begin() + m_gapEnd + delta,
            m_buffer.end() - delta);
        m_gapEnd += (finalSize - oldSize);
    }
}

//----------------------------------------------------------------------------------------------------------------------

void EditorData::insert(Pos p, const char *start, const char* end)
{
    ensureGapSize(end - start);
    setInsertPoint(p);
    while (start < end) m_buffer[m_gapStart++] = *start;
}

//----------------------------------------------------------------------------------------------------------------------

