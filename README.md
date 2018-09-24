# DirectQII
Readme


HOW VIDEO MODES WORK
==================================================================================================================================
Set vid_fullscreen 0 for a windowed mode or vid_fullscreen 1 for a fullscreen mode.  I don't currently offer a borderless-windowed mode, but if I ever add one it will be vid_fullscreen -1.

In a windowed mode set vid_width and vid_height to your desired width and height, followed by vid_restart to apply the changes.

In a fullscreen mode set vid_mode to the mode number you wish to use, followed by vid_restart to apply the changes.
Video modes are correctly enumerated from your display hardware rather than the engine having a hard-coded list of modes.
A fullscreen video mode includes width, height, refresh rate, scaling and scanline ordering combined in a single mode.

Set vid_vsync to 0 or 1 to toggle vsync off or on; this does not need a vid_restart.

Set vid_gamma to your desired gamma value; values between 0.7 (brighter) and 1.0 (darker) generally work well.  This does not need a vid_restart.
