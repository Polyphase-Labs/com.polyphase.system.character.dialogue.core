# Platform support

The core runtime is exception-free and RTTI-free, designed to compile on:

- Windows (editor + game)
- Linux (editor + game)
- 3DS (-fno-rtti, -fno-exceptions)
- Wii / GameCube (devkitPPC)

Editor-only code (the inspector, preview window, validator) is guarded by
`#if EDITOR` and never compiles into a console build.

The Ink addon (`com.polyphase.system.character.dialogue.ink`) is an editor-time
importer only. It produces `.dialogue` files which the core addon
consumes. No Ink runtime ships to consoles.

## Cooking

`DialogueAsset` cooks identically on all platforms — same byte stream layout
via `Stream`. Endianness is handled by the engine's `Stream` primitives.

If you later need per-platform variants (e.g. precomputed string tables for
3DS RAM constraints), add them after the trailer magic
(`kDialogueTrailerMagic = 0xD1A107C0`) in `DialogueAsset::SaveStream`. Older
readers presence-check each appended field, so adding new ones doesn't break
existing assets.
