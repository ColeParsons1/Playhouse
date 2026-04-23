"""
Bulk-import Mixamo .fbx animations from:
  Content/Characters/Mixamo/Animations/
onto:
  /Game/Characters/Mixamo/Ch15_nonPBR_Skeleton

How to run:
  In UE editor -> Tools -> Execute Python Script -> pick this file.

Re-runnable behavior:
  - Skips FBX files that already have matching .uasset files.
  - Always attempts a post-import skeleton retarget fix.
"""

import os
import unreal


SKELETON_ASSET = "/Game/Characters/Mixamo/Ch15_nonPBR_Skeleton.Ch15_nonPBR_Skeleton"
ANIM_DIR_FS = os.path.normpath(
    unreal.SystemLibrary.get_project_content_directory() + "Characters/Mixamo/Animations/"
)
ANIM_DIR_PKG = "/Game/Characters/Mixamo/Animations"

HIPS_BONE_CANDIDATES = (
    "Hips",
    "mixamorig:Hips",
    "hips",
    "mixamorig_hips",
)


def find_new_fbxs():
    """Return absolute .fbx paths that do not yet have matching .uasset files."""
    out = []
    if not os.path.isdir(ANIM_DIR_FS):
        return out
    for fname in sorted(os.listdir(ANIM_DIR_FS)):
        if not fname.lower().endswith(".fbx"):
            continue
        stem = os.path.splitext(fname)[0]
        if os.path.exists(os.path.join(ANIM_DIR_FS, stem + ".uasset")):
            continue
        out.append(os.path.join(ANIM_DIR_FS, fname))
    return out


def build_import_task(fbx_path, skeleton):
    """Build an AssetImportTask for an FBX as Animation (no mesh/material import)."""
    fbx_options = unreal.FbxImportUI()
    fbx_options.import_animations = True
    fbx_options.import_mesh = False
    fbx_options.import_as_skeletal = True
    fbx_options.import_materials = False
    fbx_options.import_textures = False
    fbx_options.skeleton = skeleton
    fbx_options.mesh_type_to_import = unreal.FBXImportType.FBXIT_ANIMATION
    fbx_options.anim_sequence_import_data.set_editor_property("import_uniform_scale", 1.0)
    fbx_options.anim_sequence_import_data.set_editor_property("use_default_sample_rate", True)

    task = unreal.AssetImportTask()
    task.filename = fbx_path
    task.destination_path = ANIM_DIR_PKG
    task.replace_existing = True
    task.automated = True
    task.save = True
    task.options = fbx_options
    return task


def _get_enum_value(*candidate_names):
    enum_cls = getattr(unreal, "BoneTranslationRetargetingMode", None)
    if not enum_cls:
        return None
    for name in candidate_names:
        if hasattr(enum_cls, name):
            return getattr(enum_cls, name)
    return None


def _set_bone_retarget_mode(skeleton, bone_name, mode, recurse):
    """
    Apply one bone retarget mode, tolerating API differences across UE versions.
    """
    method_names = (
        "set_bone_translation_retargeting_mode",
        "set_bone_translation_retargeting_mode_by_name",
    )
    for method_name in method_names:
        method = getattr(skeleton, method_name, None)
        if not callable(method):
            continue
        try:
            method(bone_name, mode, recurse)
            return True
        except TypeError:
            try:
                method(bone_name, mode)
                return True
            except Exception:
                continue
        except Exception:
            continue
    return False


def _iter_reference_bone_names(skeleton):
    """
    Yield reference skeleton bone names if available in this UE build.
    """
    getters = (
        lambda: skeleton.get_reference_skeleton(),
        lambda: skeleton.get_editor_property("reference_skeleton"),
    )

    for get_ref in getters:
        try:
            ref = get_ref()
        except Exception:
            continue
        if not ref:
            continue
        for get_names in ("get_raw_bone_names", "get_bone_names"):
            fn = getattr(ref, get_names, None)
            if callable(fn):
                try:
                    for name in fn():
                        yield str(name)
                except Exception:
                    pass


def _find_hips_bone(skeleton):
    """
    Resolve the most likely Hips bone name on a Mixamo skeleton.
    """
    names = set(HIPS_BONE_CANDIDATES)
    for name in _iter_reference_bone_names(skeleton):
        names.add(name)

    for name in sorted(names):
        lower = name.lower()
        if lower == "hips" or lower.endswith(":hips") or lower.endswith("_hips"):
            return name
    for name in HIPS_BONE_CANDIDATES:
        if name in names:
            return name
    return HIPS_BONE_CANDIDATES[0]


def apply_mixamo_retarget_fix(skeleton):
    """
    Equivalent to manual Skeleton Tree fix:
      1) Root recursively -> Skeleton
      2) Hips only -> AnimationScaled
    """
    mode_skeleton = _get_enum_value("SKELETON", "SKELETON_MODE")
    mode_anim_scaled = _get_enum_value("ANIMATION_SCALED", "ANIM_SCALED", "ANIMATIONSCALED")

    if mode_skeleton is None or mode_anim_scaled is None:
        unreal.log_warning(
            "Could not resolve BoneTranslationRetargetingMode enum; "
            "skipping auto-retarget fix."
        )
        return False

    hips_name = _find_hips_bone(skeleton)

    root_ok = _set_bone_retarget_mode(skeleton, "root", mode_skeleton, True)
    hips_ok = _set_bone_retarget_mode(skeleton, hips_name, mode_anim_scaled, False)

    if not root_ok:
        # Many Mixamo skeletons are rooted at Hips rather than an explicit root bone.
        for fallback_root in ("Hips", "mixamorig:Hips", hips_name):
            if _set_bone_retarget_mode(skeleton, fallback_root, mode_skeleton, True):
                root_ok = True
                break

    if root_ok and hips_ok:
        unreal.EditorAssetLibrary.save_loaded_asset(skeleton)
        unreal.log(
            f"Retarget fix applied on skeleton: recursive root=Skeleton, {hips_name}=AnimationScaled."
        )
        return True

    unreal.log_warning(
        "Retarget fix was only partially applied. "
        f"root_ok={root_ok}, hips_ok={hips_ok}. "
        "If preview is still jumbled, set Skeleton Tree manually: recursive Skeleton + Hips AnimationScaled."
    )
    return False


def main():
    skeleton = unreal.EditorAssetLibrary.load_asset(SKELETON_ASSET)
    if not skeleton:
        unreal.log_error(f"Skeleton not found at {SKELETON_ASSET}")
        return

    fbxs = find_new_fbxs()
    if not fbxs:
        unreal.log("No new .fbx animations to import - already up to date.")
    else:
        tasks = [build_import_task(path, skeleton) for path in fbxs]
        unreal.log(f"Importing {len(tasks)} animations onto {SKELETON_ASSET}")
        unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks)

        for task in tasks:
            for asset_path in task.imported_object_paths:
                unreal.log(f"  imported -> {asset_path}")

    apply_mixamo_retarget_fix(skeleton)
    unreal.log("Done. Save All (Ctrl+Shift+S) to persist.")


main()
