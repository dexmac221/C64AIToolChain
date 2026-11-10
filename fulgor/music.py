#!/usr/bin/env python3
"""
music.py - the tune and the sound effects of FULGOR, as data for sound.s.

Three voices, a row every six frames (sixteenths at 125 BPM):

  voice 0  bass, with the drums in its gaps (kick and snare are
           wavetable instruments on the same voice, the old trick)
  voice 1  chords as 50 Hz arpeggios, in a 3-3-2 rhythm; the sound
           effects borrow this voice
  voice 2  the lead, sawtooth with a late vibrato

The piece is in A minor: four bars of introduction, then two eight-bar
strains (Am Am F G | Am Am F E, then Dm Dm G C | F Dm E E) that repeat.

Pattern bytes: $00-$5F a note (C-0 = 0), $60 a rest, $80+n rows per
note = n+1, $A0+n instrument n, $FF end. Wavetable steps are (control,
note): note with bit 7 set is absolute, otherwise added to the played
note; control 0 jumps to the step named by the note byte.
"""

NOTE = {"C": 0, "C#": 1, "D": 2, "D#": 3, "E": 4, "F": 5, "F#": 6, "G": 7,
        "G#": 8, "A": 9, "A#": 10, "B": 11}


def n(name):
    """'A4' -> note number (C0 = 0)."""
    return NOTE[name[:-1]] * 1 + 12 * int(name[-1])


# ---------------------------------------------------------- instruments
I_BASS, I_KICK, I_SNARE, I_MIN, I_MAJ, I_LEAD = range(6)
# (attack/decay, sustain/release, wavetable steps)
ABS = 0x80
INSTR = [
    (0x09, 0x88, [(0x41, 0), ("loop", 0)]),                       # bass
    (0x00, 0xF8, [(0x81, ABS | 58), (0x41, ABS | 26), (0x41, ABS | 19),
                  (0x41, ABS | 14), (0x40, ABS | 10), ("loop", 4)]),   # kick
    (0x00, 0xA9, [(0x81, ABS | 72), (0x41, ABS | 43), (0x81, ABS | 66),
                  (0x81, ABS | 62), (0x80, ABS | 58), ("loop", 4)]),   # snare
    (0x08, 0x66, [(0x41, 0), (0x41, 3), (0x41, 7), (0x41, 12),
                  ("loop", 0)]),                                  # minor
    (0x08, 0x66, [(0x41, 0), (0x41, 4), (0x41, 7), (0x41, 12),
                  ("loop", 0)]),                                  # major
    (0x28, 0xC9, [(0x21, 0), ("loop", 0)]),                       # lead
]

# ------------------------------------------------------------- patterns
def bass_bar(root):
    """K . b' . S . b b' K . b' . S . b' b  (b' an octave up)."""
    lo, hi = n(root), n(root) + 12
    return [(I_KICK, lo, 2), (I_BASS, hi, 2), (I_SNARE, lo, 2),
            (I_BASS, lo, 1), (I_BASS, hi, 1), (I_KICK, lo, 2),
            (I_BASS, hi, 2), (I_SNARE, lo, 2), (I_BASS, hi, 1),
            (I_BASS, lo, 1)]


def arp_bar(root, minor):
    ins = I_MIN if minor else I_MAJ
    r = n(root)
    return [(ins, r, 3), (ins, r, 3), (ins, r, 2)] * 2


def lead_bar(text):
    """'A4:4 C5:2 -:2' -> events; '-' is a rest."""
    ev = []
    for tok in text.split():
        name, rows = tok.split(":")
        ev.append((I_LEAD, None if name == "-" else n(name), int(rows)))
    assert sum(e[2] for e in ev) == 16, text
    return ev


CHORDS = {  # name: (bass root, arpeggio root, minor?)
    "Am": ("A1", "A3", True), "F": ("F1", "F3", False),
    "G": ("G1", "G3", False), "E": ("E1", "E3", False),
    "Dm": ("D2", "D4", True), "C": ("C2", "C4", False),
}
INTRO = ["Am", "Am", "F", "E"]
STRAIN_A = ["Am", "Am", "F", "G", "Am", "Am", "F", "E"]
STRAIN_B = ["Dm", "Dm", "G", "C", "F", "Dm", "E", "E"]
LEAD_A = [
    "A4:4 C5:2 E5:2 D5:2 C5:2 A4:4",
    "E5:6 D5:2 C5:2 D5:2 E5:4",
    "F5:4 A5:4 G5:2 F5:2 C5:4",
    "D5:4 G5:4 F5:2 E5:2 D5:4",
    "E5:4 A5:4 G5:2 E5:2 C5:4",
    "D5:2 E5:2 G5:4 E5:4 D5:2 C5:2",
    "A4:4 C5:4 F5:4 E5:2 C5:2",
    "B4:4 E5:4 G#5:4 B5:4",
]
LEAD_B = [
    "D5:4 F5:4 A5:6 F5:2",
    "G5:2 F5:2 E5:2 D5:2 F5:4 D5:4",
    "B4:4 D5:4 G5:6 D5:2",
    "E5:4 G5:4 C6:4 G5:2 E5:2",
    "A5:4 F5:4 C5:4 F5:2 A5:2",
    "A5:2 G5:2 F5:2 E5:2 D5:4 F5:4",
    "E5:6 G#5:2 B5:4 G#5:4",
    "E5:4 B4:4 E5:2 G#5:2 B5:2 E6:2",
]

# ------------------------------------------------------- sound effects
# (control, absolute note) one a frame, on voice 1; index + 1 is the id
# the game writes to sfx_req (see defs.inc).
SFX = [
    [(0x41, 84), (0x41, 78), (0x41, 72), (0x41, 67), (0x40, 62)],     # shot
    [(0x11, 84), (0x11, 84), (0x11, 91), (0x11, 91), (0x11, 96),
     (0x10, 96)],                                                     # gem
    [(0x21, 60), (0x21, 64), (0x21, 67), (0x21, 72), (0x21, 76),
     (0x21, 79), (0x21, 84), (0x21, 88), (0x21, 91), (0x20, 91)],     # power
    [(0x41, 31), (0x41, 29), (0x41, 27), (0x41, 25), (0x41, 23),
     (0x41, 21), (0x40, 19)],                                         # hurt
    [(0x81, 56), (0x81, 52), (0x81, 48), (0x81, 44), (0x81, 40),
     (0x81, 37), (0x81, 34), (0x81, 31), (0x81, 28), (0x81, 25),
     (0x80, 22), (0x80, 20)],                                         # boom
    [(0x81, 74), (0x41, 52), (0x40, 46)],                             # hit
    [(0x21, 72), (0x21, 66), (0x21, 60), (0x21, 54), (0x20, 48)],     # enemy shot
]


# -------------------------------------------------------------- compile
def compile_pattern(events):
    out, ins, dur = [], None, None
    for i, note, rows in events:
        if i != ins and note is not None:
            out.append(0xA0 + i)
            ins = i
        if rows != dur:
            out.append(0x80 + rows - 1)
            dur = rows
        out.append(0x60 if note is None else note)
    return out + [0xFF]


def build():
    patterns, index = [], {}

    def pat(events):
        b = tuple(compile_pattern(events))
        if b not in index:
            index[b] = len(patterns)
            patterns.append(list(b))
        return index[b]

    silent = pat([(I_LEAD, None, 16)])
    orders = [[], [], []]
    loops = [0, 0, 0]
    for strain, lead in ((INTRO, None), (STRAIN_A, LEAD_A), (STRAIN_B, LEAD_B)):
        if lead is LEAD_A:
            loops = [len(o) for o in orders]
        for k, ch in enumerate(strain):
            b, a, minor = CHORDS[ch]
            orders[0].append(pat(bass_bar(b)))
            orders[1].append(pat(arp_bar(a, minor)))
            orders[2].append(silent if lead is None else pat(lead_bar(lead[k])))
    flat, start, loop = [], [], []
    for v in range(3):
        start.append(len(flat))
        loop.append(len(flat) + loops[v])
        flat += orders[v] + [0xFF]

    wt_ctl, wt_note, ins_wt = [], [], []
    for ad, sr, steps in INSTR:
        base = len(wt_ctl)
        ins_wt.append(base)
        for c, v in steps:
            if c == "loop":
                wt_ctl.append(0)
                wt_note.append(base + v)
            else:
                wt_ctl.append(c)
                wt_note.append(v)

    sfx_ctl, sfx_note, sfx_start = [0], [0], []
    for s in SFX:
        sfx_start.append(len(sfx_ctl))
        for c, v in s:
            sfx_ctl.append(c)
            sfx_note.append(v)
        sfx_ctl.append(0)
        sfx_note.append(0)

    # PAL: register = Hz * 2^24 / 985248
    freq = [int(round(440.0 * 2 ** ((k - 57) / 12.0) * 16777216 / 985248))
            for k in range(96)]
    freq = [min(f, 0xFFFF) for f in freq]
    return dict(patterns=patterns, orders=flat, ord_start=start,
                ord_loop=loop, wt_ctl=wt_ctl, wt_note=wt_note,
                ins_wt=ins_wt, ins_ad=[i[0] for i in INSTR],
                ins_sr=[i[1] for i in INSTR], sfx_ctl=sfx_ctl,
                sfx_note=sfx_note, sfx_start=sfx_start, freq=freq)


def emit(path):
    d = build()

    def bl(data, per=16):
        return "".join("    .byte " + ",".join("$%02X" % b for b in data[o:o + per])
                       + "\n" for o in range(0, len(data), per))
    with open(path, "w") as f:
        f.write("; generated by music.py - do not edit\n")
        f.write(".export ftab_lo, ftab_hi, ins_wt, ins_ad, ins_sr, wt_ctl, wt_note\n")
        f.write(".export pat_lo, pat_hi, orders, ord_start, ord_loop\n")
        f.write(".export sfx_start, sfx_ctl, sfx_note\n")
        f.write('.segment "RODATA"\n')
        f.write("ftab_lo:\n" + bl([v & 255 for v in d["freq"]]))
        f.write("ftab_hi:\n" + bl([v >> 8 for v in d["freq"]]))
        for k in ("ins_wt", "ins_ad", "ins_sr", "wt_ctl", "wt_note", "orders",
                  "ord_start", "ord_loop", "sfx_start", "sfx_ctl", "sfx_note"):
            f.write(k + ":\n" + bl(d[k]))
        f.write("pat_lo:\n")
        for i in range(len(d["patterns"])):
            f.write("    .byte <pat%d\n" % i)
        f.write("pat_hi:\n")
        for i in range(len(d["patterns"])):
            f.write("    .byte >pat%d\n" % i)
        for i, p in enumerate(d["patterns"]):
            f.write("pat%d:\n" % i + bl(p))
    size = sum(len(p) for p in d["patterns"]) + len(d["orders"])
    print("music: %d patterns, %d bytes of song" % (len(d["patterns"]), size))


if __name__ == "__main__":
    import os
    emit(os.path.join(os.path.dirname(os.path.abspath(__file__)), "music.s"))
