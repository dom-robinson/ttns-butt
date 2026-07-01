# TTNS Deck — licensing

**Not legal advice.** Summary for distributors and developers.

## This program (TTNS Deck / ttns-butt)

| Item | Detail |
|------|--------|
| **Upstream** | [butt](https://github.com/romansavrulin/butt) 0.1.16 by Daniel Nöthen |
| **License** | **GNU GPL version 2** — see [`COPYING`](../COPYING) in the repository root |
| **TTNS fork** | Modified version; TTNS-specific source is also under GPL-2.0 when distributed as part of this program |
| **Source** | https://github.com/dom-robinson/ttns-butt (match the release tag to your binary) |

### Distributing binaries to DJs

GPL-2.0 applies even for internal team distribution. You must:

- Include the GPL license and a modified-work notice
- Make corresponding **source** available (public GitHub repo + tag is sufficient if recipients know the URL)

You do **not** have to send patches or binaries back to the original butt authors.

### TTNS branding

`assets/ttns-logo.png`, `.icns`, and `.ico` are **copyright The Thursday Night Show** — not GPL. See [`assets/README.md`](../assets/README.md). Official TTNS Deck builds include them for presenters only.

---

## Third-party libraries

TTNS Deck links against codecs, FLTK, PortAudio, etc. at build time. On **Windows**, many are redistributed as DLLs.

Full attribution and the **Fraunhofer FDK-AAC** license text (required for binary packages) are in:

- [`docs/THIRD_PARTY_NOTICES.md`](THIRD_PARTY_NOTICES.md)
- [`docs/licenses/fdk-aac-LICENSE.txt`](licenses/fdk-aac-LICENSE.txt)

**Binary packages** (macOS `.zip`, Linux `.tar.gz`, Windows `.zip`) also ship a `legal/` folder (or `Resources/legal/` inside the macOS app) with `COPYING`, `DISTRIBUTION_LICENSE.txt`, and the files above — staged by [`scripts/copy-distribution-licenses.sh`](../scripts/copy-distribution-licenses.sh).

### Notable constraints

| Component | Issue |
|-----------|--------|
| **libfdk-aac** | Custom Fraunhofer license; **no AAC patent grant** — see full text in `docs/licenses/` |
| **libsamplerate** | GPL-2.0+ — compatible with this app |
| **FLTK, LAME** | LGPL — satisfied by dynamic linking / bundled DLLs with notices |

---

## In-app notice

**Help → About** (or equivalent) credits Daniel Nöthen and GPL-2.0.
