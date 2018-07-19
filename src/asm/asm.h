//----------------------------------------------------------------------------------------------------------------------
// Assembler
//----------------------------------------------------------------------------------------------------------------------

#pragma once

#include <config.h>
#include <asm/disasm.h>
#include <asm/expr.h>
#include <asm/lex.h>
#include <asm/stringtable.h>
#include <emulator/spectrum.h>
#include <utils/filename.h>

#include <array>
#include <vector>

//----------------------------------------------------------------------------------------------------------------------
// MemoryMap
// Manages writing to memory from the assembler.  At the bottom level is the full physical memory map.  Above that
// is a layer that matches the actual address-space that are defined by assembler directives such as org.  The job
// of the memory map is 3-fold:
//
//  - Store bytes that are generated by the assembler
//  - Understand which parts of the address space have been written to and allow them to be only written once per
//    pass.
//  - Provide a flat interface to different areas of memory.  Multiple areas of memory can be viewed as one contiguous
//    piece of memory.  The assembler will try to fill this up and if it runs out, an error will occur.
//----------------------------------------------------------------------------------------------------------------------

class MemoryMap
{
public:
    MemoryMap(Spectrum& speccy);

    //
    // Interface parameters
    //
    void clear(Spectrum& speccy);
    void setPass(int pass);
    void resetRange();
    void addRange(MemAddr start, MemAddr end);
    void addZ80Range(Spectrum& speccy, Z80MemAddr start, Z80MemAddr end);

    //
    // Memory writing
    // Addresses are relative to the current range settings
    //
    bool poke8(int address, u8 byte);
    bool poke16(int address, u16 word);
    void upload(Spectrum& speccy);
    bool isValidAddress(int i) const { return i >= 0 && i < int(m_addresses.size()); }
    MemAddr getAddress(int i) const { NX_ASSERT(isValidAddress(i)); return m_addresses[i]; }

private:
    class Byte
    {
    public:
        Byte();

        operator u8() const { return m_byte; }
        bool poke(u8 b, u8 currentPass);
        bool written() const;
        void clear();

    private:
        u8  m_pass;
        u8  m_byte;
    };

    Model                   m_model;
    vector<Byte>            m_memory;
    vector<MemAddr>         m_addresses;
    u8                      m_currentPass;
};

using Labels = vector<Disassembler::LabelInfo>;


//----------------------------------------------------------------------------------------------------------------------
// Assembler
//----------------------------------------------------------------------------------------------------------------------

class AssemblerWindow;

class Assembler
{
public:
    //------------------------------------------------------------------------------------------------------------------
    // Public interface
    //------------------------------------------------------------------------------------------------------------------

    Assembler(AssemblerWindow& window,
              Spectrum& speccy);

    void startAssembly(const vector<u8>& data, string sourceName);
    void output(const std::string& msg);
    int numErrors() const { return (int)m_errors.getErrors().size(); }
    void addErrorInfo(const string& fileName, const string& message, int line, int col);
    i64 getSymbol(const u8* start, const u8* end, bool ignoreCase) { return m_eval.getSymbols().addRange((const char*)start, (const char *)end, ignoreCase); }
    optional<ExprValue> calculateExpression(const vector<u8>& exprData);

    optional<MemAddr> lookUpLabel(i64 symbol) const;
    optional<ExprValue> lookUpValue(i64 symbol) const;

    Labels getLabels() const;
    void setLabels(const Labels& labels);

    vector<ErrorInfo> getErrorInfos() const { return m_errors.getErrors(); }

    struct Options
    {
        enum class Output
        {
            Memory,
            Null,
        };
        MemAddr     m_startAddress;
        Output      m_output;

        Options()
            : m_startAddress()
            , m_output(Output::Memory)
        {}
    };

    const Options& getOptions() const { return m_options; }

private:
    //------------------------------------------------------------------------------------------------------------------
    // Internal methods
    //------------------------------------------------------------------------------------------------------------------

    // Generates a vector<Lex::Element> database from a file
    bool assemble(const vector<u8>& data, string sourceName);
    bool assembleFile1(Path fileName);
    bool assembleFile2(Path fileName);

    bool addLabel(i64 symbol, MemAddr address);
    bool addValue(i64 symbol, ExprValue value);

    void dumpLex(const Lex& l);
    void dumpSymbolTable();

    const string& currentFileName() const { NX_ASSERT(!m_fileStack.empty()); return m_fileStack.back(); }
    Lex& currentLex() { NX_ASSERT(!m_fileStack.empty()); return m_sessions[m_fileStack.back()]; }
    const Lex& currentLex() const { NX_ASSERT(!m_fileStack.empty()); return m_sessions.at(m_fileStack.back()); }

    //------------------------------------------------------------------------------------------------------------------
    // Parsing utilities
    //------------------------------------------------------------------------------------------------------------------

    // Format spec:
    //
    //  *   expression
    //  %   indexed expression
    //  [   start optional
    //  ]   end optional
    //  {   start one-of
    //  }   end one-of
    //  ,   comma
    //  (   open parentheses
    //  )   close parentheses
    //  '   AF'
    //  f   NZ,Z,NC,C
    //  F   NZ,Z,NC,C,PO,PE,P,M
    //  $   symbol
    //
    //  Specific 8-bit registers: abcdehlirx        (x = IXH, IXL, IYH or IYL)
    //  Specific 16-bit registers: ABDHSX           (AF, BC, DE, HL, SP, IX/IY)
    //
    bool expect(Lex& lex, const Lex::Element* e, const char* format, const Lex::Element** outE = nullptr) const;
    bool expectExpression(Lex& lex, const Lex::Element* e, const Lex::Element** outE) const;

    int invalidInstruction(Lex& lex, const Lex::Element* e, const Lex::Element** outE);
    void nextLine(const Lex::Element*& e);

    //------------------------------------------------------------------------------------------------------------------
    // Pass 1
    // The first pass figures out the segments, label addresses.  No expressions are evaluated at this
    // point.
    //------------------------------------------------------------------------------------------------------------------

    bool pass1(Lex& lex, const vector<Lex::Element>& elems);
    int assembleInstruction1(Lex& lex, const Lex::Element* e, const Lex::Element** outE);
    int assembleLoad1(Lex& lex, const Lex::Element* e, const Lex::Element** outE);

    //------------------------------------------------------------------------------------------------------------------
    // Pass 2
    // Evaluates variable expressions and generates the opcodes
    //------------------------------------------------------------------------------------------------------------------

    struct Operand
    {
        OperandType     type;       // Type of operand
        ExprValue       expr;       // Expression (if necessary)
    };

    bool pass2(Lex& lex, const vector<Lex::Element>& elems);
    const Lex::Element* assembleInstruction2(Lex& lex, const Lex::Element* e);
    //Expression buildExpression(const Lex::Element*& e) const;
    bool buildOperand(Lex& lex, const Lex::Element*& e, Operand& op);
    optional<u8> calculateDisplacement(Lex& lex, const Lex::Element* e, ExprValue& expr);
    Path findFile(Path givenPath);
    optional<MemAddr> getZ80AddressFromExpression(Lex& lex, const Lex::Element* e, ExprValue expr);
    bool CheckIntOpRange(Lex& lex, const Lex::Element* e, Operand operand, i64 a, i64 b);

    //
    // Directives
    //
    bool doOrg(Lex& lex, const Lex::Element*& e);
    bool doEqu(Lex& lex, i64 symbol, const Lex::Element*& e);
    bool doDb(Lex& lex, const Lex::Element*& e);
    bool doDw(Lex& lex, const Lex::Element*& e);
    bool doDs(Lex& lex, const Lex::Element*& e);
    bool doOpt(Lex& lex, const Lex::Element*& e);

    //
    // Options
    //
    bool doOptStart(Lex& lex, const Lex::Element*& e);
    bool doOptOutput(Lex& lex, const Lex::Element*& e);

    //
    // Emission utilities
    //
    u8 r(OperandType ot) const;
    u8 rp(OperandType ot) const;
    u8 rp2(OperandType ot) const;
    u8 cc(OperandType ot) const;
    u8 rot(Lex::Element::Type opCode) const;
    u8 alu(Lex::Element::Type opCode) const;

    bool emit8(Lex& l, const Lex::Element* e, u8 b);
    bool emit16(Lex& l, const Lex::Element* e, u16 w);
    bool emitXYZ(Lex& l, const Lex::Element* e, u8 x, u8 y, u8 z);
    bool emitXPQZ(Lex& l, const Lex::Element* e, u8 x, u8 p, u8 q, u8 z);

    u16 make16(Lex& lex, const Lex::Element& e, const Spectrum& speccy, ExprValue result);

private:
    //------------------------------------------------------------------------------------------------------------------
    // Member variables
    //------------------------------------------------------------------------------------------------------------------

    ExpressionEvaluator         m_eval;
    ErrorManager                m_errors;

    struct SymbolInfo
    {
        MemAddr     m_addr;

        SymbolInfo() : m_addr() {}
        SymbolInfo(MemAddr addr) : m_addr(addr) {}
    };

    map<string, Lex>            m_sessions;
    vector<string>              m_fileStack;
    AssemblerWindow&            m_assemblerWindow;
    Spectrum&                   m_speccy;

    //
    // Database generated by the passes
    //
    MemoryMap                   m_mmap;
    int                         m_address;

    Options                     m_options;
};

//----------------------------------------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------------------------------------
