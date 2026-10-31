# SofGlitch

Hierarchical rule-based glitch plugin (AU / VST3 / Standalone, JUCE).

Five rule lanes. Each lane fires on the **last N units of every E-th group of G units**, with a seeded probability,
and applies one action for `Length × unit` time. Top lane wins on overlap.

Actions: Cut, Duck, Gate, Filter, Crush, Stutter, Stutter Ramp, Reverse, Hold, Pitch, Pitch Dive, Tape Stop, Sample (per-lane sample slot).

Needs a JUCE 8 checkout (default `../JUCE`, override with `-DJUCE_DIR=...`).

Build:  cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build -j8   (AU/VST3 auto-install to ~/Library/Audio/Plug-Ins)
Test:   cmake --build build --target SofGlitchTest && ./build/SofGlitchTest_artefacts/Release/SofGlitchTest

its not that deep tho its just a glitch plugin similar to effectrix I think anyways have fun bye
