IIC+ Transpose DSP - experimental isolated module

PolyphonicTranspose.h is a JUCE AudioBuffer-based dual-head delay pitch-shifter prototype.
It is intentionally NOT connected to AudioEngine or the UI yet.

Important:
- setSemitones() accepts -12 through +12.
- At 0 semitones process() outputs the input samples unchanged (transparent bypass).
- prepare() allocates delay memory; call it from the audio-device preparation path, never from the audio callback.
- process() performs no allocation or locking.
- This algorithm shifts the composite waveform; it can produce modulation/comb artifacts.
  It must be auditioned on clean notes, chords, distorted chords and palm-muted passages
  before integration. It is not yet approved as the final sound-quality solution.

The module requires JUCE headers and is not a standalone application.
