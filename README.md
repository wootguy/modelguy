# modelguy
This tool modifies GoldSrc models without decompiling them. It's intended for use with my [player model database](https://github.com/wootguy/pmodels). There is a 3D view and some command-line options for editing models but you're better off using [HLAM](https://github.com/SamVanheer/HalfLifeAssetManager/releases) for general editing/viewing.

# Sven Co-op Model Ports
This tool was recently used (2025/07/13) to port 9000+ SC models to HL. The readmes are linked to this page, so here's more info on what was done to those models:
- Dummy animations were inserted for `run`, `walk`, `aim_2`, `shoot_2`, `aim_1`, and `shoot_1`. Neither SC nor HL make use of these, but they need to be there so that animations have the expected indexes for HL.
- Animations specific to SC were appended after the HL animations (for use with [Half-Life Co-op](https://github.com/wootguy/SevenKewp)).
- The `jump`, `ref_shoot_shotgun`, and `crouch_shoot_shotgun` were cut short so they don't play too fast.
  - The original animations were preserved and appended after the HL anims.
- The `ref_shoot_bow`, `crouch_shoot_bow`, `ref_shoot_rpg`, `crouch_shoot_rpg`, `ref_shoot_squeak`, `crouch_shoot_squeak` animations were extended so that they don't play too slowly in HL. This is reversible without duplicating animations.
- The `longjump` animation was duplicated because hit detection for the SC animation is very different than the HL one. HL has a lot of forward movement built into it while SC has none. Longjumping with these models will make you hard to hit unless a server mod chooses the alternate animation (requires `player.mdl` to have both animations).
- Large textures were downscaled to a maximuim of 512x512 pixels (the client crashes with no error message otherwise).
- Textures were resized to a multiple of 16 pixels (client crashes with `GL_Upload16: s&3` otherwise).
- Chrome textures were resized to exactly 64x64 (required for special effects like the eye tracking used in km_karenkujo).
- The `flatshade` flag was enabled on `fullbright` textures (fullbright doesn't work in HL).
- External texture/animation models were removed.
- Animation sound event paths were given `.wav` file extensions (HL doesn't support ogg/mp3/etc.).

I did the bare minimum testing to make sure the models don't crash the client, but I expect the conversions aren't perfect. A long suffix was added (`_v1sc`) in case authors want to do their own ports using original names, and in case I need to redo all the ports for some reason. About 700 models had their names shortened to make room for the suffix (model names are limited to 22 characters).

# Usage
`modelguy <command> <parameters> <input.mdl> [output.mdl]`

## Commands
```
  merge  : Merges external texture and sequence models into the output model.
           If no output file is specified, the external models will also be deleted.
  crop   : Crops a texture to the specified dimensions. Takes <width>x<height> as parameters.
  resize : Resizes a texture to the specified dimensions. Takes <width>x<height> as parameters.
  rename : Renames a texture. Takes <old name> <new name> as parameters.
  info   : Write model info to a JSON file. Takes <input.mdl> <output.json> as parameters.
  wavify : Apply .wav extension to all events. Takes <input.mdl> <output.json> as parameters.
  porthl : Port a Sven Co-op player model to Half-Life. Takes <input.mdl> and <output.mdl> as parameters.
  type   : Identify player model type. The return code is unique per mod.
  view   : View the model in 3D.
  image  : Saves a PNG image of the model. Takes <width>x<height> and <output.png> as parameters.
  layout : Show data layout for the MDL file.
  optimize : deduplicate data.
```

Examples:  
```
modelguy merge barney.mdl
  modelguy crop face.bmp 100x80 hgrunt.mdl
  modelguy rename hev_arm.bmp Remap1_000_255_255.bmp v_shotgun.mdl
  modelguy image player.mdl 800x400 player.png
  modelguy porthl vtuber_kizuna.mdl vtuber_kizuna_v1sc.mdl
```
