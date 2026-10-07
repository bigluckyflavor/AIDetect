# AIDetect — DAAT AI Audio Inspector

Forensic audio analysis instrument (JUCE, C++) that scores the likelihood a piece of audio is AI-generated.

11 hand-built heuristic features across 6 groups — spectral, dynamics, stereo, noise, repetition, temporal — feed a weighted detection engine with per-feature profiles. Timeline / evidence / report UI, with JSON report and CSV export (including user labels) for downstream training and calibration.

Honest framing, stated plainly: nothing here claims to prove AI origin. It surfaces measurable evidence and lets you decide.

Related work: a commercial line of precision audio analysis instruments at [daataudio.com](https://www.daataudio.com).
