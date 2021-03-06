//----------------------------------------------------------------------------------------------------------------------
// The emulator object
// Manages a Spectrum-derived object and the UI (including the debugger)
//----------------------------------------------------------------------------------------------------------------------

#pragma once

#include <asm/overlay_asm.h>
#include <debugger/overlay_debugger.h>
#include <disasm/overlay_disasm.h>
#include <editor/overlay_editor.h>
#include <emulator/spectrum.h>
#include <tape/tape.h>

#include <SFML/Graphics.hpp>

#include <algorithm>
#include <map>
#include <mutex>
#include <thread>

enum class Joystick
{
    Left,
    Right,
    Up,
    Down,
    Fire
};

//----------------------------------------------------------------------------------------------------------------------
// Model window
//----------------------------------------------------------------------------------------------------------------------

class ModelWindow final : public Window
{
public:
    ModelWindow(Nx& nx);

    bool visible() const { return m_selectedModel >= 0; }
    void switchModel(Model model);

private:
    void onDraw(Draw& draw) override;
    void onKey(sf::Keyboard::Key key, bool down, bool shift, bool ctrl, bool alt) override;
    void onText(char ch) override {};

private:
    vector<Model>   m_models;
    int             m_selectedModel;
    int             m_originalModel;
};

//----------------------------------------------------------------------------------------------------------------------
// Emulator overlay
//----------------------------------------------------------------------------------------------------------------------

class Nx;

class Emulator : public Overlay
{
public:
    Emulator(Nx& nx);

    void render(Draw& draw) override;
    void key(sf::Keyboard::Key key, bool down, bool shift, bool ctrl, bool alt) override;
    void text(char ch) override;

    void showStatus();

    void openFile();
    void saveFile();

    void switchModel(Model model);
    void clearKeys();

private:
    void joystickKey(Joystick key, bool down);
    void calculateKeys();

private:
    // Keyboard state
    vector<u8>          m_speccyKeys;
    vector<u8>          m_keyRows;
    int                 m_counter;

    // Model select
    ModelWindow         m_modelWindow;

};

//----------------------------------------------------------------------------------------------------------------------
// Emulator class
//----------------------------------------------------------------------------------------------------------------------

class Nx
{
public:
    Nx(int argc, char** argv);
    ~Nx();

    // Obtain a reference to the current machine.
    Spectrum& getSpeccy() { return *m_machine; }
    const Spectrum& getSpeccy() const { return *m_machine; }

    // Obtain a reference to the debugger.
    Debugger& getDebugger() { return m_debugger; }

    // Obtain a reference to the assembler.
    Assembler& getAssembler() { return m_assembler; }

    // Obtain a reference to the emulator overlay.
    Emulator& getEmulator() { return m_emulator; }

    // Render the currently generated display
    void render();

    // The emulator main loop.  Will exit when the window is closed.
    void run();

    // Generate a single frame, including processing audio.
    void frame();
    
    // Open a file, detect it's type and try to open it
    bool openFile(string fileName);

    // Save a file as a snapshot.
    bool saveFile(string fileName);
    
    // Settings
    void setSetting(string key, string value);
    string getSetting(string key, string defaultSetting = "no");
    void updateSettings();

    // Debugging
    bool isDebugging() const { return Overlay::currentOverlay() == &m_debugger; }
    void togglePause(bool breakpointHit);
    void stepOver();
    void stepIn();
    void stepOut();
    RunMode getRunMode() const { return m_runMode; }
    void setRunMode(RunMode runMode) { m_runMode = m_runMode; }

    // Peripherals
    bool usesKempstonJoystick() const { return m_kempstonJoystick; }

    // Mode selection
    void showTapeBrowser();
    void toggleDebugger();
    void showEditor();
    void showDisassembler();
    void hideAll();
    bool assemble(const vector<u8>& data, string sourceName);
    void switchModel(Model model);

    // Zoom
    void toggleZoom();
    bool getZoom() const { return m_zoom; }
    
private:
    // Window
    string getTitle() const;

    // Loading
    bool loadSnaSnapshot(string fileName);
    bool loadZ80Snapshot(string fileName);
    bool loadTape(string fileName);
    bool loadNxSnapshot(string fileName);

    // Saving
    bool saveSnaSnapshot(string fileName);
    bool saveNxSnapshot(string fileName, bool saveEmulatorSettings);
    
    // Debugging helper functions
    u16 nextInstructionAt(u16 address);
    bool isCallInstructionAt(u16 address);

    // Window scale
    void setScale(int scale);

private:
    Spectrum*           m_machine;
    Ui                  m_ui;
    Signal              m_renderSignal;
    bool                m_quit;
    int                 m_frameCounter;
    bool                m_zoom;

    // Emulator overlay
    Emulator            m_emulator;

    // Debugger state
    Debugger            m_debugger;
    RunMode             m_runMode;

    // Assembler state
    EditorOverlay       m_editorOverlay;
    AssemblerOverlay    m_assemblerOverlay;
    Assembler           m_assembler;
    DisassemblerOverlay m_disassemblerOverlay;

    // Settings
    map<string, string> m_settings;

    // Rendering
    sf::RenderWindow    m_window;

    // Peripherals
    bool                m_kempstonJoystick;

    // Tape emulation
    TapeBrowser         m_tapeBrowser;

    // Files
    Path                m_tempPath;

    // Key storage
    struct KeyInfo
    {
        bool                isKey;
        bool                pressed;
        bool                shift;
        bool                ctrl;
        bool                alt;
        sf::Keyboard::Key   code;

        KeyInfo(bool isKey, bool pressed, bool shift, bool ctrl, bool alt, sf::Keyboard::Key key)
            : isKey(isKey)
            , pressed(pressed)
            , shift(shift)
            , ctrl(ctrl)
            , alt(alt)
            , code(key)
        {}
    };
    vector<KeyInfo>         m_keys;
};

//----------------------------------------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------------------------------------
