"""
Import the 5 Mixkit vehicle engine MP3s sitting in
Content/Audio/SFX/Vehicles/ as USoundWave assets.

How to run:
  In UE editor -> Tools -> Execute Python Script -> pick this file.

Re-runnable: skips files that already have a matching .uasset.
"""

import os
import unreal


SFX_DIR_FS = os.path.normpath(
    unreal.SystemLibrary.get_project_content_directory() + "Audio/SFX/Vehicles/"
)
SFX_DIR_PKG = "/Game/Audio/SFX/Vehicles"


def find_new_mp3s():
    """Return absolute .mp3 paths that do not yet have matching .uasset files."""
    out = []
    if not os.path.isdir(SFX_DIR_FS):
        unreal.log_error(f"Vehicle SFX dir not found: {SFX_DIR_FS}")
        return out
    for fname in sorted(os.listdir(SFX_DIR_FS)):
        if not fname.lower().endswith(".mp3"):
            continue
        stem = os.path.splitext(fname)[0]
        if os.path.exists(os.path.join(SFX_DIR_FS, stem + ".uasset")):
            continue
        out.append(os.path.join(SFX_DIR_FS, fname))
    return out


def build_import_task(mp3_path):
    task = unreal.AssetImportTask()
    task.filename = mp3_path
    task.destination_path = SFX_DIR_PKG
    task.replace_existing = True
    task.automated = True
    task.save = True
    return task


def main():
    mp3s = find_new_mp3s()
    if not mp3s:
        unreal.log("No new vehicle SFX to import - already up to date.")
        return

    tasks = [build_import_task(p) for p in mp3s]
    unreal.log(f"Importing {len(tasks)} vehicle SFX into {SFX_DIR_PKG}")
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks)

    for task in tasks:
        for asset_path in task.imported_object_paths:
            unreal.log(f"  imported -> {asset_path}")

    unreal.log("Done. Save All (Ctrl+Shift+S) to persist.")


main()
