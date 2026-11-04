ASIO enablement patch for IIC+ NAM

This patch enables JUCE's ASIO backend in the CMake target. It assumes the JUCE checkout in external/JUCE includes its bundled ASIO headers (current JUCE versions do).

Apply by replacing CMakeLists.txt in your existing IICPlusNAM project with this one. Then from the project directory run:

  cmake -S . -B build -G "Visual Studio 17 2022" -A x64
  cmake --build build --config Release

Run the newly built app and open Audio Settings. The device type list should include ASIO. Select ASIO, then select the Focusrite ASIO driver/device and appropriate input/output channels.

If CMake reports JUCE_ASIO is unsupported or the compiler cannot find ASIO headers, send the last 20-30 lines of the build output. Do not replace your NAM model or IR.

Licensing note: JUCE documents that enabling JUCE_ASIO makes the program depend on Steinberg ASIO SDK files and the resulting program must comply with the applicable ASIO SDK license. This patch is for local testing; review current licensing terms before distributing an ASIO-enabled build.
