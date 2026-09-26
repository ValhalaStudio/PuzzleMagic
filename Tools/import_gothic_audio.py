import unreal

eal = unreal.EditorAssetLibrary
raw_dir = r"D:\Unreal Projects\PuzzleGame5x5\RawAudio"
dest = "/Game/Audio"
import sys

# gothic_audio.py + chant_audio.py outputs. Pass names on the command line to import only those.
names = ["SFXG_Place", "SFXG_Clear", "SFXG_Blessed", "SFXG_Holy", "SFXG_GameOver", "SFXG_Gargoyle",
         "SFXG_Relic", "SFXG_ComboLost", "SFXG_Thunder", "MUS_Gothic",
         "SFXG_Strike", "SFXG_Hex", "SFXG_Ward", "MUS_Chant",
         "SFXG_Flicker", "AMB_Abyss"]  # eldritch_audio.py
wanted = [a for a in sys.argv[1:] if a in names]
if wanted:
    names = wanted

tasks = []
for name in names:
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", f"{raw_dir}\\{name}.wav")
    task.set_editor_property("destination_path", dest)
    task.set_editor_property("destination_name", name)
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("automated", True)
    task.set_editor_property("save", True)
    task.set_editor_property("factory", unreal.SoundFactory())
    tasks.append(task)
unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks)

for name in names:
    path = f"{dest}/{name}"
    if not eal.does_asset_exist(path):
        unreal.log_error(f"import_gothic_audio: FAILED {path}")
        continue
    if name.startswith(("MUS_", "AMB_")):
        wave = eal.load_asset(path)
        wave.set_editor_property("looping", True)
        eal.save_loaded_asset(wave)
    unreal.log(f"import_gothic_audio: OK {path}")
unreal.log("import_gothic_audio: done")
