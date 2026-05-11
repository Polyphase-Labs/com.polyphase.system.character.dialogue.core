# com.polyphase.system.character.dialogue.core Verification

## Build Verification

1. Build the addon project (`.vcxproj`) in Release.
2. Confirm DLL output exists under `Build/Release/`.
3. Treat `unsuccessfulbuild` tlog deletion failures as environment cleanup noise when compile/link is green.

## Editor Verification

1. Open project in Polyphase Editor.
2. Run `Tools -> Addons -> Reload Native Addons`.
3. Confirm load logs for this addon and no runtime errors.

## Script Verification

- Run `Scripts/Test/rpg_systems_one_shot.lua` for broad systems coverage.
- Run `Scripts/Test/wave5_templates_testbed.lua` for template-specific checks (`Orge`/`PointClick`).
- Add scene-local smoke scripts when this addon introduces new nodes/widgets.

## Functional Checklist

- Core API calls succeed from Lua console/scripts.
- Bridge variables update as expected for gameplay/UI.
- Save/load round-trip preserves addon-owned state where applicable.
- Hot reload (`Reload Native Addons`) does not leak state or crash.
