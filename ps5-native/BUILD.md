# ClanBomber native title build

Use Clang/LLD 18.1.8, Ninja, Python 3, CMake, [PS5 Payload SDK v0.45](https://github.com/ps5-payload-dev/sdk/tree/v0.45) and the [PacBrew v0.41.1](https://github.com/ps5-payload-dev/pacbrew-repo/tree/v0.41.1) PS5 static library prefix. The PacBrew recipe tree pins library sources and patches. Extract the development archive into an isolated SDK, preserving the target/user/homebrew layout. On Windows materialize libm.a and libpng.a as copies of libopenlibm.a and libpng16.a if extraction omitted the archive symlinks.

Configure the repository root with Ninja, CMAKE_TOOLCHAIN_FILE pointing at the SDK's toolchain/prospero.cmake and CMAKE_PREFIX_PATH pointing at its target/user/homebrew. Put LLVM on PATH and build the CMake project. The first ELF uses the SDK's payload startup and is intermediate only.

Run `python ps5-native/link-title.py --sdk SDK_ROOT --llvm LLVM_BIN --build CMAKE_BUILD --output TITLE_OUTPUT --converter PS5_NATIVE_TOOL`. The script uses the CMake link libraries and game objects with the included native-title CRT, wraps the unsupported native keyboard calls, relinks, converts, signs and inspects the SELF.

The CRT, linker scripts, converter source and loader companion source are from [PSXS5 at 5c17ea31](https://github.com/SynoPiia/PSXS5/tree/5c17ea31aec5791c12451e915386fa98d670adb4). See LOADER-COMPANION.json for the companion digest and generator source. Public source snapshots for PSXS5, SDK and PacBrew build recipes are available as release assets.

Stage the signed eboot.bin and native metadata with ClanBomber2/{fonts,maps,pics,wavs}, characters, and sce_module/libc.prx. The source art for the launcher icon is upstream/src/pics/cb_logo_small.png. Preserve the included GPL, DejaVu and asset attribution notices.
