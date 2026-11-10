# Sky band writes against the visible window

Question (a reviewer, through the human): do the `$D021` band writes land
inside the right border, or can they colour the last pixels of a row's
last line? In the 38-column mode FULGOR uses, the window closes around
cycle 55 in Bauer's numbering.

Measured on the shipping build, VICE x64sc 3.7.1, 2026-11-10:

| | result |
|---|---|
| breakpoint on `nb_w` (`sta $d021`), 8 camera phases | the store starts on cycles 55-60 of the row's last line (VICE numbering, from 0); the write is its fourth cycle, 58-63 |
| first, messier sampling | starts also up to cycle 3 of the next line, when sprite fetches delay the NMI: still in the left border |
| 32 screenshots, all 8 phases, plateau and canyon ([band_edges/](band_edges/)) | 40 band edges with open sky at the right end: the new colour never shows in the last visible pixels; 160 with open sky at the left end: the old colour never shows in the first visible pixels of the next line |

Checker: [band_edges.py](band_edges.py). The earlier diary figure
"cycles 52-62" was the store instruction's first cycle, probably from an
earlier calibration; corrected in DEVLOG.md.
