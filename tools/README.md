# Diagnostic tools

Players don't need any of this. It is the code that found the cause, kept so others can repeat it.

## logger/

A `dsound.dll` that logs the game's XAudio2 2.0 calls. Build it with `make logger` (result in
`dist/logger/`) and install it like the fix, in its place. Single player only.

- `dsound_log.c` is version 2. It writes `wawaudio.log` next to itself (or to `%LOCALAPPDATA%`).
  With a `wawaudio_fix.txt` beside it, it also starts held streamed voices. The file may contain a
  delay in milliseconds; empty means 250.
- `dsound_log_v1.c` is version 1, which tested a wrong theory: that the game deferred its `Start`
  calls. Its `wawaudio_fix.txt` did not help.
- `test/deferred_start.c` shows what XAudio2 2.0 does with a deferred `Start`.
- `test/unstarted_callbacks.c` shows that real XAudio2 2.0 makes no callbacks for an unstarted voice.
- `test/auto_start.c` checks version 2's workaround.

The tests are silent and run on Windows with XAudio2 2.0 installed. Logs contain the game's path
on the console, so look through one before sharing it.

## analysis/

Python 3 scripts. The `clip_*` ones need numpy.

- `held_voices.py LOG...` lists streamed voices, late starts and pauses in a `wawaudio.log`.
- `voices_by_format.py LOG...` groups the voices by format and shows how long each took to play.
- `clip_levels.py`, `clip_onsets.py` and `clip_pan.py` take a 16-bit 48 kHz stereo WAV and show
  loudness, when sounds start and end, and left/right balance. `clip_pan.py` also takes ranges in
  seconds, such as `0-0.3 5.8-6.9`.
