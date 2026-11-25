# waw-dialog-fix

Fixes missing NPC dialog in the single-player game *Call of Duty: World at War* (PC version) when it
runs on PS5 through [Playden](https://github.com/leolobato/playden-ps5).

**The symptom:** characters move their mouths but you hear nothing. When you pause and unpause, all
the missed lines play at once, and then it starts again.

> **Status: experimental.** An earlier logging build with the same workaround fixed the dialog for
> one player (Playden 0.12.0-beta.5, runtime kit 0.36, Steam version). This smaller build passes its
> tests on Windows but has **not been confirmed on a PS5 yet**. Remove this note once it has.

## Install

You need FTP or similar access to the game's folder on the console.

1. Copy `dsound.dll` into the folder that contains `CoDWaW.exe`. If there is already a `dsound.dll`
   there, stop: it belongs to another mod.
2. In Playden, open **Game settings > Library overrides** (under Advanced). Keep what is there and
   add `;dsound=n,b`. If the field is empty, enter `dsound=n,b`.
3. Start the game.

## Uninstall

Delete `dsound.dll` and take `dsound=n,b` out of Library overrides.

**Do this before playing multiplayer.** The fix only acts in `CoDWaW.exe` (single player and
co-op), but the file is still loaded by `CoDWaWmp.exe`, and anti-cheat may object to it.

## Known limits

- It is a workaround. Why the game does not start these sounds under Playden is not known.
- Dialog starts up to about a quarter of a second late (`DELAY_MS`, see Build).
- A line the game prepares in advance on purpose could play early.
- A pause is recognised by three sounds being stopped in a row. If fewer are playing, a line may
  carry on in the pause menu.
- Music or ambience that was silent before may now be heard. The game held those sounds too.

## How it works

The game plays sound through XAudio2 2.0. Logs of it under Playden show that it creates a voice for
each streamed sound and gives it audio, but calls `Start` only when the player unpauses. Sounds that
are loaded whole are started normally, and once `Start` is called a voice plays at once, so the
audio driver does not appear to be at fault.

`dsound.dll` is a library the game loads. This one passes every DirectSound call to the real one. In
`CoDWaW.exe` it also watches the game's XAudio2 voices: a streamed voice that has had audio for
250 ms without being started is started when the game next asks for its state. Voices started this
way are stopped while the game is paused. It writes no files, changes no game files, has no network
code, and only imports `kernel32.dll`.

## Build

With mingw-w64 for 32-bit Windows (`apt install gcc-mingw-w64-i686 make python3`):

    make                  # dist/dsound.dll
    make DELAY_MS=100     # a different delay (run make clean first)
    make release          # dist/waw-dialog-fix.zip and dist/SHA256SUMS
    make logger           # dist/logger/, the diagnostic logging DLL and its tests
    make test             # dist/test/; run CoDWaW.exe and CoDWaWmp.exe there on Windows

The test programs are silent. They need XAudio2 2.0 from the DirectX End-User Runtimes
(June 2010), and are only named like the game because the fix checks the program's name.

## Source

- `src/` is the fix, and `test/` its test.
- `tools/` has the logging DLL and analysis scripts used to find the cause: see `tools/README.md`.
  They are not in the release zip.

## Licence

MIT, see [LICENSE](LICENSE). Not affiliated with Activision or Playden. No game files are included.
