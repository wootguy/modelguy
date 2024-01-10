### This tool is mostly obsolete. Use HLAM instead. It can do almost everything this program can, and works with more models.
https://github.com/Solokiller/HL_Tools/releases


# modelguy
This tool modifies Half-Life models without decompiling them.

Usage:  
`modelguy <command> <parameters> <input.mdl> [output.mdl]`

## Commands
* `merge` - Merges external texture and sequence models into the output model. If no output file is specified, the external models will also be deleted.  
* `crop` - Crops a texture to the specified dimensions. Used after compiling model. Takes `<width>x<height>` as parameters.
* `rename` - Renames a texture. Takes `<old name> <new name>` as parameters.
* `info` - Write model info to a JSON file. Takes `<input.mdl> <output.json>` as parameters.

Examples:  
`modelguy merge barney.mdl`  
`modelguy crop face.bmp 100x80 hgrunt.mdl`  
`modelguy rename hev_arm.bmp Remap1_000_255_255.bmp v_shotgun.mdl`  
`modelguy info barney.mdl barney.json`
