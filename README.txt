DARKMOD SINGLE EXE
==================

Shipping unit:
    DarkModLauncher.exe, and nothing else.

It contains:
    - Color replacement UI (palettes, per-family overrides, mapping editor)
    - 1C process discovery
    - Inject DLL / Remove DLL
    - Embedded DarkModDLL payload

It needs no Visual C++ Redistributable and no other installed component.


BUILD
=====

1. Open DarkMod.sln in Visual Studio, x64, Release (or Debug).
2. Build Solution.

Two projects:

    DarkModDLL       -> bin\<Config>\DarkModDLL.dll
    DarkModLauncher  -> bin\<Config>\DarkModLauncher.exe

DarkMod.sln declares DarkModLauncher as dependent on DarkModDLL, so the
DLL is finished before the launcher's resource step runs.

DarkModLauncher.rc stores the whole DLL as RCDATA under the id in
DarkModResource.h. The .rc names the file without a path; rc.exe finds it
because the project puts $(OutDir) on the resource include path. That is
why one line covers both Debug and Release.

Requirements the projects assume:

    Platform toolset  v145
    Detours           C:\crs\Detours   (include\, lib.X64\)

Both projects use the static CRT (/MTd, /MT). This is not just packaging:
the debug CRT is not redistributable at all, so a /MDd build could never
have started on another machine. The injected DLL also gets its own CRT
instance inside 1C instead of sharing one.

Build note: while the DLL is injected into 1C, bin\<Config>\DarkModDLL.dll
is locked and the build cannot replace it. Click "Remove DLL" (or close
1C) first.

embed_dll.py is obsolete. It generated a C array into DarkModUI\, a
project that never existed, and was never called. The .rc replaced it.


RUNTIME LAYOUT
==============

Everything the program owns lives in one directory:

    %LOCALAPPDATA%\DarkMod\DarkModDLL.dll
    %LOCALAPPDATA%\DarkMod\darkmod.ini
    %LOCALAPPDATA%\DarkMod\darkmod_fonts.ini
    %LOCALAPPDATA%\DarkMod\darkmod_colors.json
    %LOCALAPPDATA%\DarkMod\darkmod_palette_overrides.ini
    %LOCALAPPDATA%\DarkMod\darkmod_custom_palette.ini
    %LOCALAPPDATA%\DarkMod\darkmod.log            (written by the DLL)

One directory and not two, because the injected DLL derives every path
from its own module directory. Splitting them would leave it looking for
the configuration next to itself and finding nothing.

%LOCALAPPDATA% and not the exe directory, so the single exe can sit
anywhere, including a read-only location.

The DLL has to exist as a real file: injection goes through
CreateRemoteThread(LoadLibraryW), which takes a path, and the launcher
later locates the module inside 1C by name. So the exe writes the file
out on startup rather than loading it from memory. If the copy on disk
already matches the embedded bytes, nothing is written - which is also
what keeps a currently injected DLL from being reported as an error.

On first run the launcher copies any configuration files it finds next to
the exe into the data directory, without overwriting. That carries an
older side-by-side setup over instead of silently starting from defaults.


USAGE
=====

1. Start 1C.
2. Start DarkModLauncher.exe.
3. Select a 1C process (usually 1cv8c.exe).
4. Pick a palette, or edit mappings directly.
5. Click "Apply".
6. Click "Inject DLL".
7. To remove it, click "Remove DLL".

Changes are not live. The launcher writes the palette to darkmod.ini and
the injected DLL picks it up on its next poll; the MOD slider is what
applies it.

A good first test is:

    #FFFFFF -> #FF0000

which makes the replacement visually obvious.


NOTES
=====

- x64 only.
- If 1C runs elevated, DarkModLauncher.exe must also run elevated.
- Removal uses the exported DarkModUnload entry point inside the injected
  DLL. The DLL stops its worker and calls FreeLibraryAndExitThread itself;
  this avoids freeing a DLL while one of its own threads is still running.
- The rendering hook changes Cairo source RGB values and preserves alpha.
  There is a second, raster path for image surfaces.
