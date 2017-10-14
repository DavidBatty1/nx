//----------------------------------------------------------------------------------------------------------------------
// The emulator object
// Manages a Spectrum-derived object and the UI (including the debugger)
//----------------------------------------------------------------------------------------------------------------------

#pragma once

#include "spectrum.h"
#include "debugger.h"

#include <SFML/Graphics.hpp>
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
// Emulator class
//----------------------------------------------------------------------------------------------------------------------

class Nx
{
public:
    Nx(int argc, char** argv);
    ~Nx();

    // Render the currently generated display
    void render();

    // The emulator main loop.  Will exit when the window is closed.
    void run();

    // Generate a single frame, including processing audio.
    void frame();
    
    // Load a snapshot
    bool loadSnapshot(string fileName);
    
    // Settings
    void setSetting(string key, string value);
    string getSetting(string key, string defaultSetting = "no");
    void updateSettings();

    // Debugging
    void togglePause(bool breakpointHit);
    void stepOver();
    void stepIn();

    // Peripherals
    bool usesKempstonJoystick() const { return m_kempstonJoystick; }
    
private:
    // Keyboard routines
    void spectrumKey(sf::Keyboard::Key key, bool down, bool shift, bool ctrl, bool alt);
    void debuggerKey(sf::Keyboard::Key key);
    void joystickKey(Joystick key, bool down);
    void calculateKeys();
    
    // File routines
    vector<u8> loadFile(string fileName);

private:
    Spectrum*           m_machine;
    Signal              m_renderSignal;
    bool                m_quit;

    // Keyboard state
    vector<bool>        m_speccyKeys;
    vector<u8>          m_keyRows;

    // Debugger state
    Debugger            m_debugger;
    bool                m_debuggerEnabled;
    RunMode             m_runMode;

    // Settings
    map<string, string> m_settings;

    // Rendering
    sf::RenderWindow    m_window;

    // Peripherals
    bool                m_kempstonJoystick;
};

//----------------------------------------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------------------------------------
