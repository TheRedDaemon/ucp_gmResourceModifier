# UCP Gm Resource Modifier

This repository contains a module for the "Unofficial Crusader Patch Version 3" (UCP3), a modification for Stronghold Crusader.
This module exposes the possibility to modify the loaded gm1 resources of the game.


### Motivation and Plan

The gm files of Crusader are handled differently than the tgx files.
While the later are loaded from disc on demand, the former are loaded at the start. (They might be a big part or even majority of the loading bar.)
Replacing them requires more than intercepting and changing paths.

For this reason, the control structures of the stored gm files were identified to replace them on demand.
In more detail, this module allows to load gm1-files, which are then stored as resources.
If requested and the wanted replacement fits, the control structures of the vanilla file are copied and then replaced with the other resource.
The main byte array is not touched. The replacement pointer simply points into the loaded resource instead.
Resetting is also possible and so it freeing the loaded resources (if they are not used anymore).

One can replace either a whole file or individual pictures. They need to be of the same type, though, since SHC stores different formats in a gm1 file.

As an additional feature, one can use this module to create a interface type single picture resource from a picture.
The converter calls Windows functions, so the supported depends on the OS (and what Wine supports).


### Usage

The module is part of the UCP3. Certain commits of the main branch of this repository are included as a git submodule.
It is therefore not needed to download additional content.

However, should issues or suggestions arise that are related to this module, feel free to add a new GitHub issue.
Support is currently only guaranteed for the western versions of Crusader 1.41 and Crusader Extreme 1.41.1-E.
Other, eastern versions of 1.41 might work.


### C++-Exports

The API is provided via exported C functions and can be accessed via the `ucp.dll`-API.  
To make using the module easier, the header [gmResourceModifierHeader.h](ucp_gmResourceModifier/ucp_gmResourceModifier/gmResourceModifierHeader.h) can be copied into your project.  
It is used by calling the function *initModuleFunctions()* in your module once after it was loaded, for example in the lua require-call. It tries to receive the provided functions and returns *true* if successful. For this to work, the *gmResourceModifier* needs to be a dependency of your module.
The provided functions are the following:

* `int LoadGm1Resource(const char* filepath)`  
  Create a resource by trying to load the gm1-file the path points to.
  Returns the id of the resource to use for the other functions or `-1`, if it fails.
  Save the id! If it gets lost, one ends up with leaked memory in a way.

* `bool SetGm(int gmID, int imageInGm, int resourceId, int imageInResource)`  
  This options sets replaced gm data using the loaded resources. It takes different parameters:
    * The `gmID` is the index of the gm file in the games memory.  
    * The `imageInGm` is the index of the picture in the gm file.  
    * The `resourceId` is the id of the loaded resource.  
    * The `imageInResource` is the index of the picture in the resource file.  
  
  The actual action depends on the given parameters. In all cases the not mentioned parameters are `-1`:
    * `gmID`: Resets the requested gm-data to vanilla.
    * `gmID`, `imageInGm`: Resets the requested image in the gm-data to vanilla.
    * `gmID`, `resourceId`: Replaces the gm with the resource, if possible.
    * `gmID`, `imageInGm`, `imageInGm`, `resourceId`: Replaces the specific image, if possible.
  
  The return value indicates if it was successful.

* `bool FreeGm1Resource(int resourceId)`  
  Frees the resource, but only if it is used by nothing.
  The return value indicates if it was successful.

* `int LoadResourceFromImage(const char* filepath)`  
  Similar to `LoadGm1Resource`, but takes a path to an image file.
  If possible, the image is transformed to a single or multi(gif) picture resource of the interface type.
  It can therefore only be used by this or compatible types.
  The supported formats depend on the OS. 


### Lua-Exports

The Lua exports are parameters and functions accessible through the module object.

* `int LoadGm1Resource(string filepath)`  
  Identical to C++ version.

* `bool SetGm(int gmID, int imageInGm, int resourceId, int imageInResource)`  
  Identical to C++ version.

* `bool FreeGm1Resource(int resourceId)`  
  Identical to C++ version.

* `int LoadResourceFromImage(string filepath)`  
  Identical to C++ version.

### Special Thanks

To all of the UCP Team, the [Ghidra project](https://github.com/NationalSecurityAgency/ghidra) and
of course to [Firefly Studios](https://fireflyworlds.com/), the creators of Stronghold Crusader.
