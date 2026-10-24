# DEMO MUSIC: imports an audio file as /Game/Audio/MUS_Demo, the track the demo build plays in place of the chant (see
# APuzzleGameMode::PlayNextTrack). Re-run it to swap the tune.
# Run: UnrealEditor-Cmd.exe <uproject> -run=pythonscript -script="<this file> <audio file>"   (the editor must be closed)
import sys
import unreal

source = None
for arg in sys.argv[1:]:
    if arg.lower().endswith((".mp3", ".wav", ".ogg", ".flac")):
        source = arg
if source is None:
    unreal.log_error("import_demo_music.py: pass the audio file after the script path")
else:
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", source)
    task.set_editor_property("destination_path", "/Game/Audio")
    task.set_editor_property("destination_name", "MUS_Demo")
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("automated", True)
    task.set_editor_property("save", True)
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    paths = list(task.get_editor_property("imported_object_paths"))
    if paths:
        wave = unreal.EditorAssetLibrary.load_asset("/Game/Audio/MUS_Demo")
        unreal.log("import_demo_music.py: imported %s, %.1f s, %d channel(s), %d Hz" % (paths[0], wave.get_editor_property("duration"), wave.get_editor_property("num_channels"), wave.get_editor_property("sample_rate")))
    else:
        unreal.log_error("import_demo_music.py: the engine could not import " + source)
