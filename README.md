# IVTC DN

![IVTC DN](https://user-images.githubusercontent.com/3258334/169749030-038619c2-1534-41d2-9034-212c4813a457.png)

IVTC DN is a GUI tool to manually perform Inverse Telecine (IVTC) of [Vapoursynth](https://github.com/vapoursynth/vapoursynth) scripts. It uses [Walnut](https://github.com/TheCherno/Walnut), which uses [Dear ImGui](https://github.com/ocornut/imgui), to render the UI.

The intended outcome is similar to that of other manual IVTC tools such as [Wobbly](https://github.com/dubhater/Wobbly) and [Yatta](https://github.com/hatt/Yatta), but the approach taken by IVTC DN is different.

What IVTC DN does:
 - Does clearly display every input field to the user
 - Does allow the user to make field matching and decimation decisions at the same time
 - Does limit required prerequisite plugins
 - Does output a single project file; can be used for one-line IVTC in scripts

What IVTC DN doesn't do:
 - Doesn't support anything other than constant 10 input field -> 4 output frame cycles (not a priority)
 - Doesn't handle errors well (high priority)
 - Doesn't provide any automated pattern guessing (low priority)
 - Doesn't provide feedback on where bad matches may remain in output (medium priority)

# Usage

## Keybindings

While moused over an input field these keys will operate on that field:
 - `A, B, C, D` Changes the "note" of the field to the respective letter.
 - `1, 2, 3, 4` Changes the "action" of the field to indicate that it should used for the respective output frame (note: the output frames are internally numbered [0, 3] but the `0` key is far away on a qwerty keyboard).
 - `S` Marks that this field starts a new scene.

While moused over an output frame these keys will operate on that frame:
 - `F` Toggles which frame (Previous/Next) should be frozen if no input fields are used for the output frame.

Keys that apply generally:
 - `R` Reload the output frames to reflect any changes.
 - `T` Apply the actions and notes of the fields in the current cycle to all other cycles in the same scene (note: this is not extensively tested and may have unhandled edge cases).
 - `Ctrl+S` Save the current project file.
 - `Ctrl+O` Open an existing project file.
 - `Ctrl+N` Start a new project.

## Pre-requisites

Users are expected to have a working Vapoursynth installation (portable is fine). See the [Vapoursynth Docs](https://www.vapoursynth.com/doc/) for details on what it is and how to install it.

Additionally the [IVTC DN plugin](https://github.com/Mikewando/IVTC-DN-plugin) is required and must be placed somewhere that Vapoursynth will [autoload](https://www.vapoursynth.com/doc/installation.html#plugin-autoloading).

IVTC DN projects are based on input Vapoursynth scripts, so you must have such a script to use the tool. The script must output a `YUV` or `RGB` clip with the `_FieldBased` property set to `1` (bottom field first) or `2` (top field first). The property may be set the source filter (e.g. `d2v.Source()`) or it can be set manually (e.g. `std.SetFrameProps(_FieldBased=2)`). Example input script:

```python
import vapoursynth as vs

clip = vs.core.ffms2.Source("example.mkv") # Load telecined source video
clip = clip.std.SetFrameProps(_FieldBased=2) # Manually set TFF
clip.set_output() # Output is field-based telecined content
```

## Getting Started

The first time you open IVTC DN there will be a few small panels (one for input fields, one for output frames, and one for controls). You can resize these as you see fit, drag them out of the window, or dock them onto various points of the window. Their position will be remembered for subsequent use by the `imgui.ini` file which should be created alongside the ITVC DN binary.

Once you have the panels arranged to your liking you can load the input script. To do so you can simply drag and drop your `.vpy` file onto the IVTC DN window, or you can select `File > New project...` and navigate to the input script.

## IVTC Something

Once you can see input fields and output frames you can choose which fields should be used for each output frame by adjusting the actions of each input field. See the `Keybindings` section for more details.

The `A|B|C|D` notes displayed on fields are purely informational, and are simply intended to help a user keep track of the cycle. The idea is that it is easier for a user to select the "best" version of a "duplicate" field if the fields are annotated.

If you select only a single field for an output frame that field will be line-doubled naively. You can override the line-doubling behavior when you use the project file in an output script, but within the GUI the behavior will always be naive.

If you deselect all fields for an output frame it will be replaced with the previous available output frame by default. You can toggle this behavior to use the next available output frame by pressing `F` on the output frame.

Once you are happy with your result (or if you just want to save progress) you can save the project file by pressing `Ctrl+S` or selecting `File > Save project`. If it's the first time you saved you will be prompted to select a destintation for the project file (which uses `.ivtc` as the extension).

## Using the Project File

Using the project file in an output script is straightforward. The [IVTC DN plugin](https://github.com/Mikewando/IVTC-DN-plugin) README has more details, but as a simple example:

```python
import vapoursynth as vs

clip = vs.core.ffms2.Source("example.mkv") # Load telecined source video
clip = clip.std.SetFrameProps(_FieldBased=2) # Manually set TFF
clip = clip.std.SeparateFields() # You could also omit the previous line and set tff=1 here
clip = clip.ivtcdn.IVTC("example.ivtc") # Use the IVTC DN project file
clip.set_output() # Output is progressive and IVTC'd content
```

# Building

I haven't spent much time testing builds on different systems, so this section is sparse. Broadly most dependencies should be bundled, so hopefully if you are familiar with C++ builds you can build it.

`premake5 vs2022` _should_ generate a visual studio 2022 project file for windows builds.

`premake5 gmake2` _should_ generate a gnu makefile for linux builds.

# 3rd party libaries

 - [Walnut](https://github.com/TheCherno/Walnut)
   - [Dear ImGui](https://github.com/ocornut/imgui)
   - [GLFW](https://github.com/glfw/glfw)
   - [stb_image](https://github.com/nothings/stb)
   - [GLM](https://github.com/g-truc/glm)
 - [Vapoursynth](https://github.com/vapoursynth/vapoursynth)
 - [ImGuiFileDialog](https://github.com/aiekick/ImGuiFileDialog)
 - [libp2p](https://github.com/sekrit-twc/libp2p)
 - [nlohmann/json](https://github.com/nlohmann/json)
 - [miniz](https://github.com/richgel999/miniz)
 - [gzip-hpp](https://github.com/mapbox/gzip-hpp)
