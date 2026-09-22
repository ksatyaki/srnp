# Fonts shipped with PairView

| File | Font | Licence |
|:-----|:-----|:--------|
| `IBMPlexSans-Regular.ttf` | IBM Plex Sans, the interface text | SIL OFL 1.1 — `LICENSE-IBMPlex.txt` |
| `IBMPlexSans-SemiBold.ttf` | IBM Plex Sans SemiBold, headings and buttons | SIL OFL 1.1 — `LICENSE-IBMPlex.txt` |
| `fa-solid-900.ttf` | Font Awesome 6 Free Solid, the icons | Fonts SIL OFL 1.1, icons CC BY 4.0 — `LICENSE-FontAwesome.txt` |

All three are redistributable alongside GPLv3 code. Font Awesome's icons are CC
BY 4.0, which is why the licence file is shipped next to them.

They are loaded from disk at startup, from the installed data directory first
and the source tree second. If neither is there PairView falls back to ImGui's
built-in font and says so on standard error.
