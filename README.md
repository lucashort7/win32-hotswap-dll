# win32-hotswap-dll

Load a DLL into a running process, or unload one, without restarting it. Built for the mod loop: build the DLL, swap it into the game, read the log, repeat.

```
hotswap inject CONTROLResonant.exe C:\path\to\mod.dll
hotswap eject  CONTROLResonant.exe mod.dll
hotswap list   CONTROLResonant.exe
```

`inject` writes the DLL's path into the process and runs `LoadLibraryA` there on a remote thread. `eject` runs `FreeLibrary` on the module's handle the same way. `LoadLibrary` on a DLL already loaded only bumps its reference count, so a swap is eject then inject, and a DLL a loader picked up at boot cannot be re-injected until it is out.

An eject unmaps the code and nothing else: a DLL that hooked functions must undo its hooks first, or the next call into it crashes the process. The clean shape is a DLL that unloads itself on a command (`FreeLibraryAndExitThread` after its cleanup), leaving this tool with `inject` only.

## Build

MinGW and CMake on Windows. From WSL: `cmd.exe /c build.bat`, or by hand:

```
cmake -S . -B build -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

MIT.
