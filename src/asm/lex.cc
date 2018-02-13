//----------------------------------------------------------------------------------------------------------------------
// Lexical analyser
//----------------------------------------------------------------------------------------------------------------------

#include <asm/asm.h>
#include <asm/lex.h>
#include <emulator/nxfile.h>
#include <utils/format.h>

//----------------------------------------------------------------------------------------------------------------------
// Lexical tables
//----------------------------------------------------------------------------------------------------------------------

// This table represents the validity of a name (symbol or keyword) character.
//
//      0 = Cannot be found within a name.
//      1 = Can be found within a name.
//      2 = Can be found within a name but not as the initial character.
//
static char gNameChar[128] =
{
    //          00 01 02 03 04 05 06 07 08 09 0a 0b 0c 0d 0e 0f // Characters
    /* 00 */    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //
    /* 10 */    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //
    /* 20 */    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //  !"#$%&' ()*+,-./
    /* 30 */    2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 0, 0, 0, 0, 0, 0, // 01234567 89:;<=>?
    /* 40 */    0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, // @ABCDEFG HIJKLMNO
    /* 50 */    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 1, // PQRSTUVW XYZ[\]^_
    /* 60 */    0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, // `abcdefg hijklmno
    /* 70 */    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, // pqrstuvw xyz{|}~
};

// Keywords
static const char* gKeywords[int(Lex::Element::Type::COUNT) - int(Lex::Element::Type::KEYWORDS)] =
{
    0,
    "A",
    "ADC",
    "ADD",
    "AF",
    "AND",
    "B",
    "BC",
    "BIT",
    "C",
    "CALL",
    "CCF",
    "CP",
    "CPD",
    "CPDR",
    "CPI",
    "CPIR",
    "CPL",
    "D",
    "DAA",
    "DE",
    "DEC",
    "DI",
    "DJNZ",
    "E",
    "EI",
    "EQU",
    "EX",
    "EXX",
    "H",
    "HALT",
    "HL",
    "I",
    "IM",
    "IN",
    "INC",
    "IND",
    "INDR",
    "INI",
    "INIR",
    "IX",
    "IY",
    "JP",
    "JR",
    "L",
    "LD",
    "LDD",
    "LDDR",
    "LDI",
    "LDIR",
    "M",
    "NC",
    "NEG",
    "NOP",
    "NZ",
    "OR",
    "ORG",
    "OTDR",
    "OTIR",
    "OUT",
    "OUTD",
    "OUTI",
    "P",
    "PE",
    "PO",
    "POP",
    "PUSH",
    "R",
    "RES",
    "RET",
    "RETI",
    "RETN",
    "RL",
    "RLA",
    "RLC",
    "RLCA",
    "RLD",
    "RR",
    "RRA",
    "RRC",
    "RRCA",
    "RRD",
    "RST",
    "SBC",
    "SCF",
    "SET",
    "SLA",
    "SLL",
    "SP",
    "SRA",
    "SRL",
    "SUB",
    "XOR",
    "Z",
};

//----------------------------------------------------------------------------------------------------------------------
// Lexer implementation
//----------------------------------------------------------------------------------------------------------------------

Lex::Lex()
{
}

void Lex::parse(Assembler& assembler, string fileName)
{
    m_file = NxFile::loadFile(fileName);
    m_fileName = fileName;
    m_start = m_file.data();
    m_end = m_file.data() + m_file.size();
    m_cursor = m_file.data();
    m_position = Element::Pos{ 0, 1, 1 };
    m_lastPosition = Element::Pos{ 0, 1, 1 };

    while (next(assembler) != Element::Type::EndOfFile) ;
}

char Lex::nextChar()
{
    char c;

    m_lastPosition = m_position;
    m_lastCursor = m_cursor;
    if (m_cursor == m_end) return 0;

    c = *m_cursor++;
    ++m_position.m_col;

    // Convert to uppercase.
    if (c >= 'a' && c <= 'z') c -= 32;

    // Check for newlines
    if ('\r' == c || '\n' == c)
    {
        ++m_position.m_line;
        m_position.m_col = 1;
        if (c == '\r')
        {
            // Handle Mac or Windows newlines
            if ((m_cursor < m_end) && (*m_cursor == '\n'))
            {
                // This is CRLF (Windows)
                ++m_cursor;
            }
            // Either way, make sure the character is always '\n'.
            c = '\n';
        }
        m_position.m_lineOffset = m_cursor - m_start;
    }

    return c;
}

void Lex::ungetChar()
{
    m_position = m_lastPosition;
    m_cursor = m_lastCursor;
}

Lex::Element::Type Lex::error(Assembler& assembler, const std::string& msg)
{
    assembler.output(stringFormat("!{0}({1}): Lexical Error: {2}", m_fileName, m_lastPosition.m_line, msg));

    i32 x = m_lastPosition.m_col - 1;
    const u8* lineStart = m_file.data() + m_lastPosition.m_lineOffset;
    const u8* fileEnd = m_file.data() + m_file.size();

    // Print the line that token resides in
    const u8* p = lineStart;
    while ((p < fileEnd) && (*p != '\r') && (*p != '\n')) ++p;
    string line(lineStart, p);
    assembler.output(line);

    // Print the cursor point where the error is
    line.clear();
    for (int j = 0; j < x; ++j) line += ' ';
    line += '^';
    assembler.output(line);
    assembler.addError();

    return Element::Type::Error;
}

Lex::Element::Type Lex::next(Assembler& assembler)
{
    error(assembler, "Assembler not implemented yet!");
    return Element::Type::EndOfFile;
}
