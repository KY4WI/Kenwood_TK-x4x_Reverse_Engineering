# TK-x4x channel bench tests

Synthetic codeplugs for verifying TX Inhibit, receive-only channels, Talk Around, and the TK-940/941 stock versus Enhanced TX Patched transmit behavior.

## Test files

Each raw channel file is exactly **6,144 bytes**. Files are individually available here, without an archive:

- [Universal three-System image](TK-x4x_UNIVERSAL_TEST_CH.bin): **System 1 = TK-840**, **System 2 = TK-940**, **System 3 = TK-941**.
- [TK-840 raw channel file](TK-840_TEST_CH.bin)
- [TK-940 raw channel file](TK-940_TEST_CH.bin)
- [TK-941 raw channel file](TK-941_TEST_CH.bin)
- [TK-840 CHIRP test image](TK-840_CHIRP_UPLOAD_TEST.img)
- [TK-940 CHIRP test image](TK-940_CHIRP_UPLOAD_TEST.img)
- [TK-941 CHIRP test image](TK-941_CHIRP_UPLOAD_TEST.img)

Read the **[channel-by-channel test guide](TEST_GUIDE.md)** for expected RX/TX frequencies, model-specific and patched-firmware behavior, and testing instructions. The [generator script](build_tkx4x_test_codeplugs.py) makes every file here, including the guide; the content check regenerates them on each push and refuses any difference.

The **Toolkit** reads raw channel files (`_CH.bin`, 6,144 bytes) and writes them without changing channel data. It does **not** accept the larger metadata-bearing CHIRP `.img` files. **CHIRP** accepts those `.img` files and can modify the data during upload, notably forcing TX Inhibit when the stored TX frequency is absent. The CHIRP test images deliberately contain two absent-TX records with TX Inhibit initially cleared, so this upload-time behavior can be verified.

**Safety:** These are test images, not operational channel plans. Back up your radio; use a properly rated dummy load and suitable RF test equipment. Do not radiate test transmissions. The universal codeplug deliberately mixes model-specific frequency words: only test transmissions intended for the connected model. Physical RF results have not yet been validated on the radios.
