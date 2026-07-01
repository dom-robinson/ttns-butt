# Third-party notices — TTNS Deck

TTNS Deck links against the libraries below at build time. On **Windows**, many
are also redistributed as DLLs next to `ttns-deck.exe`. On **macOS** and
**Linux**, the system or package manager typically provides them at runtime.

This file satisfies attribution requirements for those components. The
**complete** Fraunhofer FDK-AAC license text is in
[`licenses/fdk-aac-LICENSE.txt`](licenses/fdk-aac-LICENSE.txt) (required for
binary redistribution).

---

## FLTK (Fast Light Tool Kit)

- **Use:** GUI toolkit  
- **License:** GNU LGPL-2.0  
- **Project:** https://www.fltk.org/

Used via dynamic linking. LGPL requires that users be able to replace the
library; shipping the DLL/.so/.dylib (or documenting how to obtain a
compatible build) satisfies this for our packages.

---

## PortAudio

- **Use:** audio input/output  
- **License:** MIT-style (copyright Ross Bencina and Phil Burk)  
- **Project:** https://www.portaudio.com/

Permission is granted to use, copy, modify, merge, publish, distribute,
sublicense, and/or sell copies, provided the copyright notice and
permission notice are included in all copies or substantial portions.

---

## LAME (libmp3lame)

- **Use:** MP3 encoding  
- **License:** GNU LGPL-2.0 or later  
- **Project:** https://lame.sourceforge.io/

Dynamically linked.

---

## Xiph.org — libvorbis, libogg

- **Use:** Ogg Vorbis encoding  
- **License:** BSD-style (see Xiph license)  
- **Project:** https://xiph.org/vorbis/

Copyright (c) Xiph.org Foundation. Redistribution permitted with copyright
and disclaimer retained.

---

## FLAC (libFLAC)

- **Use:** FLAC encoding  
- **License:** BSD / GPL / LGPL (triple-licensed; libFLAC is commonly used under BSD)  
- **Project:** https://xiph.org/flac/

---

## Opus (libopus)

- **Use:** Opus encoding  
- **License:** BSD-style (Xiph)  
- **Project:** https://opus-codec.org/

---

## libsamplerate (Secret Rabbit Code)

- **Use:** sample-rate conversion  
- **License:** GNU GPL-2.0 or later  
- **Project:** http://www.mega-nerd.com/SRC/

GPL-2.0+ is compatible with TTNS Deck (also GPL-2.0).

---

## Fraunhofer FDK-AAC (libfdk-aac)

- **Use:** AAC encoding  
- **License:** Fraunhofer FDK AAC Codec license (custom; **not** GPL)  
- **Full text:** [`licenses/fdk-aac-LICENSE.txt`](licenses/fdk-aac-LICENSE.txt)

**Important:** This license does **not** grant AAC patent rights. Encoding AAC
may require separate patent licensing depending on your jurisdiction and use.
Debian classifies `libfdk-aac` as non-free for this reason.

---

## zlib, libpng, libjpeg-turbo

- **Use:** image support for FLTK (`fltk_images`)  
- **Licenses:** zlib License; libpng License; IJG / BSD-style (libjpeg-turbo)  
- **Projects:** https://zlib.net/ , http://www.libpng.org/ , https://libjpeg-turbo.org/

Bundled on Windows as DLL dependencies of FLTK.

---

## MinGW runtime (Windows only)

- **Use:** `libgcc_s_seh-1.dll`, `libstdc++-6.dll`, `libwinpthread-1.dll`  
- **License:** GNU GPL-3.0 with GCC Runtime Library Exception  

Standard compiler runtime libraries shipped with MinGW-w64 builds.

---

## Apple frameworks (macOS only)

AVFoundation and CoreMedia are used for M4A/MP3 cart decoding on macOS. These
are system frameworks; no separate redistribution.

---

## Original butt artwork

Small UI bitmaps (record/connect icons) and legacy pixmaps from upstream butt
remain under the same GPL-2.0 as butt. The original application icon was
contributed under Creative Commons; see `THANKS` in the source tree.

TTNS branding (`ttns-logo.png`, `.icns`, `.ico`) is copyright The Thursday
Night Show — see `assets/README.md` in the source repository.
