# RePak
rpak building tool
---

## Using RePak:
* [Read here about the file structure and json format](https://r2northstar.readthedocs.io/en/latest/repak/)
    * The materials might be slightly different between r5 and r2
* look at something like [common_sdk.rpak](https://github.com/kralrindo/common_sdk.rpak) or [ui_sdk.rpak](https://github.com/AyeZeeBB/ui_sdk.rpak) for examples
* You can use [Zee's atlas creator](https://atlas.r5reloaded.com/) to easily generate the atlas and json
* Drag the json on to repak.exe to create the rpak

## Loading the rpak in R5:
* Place the created rpak in `R5R\paks\Win32` or `\Win64`
* In the scripts, write `pak_requestload( "name" )` somewhere 
    * Probably in some initialization file, like loading the survival game mode
* Call the asset from the path specified in the json config