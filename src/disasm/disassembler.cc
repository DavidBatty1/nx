//----------------------------------------------------------------------------------------------------------------------
// Disassembler document
//----------------------------------------------------------------------------------------------------------------------

#include <asm/disasm.h>
#include <disasm/disassembler.h>
#include <emulator/nxfile.h>
#include <utils/tinyfiledialogs.h>
#include <utils/ui.h>

//----------------------------------------------------------------------------------------------------------------------
// Constructor
//----------------------------------------------------------------------------------------------------------------------

DisassemblerDoc::DisassemblerDoc(Spectrum& speccy)
    : m_speccy(&speccy)
    , m_nextTag(1)
{
    switch (speccy.getModel())
    {
    case Model::ZX48:
        {
            m_mmap.resize(65536);
            for (int i = 0; i < 65536; ++i)
            {
                m_mmap[i] = speccy.peek(u16(i));
            }
            MemAddr start = m_speccy->convertAddress(Z80MemAddr(0x4000));
            MemAddr end = m_speccy->convertAddress(Z80MemAddr(0xffff));
        }
        break;

    case Model::ZX128:
    case Model::ZXPlus2:
    case Model::ZXNext:
        break;
            
    default:
        NX_ASSERT(0);
    }
}

//----------------------------------------------------------------------------------------------------------------------
// Internal functions
//----------------------------------------------------------------------------------------------------------------------

void DisassemblerDoc::reset()
{
    m_lines.clear();
    m_labelMap.clear();
    m_addrMap.clear();
    m_changed = false;
}

void DisassemblerDoc::insertComment(int line, int tag, string comment)
{
    // Insert a blank line if the insert point contains a label or instruction
    if (line < m_lines.size() &&
        (m_lines[line].type == LineType::Label ||
         m_lines[line].type == LineType::Instruction))
    {
        insertLine(line, Line{ tag, LineType::Blank, {}, {}, {} });
    }
    insertLine(line, Line{ tag, LineType::FullComment, MemAddr(), MemAddr(), comment });
}

void DisassemblerDoc::setComment(int line, string comment)
{
    NX_ASSERT(m_lines[line].type == LineType::FullComment || m_lines[line].type == LineType::Instruction);
    m_lines[line].text = comment;
    changed();
}

int DisassemblerDoc::generateCode(MemAddr addr, int tag, string label)
{
    if (optional<int> lineIndex = findLine(addr); lineIndex)
    {
        int i = *lineIndex;
        MemAddr end;

        if (i < getNumLines())
        {
            Line& line = getLine(i);

            NX_ASSERT(line.type != LineType::Blank);
            NX_ASSERT(line.type != LineType::FullComment);

            end = line.startAddress;
            if (line.startAddress <= addr)
            {
                Overlay::currentOverlay()->error("Code already generated for this entry point");
                return -1;
            }
        }
        else
        {
            end = m_speccy->convertAddress(Z80MemAddr(0xffff));
        }

        MemAddr c = addr;

        if (i < getNumLines()) i = deleteLine(i);

        if (i > 0 && getLine(i-1).type != LineType::Blank) insertLine(i++, Line{ tag, LineType::Blank, {}, {}, {} });
        int startLine = i;
        insertLine(i++, Line{ tag, LineType::Label, c, c, label });

        Disassembler dis;
        bool endFound = false;
        while (!endFound && (c < end))
        {
            // Grab the next 4 bytes
            // #todo: refactor the memory system out so we can clone it and not use memory maps
            u16 a = m_speccy->convertAddress(c);
            u8 b1 = m_mmap[a];
            u8 b2 = (a <= 0xfffe) ? m_mmap[a + 1] : 0;
            u8 b3 = (a <= 0xfffd) ? m_mmap[a + 2] : 0;
            u8 b4 = (a <= 0xfffc) ? m_mmap[a + 3] : 0;
            u16 na = dis.disassemble(a, b1, b2, b3, b4);
            c = m_speccy->convertAddress(Z80MemAddr(a));
            MemAddr nc = m_speccy->convertAddress(Z80MemAddr(na));

            // Lets look at the opcode to see if we continue.  We stop at JP, RET, RETI and RETN
            switch (b1)
            {
            case 0xc3:      // JP nnnn
            case 0xc9:      // RET
            case 0xe9:      // JP (HL)
                endFound = true;
                break;

            case 0xed:
                switch (b2)
                {
                case 0x45:
                case 0x4d:
                case 0x55:
                case 0x5d:
                case 0x65:
                case 0x6d:
                case 0x75:
                case 0x7d:
                    endFound = true;

                default:
                    break;
                }
                break;

            case 0xdd:      // IX
            case 0xfd:      // IY
                endFound = (b2 == 0xe9);    // JP (IX+n)/(IY+n)
                break;
            }

            // Add a line for the code
            insertLine(i++, Line{ tag, c, nc - 1, dis.opCodeString(), dis.operandString() });

            c = nc;
            a = na;
        }

        if (c != end)
        {
            insertLine(i++, Line{ tag, LineType::Blank, {},{},{} });
        }

        changed();
        return startLine;
    }
    else
    {
        Overlay::currentOverlay()->error("Invalid code entry point.");
        return -1;
    }

}

int DisassemblerDoc::deleteLine(int line)
{
    int tag = m_lines[line].tag;
    int newLine = line;
    bool unknownRangeFound = false;
    MemAddr start, end;

    for (int i = 0; i < line; ++i)
    {
        Line& line = m_lines[i];
        if (line.tag == tag) --newLine;
    }

    m_lines.erase(remove_if(m_lines.begin(), m_lines.end(), [tag](const auto& line) {
        return line.tag == tag;
    }), m_lines.end());

    while ((newLine < (int)m_lines.size()) &&
        (m_lines[newLine].type == LineType::Blank))
    {
        m_lines.erase(m_lines.begin() + newLine);
    }

    changed();
    return newLine;
}

//----------------------------------------------------------------------------------------------------------------------
// File operations
//
// Disassembly file format:
//
// Uses NX file format.
//
// BLOCK TYPES & FORMATS:
//
//      MM48 (length = 65536)
//          Offset  Length  Description
//          0       65536   Bytes
//
//      DCMD
//          Offset  Length  Description
//          0       4       Number of commands
//          4       ?       Commands of the format:
//
//              Offset  Length  Description
//              0       1       Type
//              1       4       Line
//              5       8       Parameter 1
//              13      ?       Text
//----------------------------------------------------------------------------------------------------------------------

bool DisassemblerDoc::load(string fileName)
{
    reset();

    NxFile f;
    if (f.load(fileName))
    {
        if (f.checkSection('MM48', 0))
        {
            const BlockSection& mm48 = f['MM48'];
            mm48.peekData(0, m_mmap, 65536);
        }
        else
        {
            return false;
        }

        if (f.checkSection('DCMD', 0))
        {
            const BlockSection& dcmd = f['DCMD'];
            int numLines = (int)dcmd.peek32(0);
            int x = 4;
            for (int i = 0; i < numLines; ++i)
            {
                int tag = dcmd.peek32(x);
                LineType type = (LineType)dcmd.peek8(x+4);
                MemAddr start = dcmd.peekAddr(x + 5);
                MemAddr end = dcmd.peekAddr(x + 9);
                string text = dcmd.peekString(x + 13);
                x += (13 + (int)text.size() + 1);
                string opCode = dcmd.peekString(x);
                x += (int)opCode.size() + 1;
                string operand = dcmd.peekString(x);
                x += (int)operand.size() + 1;

                m_lines.emplace_back(tag, type, start, end, text, opCode, operand);
            }
            m_nextTag = dcmd.peek32(x);
        }

        if (f.checkSection('LABL', 0))
        {
            const BlockSection& labl = f['LABL'];
            u32 numLabels = labl.peek32(0);
            int x = 4;
            for (u32 i = 0; i < numLabels; ++i)
            {
                string label = labl.peekString(x);
                x += ((int)label.size() + 5);
                MemAddr a = labl.peekAddr(x);
                x += 4;
                addLabel(label, a);
            }
        }
    }

    m_changed = false;
    return true;
}

bool DisassemblerDoc::save(string fileName)
{
    NxFile f;

    //
    // MM48 section
    //
    BlockSection mm48('MM48', 0);
    NX_ASSERT(m_mmap.size() == 65536);
    mm48.pokeData(m_mmap);
    f.addSection(mm48);

    //
    // DCMD section
    //
    BlockSection dcmd('DCMD', 0);
    dcmd.poke32(u32(m_lines.size()));
    for (const auto& line : m_lines)
    {
        dcmd.poke32((u32)line.tag);
        dcmd.poke8((u8)line.type);
        dcmd.pokeAddr(line.startAddress);
        dcmd.pokeAddr(line.endAddress);
        dcmd.pokeString(line.text);
        dcmd.pokeString(line.opCode);
        dcmd.pokeString(line.operand);
    }
    dcmd.poke32((u32)m_nextTag);
    f.addSection(dcmd);

    //
    // LABL section
    //
    BlockSection labl('LABL', 0);
    NX_ASSERT(m_labelMap.size() == m_addrMap.size());
    u32 numLabels = (u32)m_labelMap.size();
    labl.poke32(numLabels);
    for (const auto& label : m_labelMap)
    {
        labl.pokeString(label.first);
        labl.pokeAddr(label.second.second);
    }

    m_changed = false;
    return f.save(fileName);
}

optional<int> DisassemblerDoc::findLine(MemAddr addr) const
{
    for (size_t i = 0; i < m_lines.size(); ++i)
    {
        const Line& line = m_lines[i];
        if (line.type == LineType::Label || line.type == LineType::Instruction)
        {
            if (addr >= line.startAddress && addr <= line.endAddress)
            {
                return int(i);
            }
        }
        else if (line.type == LineType::Label)
        {
            if (addr == line.startAddress) return int(i);
        }
    }

    return (int)m_lines.size();
}

string DisassemblerDoc::addLabel(string label, MemAddr addr)
{
    auto it = m_addrMap.find(addr);
    if (it != m_addrMap.end())
    {
        // We already have a label for this address.
        return it->second.first;
    }

    // This is a new label
    LabelInfo info = make_pair(label, addr);
    m_labelMap[label] = info;
    m_addrMap[addr] = info;
    return label;
}
