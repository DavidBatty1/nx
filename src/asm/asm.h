//----------------------------------------------------------------------------------------------------------------------
// Assembler
//----------------------------------------------------------------------------------------------------------------------

#pragma once

#include <asm/lex.h>
#include <asm/stringtable.h>
#include <emulator/spectrum.h>

#include <array>
#include <map>
#include <optional>
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

    using Address = u32;

    //
    // Interface parameters
    //
    void setPass(int pass);
    void resetRange();
    void addRange(Address start, Address end);
    void addZ80Range(u16 start, u16 end);

    //
    // Memory writing
    // Addresses are relative to the current range settings
    //
    bool poke8(int address, u8 byte);
    bool poke16(int address, u16 word);
    void upload(Spectrum& speccy);
    bool isValidAddress(int i) const { return i >= 0 && i < int(m_addresses.size()); }
    Address getAddress(int i) const { assert(isValidAddress(i)); return m_addresses[i]; }

private:
    class Byte
    {
    public:
        Byte();

        operator u8() const { return m_byte; }
        bool poke(u8 b, u8 currentPass);
        bool written() const;

    private:
        u8  m_pass;
        u8  m_byte;
    };

    Model                   m_model;
    array<u8, kNumSlots>    m_slots;
    vector<Byte>            m_memory;
    vector<Address>         m_addresses;
    u8                      m_currentPass;
    u16                     m_offset;
};


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
              Spectrum& speccy,
              const vector<u8>& data,
              string sourceName);

    void output(const std::string& msg);
    void addError();
    int numErrors() const { return m_numErrors; }
    void error(const Lex& l, const Lex::Element& el, const string& message);
    i64 getSymbol(const u8* start, const u8* end) { return m_lexSymbols.add((const char*)start, (const char *)end); }

    optional<i64> lookUpLabel(i64 symbol);
    optional<i64> lookUpValue(i64 symbol);

private:
    //------------------------------------------------------------------------------------------------------------------
    // Internal methods
    //------------------------------------------------------------------------------------------------------------------

    // Generates a vector<Lex::Element> database from a file
    void startAssembly(const vector<u8>& data, string sourceName);
    void assemble(const vector<u8>& data, string sourceName);
    void assembleFile(string fileName);

    bool addSymbol(i64 symbol, MemoryMap::Address address);
    bool addValue(i64 symbol, i64 value);

    void dumpLex(const Lex& l);
    void dumpSymbolTable();

    //------------------------------------------------------------------------------------------------------------------
    // Parsing utilties
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
    //
    //  Specific 8-bit registers: abcdehlirx        (x = IXH, IXL, IYH or IYL)
    //  Specific 16-bit registers: ABDHSX           (AF, BC, DE, HL, SP, IX/IY)
    //
    bool expect(Lex& lex, const Lex::Element* e, const char* format, const Lex::Element** outE = nullptr);
    bool expectExpression(Lex& lex, const Lex::Element* e, const Lex::Element** outE);

    int invalidInstruction(Lex& lex, const Lex::Element* e, const Lex::Element** outE);

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

    enum class OperandType
    {
        None,                   // No operand exists
        Expression,             // A valid expression
        AddressedExpression,    // A valid address expression (i.e. (nnnn)).
        IX_Expression,
        IY_Expression,

        A,
        B,
        C,
        D,
        E,
        H,
        L,
        I,
        R,
        AF,
        AF_,
        BC,
        DE,
        HL,
        IX,
        IY,
        IXH,
        IXL,
        IYH,
        IYL,
        SP,
        NC,
        Z,
        NZ,
        PO,
        PE,
        M,
        P,
        Address_BC,
        Address_DE,
        Address_HL,
        Address_SP,
        Address_C,
    };

    class Expression
    {
    public:
        Expression();

        enum class ValueType
        {
            UnaryOp,
            BinaryOp,
            OpenParen,
            CloseParen,
            Integer,
            Symbol,
            Char,
            Dollar,
        };

        struct Value
        {
            ValueType               type;
            i64                     value;
            const Lex::Element*     elem;       // The element that described the operand

            Value(ValueType type, i64 value, const Lex::Element* e) : type(type), value(value), elem(e) {}
        };

        void addValue(ValueType type, i64 value, const Lex::Element* e);
        void addUnaryOp(Lex::Element::Type op, const Lex::Element* e);
        void addBinaryOp(Lex::Element::Type op, const Lex::Element* e);
        void addOpen(const Lex::Element* e);
        void addClose(const Lex::Element* e);
        void set(i64 result) { m_result = result; }

        bool eval(Assembler& assembler, Lex& lex, MemoryMap::Address currentAddress);

        i64 result() const { return m_result; }
        u8 r8() const { return u8(m_result); }
        u16 r16() const { return u16(m_result); }

    private:
        vector<Value>   m_queue;
        i64             m_result;
    };

    struct Operand
    {
        OperandType     type;       // Type of operand
        Expression      expr;       // Expression (if necessary)
    };

    bool pass2(Lex& lex, const vector<Lex::Element>& elems);
    const Lex::Element* assembleInstruction2(Lex& lex, const Lex::Element* e);
    Expression buildExpression(const Lex::Element*& e);
    bool buildOperand(Lex& lex, const Lex::Element*& e, Operand& op);
    optional<u8> calculateDisplacement(Lex& lex, const Lex::Element* e, Expression& expr);

    //
    // Directives
    //
    bool doOrg(Lex& lex, const Lex::Element*& e);
    bool doEqu(Lex& lex, i64 symbol, const Lex::Element*& e);

    //
    // Emission utilities
    //
    u8 r(OperandType ot) const;
    u8 rp(OperandType ot) const;
    u8 rp2(OperandType ot) const;
    u8 cc(OperandType ot) const;
    u8 rot(Lex::Element::Type opCode) const;
    u8 alu(Lex::Element::Type opCode) const;

    void emit8(u8 b);
    void emit16(u16 w);
    void emitXYZ(u8 x, u8 y, u8 z);
    void emitXPQZ(u8 x, u8 p, u8 q, u8 z);

private:
    //------------------------------------------------------------------------------------------------------------------
    // Member variables
    //------------------------------------------------------------------------------------------------------------------

    struct SymbolInfo
    {
        MemoryMap::Address  m_addr;

        SymbolInfo() : m_addr(0) {}
        SymbolInfo(MemoryMap::Address addr) : m_addr(addr) {}
    };

    vector<Lex>                 m_sessions;
    AssemblerWindow&            m_assemblerWindow;
    Spectrum&                   m_speccy;
    int                         m_numErrors;

    // Symbols (labels)
    map<i64, SymbolInfo>        m_symbolTable;
    map<i64, i64>               m_values;
    StringTable                 m_lexSymbols;   // Symbols shared by all Lex instances

    // Variables
    map<i64, i64>               m_variables;

    //
    // Database generated by the passes
    //
    MemoryMap                   m_mmap;
    int                         m_address;
};

//----------------------------------------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------------------------------------
