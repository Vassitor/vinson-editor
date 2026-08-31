# Performance

[简体中文](PERFORMANCE.zh-CN.md) | English

## Large-file design

Application code does not access raw Scintilla messages outside `EditorWidget`,
and file workers never touch the GUI control directly.

Files are classified at centralized 64 MiB and 512 MiB thresholds. Before
loading begins, Large and Very Large files receive a newly created Scintilla
document with `SC_DOCUMENTOPTION_TEXT_LARGE | SC_DOCUMENTOPTION_STYLES_NONE`.
The creator reference is released after attachment, the null lexer is selected,
and word wrap defaults off.

The source file size plus a 1 MiB edit reserve is passed to
`SCI_CREATEDOCUMENT`. The known capacity avoids repeated full-buffer growth
during loading; the reserve prevents the first small edit from triggering
Scintilla's much larger geometric expansion.

The loader reads 256 KiB source chunks on a worker thread. At most four decoded
chunks may be outstanding, bounding the queued payload to approximately 1 MiB.
Valid ASCII/UTF-8 data is incrementally validated and passed through without a
UTF-16 `QString` round trip. UTF-16 input still uses stateful, per-chunk
conversion. The GUI acknowledges each chunk only after appending it to
Scintilla, and loading remains cancelable.

Line-ending detection shares this streaming path and recognizes CRLF sequences
split across chunks without an additional full-file scan.

Large-mode search uses Scintilla target ranges in overlapping 8 MiB slices.
The overlap preserves matches that cross a slice boundary. Between misses, the
application processes paint, timer, and other non-user-input events; it does
not copy or index the complete document. Replace All remains synchronous and
Very Large mode requires confirmation.

Normal saving takes one complete UTF-8 snapshot. Large modes instead let the
worker request one 256 KiB Scintilla range at a time, perform streaming
UTF-8/UTF-16 conversion, and commit through `QSaveFile`. The editor is disabled
during this operation so offsets remain stable. Cancellation or failure
discards the temporary output rather than truncating the source.

## Reproducing the benchmark

Use a Release build. The generator creates exact-size ASCII/LF files with a
unique search marker near the end:

```bash
cmake --preset release
cmake --build --preset release --target vinson-large-file-benchmark
QT_QPA_PLATFORM=offscreen ./build/release/vinson-large-file-benchmark \
  --generate /tmp/vinson-editor-phase8
```

Run each size in a fresh process so allocator state from a previous document
does not affect RSS:

```bash
QT_QPA_PLATFORM=offscreen ./build/release/vinson-large-file-benchmark \
  --file /tmp/vinson-editor-phase8/benchmark-10MiB.txt
QT_QPA_PLATFORM=offscreen ./build/release/vinson-large-file-benchmark \
  --file /tmp/vinson-editor-phase8/benchmark-100MiB.txt
QT_QPA_PLATFORM=offscreen ./build/release/vinson-large-file-benchmark \
  --file /tmp/vinson-editor-phase8/benchmark-500MiB.txt
QT_QPA_PLATFORM=offscreen ./build/release/vinson-large-file-benchmark \
  --file /tmp/vinson-editor-phase8/benchmark-1024MiB.txt
```

The tool emits one JSON object. It measures asynchronous open, peak Linux RSS,
event-loop timer gaps, a search to the near-end marker, 100 evenly spaced line
jumps, a ten-byte middle insertion and undo, and atomic save. The saved file's
size is verified before its temporary copy is removed.

## Results — 2026-08-28

Environment: Release (`-O3`), x86-64 Linux 7.0.0-29, Qt 6.10.2, GCC 15.2.0,
Intel Core i7-10870H (8 cores/16 threads), 30 GiB RAM. Fixtures and save targets
were on a 16 GiB `/tmp` tmpfs. These save numbers therefore describe the editor
pipeline and memory-backed I/O, not physical SSD or Windows filesystem speed.

| File | Mode | Open | Load max event gap | Near-end search | Search max event gap | 100 line jumps total / max | Middle edit | Save | Save max event gap |
| ---: | --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| 10 MiB | Normal | 0.197 s | 65 ms | 0.086 s | 86 ms | 1.332 s / 16 ms | 2 ms | 0.039 s | 28 ms |
| 100 MiB | Large | 1.348 s | 158 ms | 0.615 s | 64 ms | 1.103 s / 12 ms | 11 ms | 0.273 s | 12 ms |
| 500 MiB | Large | 6.942 s | 747 ms | 2.994 s | 61 ms | 1.086 s / 12 ms | 59 ms | 1.416 s | 14 ms |
| 1 GiB | Very Large | 14.087 s | 1,517 ms | 6.009 s | 69 ms | 1.086 s / 11 ms | 126 ms | 2.703 s | 15 ms |

The 10 ms responsiveness timer fired 6, 65, 331, and 680 times during the four
loads. Thus the event loop continued to make progress rather than entering one
operation-long stall. The largest observed pause was 1.517 seconds for 1 GiB,
primarily while Scintilla allocated its initial contiguous gap buffer. Search
is still a multi-second operation at 1 GiB, but slicing reduced its longest
observed event gap to 69 ms while retaining direct Scintilla search.

The offscreen line-jump measurement is a repeatable navigation/redraw proxy,
not a monitor frame-rate claim. Visual scroll smoothness still depends on the
window system, GPU, font, transparency, and display refresh rate.

### Resident memory

| File | Baseline RSS | Peak RSS while loading | Peak growth | RSS after edit/undo | Peak growth while saving |
| ---: | ---: | ---: | ---: | ---: | ---: |
| 10 MiB | 30.9 MiB | 59.7 MiB | 28.8 MiB | 59.7 MiB | 10.0 MiB |
| 100 MiB | 31.1 MiB | 149.9 MiB | 118.8 MiB | 149.5 MiB | < 0.1 MiB |
| 500 MiB | 31.1 MiB | 601.8 MiB | 570.8 MiB | 601.5 MiB | < 0.1 MiB |
| 1 GiB | 31.1 MiB | 1,190.5 MiB | 1,159.4 MiB | 1,190.1 MiB | < 0.1 MiB |

The 1 GiB load grew RSS by about 1.13 times the file size, not 5–10 times. The
remaining overhead is principally Scintilla text capacity and line-position
metadata; style-character storage is absent. The 1 MiB reserve eliminated the
previous first-edit expansion (about 128 MiB at 500 MiB and 256 MiB at 1 GiB)
without a measurable post-edit RSS increase. Large streaming saves also showed
no meaningful RSS increase. The Normal path intentionally showed one additional
10 MiB snapshot while saving the 10 MiB fixture.

## Known limitations

- Creating Scintilla's initial 1 GiB gap buffer still causes a measured pause of
  about 1.5 seconds on this machine. Moving document construction fully off the
  GUI thread would require adopting Scintilla's background-loader lifecycle and
  is deferred until more platform measurements justify that complexity.
- Search slicing keeps paint and timers moving but intentionally defers user
  input until the current search finishes; search cancellation is not yet
  exposed.
- Middle insertion remains proportional to the amount of text moved through the
  Scintilla gap buffer, although the measured 1 GiB insertion was 126 ms after
  reserving edit capacity.
- UTF-16 loading performs bounded UTF-16-to-UTF-8 conversion and may have a
  different memory/time profile from the ASCII fixtures measured here.
- Transparent backgrounds use direct drawing and may cost more than opaque
  backgrounds. The offscreen benchmark used the default opaque appearance.
- No claim is made for unlimited file size or zero-latency 1 GiB operation.
  Scintilla remains a full-document editor; paged/memory-mapped alternatives are
  not justified by the measured memory ratio for the current MVP.
