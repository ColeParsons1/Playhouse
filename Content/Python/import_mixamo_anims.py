"""
Bulk-import all Mixamo .fbx animations from
Content/Characters/Mixamo/Animations/ onto Ch15_nonPBR_Skeleton.

How to run:
  In UE editor → Tools → Execute Python Script → pick this file.
  (Or paste the contents into the Output Log's Python REPL.)

Re-runnable: skips files that already have a matching .uasset.
"""

import os
import unreal


SKELETON_ASSET = "/Game/Characters/Mixamo/Ch15_nonPBR_Skeleton.Ch15_nonPBR_Skeleton"
ANIM_DIR_FS    = os.path.normpath(unreal.SystemLibrary.get_project_content_directory()
                                  + "Characters/Mixamo/Animations/")
ANIM_DIR_PKG   = "/Game/Characters/Mixamo/Animations"


def find_new_fbxs():
    """Return abs paths of .fbx files in the anim dir that have no matching .uasset."""
    out = []
    for fname in os.listdir(ANIM_DIR_FS):
        if not fname.lower().endswith(".fbx"):
            continue
        stem = os.path.splitext(fname)[0]
        if os.path.exists(os.path.join(ANIM_DIR_FS, stem + ".uasset")):
            continue
        out.append(os.path.join(ANIM_DIR_FS, fname))
    return out


def build_import_task(fbx_path, skeleton):
    """Build an AssetImportTask for an FBX as Animation (uses skeleton, no skin)."""
    fbx_options = unreal.FbxImportUI()
    fbx_options.import_animations  = True
    fbx_options.import_mesh        = False
    fbx_options.import_as_skeletal = True
    fbx_options.import_materials   = False
    fbx_options.import_textures    = False
    fbx_options.skeleton           = skeleton
    fbx_options.mesh_type_to_import = unreal.FBXImportType.FBXIT_ANIMATION
    fbx_options.anim_sequence_import_data.set_editor_property(
        "import_uniform_scale", 1.0)
    fbx_options.anim_sequence_import_data.set_editor_property(
        "use_default_sample_rate", True)

    task = unreal.AssetImportTask()
    task.filename       = fbx_path
    task.destination_path = ANIM_DIR_PKG
    task.replace_existing = True
    task.automated      = True
    task.save           = True
    task.options        = fbx_options
    return task


def main():
    skeleton = unreal.EditorAssetLibrary.load_asset(SKELETON_ASSET)
    if not skeleton:
        unreal.log_error(f"Skeleton not found at {SKELETON_ASSET}")
        return

    fbxs = find_new_fbxs()
    if not fbxs:
        unreal.log("No new .fbx animations to import — already up to date.")
        return

    tasks = [build_import_task(f, skeleton) for f in fbxs]
    unreal.log(f"Importing {len(tasks)} animations onto {SKELETON_ASSET}")
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks)

    for t in tasks:
        for asset in t.imported_object_paths:
            unreal.log(f"  imported -> {asset}")
    unreal.log("Done. Save All (Ctrl+Shift+S) to persist.")


main()
