# Earlier TK-x4x utilities

Copyright (c) 2026 Dave, KY4WI

These programs are earlier standalone implementations of features now provided by [TKx4xToolkit](https://github.com/KY4WI/Kenwood_TK-x4x_Toolkit). They are retained as source and protocol references, **not as supported radio-programming tools**.

- [tk840download](tk840download-README.md): read and validate radio memory
- [tk840upload](tk840upload_README.md): experimental firmware and codeplug writer
- [tk941patch](tk941patch_README.md): early data-driven patcher, with [patch definitions](tk941patches.tkpatch)

Build them from this directory with `make` on Linux/POSIX.

**Do not use the bundled TK-941 TX-field patch for live flashing.** Its original encoding was superseded by the corrected implementation in TKx4xToolkit. Firmware loading through `tk840upload` was not fully hardware-verified at the time this version was written. The downloader and offline patcher can overwrite existing files. Preserve backups and use Toolkit for normal service.

Current technical references: [programming protocol](../../docs/programming/Programming_Protocol.md) and [hardware communication](../../docs/hardware/README.md). No proprietary firmware, mask-ROM dumps, or codeplugs are provided with these utilities.
