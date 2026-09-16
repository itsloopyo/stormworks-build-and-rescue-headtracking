# Third-Party Notices

StormworksHeadTracking bundles, statically links, or credits the third-party components
listed below. Each remains the property of its authors and is used under its own
licence. Where a licence requires the copyright notice, the conditions and the
disclaimer to accompany a binary distribution, the full text is reproduced here
verbatim, and this file ships at the root of every release ZIP we publish.

The mod's own code is MIT licensed, copyright itsloopyo; see
`LICENSE` at the root of this repository and of every release ZIP.

This project redistributes no part of Stormworks: Build and Rescue: no game
code, no game assets, no proprietary DLLs and no data files. The closing section
sets out what it does record about the game, and on what basis.

| Component | Version | Licence | How it ships |
|-----------|---------|---------|--------------|
| cameraunlock-core | c480d8a8177753966a7d33b857f1db12f5e9fe39 | MIT | Compiled into `opengl32.dll` |
| MinHook | 1.3.4, with the local change noted below | BSD-2-Clause | Compiled into `opengl32.dll` through cameraunlock-core |
| Hacker Disassembler Engine 32/64 | as shipped inside MinHook 1.3.4 | BSD-2-Clause | Compiled into `opengl32.dll` as part of MinHook |
| OpenTrack | n/a | ISC | Not bundled; UDP protocol interoperability only |
| Microsoft `opengl32.dll` | the user's own | Microsoft's own terms | Not bundled; copied from the user's own Windows install at install time |

---

## cameraunlock-core

- **Version:** commit `c480d8a8177753966a7d33b857f1db12f5e9fe39`
- **License:** MIT
- **Upstream:** https://github.com/itsloopyo/cameraunlock-core
- **Usage:** Shared head-tracking runtime (UDP receiver, pose processing, INI config, hotkey polling, logging). Git submodule at `cameraunlock-core/`, statically linked.
- **Bundled:** yes, as compiled code inside `opengl32.dll`. Its game-detection data and install scripts also ship in the release ZIP's `shared/`.

Our own code, MIT licensed, reproduced here so the notices are complete.

```
MIT License

Copyright (c) 2026 itsloopyo

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
```

---

## MinHook, and Hacker Disassembler Engine 32/64

- **Version:** 1.3.4, the tagged upstream release, with one local change recorded in `cameraunlock-core/vendor/minhook/LOCAL-CHANGES.md`: `MH_Initialize` calls `GetProcessHeap()` instead of standing up a private heap inside the game process. BSD-2-Clause permits that change and does not require it to be marked; it is named here anyway.
- **License:** BSD-2-Clause
- **Upstream:** https://github.com/TsudaKageyu/minhook
- **Usage:** `frustum_hook.cpp` detours the renderer's frustum builder so culling follows the head-tracked view. The detour is placed through cameraunlock-core's `cameraunlock_hooks` target, which this mod enables with `CAMERAUNLOCK_BUILD_HOOKS=ON` and which links the MinHook copy vendored in core.
- **Bundled:** yes, as compiled code inside `opengl32.dll`. No MinHook source or binary ships in the release ZIP.

MinHook's `LICENSE.txt` carries two separate copyrights: Tsuda Kageyu's for
MinHook, and Vyacheslav Patkov's for Hacker Disassembler Engine 32/64, whose
`hde32.c` and `hde64.c` are compiled in as MinHook's instruction length decoder.
Clause 2 of each requires the notice, the conditions and the disclaimer to
accompany a binary distribution, so both are reproduced verbatim below.

```
MinHook - The Minimalistic API Hooking Library for x64/x86
Copyright (C) 2009-2017 Tsuda Kageyu.
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:

 1. Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.
 2. Redistributions in binary form must reproduce the above copyright
    notice, this list of conditions and the following disclaimer in the
    documentation and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER
OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

================================================================================
Portions of this software are Copyright (c) 2008-2009, Vyacheslav Patkov.
================================================================================
Hacker Disassembler Engine 32 C
Copyright (c) 2008-2009, Vyacheslav Patkov.
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:

 1. Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.
 2. Redistributions in binary form must reproduce the above copyright
    notice, this list of conditions and the following disclaimer in the
    documentation and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE REGENTS OR
CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

-------------------------------------------------------------------------------
Hacker Disassembler Engine 64 C
Copyright (c) 2008-2009, Vyacheslav Patkov.
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:

 1. Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.
 2. Redistributions in binary form must reproduce the above copyright
    notice, this list of conditions and the following disclaimer in the
    documentation and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE REGENTS OR
CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
```

MinHook's own credit list, `AUTHORS.txt`, names Tsuda Kageyu (creator and
maintainer), Michael Maltsev and Andrey Unis.

---

## OpenTrack

- **Version:** n/a (wire format only; no released version is consumed)
- **License:** ISC
- **Upstream:** https://github.com/opentrack/opentrack
- **Usage:** This mod consumes the OpenTrack UDP pose datagram layout, on port 4242 by default, so that OpenTrack and compatible trackers can drive it.
- **Bundled:** no. Not bundled and not linked. No OpenTrack code, headers or binaries are copied, linked or redistributed, so its licence triggers no notice obligation here. It is credited because the wire format is its work.

---

## Microsoft `opengl32.dll`, and the OpenGL ABI

- **Version:** whichever ships with the user's own Windows install
- **License:** Microsoft's own terms, which grant no right to redistribute it
- **Bundled:** no, and it must never be. Nothing this repository produces or publishes contains any part of Microsoft's `opengl32.dll`.

The mod ships as an `opengl32.dll` planted next to `stormworks64.exe`. It
intercepts two entry points and forwards every other export to
`opengl32_real.dll`, which is a copy of the user's own
`%WINDIR%\System32\opengl32.dll` taken on their own machine at install time by
`install.cmd`, and declared in `launcher-manifest.json` as a `system-file-copy`
runtime requirement. That file is not redistributable, which is why no archive
we publish can carry it and why this mod has no mod-manager archive at all.

`src/StormworksHeadTracking/gl_forwards.h` is a generated list of export
*names*, produced by `scripts/gen_gl_forward.py` reading the export name table
of the system DLL on the build machine. It holds names and nothing else: no
Microsoft code, no addresses, no bytes. Those names are the OpenGL 1.1 and WGL
ABI, and re-exporting them is what makes a forwarding proxy work at all.

OpenGL is a registered trademark of Hewlett Packard Enterprise. Microsoft and
Windows are trademarks of Microsoft Corporation. Both are named here only to
identify the interface and the operating system this mod runs against, which is
nominative use and not a claim of any right in them.

---

## Stormworks: Build and Rescue

Stormworks: Build and Rescue and all related names, logos, characters and
marks are trademarks of their respective owners. They are used here only to
identify the game this mod applies to, which is nominative use and not a claim
of any right in them. This project is an unofficial, fan-made modification. It
is not affiliated with, endorsed by, or sponsored by Geometa, the game's
developer and publisher, or by any other rights holder. It redistributes no
game code, no game assets and no proprietary DLLs, and it requires a
legitimately purchased copy of the game.

What it does record about the game is a short set of measurements, held in
`src/StormworksHeadTracking/builds/`: one function address expressed as an RVA,
the byte offsets of the camera fields that function reads, and a PE header
fingerprint that confirms the running executable is the build those numbers were
measured on. They were established by the authors through independent analysis
of a legitimately owned copy, and they are facts recorded as numbers. No
decompiled or disassembled game code, no byte signature of game code, and no
game shader or data file is stored in this repository. The shader uniform names
in `camera_uniforms.cpp` are identifiers the mod has to match in order to
interoperate with the renderer, in the same way as the GL export names above.
